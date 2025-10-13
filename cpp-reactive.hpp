#pragma once

#include <iostream>
#include <list>
#include <thread>
#include <vector>
#include <unordered_set>
#include <optional>
#include <functional>
#include <memory>
#include <type_traits>

namespace cppreactive {
    template <typename T>
    class Reactive {
        /// Weak reference to Reactive class
        class Weak {
         protected:
            Reactive* m_reactive;
            std::mutex m_mutex;
            friend class Reactive;
         public:
            Weak(Reactive& r) : m_reactive(&r) {}
            auto lock() {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (!m_reactive) return false;

                struct Ret {
                    Reactive* reactive;
                    std::unique_lock<std::mutex> lock;
                    operator bool() const { return reactive != nullptr; }
                };

                return Ret {m_reactive, std::move(lock)};
            }
        };
        friend class Weak;

        T m_value;
        std::unordered_set<std::thread::id> m_contexts;
        std::list<std::function<void(T const&)>> m_listeners;
        std::vector<std::unique_ptr<Weak>> m_refs;
        mutable std::mutex m_mutex;

        void removeWeak(std::unique_ptr<Weak>& weak) {
            std::lock_guard<std::mutex> lock(m_mutex);

            auto it = std::remove(m_refs.begin(), m_refs.end(), weak);
            m_refs.erase(it, m_refs.end());
        }
     public:
        using ListenerIter = typename decltype(m_listeners)::iterator;

        /// Session allows you to obtain a mutable reference to the value while ensuring it properly triggers reactions. 
        /// They provide a thread-safe way to access Reactive, but a Session instance should not be used across threads.
        class Session {
            std::unique_ptr<Weak> m_weak;
            T m_tempVal;
            Session(std::unique_ptr<Weak>&& w) : m_weak(std::move(w)), m_tempVal(m_weak->m_reactive) {
                std::lock_guard<std::mutex> lock(m_weak->m_reactive.m_mutex);
                m_weak->m_reactive.m_contexts.insert(std::this_thread::get_id());
            }
            friend class Reactive;
         public:
            Session(Session const&) = delete;
            void operator=(Session const&) = delete;
            Session(Session&& g) : m_weak(std::move(g.m_weak)), m_tempVal(std::move(g.m_tempVal)) {}

            T operator->() requires std::is_pointer_v<T> { return m_tempVal; }
            T* operator->() requires (!std::is_pointer_v<T>) { return &m_tempVal; }
            T& operator*() { return m_tempVal; }
            void operator=(T const& t) { m_tempVal = t; }
            void operator=(T&& t) { m_tempVal = t; }

            operator T() const {
                return m_tempVal;
            }

            ~Session() {
                if (auto guard = m_weak->lock()) {
                    m_weak->m_reactive.m_mutex.lock();
                    m_weak->m_reactive.m_contexts.erase(std::this_thread::get_id());
                    m_weak->m_reactive.m_mutex.unlock();

                    m_weak->m_reactive.removeWeak(m_weak);
                    m_weak->m_reactive.set(m_tempVal);
                }
            }
        };
        friend class Session;

        /// Ref is a way to scope reactions and obtain a non-owning reference to a Reactive class that guarantees memory safety
        class Ref {
            struct _list_iterator_hash {
                size_t operator()(ListenerIter const& i) const {
                    return std::hash<decltype(&*i)>()(&*i);
                }
            };

            std::mutex m_mutex;
            std::unordered_set<ListenerIter, _list_iterator_hash> m_listeners;
            std::unique_ptr<Weak> m_weak;
            Ref(std::unique_ptr<Weak>&& w) : m_weak(std::move(w)) {}
            friend class Reactive;
         public:
            Ref(Ref&& r) : m_weak(std::move(r.m_weak)) {
                std::lock_guard<std::mutex> lock(r.m_mutex);
            
                m_listeners = std::move(r.m_listeners);
                m_weak = std::move(r.m_weak);
            }
            Ref() = default;
            Ref const& operator=(Ref&& r) {
                std::lock_guard<std::mutex> lock(r.m_mutex);

                m_weak = std::move(r.m_weak);
                m_listeners = std::move(r.m_listeners);
            }

