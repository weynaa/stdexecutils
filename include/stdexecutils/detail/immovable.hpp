#pragma once

namespace stdexecutils {
namespace detail {
struct immovable {
	immovable()                               = default;
	immovable(immovable&&)                    = delete;
	auto operator=(immovable&&) -> immovable& = delete;
};
} // namespace __detail
} // namespace stdexecutils
