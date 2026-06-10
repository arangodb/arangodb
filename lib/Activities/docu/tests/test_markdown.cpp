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
                  .owner_file = "a/b.h",
                  .owner_line = 10,
                  .type = "ns::Foo",
                  .data_type = "ns::FooData",
                  .type_definition =
                      {Struct{.name = "FooData",
                              .fields = {Member{.name = "id", .type = "int"},
                                         Member{.name = "label",
                                                .type = "std::string"}}},
                       Struct{.name = "Bar", .fields = {}}}},
              ActivityDeclaration{.owner_file = "c.cpp",
                                  .owner_line = 3,
                                  .type = "ns::Empty",
                                  .data_type = std::nullopt,
                                  .type_definition = {}}}),
          "# Activities\n"
          "\n"
          "## ns::Foo\n"
          "owner: a/b.h:10\n"
          "data: ns::FooData\n"
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
          "owner: c.cpp:3\n")) {
    std::cout << "[ SUCCESS ] activities_to_markdown\n";
    return 0;
  }
  std::cout << "[ FAILED  ] activities_to_markdown\n";
  return 1;
}
