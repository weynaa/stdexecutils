#include <mw/stdexecutils.hpp>

int main() {
	auto fut = mw::spawn_future(stdexec::just(42));
	const auto [result] = fut.get();
	return result == 42;
}
