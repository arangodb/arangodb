////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
///
////////////////////////////////////////////////////////////////////////////////
#include "Permissions.h"

#include <Assertions/ProdAssert.h>

#include <format>
#include <span>
#include <string>

namespace arangodb {

auto to_string(CollectionAccessLevel level) -> std::string_view {
  switch (level) {
    case CollectionAccessLevel::Read:
      return "read";
    case CollectionAccessLevel::WriteData:
      return "writedata";
    case CollectionAccessLevel::WriteMeta:
      return "writemeta";
  }
  ADB_PROD_CRASH();
}

auto to_string(DatabaseAccessLevel level) -> std::string_view {
  switch (level) {
    case DatabaseAccessLevel::Read:
      return "read";
    case DatabaseAccessLevel::Write:
      return "write";
  }
  ADB_PROD_CRASH();
}

auto to_string(ViewAccessLevel level) -> std::string_view {
  switch (level) {
    case ViewAccessLevel::Read:
      return "read";
    case ViewAccessLevel::Modify:
      return "modify";
  }
  ADB_PROD_CRASH();
}

auto to_string(AnalyzerAccessLevel level) -> std::string_view {
  switch (level) {
    case AnalyzerAccessLevel::Read:
      return "read";
    case AnalyzerAccessLevel::Modify:
      return "modify";
  }
  ADB_PROD_CRASH();
}

auto to_string(GraphAccessLevel level) -> std::string_view {
  switch (level) {
    case GraphAccessLevel::Read:
      return "read";
    case GraphAccessLevel::Modify:
      return "modify";
  }
  ADB_PROD_CRASH();
}

}  // namespace arangodb

namespace arangodb::auth::perms {
namespace {

/**
 * Render a list of names as a comma-separated, single-quoted enumeration.
 *
 * Example: {"a", "b"} becomes "'a', 'b'"; an empty range becomes "".
 */
auto joinQuoted(std::span<std::string const> names) -> std::string {
  std::string result;
  for (auto const& name : names) {
    if (!result.empty()) {
      result += ", ";
    }
    result += std::format("'{}'", name);
  }
  return result;
}

}  // namespace

auto describe(SeeDatabase const& perm) -> std::string {
  return std::format("see database '{}'", perm.name);
}

auto describe(CreateDatabase const& perm) -> std::string {
  return std::format("create database '{}'", perm.name);
}

auto describe(DropDatabase const& perm) -> std::string {
  return std::format("drop database '{}'", perm.name);
}

auto describe(UseDatabase const& perm) -> std::string {
  return std::format("use database '{}' with access level '{}'", perm.name,
                     to_string(perm.level));
}

auto describe(SeeCollection const& perm) -> std::string {
  return std::format("see collection '{}' in database '{}'", perm.name,
                     perm.db);
}

auto describe(CreateCollection const& perm) -> std::string {
  return std::format("create collection '{}' in database '{}'", perm.name,
                     perm.db);
}

auto describe(DropCollection const& perm) -> std::string {
  return std::format("drop collection '{}' in database '{}'", perm.name,
                     perm.db);
}

auto describe(UseCollection const& perm) -> std::string {
  return std::format(
      "use collection '{}' in database '{}' with access level '{}'", perm.name,
      perm.db, to_string(perm.level));
}

auto describe(DumpCollection const& perm) -> std::string {
  return std::format("dump collection '{}' in database '{}'", perm.name,
                     perm.db);
}

auto describe(RestoreCollection const& perm) -> std::string {
  return std::format("restore collection '{}' in database '{}' {} overwrite",
                     perm.name, perm.db, perm.overwrite ? "with" : "without");
}

auto describe(RestoreCreateIndex const& perm) -> std::string {
  return std::format(
      "create index during restore on collection '{}' in database '{}'",
      perm.collName, perm.db);
}

auto describe(RestoreCreateView const& perm) -> std::string {
  return std::format(
      "create view '{}' during restore in database '{}' with linked "
      "collections [{}]",
      perm.viewName, perm.db, joinQuoted(perm.linkedCollNames));
}

auto describe(RestoreDropView const& perm) -> std::string {
  return std::format("drop view '{}' during restore in database '{}'",
                     perm.viewName, perm.db);
}

auto describe(RestoreWriteData const& perm) -> std::string {
  return std::format(
      "write data during restore to collection '{}' in database '{}'",
      perm.collName, perm.db);
}

auto describe(SeeView const& perm) -> std::string {
  return std::format("see view '{}' in database '{}'", perm.name, perm.db);
}

auto describe(CreateView const& perm) -> std::string {
  return std::format(
      "create view '{}' in database '{}' with linked collections [{}]",
      perm.name, perm.db, joinQuoted(perm.linkedCollections));
}

auto describe(ModifyView const& perm) -> std::string {
  return std::format(
      "modify view '{}' in database '{}' with linked collections [{}]",
      perm.name, perm.db, joinQuoted(perm.linkedCollections));
}

auto describe(RenameView const& perm) -> std::string {
  return std::format("rename view '{}' to '{}' in database '{}'", perm.oldName,
                     perm.newName, perm.db);
}

auto describe(DropView const& perm) -> std::string {
  return std::format("drop view '{}' in database '{}'", perm.name, perm.db);
}

auto describe(UseView const& perm) -> std::string {
  return std::format("use view '{}' in database '{}' with access level '{}'",
                     perm.name, perm.db, to_string(perm.level));
}

auto describe(SeeAnalyzer const& perm) -> std::string {
  return std::format("see analyzer '{}' in database '{}'", perm.name, perm.db);
}

auto describe(CreateAnalyzer const& perm) -> std::string {
  return std::format("create analyzer '{}' in database '{}'", perm.name,
                     perm.db);
}

auto describe(DropAnalyzer const& perm) -> std::string {
  return std::format("drop analyzer '{}' in database '{}'", perm.name, perm.db);
}

auto describe(UseAnalyzer const& perm) -> std::string {
  return std::format(
      "use analyzer '{}' in database '{}' with access level '{}'", perm.name,
      perm.db, to_string(perm.level));
}

auto describe(SeeGraph const& perm) -> std::string {
  return std::format("see graph '{}' in database '{}'", perm.name, perm.db);
}

auto describe(CreateGraph const& perm) -> std::string {
  return std::format(
      "create graph '{}' in database '{}' with collections to create [{}] and "
      "collections to read [{}]",
      perm.name, perm.db, joinQuoted(perm.collectionNamesToCreate),
      joinQuoted(perm.collectionNamesToRead));
}

auto describe(DropGraph const& perm) -> std::string {
  return std::format("drop graph '{}' in database '{}' with collections [{}]",
                     perm.name, perm.db, joinQuoted(perm.collectionNames));
}

auto describe(UseGraph const& perm) -> std::string {
  return std::format("use graph '{}' in database '{}' with access level '{}'",
                     perm.name, perm.db, to_string(perm.level));
}

auto describe(ReadUser const& perm) -> std::string {
  return std::format("read user '{}'", perm.name);
}

auto describe(WriteUser const& perm) -> std::string {
  return std::format("write user '{}'", perm.name);
}

}  // namespace arangodb::auth::perms
