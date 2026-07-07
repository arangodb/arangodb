////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "find_activity_subclasses.h"
#include <gtest/gtest.h>

#ifndef PROJECT_ROOT
#error "PROJECT_ROOT must be defined at compile time (absolute path to repo)"
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
auto includes(std::vector<ActivityDeclaration> const& activities,
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

}  // namespace

std::string const project_root = PROJECT_ROOT;

TEST(ActivityDocumentationTest, finds_transaction_activity) {
  EXPECT_TRUE(includes(
      find_all_activities(project_root +
                          "/arangod/StorageEngine/TransactionState.cpp"),
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
                              Member{.name = "user", .type = "std::string"},
                              Member{.name = "database", .type = "std::string"},
                              Member{.name = "tid",
                                     .type = "arangodb::TransactionId"},
                              Member{.name = "status",
                                     .type = "arangodb::transaction::Status"},
                              Member{.name = "collections",
                                     .type = "std::vector<"
                                             "arangodb::transaction::activity:"
                                             ":TransactionCollection>"},
                          }},
                  Struct{.name = "arangodb::TransactionId", .fields = {}},
                  Struct{.name = "arangodb::transaction::activity::"
                                 "TransactionCollection",
                         .fields =
                             {
                                 Member{.name = "name", .type = "std::string"},
                                 Member{.name = "cid",
                                        .type = "arangodb::DataSourceId"},
                                 Member{.name = "accessType",
                                        .type = "arangodb::AccessMode::Type"},
                                 Member{.name = "lockStatus",
                                        .type = "arangodb::transaction::"
                                                "activity::LockStatus"},
                             }},
                  Struct{.name = "arangodb::DataSourceId", .fields = {}}},
      }));
}

TEST(ActivityDocumentationTest, finds_maintenance_activity) {
  EXPECT_TRUE(includes(
      find_all_activities(project_root + "/arangod/Cluster/ActionBase.h"),
      ActivityDeclaration{
          .owner = "arangodb::maintenance::ActionBase",
          .type = "arangodb::maintenance::activity::ActionActivity",
          .data_type_definition = std::vector<Struct>{
              Struct{.name = "arangodb::maintenance::ActionDescription",
                     // TODO show all inspected fields
                     .fields = {}}}}));
}

TEST(ActivityDocumentationTest, finds_collection_creation_activity) {
  EXPECT_TRUE(includes(
      find_all_activities(project_root +
                          "/arangod/VocBase/Methods/Collections.cpp"),
      ActivityDeclaration{
          .owner = "arangodb::methods::Collections::create",
          .type = "arangodb::activities::GenericActivity",
          .data_type_definition = std::vector<Struct>{
              {.name = "arangodb::activities::GenericActivityData"}}}));
}