            std::optional<ListenerIter> react(std::function<void(T const&)> fn) {
                if (auto guard = m_weak->lock()) {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    auto lis = m_weak->m_reactive.react(fn);
                    m_listeners.insert(lis);
                    return lis;
                }
                return {};
            }

            void unreact(ListenerIter it) {
                if (auto guard = m_weak->lock()) {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_weak->m_reactive.unreact(it);
                    m_listeners.erase(it);
                }
            }

            std::optional<T> get() {
                if (auto guard = m_weak->lock()) {
                    return m_weak->m_reactive.get();
                }
                return {};
            }

            template <typename Q>
            bool set(Q&& val) {
                if (auto guard = m_weak->lock()) {
                    m_weak->m_reactive.set(val);
                    return true;
                }
                return false;
            }

            std::optional<Session> session() requires std::is_copy_constructible_v<T> {
                if (auto guard = m_weak->lock()) {
                    return m_weak->m_reactive.session();
                }
                return {};
            }

            ~Ref() {
                if (auto guard = m_weak->lock()) {
                    for (auto const& lis : m_listeners)
                        m_weak->m_reactive.unreact(lis);
                    m_weak->m_reactive.removeWeak(m_weak);
                }
            }
        };
        friend class Ref;

        Reactive(T const& initial) : m_value(initial) {}
        Reactive() requires std::is_default_constructible_v<T> : m_value() {}
        Reactive(T&& initial) : m_value(std::move(initial)) {}
        Reactive(Reactive const& other) : m_value(other.m_value) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_value = other.m_value;
        }
        Reactive(Reactive&& other) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_value = std::move(other.m_value);
        }

        ~Reactive() {
            std::lock_guard<std::mutex> lock(m_mutex);

            for (auto& ref : m_refs)
                ref->m_mutex.lock();

            for (auto& ref : m_refs) {
                ref->m_reactive = nullptr;
                ref->m_mutex.unlock();
            }
        }


        template <typename Q> // Must do this or else it won't be a forwarding ref
        void set(Q&& val) {
            m_mutex.lock();

            auto this_id = std::this_thread::get_id();

            if (m_contexts.contains(this_id)) {
                std::cerr << "Attempt to modify value within its own listener!" << std::endl;
                return;
            }
            m_contexts.insert(this_id);

            m_value = std::forward<Q>(val);
            auto copy = m_listeners;
            m_mutex.unlock();

            for (auto const& fn : copy)
                fn(std::forward<Q>(val));

            m_mutex.lock();
            m_contexts.erase(this_id);
            m_mutex.unlock();
        }

        T const& get() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_value;
        }

        template <typename Q>
        Reactive<T>& operator=(Q&& val) {
            set(std::forward<Q>(val));
            return *this;
        }


        Reactive<T>& operator=(Reactive<T> const& val) {
            set(*val);
            return *this;
        }
        operator T() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_value;
        }

        std::remove_pointer_t<T> const* operator->() const requires (std::is_pointer_v<T>) {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_value;
        }
        std::remove_pointer_t<T> const& operator*() const requires (std::is_pointer_v<T>) {
            std::lock_guard<std::mutex> lock(m_mutex);
            return *m_value;
        }

        T const* operator->() const requires (!std::is_pointer_v<T>) {
            std::lock_guard<std::mutex> lock(m_mutex);
            return &m_value;
        }
        T const& operator*() const requires (!std::is_pointer_v<T>) {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_value;
        }

        ListenerIter react(std::function<void(T const&)> fn) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_listeners.push_back(fn);
            return --m_listeners.end();
        }
        void unreact(ListenerIter it) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_listeners.erase(it);
        }

        bool isInContext() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_contexts.contains(std::this_thread::get_id());
        }

        Session session() requires std::is_copy_constructible_v<T> {
            return Session(std::make_unique<Weak>(Weak(*this)));
        }
        Ref ref() {
            return Ref(std::make_unique<Weak>(Weak(*this)));
        }
    };
}
