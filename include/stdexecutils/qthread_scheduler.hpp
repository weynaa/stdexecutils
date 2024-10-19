#pragma once

#include <stdexecutils/detail/immovable.hpp>

#include <QAbstractEventDispatcher>
#include <QObject>
#include <QThread>
#include <QTimer>

#include <stdexec/execution.hpp>

#include <chrono>

namespace stdexecutils {

class QThreadScheduler {
public:
	using __id = QThreadScheduler;
	using __t  = QThreadScheduler;

	struct env {

		explicit env(QThread* thread) noexcept : m_thread(thread) {}

		template <stdexec::__completion_tag Tag>
		auto query(stdexec::get_completion_scheduler_t<Tag>) const noexcept
		    -> QThreadScheduler {
			return {m_thread};
		}

	private:
		QThread* const m_thread;
	};

	// Sender for schedule
	template <class Recv>
	struct op_state : public detail::immovable {
		op_state(Recv&& receiver, QThread* thread)
		    : m_receiver(std::move(receiver)), m_thread(thread) {}
		void start() noexcept {
			stdexec::stoppable_token auto stop_token =
			    stdexec::get_stop_token(stdexec::get_env(m_receiver));
			if (stop_token.stop_requested()) {
				stdexec::set_stopped(std::move(m_receiver));
				return;
			}
			QMetaObject::invokeMethod(
			    m_thread->eventDispatcher(),
			    [this, stop_token]() {
				    if (stop_token.stop_requested()) {
					    stdexec::set_stopped(std::move(m_receiver));
					    return;
				    }
				    stdexec::set_value(std::move(m_receiver));
			    },
			    Qt::QueuedConnection);
		}

	private:
		Recv           m_receiver;
		QThread* const m_thread;
	};
	struct sender {
		using __id = sender;
		using __t  = sender;

		using sender_concept        = stdexec::sender_t;
		using completion_signatures = stdexec::completion_signatures< //
		    stdexec::set_value_t(),                                   //
		    stdexec::set_stopped_t()>;

		explicit sender(QThread* thread) noexcept : m_thread(thread) {}

		template <class R>
		auto connect(R r) const -> op_state<R> {
			return op_state<R>(std::move(r), m_thread);
		};

		auto get_env() const noexcept -> env { return env{m_thread}; }

	private:
		QThread* const m_thread;
	};

	// Operation state for schedule_at and scheduler_after
	template <class Recv>
	struct timeout_op_state : public detail::immovable {

		using deadline_or_delay =
		    std::variant<std::chrono::system_clock::time_point,
		                 std::chrono::system_clock::duration>;

		timeout_op_state(Recv&& receiver, QThread* thread,
		                 deadline_or_delay deadlineOrDelay)
		    : m_receiver(std::move(receiver)), m_thread(thread),
		      m_deadlineOrDelay(deadlineOrDelay), m_timer(new QTimer(m_thread)) {}

		~timeout_op_state() { delete m_timer; }

		void start() noexcept {
			stdexec::stoppable_token auto stop_token =
			    stdexec::get_stop_token(stdexec::get_env(m_receiver));

			if (stop_token.stop_requested()) {
				stdexec::set_stopped(std::move(m_receiver));
				return;
			}
			m_done = false;

			m_timer->setSingleShot(true);
			const auto intervalFromNow =
			    std::holds_alternative<std::chrono::system_clock::time_point>(
			        m_deadlineOrDelay)
			        ? std::chrono::duration_cast<std::chrono::milliseconds>(
			              std::get<std::chrono::system_clock::time_point>(
			                  m_deadlineOrDelay) -
			              std::chrono::system_clock::now())
			        : std::chrono::duration_cast<std::chrono::milliseconds>(
			              std::get<std::chrono::system_clock::duration>(
			                  m_deadlineOrDelay));

			m_timer->setInterval(
			    std::max(std::chrono::milliseconds(0), intervalFromNow));
			QObject::connect(
			    m_timer, &QTimer::timeout, m_timer,
			    [this]() {
				    auto done = m_done.load();
				    if (!done && m_done.compare_exchange_strong(done, true)) {
					    m_stoppedCallback.reset();
					    stdexec::set_value(std::move(m_receiver));
				    }
			    },
			    Qt::QueuedConnection);
			m_timer->start();
			if (stop_token.stop_possible()) {

				m_stoppedCallback = std::make_unique<stop_callback>(
				    std::move(stop_token), stop_callback_fun{*this});
			}
		}

