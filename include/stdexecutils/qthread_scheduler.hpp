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
			QMetaObject::invokeMethod(
			    m_thread->eventDispatcher(),
			    [this]() { stdexec::set_value(std::move(m_receiver)); },
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

	// Sender for schedule_at
	template <class Recv>
	struct timeout_op_state : public detail::immovable {
		timeout_op_state(Recv&& receiver, QThread* thread,
		                 std::chrono::system_clock::time_point deadline)
		    : m_receiver(std::move(receiver)), m_thread(thread),
		      m_deadline(deadline), m_timer(new QTimer(m_thread)) {}

		~timeout_op_state() { delete m_timer; }

		void start() noexcept {
			m_timer->setSingleShot(true);
			const auto intervalFromNow =
			    std::max(std::chrono::milliseconds(0),
			             std::chrono::duration_cast<std::chrono::milliseconds>(
			                 m_deadline - std::chrono::system_clock::now()));
			m_timer->setInterval(intervalFromNow);
			QObject::connect(
			    m_timer, &QTimer::timeout, m_timer,
			    [this]() { stdexec::set_value(std::move(m_receiver)); },
			    Qt::QueuedConnection);
			m_timer->start();
		}

	private:
		Recv                                        m_receiver;
		QThread* const                              m_thread;
		const std::chrono::system_clock::time_point m_deadline;
		QTimer* const                               m_timer;
	};
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
	template <class Recv>
	struct delay_op_state : public detail::immovable {
		delay_op_state(Recv&& receiver, QThread* thread,
		               std::chrono::system_clock::duration duration)
		    : m_receiver(std::move(receiver)), m_thread(thread),
		      m_duration(duration), m_timer(new QTimer(m_thread)) {}

		~delay_op_state() { delete m_timer; }

		void start() noexcept {
			m_timer->setSingleShot(true);
			m_timer->setInterval(
			    std::chrono::duration_cast<std::chrono::milliseconds>(m_duration));
			QObject::connect(
			    m_timer, &QTimer::timeout, m_timer,
			    [this]() { stdexec::set_value(std::move(m_receiver)); },
			    Qt::QueuedConnection);
			m_timer->start();
		}

	private:
		Recv                                      m_receiver;
		QThread* const                            m_thread;
		const std::chrono::system_clock::duration m_duration;
		QTimer* const                             m_timer;
	};
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
		auto connect(R r) const -> delay_op_state<R> {
			return delay_op_state<R>(std::move(r), m_thread, m_duration);
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
