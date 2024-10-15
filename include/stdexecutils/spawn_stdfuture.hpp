#ifndef STDEXEC_UTILS_H
#define STDEXEC_UTILS_H

#include <exec/async_scope.hpp>
#include <stdexec/execution.hpp>

#include <future>
#include <optional>

namespace stdexecutils {

/**
 * @brief consume a sender into an std::future
 * @param s Sender to consume into a std::future
 * @detail Like with stdexec::sync_wait a potential exception is
 * rethrown upon calling future.get(), if the sender was stopped the optional
 * will be empty
 */
template <stdexec::sender Sender>
auto spawn_stdfuture(Sender&& s)
    -> std::future<decltype(stdexec::sync_wait(s))> {
	struct op_state {
		std::promise<decltype(stdexec::sync_wait(s))> promise;
		exec::async_scope                             scope;
	};

	auto opState = std::make_shared<op_state>();
	opState->scope.spawn(
	    std::move(s) | stdexec::then([opState](auto&&... result) {
		    opState->promise.set_value(std::make_optional(
		        std::make_tuple(std::forward<decltype(result)>(result)...)));
	    }) |
	    stdexec::upon_error([opState](auto&& exception) {
		    opState->promise.set_exception(exception);
	    }) |
	    stdexec::upon_stopped(
	        [opState]() { opState->promise.set_value(std::nullopt); }));
	return opState->promise.get_future();
}

} // namespace stdexecutils

#endif // STDEXEC_UTILS_H