	private:
		struct stop_callback_fun {
			timeout_op_state& op_state;

			void operator()() {
				auto done = op_state.m_done.load();
				if (!done && op_state.m_done.compare_exchange_strong(done, true)) {
					op_state.m_timer->stop();
					QMetaObject::invokeMethod(
					    op_state.m_thread->eventDispatcher(),
					    [=]() { stdexec::set_stopped(std::move(op_state.m_receiver)); },
					    Qt::QueuedConnection);
				}
			}
		};
		friend struct stop_callback_fun;
		using stop_callback = stdexec::stop_callback_for_t<
		    stdexec::stop_token_of_t<stdexec::env_of_t<Recv>>, stop_callback_fun>;

		Recv                           m_receiver;
		QThread* const                 m_thread;
		const deadline_or_delay        m_deadlineOrDelay;
		QTimer* const                  m_timer;
		std::atomic_bool               m_done;
		std::unique_ptr<stop_callback> m_stoppedCallback;
	};

	// Sender for schedule_at
	struct timeout_sender {
		using __id = timeout_sender;
		using __t  = timeout_sender;

		using sender_concept        = stdexec::sender_t;
		using completion_signatures = stdexec::completion_signatures< //
		    stdexec::set_value_t(),                                   //
		    stdexec::set_stopped_t()>;

		explicit timeout_sender(
		    QThread*                              thread,
		    std::chrono::system_clock::time_point deadline) noexcept
		    : m_thread(thread), m_deadline(deadline) {}

		template <class R>
		auto connect(R r) const -> timeout_op_state<R> {
			return timeout_op_state<R>(std::move(r), m_thread, m_deadline);
		};

		auto get_env() const noexcept -> env { return env{m_thread}; }

	private:
		QThread* const                              m_thread;
		const std::chrono::system_clock::time_point m_deadline;
	};

	// Sender for schedule_after
	struct delay_sender {
		using __id = delay_sender;
		using __t  = delay_sender;

		using sender_concept        = stdexec::sender_t;
		using completion_signatures = stdexec::completion_signatures< //
		    stdexec::set_value_t(),                                   //
		    stdexec::set_stopped_t()>;

		explicit delay_sender(QThread*                            thread,
		                      std::chrono::system_clock::duration duration) noexcept
		    : m_thread(thread), m_duration(duration) {}

		template <class R>
		auto connect(R r) const -> timeout_op_state<R> {
			return timeout_op_state<R>(std::move(r), m_thread, m_duration);
		};

		auto get_env() const noexcept -> env { return env{m_thread}; }

	private:
		QThread* const                            m_thread;
		const std::chrono::system_clock::duration m_duration;
	};

	explicit QThreadScheduler(QThread* thread) noexcept : m_thread(thread) {}
	explicit QThreadScheduler(QObject* object) noexcept
	    : m_thread(object->thread()) {}

	auto schedule() const -> sender { return sender{m_thread}; }

	auto schedule_at(std::chrono::system_clock::time_point deadline) const
	    -> timeout_sender {
		return timeout_sender{m_thread, deadline};
	};

	auto schedule_after(std::chrono::system_clock::duration deadline) const
	    -> delay_sender {
		return delay_sender{m_thread, deadline};
	};

	auto operator==(const QThreadScheduler&) const noexcept -> bool = default;

private:
	QThread* const m_thread;
};
} // namespace stdexecutils
