#pragma once

#include <iostream>
#include <list>
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
            Reactive& m_reactive;
            bool m_valid;
            friend class Reactive;
         public:
            Weak(Reactive& r) : m_reactive(r), m_valid(true) {}
            bool isValid() {
                return m_valid;
            }
        };
        friend class Weak;

        T m_value;
        bool m_inCtx;
        std::list<std::function<void(T const&)>> m_listeners;
        std::vector<std::unique_ptr<Weak>> m_refs;

        void removeWeak(std::unique_ptr<Weak>& weak) {
            auto it = std::remove(m_refs.begin(), m_refs.end(), weak);
            m_refs.erase(it, m_refs.end());
        }
     public:
        using ListenerIter = typename decltype(m_listeners)::iterator;

        /// Session allows you to obtain a mutable reference to the value while ensuring it properly triggers reactions 
        class Session {
            std::unique_ptr<Weak> m_weak;
            T m_tempVal;
            Session(std::unique_ptr<Weak>&& w) : m_weak(std::move(w)), m_tempVal(m_weak->m_reactive) {
                m_weak->m_reactive.m_inCtx = true;
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

            bool isValid() {
                return m_weak && m_weak->isValid();
            }

            ~Session() {
                if (isValid()) {
                    m_weak->m_reactive.m_inCtx = false;
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

            std::unordered_set<ListenerIter, _list_iterator_hash> m_listeners;
            std::unique_ptr<Weak> m_weak;
            Ref(std::unique_ptr<Weak>&& w) : m_weak(std::move(w)) {}
            friend class Reactive;
         public:
            Ref(Ref&& r) : m_weak(std::move(r.m_weak)), m_listeners(std::move(r.m_listeners)) {}
            Ref() = default;
            Ref const& operator=(Ref&& r) {
                m_weak = std::move(r.m_weak);
                m_listeners = std::move(r.m_listeners);
            }

            bool isValid() {
                return m_weak && m_weak->isValid();
            }

            std::optional<ListenerIter> react(std::function<void(T const&)> fn) {
                if (!isValid()) return {};
                auto lis = m_weak->m_reactive.react(fn);
                m_listeners.insert(lis);
                return lis;
            }

            void unreact(ListenerIter it) {
                if (!isValid()) return;
                m_weak->m_reactive.unreact(it);
                m_listeners.erase(it);
            }

            std::optional<T> get() {
                if (!isValid()) return {};
                return m_weak->m_reactive.get();
            }

            template <typename Q>
            bool set(Q&& val) {
                if (!isValid()) return false;
                m_weak->m_reactive.set(val);
                return true;
            }

            std::optional<Session> session() requires std::is_copy_constructible_v<T> {
                if (!isValid()) return {};
                return m_weak->m_reactive.session();
            }

            ~Ref() {
                if (isValid()) {
                    for (auto const& lis : m_listeners)
                        m_weak->m_reactive.unreact(lis);
                    m_weak->m_reactive.removeWeak(m_weak);
                }
            }
        };
        friend class Ref;

        Reactive(T const& initial) : m_value(initial), m_inCtx(false) {}
        Reactive() requires std::is_default_constructible_v<T> : m_value(), m_inCtx(false) {}
        Reactive(T&& initial) : m_value(std::move(initial)), m_inCtx(false) {}
        Reactive(Reactive const& other) : m_value(other.m_value), m_inCtx(false) {}
        Reactive(Reactive&& other) : m_value(std::move(other.m_value)), m_inCtx(false) {}

        template <typename Q> // Must do this or else it won't be a forwarding ref
        void set(Q&& val) {
            if (m_inCtx) {
                std::cerr << "Attempt to modify value within its own listener!" << std::endl;
                return;
            }

            m_value = std::forward<Q>(val);
            m_inCtx = true;
            for (auto const& fn : m_listeners)
                fn(m_value);
            m_inCtx = false;
        }

        T const& get() const { return m_value; }

        template <typename Q>
        Reactive<T>& operator=(Q&& val) {
            set(std::forward<Q>(val));
            return *this;
        }


        Reactive<T>& operator=(Reactive<T> const& val) {
            set(*val);
            return *this;
        }
        operator T() const { return m_value; }

        std::remove_pointer_t<T> const* operator->() const requires (std::is_pointer_v<T>) {
            return m_value;
        }
        std::remove_pointer_t<T> const& operator*() const requires (std::is_pointer_v<T>) { return *m_value; }

        T const* operator->() const requires (!std::is_pointer_v<T>) {
            return &m_value;
        }
        T const& operator*() const requires (!std::is_pointer_v<T>) { return m_value; }

        ListenerIter react(std::function<void(T const&)> fn) {
            m_listeners.push_back(fn);
            return --m_listeners.end();
        }
        void unreact(ListenerIter it) {
            m_listeners.erase(it);
        }

        bool isInContext() const { return m_inCtx; }

        Session session() requires std::is_copy_constructible_v<T> {
            return Session(std::make_unique<Weak>(Weak(*this)));
        }
        Ref ref() {
            return Ref(std::make_unique<Weak>(Weak(*this)));
        }
    };
}
