#include <gtest/gtest.h>
#include <stdexecutils/spawn_stdfuture.hpp>

#include <exec/static_thread_pool.hpp>

using namespace stdexecutils;


TEST(SpawnFuture, EmptyResultTest) {
  auto       future = spawn_stdfuture(stdexec::just());
  const auto result = future.get();
  EXPECT_TRUE(result.has_value());
}

TEST(SpawnFuture, SingleResultTest) {
  auto fut = spawn_stdfuture(stdexec::just(1));

  const auto [result] = *fut.get();
  EXPECT_EQ(result, 1);
}

TEST(SpawnFuture, MulitpleResultTest) {
  auto fut = spawn_stdfuture(
      stdexec::when_all(stdexec::just(1), stdexec::just(2), stdexec::just(3)));

  const auto [result1, result2, result3] = *fut.get();
  EXPECT_EQ(result1, 1);
  EXPECT_EQ(result2, 2);
  EXPECT_EQ(result3, 3);
}

TEST(SpawnFuture, FailureTest) {
  auto fut = spawn_stdfuture(stdexec::just() | stdexec::then([]() {
                               throw std::runtime_error("test");
                             }));

  try {
    const auto result = fut.get();
  } catch (std::runtime_error e) {
    EXPECT_TRUE(std::string("test") == e.what());
    return;
  }
  FAIL() << "did not throw";
}

template <class S>
struct scheduler_env {
	template <stdexec::__completion_tag Tag>
	S query(stdexec::get_completion_scheduler_t<Tag>) const noexcept {
		return {};
	}
};
struct stopped_scheduler {
	using __id = stopped_scheduler;
	using __t  = stopped_scheduler;

	template <class R>
	struct oper {
		R recv_;

		explicit oper(R recv) : recv_(std::move(recv)) {}
		oper(oper&&)            = delete;
		oper& operator=(oper&&) = delete;

		void start() & noexcept { stdexec::set_stopped(static_cast<R&&>(recv_)); }
	};

	struct my_sender {
		using __id = my_sender;
		using __t  = my_sender;

		using sender_concept        = stdexec::sender_t;
		using completion_signatures = stdexec::completion_signatures< //
		    stdexec::set_value_t(),                                   //
		    stdexec::set_stopped_t()>;

		template <class R>
		oper<R> connect(R r) const {
			return oper<R>(std::move(r));
		}

		scheduler_env<stopped_scheduler> get_env() const noexcept { return {}; }
	};

	my_sender schedule() const { return {}; }

	bool operator==(const stopped_scheduler&) const noexcept = default;
};

TEST(SpawnFuture, StoppedTest) {
	stopped_scheduler sched;
	auto       future = spawn_stdfuture(sched.schedule());
	const auto result = future.get();
	EXPECT_TRUE(!result.has_value());
}
