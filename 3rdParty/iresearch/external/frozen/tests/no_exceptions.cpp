#include <frozen/unordered_map.h>

#include <csignal>
#include <cstdlib>

// https://discourse.cmake.org/t/tests-that-are-meant-to-abort/537/3
// This is a hack to implement death tests in CTest.
extern "C" void error_test_handle_abort(int) { std::_Exit(EXIT_FAILURE); }

struct test_override_abort {
  test_override_abort() noexcept {
    std::signal(SIGABRT, error_test_handle_abort);
  }
};

test_override_abort handler{};

int main() {
  constexpr frozen::unordered_map<unsigned, unsigned, 2> ce = {{1, 2}, {3, 4}};
  ce.at(0xDEAD);
  return 0;
}
