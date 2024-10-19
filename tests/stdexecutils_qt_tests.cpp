#include <QCoreApplication>
#include <exec/async_scope.hpp>
#include <gtest/gtest.h>
#include <stdexecutils/qthread_scheduler.hpp>

using namespace stdexecutils;

using namespace std::chrono_literals;

static_assert(stdexec::scheduler<stdexecutils::QThreadScheduler>,
              "scheduler is not fulfilling the concept");

TEST(QThreadScheduler, BasicSchedulingContinuation) {

	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	stdexecutils::QThreadScheduler scheduler(&application);

	const auto        tid = std::this_thread::get_id();
	exec::async_scope scope;
	scope.spawn(stdexec::schedule(scheduler) | stdexec::then([&]() {
		            // continuation in the qthread event loop
		            EXPECT_EQ(tid, std::this_thread::get_id());
		            application.exit();
	            }));

	application.exec();
}

TEST(QThreadScheduler, BasicSchedulingErrorContinuation) {

	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	stdexecutils::QThreadScheduler scheduler(&application);
	const auto                     tid = std::this_thread::get_id();
	exec::async_scope              scope;
	scope.spawn(stdexec::schedule(scheduler) |
	            stdexec::then([]() { throw std::runtime_error("test"); }) |
	            stdexec::upon_error([&](std::exception_ptr exception) {
		            // continuation in the qthread event loop
		            EXPECT_EQ(tid, std::this_thread::get_id());
		            application.exit();
	            }));
	application.exec();
}

TEST(QThreadScheduler, BasicSchedulingStopped) {

	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	stdexecutils::QThreadScheduler scheduler(&application);
	const auto                     tid = std::this_thread::get_id();
	exec::async_scope              scope;
	scope.spawn(stdexec::schedule(scheduler) |
	            stdexec::then([]() { throw std::runtime_error("test"); }) |
	            stdexec::upon_stopped([&]() {
		            // continuation in the qthread event loop
		            EXPECT_EQ(tid, std::this_thread::get_id());
		            application.exit();
	            }));
	scope.request_stop();
	application.exec();
}

TEST(QThreadScheduler, ScheduleAt) {

	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	stdexecutils::QThreadScheduler scheduler(&application);

	constexpr auto duration = 500ms;
	const auto     now      = std::chrono::system_clock::now();
	const auto     deadline = now + duration;

	static_assert(stdexec::sender<decltype(scheduler.schedule_at(deadline))>,
	              "schedule_at is not a sender");

	exec::async_scope scope;
	scope.spawn(scheduler.schedule_at(deadline) | stdexec::then([&]() {
		            // continuation in the qthread event loop
		            EXPECT_GT((std::chrono::system_clock::now() - now).count(),
		                      duration.count() * 0.9);
		            application.exit();
	            }));
	application.exec();
}

TEST(QThreadScheduler, ScheduleAtStopped) {

	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	stdexecutils::QThreadScheduler scheduler(&application);

	constexpr auto duration = 10s;
	const auto     now      = std::chrono::system_clock::now();
	const auto     deadline = now + duration;

	static_assert(stdexec::sender<decltype(scheduler.schedule_at(deadline))>,
	              "schedule_at is not a sender");

	exec::async_scope scope;
	scope.spawn(scheduler.schedule_at(deadline)                     //
	            | stdexec::then([&]() { FAIL() << "not stopped"; }) //
	            |
	            stdexec::upon_stopped([&]() {
		            EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(
		                          std::chrono::system_clock::now() - now)
		                          .count(),
		                      std::chrono::duration_cast<std::chrono::milliseconds>(
		                          duration)
		                              .count() *
		                          0.1);
		            application.exit();
	            }));
	scope.spawn(scheduler.schedule_after(100ms) |
	            stdexec::then([&]() { scope.request_stop(); }));
	application.exec();
}

TEST(QThreadScheduler, ScheduleAfter) {

	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	stdexecutils::QThreadScheduler scheduler(&application);
	static_assert(stdexec::sender<decltype(scheduler.schedule())>,
	              "scheduler is not a sender");
	constexpr auto duration = 500ms;
	const auto     now      = std::chrono::system_clock::now();

	static_assert(stdexec::sender<decltype(scheduler.schedule_after(duration))>,
	              "schedule_after is not a sender");

	exec::async_scope scope;
	scope.spawn(scheduler.schedule_after(duration) | stdexec::then([&]() {
		            // continuation in the qthread event loop
		            EXPECT_GT((std::chrono::system_clock::now() - now).count(),
		                      duration.count() * 0.9);
		            application.exit();
	            }));
	application.exec();
}

TEST(QThreadScheduler, ScheduleAfterStopped) {

	int              argc = 0;
	QCoreApplication application(argc, nullptr);

	stdexecutils::QThreadScheduler scheduler(&application);

	constexpr auto duration = 10s;
	const auto     now      = std::chrono::system_clock::now();

	static_assert(stdexec::sender<decltype(scheduler.schedule_after(duration))>,
	              "schedule_at is not a sender");

	exec::async_scope scope;
	scope.spawn(scheduler.schedule_after(duration)                  //
	            | stdexec::then([&]() { FAIL() << "not stopped"; }) //
	            |
	            stdexec::upon_stopped([&]() {
		            EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(
		                          std::chrono::system_clock::now() - now)
		                          .count(),
		                      std::chrono::duration_cast<std::chrono::milliseconds>(
		                          duration)
		                              .count() *
		                          0.1);
		            application.exit();
	            }));
	scope.spawn(scheduler.schedule_after(100ms) |
	            stdexec::then([&]() { scope.request_stop(); }));
	application.exec();
}
