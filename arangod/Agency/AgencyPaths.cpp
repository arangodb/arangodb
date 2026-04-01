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
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "AgencyPaths.h"
#include <Replication2/ReplicatedLog/LogCommon.h>

namespace arangodb::cluster::paths {

auto root() -> std::shared_ptr<Root const> {
  return Root::make_shared();
}

auto to_string(Path const& path) -> std::string { return path.str(); }

namespace aliases {
auto arango() -> std::shared_ptr<Root::Arango const> {
  return root()->arango();
}

auto plan() -> std::shared_ptr<Root::Arango::Plan const> {
  return root()->arango()->plan();
}

auto current() -> std::shared_ptr<Root::Arango::Current const> {
  return root()->arango()->current();
}

auto target() -> std::shared_ptr<Root::Arango::Target const> {
  return root()->arango()->target();
}

auto supervision()
    -> std::shared_ptr<Root::Arango::Supervision const> {
  return root()->arango()->supervision();
}

}  //aliases

auto Root::Arango::Target::ReplicatedLogs::Database::log(
    replication2::LogId id) const -> std::shared_ptr<const Log> {
  return Log::make_shared(shared_from_this(), std::to_string(id.id()));
}

auto Root::Arango::Plan::ReplicatedLogs::Database::log(
    replication2::LogId id) const -> std::shared_ptr<const Log> {
  return Log::make_shared(shared_from_this(), std::to_string(id.id()));
}

auto Root::Arango::Current::ReplicatedLogs::Database::log(
    replication2::LogId id) const -> std::shared_ptr<const Log> {
  return Log::make_shared(shared_from_this(), std::to_string(id.id()));
}

}  // arangodb::cluster::paths