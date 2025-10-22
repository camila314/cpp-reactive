#include <Signal.hpp>

using namespace cppreactive;

/**
 * Holds the effect function and stores references to any attached signals.
 * 
 * Signals are stored by their ID and a function that unreacts them. I store
 * a function for proper type erasure as Reactive is templated and this is not.
 * 
 * Completely internally-used type. To the end user, this is completely opaque.
 * No instances of Observer are ever stored outside of heap-allocated space managed
 * by ObserverStack, meaning that changing the underlying structure does not break ABI.
 */
struct cppreactive::Observer {
    std::mutex m_mutex;
    std::function<void()> const m_effect;
    std::unordered_map<uint64_t, std::function<void()>> m_signals;

    Observer(std::function<void()> effect) : m_effect(effect) {}
    Observer(Observer const&) = delete;

    bool signalAdded(uint64_t id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_signals.find(id) != m_signals.end();
    }
    void addSignal(uint64_t id, std::function<void()> unreactFunc) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_signals[id] = unreactFunc;
    }
    void unreactAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [id, func] : m_signals) {
            func();
        }
        m_signals.clear();
    }

    ~Observer() {
        unreactAll();
    }
};

bool ObserverStack::observerSignalAdded(std::shared_ptr<Observer> ob, uint64_t id) {
    return ob->signalAdded(id);
}
void ObserverStack::observerAddSignal(std::shared_ptr<Observer> ob, uint64_t id, std::function<void()> unreactFunc) {
    return ob->addSignal(id, unreactFunc);
}


// MUST BE CALLED BY USER
void ObserverStack::update() {
    m_mutex.lock();
    auto scheduled = scheduledObs;
    m_mutex.unlock();

    for (auto& ptr : scheduled) {
        if (auto lock = ptr.lock()) {
            run(lock);
        }
    }

    m_mutex.lock();
    scheduledObs.clear();
    m_mutex.unlock();
}



ObserverStack* ObserverStack::shared() {
    static ObserverStack instance;
    return &instance;
}

std::shared_ptr<Observer> ObserverStack::create(std::function<void()> effect) {
    auto ptr = std::make_shared<Observer>(std::move(effect));

    return ptr;
}

std::shared_ptr<Observer> ObserverStack::top() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (activeObs.empty())
        return nullptr;

    if (auto lock = activeObs.back().lock()) {
        return lock;
    } else {
        activeObs.pop_back();
        return top();
    }
}

void ObserverStack::run(std::shared_ptr<Observer> ob) {
    m_mutex.lock();

    // Avoid circular effects while also pruning
    for (size_t i = 0; i < activeObs.size(); ++i) {
        if (auto lock = activeObs[i].lock()) {
            if (lock == ob) {
                std::cout << "circular!\n";
                return;
            }
        } else {
            activeObs.erase(activeObs.begin() + i);
            --i;
        }
    }

    activeObs.push_back(ob);
    ob->unreactAll();

    m_mutex.unlock();
    ob->m_effect();
    m_mutex.lock();

    activeObs.pop_back();

    m_mutex.unlock();
}

void ObserverStack::schedule(std::shared_ptr<Observer> ob) {
    std::lock_guard<std::mutex> lock(m_mutex);
    scheduledObs.push_back(ob);
}

void Observatory::unreact(std::shared_ptr<Observer> ob) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_observers.erase(std::remove(m_observers.begin(), m_observers.end(), ob), m_observers.end());
}
