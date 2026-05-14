#include "find_activity_subclasses.h"

#include <iostream>
#include <string>
#include <vector>

#ifndef PROJECT_ROOT
#error "PROJECT_ROOT must be defined at compile time (absolute path to repo)"
#endif

namespace {

/**
 * Format the IR types in a vaguely C++-literal-ish form for failure
 * diagnostics, so a mismatch can be copy-pasted into source.
 */
std::ostream& operator<<(std::ostream& os, Member const& member) {
  return os << "Member{.name=\"" << member.name << "\", .type=\""
            << member.type << "\"}";
}
std::ostream& operator<<(std::ostream& os, Struct const& strct) {
  os << "Struct{.name=\"" << strct.name << "\", .fields={";
  for (size_t index = 0; index < strct.fields.size(); ++index) {
    if (index != 0) os << ", ";
    os << strct.fields[index];
  }
  return os << "}}";
}
std::ostream& operator<<(std::ostream& os, ActivityDeclaration const& decl) {
  os << "ActivityDeclaration{.owner_file=\"" << decl.owner_file
     << "\", .owner_line=" << decl.owner_line << ", .data_type=\""
     << decl.data_type << "\", .field_types={";
  for (size_t index = 0; index < decl.type_definition.size(); ++index) {
    if (index != 0) os << ", ";
    os << decl.type_definition[index];
  }
  return os << "}}";
}

}  // namespace

std::string const project_root = PROJECT_ROOT;

/**
 * Assert that `expected` appears somewhere in `activities`.
 *
 * On failure, print the expected value and every actual entry to stderr
 * and return false.
 */
auto assert_includes(std::vector<ActivityDeclaration> const& activities,
                     ActivityDeclaration const& expected) -> bool {
  for (ActivityDeclaration const& activity : activities) {
    if (activity == expected) return true;
  }

  std::cerr << "    Assertion include failure\n"
            << "    Expected:\n    " << expected << "\n"
            << "    Got " << activities.size() << " activities:\n";
  for (ActivityDeclaration const& activity : activities) {
    std::cerr << "    " << activity << "\n";
  }
  return false;
}

/**
 * A named test: `fn` is called with the project root and returns true on
 * pass, false on fail.
 */
struct TestCase {
  char const* name;
  bool (*fn)(std::string const&);
};

TestCase const tests[] = {
    {"find_transaction_activity",
     [](std::string const& root) -> bool {
       return assert_includes(
           find_all_activities(root +
                             "/arangod/StorageEngine/TransactionState.cpp"),
           ActivityDeclaration{
               .owner_file = root + "/arangod/StorageEngine/TransactionState.h",
               .owner_line = 521,
               .data_type =
                   "arangodb::transaction::activity::TransactionActivityData",
               .type_definition =
                   std::vector<Struct>{
                       Struct{
                           .name = "TransactionActivityData",
                           .fields =
                               {
                                   Member{.name = "user",
                                          .type = "std::string"},
                                   Member{.name = "database",
                                          .type = "std::string"},
                                   Member{.name = "tid",
                                          .type = "TransactionId"},
                                   Member{.name = "status", .type = "Status"},
                                   Member{.name = "collections",
                                          .type = "std::vector<"
                                                  "TransactionCollection>"},
                               }},
                       Struct{.name = "TransactionId", .fields = {}},
                       Struct{.name = "TransactionCollection",
                              .fields =
                                  {
                                      Member{.name = "name",
                                             .type = "std::string"},
                                      Member{.name = "cid",
                                             .type = "DataSourceId"},
                                      Member{.name = "accessType",
                                             .type = "AccessMode::Type"},
                                      Member{.name = "lockStatus",
                                             .type = "LockStatus"},
                                  }},
                       Struct{.name = "DataSourceId", .fields = {}}},
           });
     }},
    {"find_maintenance_activity",
     [](std::string const& root) -> bool {
       return assert_includes(
           find_all_activities(root + "/arangod/Cluster/ActionBase.h"),
           ActivityDeclaration{
               .owner_file = root + "/arangod/Cluster/ActionBase.h",
               .owner_line = 266,
               .data_type = "arangodb::maintenance::ActionDescription",
               .type_definition =
                   std::vector<Struct>{Struct{.name = "ActionDescription",
                                              // TODO show all inspected fields
                                              .fields = {}}}});
     }},
    {"find_collection_creation_activity", [](std::string const& root) -> bool {
       return assert_includes(
           find_all_activities(root + "/arangod/VocBase/Methods/Collections.cpp"),
           ActivityDeclaration{
               .owner_file = root + "/arangod/VocBase/Methods/Collections.cpp",
               .owner_line = 598,
               .data_type = "arangodb::activities::GenericActivityData",
               .type_definition = std::vector<Struct>{}});
     }}};

int main() {
  int passed = 0;
  int failed = 0;
  for (TestCase const& test : tests) {
    std::cout << "[ RUN     ] " << test.name << "\n";
    if (test.fn(project_root)) {
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
