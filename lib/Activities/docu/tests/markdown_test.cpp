#include "activity_declaration.h"
#include "markdown.h"
#include "tests.h"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

/**
 * Assert two strings are equal, printing both (bracketed) on mismatch.
 */
auto assert_equal(std::string const& actual, std::string const& expected)
    -> bool {
  if (actual == expected) {
    return true;
  }
  std::cerr << "    Assertion failure\n"
            << "--- Expected: ------------------\n[" << expected << "]\n"
            << "--- Actual: --------------------\n[" << actual << "]\n";
  return false;
}

}  // namespace

TestCase const tests[] = {
    {"generates_markdown_correctly", []() -> bool {
       return assert_equal(
           markdown::activities_to_markdown(std::vector<ActivityDeclaration>{
               ActivityDeclaration{
                   .owner = "ns::Holder",
                   .type = "ns::Foo",
                   .data_type_definition =
                       {Struct{.name = "ns::FooData",
                               .fields = {Member{.name = "id", .type = "int"},
                                          Member{.name = "label",
                                                 .type = "std::string"}}},
                        Struct{.name = "ns::Bar", .fields = {}}}},
               ActivityDeclaration{.owner = "ns::run",
                                   .type = "ns::Empty",
                                   .data_type_definition = {}}}),
           R"(
## ns::Holder
type: ns::Foo

### ns::FooData
| Field | Type        |
|-------|-------------|
| id    | int         |
| label | std::string |

### ns::Bar

## ns::run
type: ns::Empty
)");
     }}};

int main() {
  int passed = 0;
  int failed = 0;
  for (TestCase const& test : tests) {
    std::cout << "[ RUN     ] " << test.name << "\n";
    if (test.fn()) {
      std::cout << "[ SUCCESS ] " << test.name << "\n";
      ++passed;
    } else {
      std::cout << "[ FAILED  ] " << test.name << "\n";
      ++failed;
    }
  }

  std::cout << "\n"
            << passed << " passed, " << failed << " failed (out of "
            << (passed + failed) << ").\n";
  return failed == 0 ? 0 : 1;
}
