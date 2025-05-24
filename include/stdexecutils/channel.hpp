#pragma once

#include <stdexec/execution.hpp>

#include <exec/inline_scheduler.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>

// A Rust-style Multi-producer, single consumer channel

namespace stdexecutils {

namespace detail {

// C++ implementation of Dmitry Vyukov's non-intrusive lock free unbound MPSC
// queue
// http://www.1024cores.net/home/lock-free-algorithms/queues/non-intrusive-mpsc-node-based-queue
// Peformance considerations: this is guaranteed wait free so great in high
// contention, but actual throughtput probably not awesome for basic values
// given we use a simple linked list
template <typename T>
class mpsc_queue_t {
public:
	mpsc_queue_t()
	    : _head(new buffer_node_t), _tail(_head.load(std::memory_order_relaxed)) {
		buffer_node_t* front = _head.load(std::memory_order_relaxed);
		front->next.store(NULL, std::memory_order_relaxed);
	}

	~mpsc_queue_t() {
		T output;
		while (this->dequeue().has_value()) {
		}
		buffer_node_t* front = _head.load(std::memory_order_relaxed);
		delete front;
	}

	void enqueue(const T& input) {
		buffer_node_t* node = new buffer_node_t;
		node->data          = input;
		node->next.store(NULL, std::memory_order_relaxed);

		buffer_node_t* prev_head = _head.exchange(node, std::memory_order_acq_rel);
		prev_head->next.store(node, std::memory_order_release);
	}
	void enqueue(T&& input) {
		buffer_node_t* node = new buffer_node_t;
		node->data          = std::move(input);
		node->next.store(NULL, std::memory_order_relaxed);

		buffer_node_t* prev_head = _head.exchange(node, std::memory_order_acq_rel);
		prev_head->next.store(node, std::memory_order_release);
	}

	auto dequeue() -> std::optional<T> {
		buffer_node_t* tail = _tail.load(std::memory_order_relaxed);
		buffer_node_t* next = tail->next.load(std::memory_order_acquire);

		if (next == NULL) {
			return std::nullopt;
		}

		std::optional<T> output(std::move(next->data));
		_tail.store(next, std::memory_order_release);
		delete tail;
		return output;
	}

private:
	struct buffer_node_t {
		T                           data;
		std::atomic<buffer_node_t*> next;
	};

	typedef char cache_line_pad_t[64];

	cache_line_pad_t            _pad0;
	std::atomic<buffer_node_t*> _head;

	cache_line_pad_t            _pad1;
	std::atomic<buffer_node_t*> _tail;

	mpsc_queue_t(const mpsc_queue_t&) {}
	void operator=(const mpsc_queue_t&) {}
};

struct queue_receiver {
	virtual ~queue_receiver() = default;
	virtual void onReceived() = 0;
};

template <class T>
struct queue_state {
	std::atomic<queue_receiver*> waitingReceiver{nullptr};
	mpsc_queue_t<T>              queue;
};

template <stdexec::receiver mpsc_receiver, class T>
struct recv_op_state : public queue_receiver {

	struct stop_callback_fun {
		recv_op_state& op_state;

		void operator()() {
			auto receiver = op_state.m_sharedState.waitingReceiver.load();
			if (receiver != nullptr &&
			    op_state.m_sharedState.waitingReceiver.compare_exchange_strong(
			        receiver, nullptr)) {
				stdexec::set_stopped(std::move(op_state.m_receiver));
			}
		}
	};
	friend struct stop_callback_fun;
	using stop_callback = stdexec::stop_callback_for_t<
	    stdexec::stop_token_of_t<stdexec::env_of_t<mpsc_receiver>>,
	    stop_callback_fun>;

	recv_op_state(mpsc_receiver&& receiver, queue_state<T>& sharedState)
	    : m_receiver(std::move(receiver)), m_sharedState(sharedState) {}

	recv_op_state(const recv_op_state&) = delete;
	recv_op_state(recv_op_state&&)      = delete;

	void start() noexcept {
		stdexec::stoppable_token auto stop_token =
		    stdexec::get_stop_token(stdexec::get_env(m_receiver));

		if (stop_token.stop_requested()) {
			stdexec::set_stopped(std::move(m_receiver));
			return;
		}

		if (stop_token.stop_possible()) {
			m_stopCallback.emplace(std::move(stop_token), stop_callback_fun{*this});
		}

		auto result = m_sharedState.queue.dequeue();

		if (result.has_value()) {
			stdexec::set_value(std::move(m_receiver), std::move(*result));
			return;
		}
		assert(m_sharedState.waitingReceiver.load() == nullptr);
		m_sharedState.waitingReceiver.store(this);
	}

	void onReceived() override {
		auto result = m_sharedState.queue.dequeue();
		assert(result.has_value()); // got notifed, so there MUST be a value
		stdexec::set_value(std::move(m_receiver), std::move(*result));
	}

	void onStopped() {}

private:
	queue_state<T>&              m_sharedState;
	mpsc_receiver                m_receiver;
	std::optional<stop_callback> m_stopCallback;
};

template <class T>
struct recv_sender {
	using __id = recv_sender<T>;
	using __t  = recv_sender<T>;

	using sender_concept        = stdexec::sender_t;
	using completion_signatures = stdexec::completion_signatures< //
	    stdexec::set_value_t(T),                                  //
	    stdexec::set_stopped_t()>;

	explicit recv_sender(queue_state<T>& sharedState) noexcept
	    : m_sharedState(sharedState) {}

	template <stdexec::receiver mpsc_receiver>
	auto connect(mpsc_receiver&& receiver) {
		return recv_op_state(std::move(receiver), m_sharedState);
	}

private:
	queue_state<T>& m_sharedState;
};

template <class T>
struct mpsc_receiver {

	mpsc_receiver(std::shared_ptr<queue_state<T>> queue)
	    : m_sharedState(std::move(queue)) {}

	stdexec::sender auto recv() { return recv_sender<T>(*m_sharedState); }

	mpsc_receiver(const mpsc_receiver&) = delete;
	mpsc_receiver(mpsc_receiver&&)      = default;

private:
	const std::shared_ptr<queue_state<T>> m_sharedState;
};

template <class T>
struct mpsc_sender {
	mpsc_sender(std::shared_ptr<queue_state<T>> queue)
	    : m_sharedState(std::move(queue)) {}

	/**
	 * @brief this function is guaranteed wait-free (finished in constant time)
	 */
	template <class Value>
	void send(Value&& val) {
		auto receiver = m_sharedState->waitingReceiver.load();
		m_sharedState->queue.enqueue(std::forward<Value>(val));
		if (receiver != nullptr &&
		    m_sharedState->waitingReceiver.compare_exchange_strong(receiver,
		                                                           nullptr)) {
			receiver->onReceived();
		}
	}

private:
	const std::shared_ptr<queue_state<T>> m_sharedState;
};
} // namespace detail

template <class T>
using mpsc_sender = detail::mpsc_sender<T>;
template <class T>
using mpsc_receiver = detail::mpsc_receiver<T>;

template <class T>
auto mpsc_channel() -> std::tuple<mpsc_sender<T>, mpsc_receiver<T>> {
	auto queueState = std::make_shared<detail::queue_state<T>>();
	return {mpsc_sender<T>(queueState), mpsc_receiver<T>(queueState)};
}
} // namespace stdexecutils
