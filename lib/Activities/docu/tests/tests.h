#include <string>

/**
 * A named test: `fn` is called with the project root and returns true on
 * pass, false on fail.
 */
struct TestCase {
  char const* name;
  bool (*fn)();
};
