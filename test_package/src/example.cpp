#include <stdexecutils/spawn_stdfuture.hpp>

#include <cstdlib>

int main() {
	auto fut = stdexecutils::spawn_stdfuture(stdexec::just(42));
	const auto [result] = *fut.get();
	return result == 42 ? EXIT_SUCCESS : EXIT_FAILURE;
}
