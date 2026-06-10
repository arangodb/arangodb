#include "activity_declaration.h"
#include "markdown.h"

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
            << "    Expected:\n[" << expected << "]\n"
            << "    Actual:\n[" << actual << "]\n";
  return false;
}

}  // namespace

int main() {
  if (assert_equal(
          activities_to_markdown(std::vector<ActivityDeclaration>{
              ActivityDeclaration{
                  .owner = "ns::Holder",
                  .type = "ns::Foo",
                  .data_type_definition =
                      {Struct{.name = "FooData",
                              .fields = {Member{.name = "id", .type = "int"},
                                         Member{.name = "label",
                                                .type = "std::string"}}},
                       Struct{.name = "Bar", .fields = {}}}},
              ActivityDeclaration{.owner = "ns::run",
                                  .type = "ns::Empty",
                                  .data_type_definition = {}}}),
          "# Activities\n"
          "\n"
          "## ns::Foo\n"
          "owner: ns::Holder\n"
          "\n"
          "### FooData\n"
          "| Field | Type        |\n"
          "|-------+-------------|\n"
          "| id    | int         |\n"
          "| label | std::string |\n"
          "\n"
          "### Bar\n"
          "\n"
          "## ns::Empty\n"
          "owner: ns::run\n")) {
    std::cout << "[ SUCCESS ] activities_to_markdown\n";
    return 0;
  }
  std::cout << "[ FAILED  ] activities_to_markdown\n";
  return 1;
}
