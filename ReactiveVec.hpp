#include <vector>
#include <Reactive.hpp>
#include <Signal.hpp>

namespace cppreactive {
	template <typename T>
	class ReactiveVec : public Reactive<std::vector<T>> {
	 public:
	    using Reactive<std::vector<T>>::Reactive;

	    struct Setter {
	        ReactiveVec::Ref ref;
	        T value;
	        size_t idx;

	        template <std::convertible_to<T> Q>
	        void operator=(Q&& value) {
	            if (auto ses = ref.session()) {
	                (*ses)->at(idx) = std::forward<Q>(value);
	            }
	        }

	        operator T() const {
	            return value;
	        }
	    };

	    template <std::convertible_to<T> Q>
	    void push_back(Q&& value) {
	        this->session()->push_back(std::forward<Q>(value));
	    }

	    template <typename ...Args>
	    void emplace_back(Args... args) {
	        this->session()->emplace_back(args...);
	    }

	    T pop_back() {
	        return this->session()->pop_back();
	    }

	    template <std::convertible_to<T> Q>
	    void insert(size_t index, Q&& value) {
	        this->session()->insert(this->get().begin() + index, std::forward<Q>(value));
	    }

	    void erase(size_t index) {
	        this->session()->erase(this->get().begin() + index);
	    }
	    void erase(size_t start, size_t end) {
	        this->session()->erase(this->get().begin() + start, this->get().begin() + end);
	    }

	    void clear() {
	        this->session()->clear();
	    }

	    size_t size() const {
	        return this->get().size();
	    }

	    bool empty() const {
	        return this->get().empty();
	    }

	    Setter operator[](size_t index) {
	        return Setter { this->ref(), this->get()[index], index };
	    }

	    Setter at(size_t index) {
	        return Setter { this->ref(), this->get().at(index), index };
	    }

	    void resize(size_t newSize) {
	        this->session()->resize(newSize);
	    }

	    decltype(auto) begin() const {
	        return this->get().cbegin();
	    }
	    decltype(auto) end() const {
	        return this->get().cend();
	    }
	    decltype(auto) back() const {
	        return this->get().back();
	    }
	    decltype(auto) front() const {
	        return this->get().front();
	    }
	};

	template <typename T>
	class Signal<std::vector<T>> : public SignalBase<ReactiveVec<T>> {
	 public:
	    using SignalBase<ReactiveVec<T>>::SignalBase;

	    template <std::convertible_to<std::vector<T>> Q>
	    Signal(Q&& value) : SignalBase<ReactiveVec<T>>(ReactiveVec<T>(value)) {}

	    bool operator==(const Signal&) const = default; 
	};
}
