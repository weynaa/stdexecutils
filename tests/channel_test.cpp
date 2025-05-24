#include <gtest/gtest.h>
#include <stdexecutils/channel.hpp>

#include <exec/async_scope.hpp>
#include <exec/single_thread_context.hpp>
#include <exec/task.hpp>

#include <chrono>
#include <thread>

using namespace std::chrono_literals;
using namespace stdexecutils;

TEST(ChannelTest, SendSingleValueTest) {
	auto [tx, rx] = mpsc_channel<int>();
	tx.send(5);
	auto recv = stdexec::sync_wait(rx.recv());
	ASSERT_TRUE(recv.has_value());
	const auto [recvValue] = *recv;
	EXPECT_EQ(recvValue, 5);
}

TEST(ChannelTest, AsyncReceive) {
	exec::single_thread_context m_thread;
	exec::async_scope           m_scope;

	constexpr auto nValues = 5;

	auto [tx, rx] = mpsc_channel<int>();

	std::vector<int> receivedValues;
	m_scope.spawn(
	    stdexec::start_on(m_thread.get_scheduler(),
	                      [&](auto rx, auto& receivedValues) -> exec::task<void> {
		                      for (auto i = 0; i < nValues; ++i) {
			                      const auto tid = std::this_thread::get_id();
			                      receivedValues.push_back(co_await rx.recv());
			                      EXPECT_EQ(std::this_thread::get_id(), tid);
		                      }
	                      }(std::move(rx), receivedValues)));

	for (auto i = 0; i < nValues; ++i) {
		std::this_thread::sleep_for(10ms);
		tx.send(i);
	}
	stdexec::sync_wait(m_scope.on_empty());

	for (auto i = 0; i < nValues; ++i) {
		EXPECT_EQ(receivedValues[i], i);
	}
}

TEST(ChannelTest, SendStop) {
	exec::single_thread_context m_thread;
	exec::async_scope           m_scope;

	constexpr auto nValues = 5;

	auto [tx, rx] = mpsc_channel<int>();

	std::vector<int> receivedValues;
	m_scope.spawn(
	    stdexec::start_on(m_thread.get_scheduler(),
	                      [&](auto rx, auto& receivedValues) -> exec::task<void> {
		                      for (auto i = 0; i < nValues; ++i) {
			                      receivedValues.push_back(co_await rx.recv());
		                      }
	                      }(std::move(rx), receivedValues)));
	m_scope.request_stop();
	for (auto i = 0; i < nValues; ++i) {
		std::this_thread::sleep_for(10ms);
		tx.send(i);
	}
	stdexec::sync_wait(m_scope.on_empty());

	EXPECT_TRUE(receivedValues.empty());
}

TEST(ChannelTest, SendStopImmediately) {
	exec::single_thread_context m_thread;
	exec::async_scope           m_scope;

	constexpr auto nValues = 5;

	auto [tx, rx] = mpsc_channel<int>();

	std::vector<int> receivedValues;
	m_scope.spawn(
	    stdexec::start_on(m_thread.get_scheduler(),
	                      [&](auto rx, auto& receivedValues) -> exec::task<void> {
		                      for (auto i = 0; i < nValues; ++i) {
			                      receivedValues.push_back(co_await rx.recv());
		                      }
	                      }(std::move(rx), receivedValues)));
	std::this_thread::sleep_for(100ms);
	m_scope.request_stop();
	stdexec::sync_wait(m_scope.on_empty());

	EXPECT_TRUE(receivedValues.empty());
}

TEST(ChannelTest, TestSerializability) {
	exec::single_thread_context m_thread1;
	exec::single_thread_context m_thread2;
	exec::async_scope           m_scope;

	constexpr auto nValues = 100;

	auto [tx, rx] = mpsc_channel<int>();

	m_scope.spawn(stdexec::start_on(m_thread1.get_scheduler(),
	                                [&](auto tx) -> exec::task<void> {
		                                for (auto i = 1; i <= nValues; ++i) {
			                                tx.send(i);
		                                }
		                                co_return;
	                                }(tx)));
	m_scope.spawn(stdexec::start_on(m_thread2.get_scheduler(),
	                                [&](auto tx) -> exec::task<void> {
		                                for (auto i = 1; i <= nValues; ++i) {
			                                tx.send(-i);
		                                }
		                                co_return;
	                                }(tx)));

	int c1 = 0, c2 = 0;
	while (c1 < nValues || c2 > -nValues) {
		auto recv = stdexec::sync_wait(rx.recv());
		ASSERT_TRUE(recv.has_value());
		const auto [recvValue] = *recv;
		if (recvValue > 0) {
			ASSERT_EQ(recvValue, c1 + 1);
			c1 = recvValue;
		} else {
			ASSERT_EQ(recvValue, c2 - 1);
			c2 = recvValue;
		}
	}
	ASSERT_EQ(c1, nValues);
	ASSERT_EQ(c2, -nValues);
}
