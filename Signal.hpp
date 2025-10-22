#pragma once

#include <Reactive.hpp>
#include <functional>

namespace cppreactive {
    class Observer;

    /**
     * Singleton to manage the stack of active and scheduled observers. Only accessed
     * via pointer, so any changes to the underlying structure do not break ABI.
     * 
     * IMPORTANT: The `update()` function needs to be called by the user. This implementation
     * of signals makes no opinion on how you schedule observations, but it must be done
     * manually and outside of the Signal reaction for safety purposes.
     * 
     * It is rare you will have to interact with this class yourself, as Observatory is the
     * recommended way of managing observers.
     */
    class ObserverStack {
        template <typename T>
        friend class Signal;

        std::mutex m_mutex;
        std::vector<std::weak_ptr<Observer>> activeObs;
        std::vector<std::weak_ptr<Observer>> scheduledObs;

        ObserverStack() = default;

        // Pointer-to-impl!!!
        static bool observerSignalAdded(std::shared_ptr<Observer> ob, uint64_t id);
        static void observerAddSignal(std::shared_ptr<Observer> ob, uint64_t id, std::function<void()> unreactFunc);
     public:
        static ObserverStack* shared();

        // MUST BE CALLED BY USER
        void update();

        std::shared_ptr<Observer> create(std::function<void()> effect);
        std::shared_ptr<Observer> top();
        void run(std::shared_ptr<Observer> ob);
        void schedule(std::shared_ptr<Observer> ob);
    };


    /**
     * Scoped manager for observers. Allows you to create and destroy Observer instances. That's it!
     */
    class Observatory {
        std::mutex m_mutex;
        std::vector<std::shared_ptr<Observer>> m_observers;
     public:
        Observatory() = default;

        template <typename F>
        std::shared_ptr<Observer> reactToChanges(F&& effect) {
            std::lock_guard<std::mutex> lock(m_mutex);

            auto ob = ObserverStack::shared()->create(std::forward<F>(effect));
            m_observers.push_back(ob);

            ObserverStack::shared()->run(ob);

            return ob;
        }

        void unreact(std::shared_ptr<Observer> ob);
    };

    static uint64_t s_signalCounter = 0;
    template <typename T>
    class Signal {
     protected:
        Reactive<T> m_reactive;
        const uint64_t m_id = s_signalCounter++;
     public:

        Signal() : m_reactive() {}
        Signal(T const& initial) : m_reactive(initial) {}
        Signal(Signal const& other) : m_reactive(other.m_reactive) {}
        Signal(Signal&& other) : m_reactive(std::move(other.m_reactive)), m_id(other.m_id) {}

        Reactive<T>& operator*() {
            if (auto top = ObserverStack::shared()->top()) {
                if (!ObserverStack::observerSignalAdded(top, m_id)) {
                    auto ptr = m_reactive.react([top](auto) {
                        ObserverStack::shared()->schedule(top);
                    });

                    ObserverStack::observerAddSignal(top, m_id, [ptr, ref = m_reactive.ref()]() mutable {
                        ref.unreact(ptr);
                    });
                }
            }

            return m_reactive;
        }

        uint64_t id() const { return m_id; }
    };

    template <typename T>
    class ComputedSignal : Signal<T> {
        Observatory m_observatory;
        std::function<T()> m_compute;
     public:
        ComputedSignal(decltype(m_compute) compute) : m_compute(compute) {
            m_observatory.reactToChanges([this]() {
                Signal<T>::operator*() = m_compute();
            });
        }

        Reactive<T> const& operator*() {
            return Signal<T>::operator*();
        }
    };
}
