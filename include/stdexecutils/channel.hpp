#pragma once

#include <stdexec/execution.hpp>

#include <memory>
#include <mutex>
#include <optional>
#include <queue>

// A Rust-style Multi-producer, single consumer channel

namespace stdexecutils {

namespace detail {

template <class T>
struct queue_receiver {
	virtual ~queue_receiver()    = default;
	virtual void onReceived(T&&) = 0;
};

template <class T>
struct queue_state {
	// Can probably be done lockfree and waitfree somehow, but I save myself the
	// pain
	std::mutex         mutex;
	queue_receiver<T>* waitingReceiver{nullptr};
	std::queue<T>      queue;
};

template <stdexec::receiver mpsc_receiver, class T>
struct recv_op_state : public queue_receiver<T> {

	struct stop_callback_fun {
		recv_op_state& op_state;

		void operator()() {
			std::unique_lock l(op_state.m_sharedState.mutex);
			if (op_state.m_sharedState.waitingReceiver != nullptr) {
				op_state.m_sharedState.waitingReceiver = nullptr;
				l.unlock();
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
		std::unique_lock l(m_sharedState.mutex);
		if (m_sharedState.waitingReceiver != nullptr) {
			stdexec::set_error(std::move(m_receiver),
			                   std::make_exception_ptr(std::runtime_error(
			                       "async receive is not supported")));
			return;
		}
		if (m_sharedState.queue.empty()) {
			m_sharedState.waitingReceiver = this;
		} else {

			auto result = m_sharedState.queue.front();
			m_sharedState.queue.pop();
			l.unlock();
			stdexec::set_value(std::move(m_receiver), std::move(result));
		}
	}

	void onReceived(T&& result) override {
		stdexec::set_value(std::move(m_receiver), std::move(result));
	}

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
	    stdexec::set_error_t(std::exception_ptr),                 //
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

	mpsc_receiver(std::shared_ptr<queue_state<T>> queue) noexcept
	    : m_sharedState(std::move(queue)) {}

	stdexec::sender auto recv() { return recv_sender<T>(*m_sharedState); }

	mpsc_receiver(const mpsc_receiver&) = delete;
	mpsc_receiver(mpsc_receiver&&)      = default;

private:
	const std::shared_ptr<queue_state<T>> m_sharedState;
};

template <class T>
struct mpsc_sender {
	mpsc_sender(std::shared_ptr<queue_state<T>> queue) noexcept
	    : m_sharedState(std::move(queue)) {}

	template <class Value>
	void send(Value&& val) {
		std::unique_lock l(m_sharedState->mutex);
		const auto       receiver = m_sharedState->waitingReceiver;
		if (receiver != nullptr) {
			m_sharedState->waitingReceiver = nullptr;
			l.unlock();
			receiver->onReceived(std::move(val));
		} else {
			m_sharedState->queue.push(std::forward<Value>(val));
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
