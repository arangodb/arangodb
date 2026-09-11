#include "find_activity_subclasses.h"

#include "sources.h"
#include "tests.h"
#include <iostream>
#include <string>
#include <vector>

#ifndef PROJECT_ROOT
#error "PROJECT_ROOT must be defined at compile time (absolute path to repo)"
#endif
#ifndef BUILD_PATH
#error "BUILD_PATH must be defined at compile time (arangodb build directory)"
#endif

namespace {

/**
 * Format the IR types in a vaguely C++-literal-ish form for failure
 * diagnostics, so a mismatch can be copy-pasted into source.
 */
std::ostream& operator<<(std::ostream& os, Member const& member) {
  return os << "Member{.name=\"" << member.name << "\", .type=\"" << member.type
            << "\"}";
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
  os << "ActivityDeclaration{.owner=\"" << decl.owner << "\", .type=\""
     << decl.type << "\", .field_types={";
  for (size_t index = 0; index < decl.data_type_definition.size(); ++index) {
    if (index != 0) os << ", ";
    os << decl.data_type_definition[index];
  }
  return os << "}}";
}

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

  std::cerr << "    Assertion failure: Activities do not include:\n"
            << "        " << expected << "\n"
            << "    Got " << activities.size() << " activities:\n";
  for (ActivityDeclaration const& activity : activities) {
    std::cerr << "        " << activity << "\n";
  }
  std::cerr << std::endl;
  return false;
}

/**
 * Activities declared in `source_paths`, read from this build's compilation
 * database.
 */
auto activities_in(std::vector<std::string> const& source_paths)
    -> std::vector<ActivityDeclaration> {
  auto const database = sources::get_database(BUILD_PATH);
  auto const& compilation_database = *std::get<sources::Database>(database);
  return find_all_activities(
      compilation_database,
      sources::get_sources(compilation_database, source_paths));
}

}  // namespace

TestCase const tests[] = {
    {"find_transaction_activity",
     []() -> bool {
       return assert_includes(
           activities_in({std::string{PROJECT_ROOT} +
                          "/arangod/StorageEngine/TransactionState.cpp"}),
           ActivityDeclaration{
               .owner = "arangodb::TransactionState",
               .type = "arangodb::transaction::activity::TransactionActivity",
               .data_type_definition =
                   std::vector<Struct>{
                       Struct{
                           .name = "arangodb::transaction::activity::"
                                   "TransactionActivityData",
                           .fields =
                               {
                                   Member{.name = "user",
                                          .type = "std::string"},
                                   Member{.name = "database",
                                          .type = "std::string"},
                                   Member{.name = "tid",
                                          .type = "arangodb::TransactionId"},
                                   Member{
                                       .name = "status",
                                       .type = "arangodb::transaction::Status"},
                                   Member{.name = "collections",
                                          .type =
                                              "std::vector<"
                                              "arangodb::transaction::activity:"
                                              ":TransactionCollection>"},
                               }},
                       Struct{.name = "arangodb::TransactionId", .fields = {}},
                       Struct{
                           .name = "arangodb::transaction::activity::"
                                   "TransactionCollection",
                           .fields =
                               {
                                   Member{.name = "name",
                                          .type = "std::string"},
                                   Member{.name = "cid",
                                          .type = "arangodb::DataSourceId"},
                                   Member{.name = "accessType",
                                          .type = "arangodb::AccessMode::Type"},
                                   Member{.name = "lockStatus",
                                          .type = "arangodb::transaction::"
                                                  "activity::LockStatus"},
                               }},
                       Struct{.name = "arangodb::DataSourceId", .fields = {}}},
           });
     }},
    {"find_maintenance_activity",
     []() -> bool {
       return assert_includes(
           activities_in(
               {std::string{PROJECT_ROOT} + "/arangod/Cluster/ActionBase.h"}),
           ActivityDeclaration{
               .owner = "arangodb::maintenance::ActionBase",
               .type = "arangodb::maintenance::activity::ActionActivity",
               .data_type_definition = std::vector<Struct>{
                   Struct{.name = "arangodb::maintenance::ActionDescription",
                          // TODO show all inspected fields
                          .fields = {}}}});
     }},
    {"find_collection_creation_activity", []() -> bool {
       return assert_includes(
           activities_in({std::string{PROJECT_ROOT} +
                          "/arangod/VocBase/Methods/Collections.cpp"}),
           ActivityDeclaration{
               .owner = "arangodb::methods::Collections::create",
               .type = "arangodb::activities::GenericActivity",
               .data_type_definition = std::vector<Struct>{
                   {.name = "arangodb::activities::GenericActivityData"}}});
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
