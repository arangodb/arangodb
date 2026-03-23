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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <velocypack/SharedSlice.h>
#include "Activities/GuardedActivity.h"
#include "Aql/QueryOptions.h"
#include "types.h"

// TODO: note that this *should* be namespace arangodb::aql::query::activity ;
// this is currently impossible because some parts of the code use using
// namespace arangodb::aql, even in headers. this breaks because boost uses the
// term query and breaks.
namespace arangodb::aql::query_activity {

struct AqlQueryActivityData {
  QueryId id;
  double startTime;
  std::string database;
  std::string user;
  std::optional<std::string> queryString;
  std::optional<velocypack::SharedSlice> options;
  std::optional<velocypack::SharedSlice> bindParameters;
  std::optional<velocypack::SharedSlice> plan;
  // These are only set in ClusterQuery.
  // TODO: pry apart Query and ClusterQuery to have dedicated
  // activities for them
  std::optional<velocypack::SharedSlice> querySlice;
  std::optional<velocypack::SharedSlice> collections;
  std::optional<velocypack::SharedSlice> variables;
  std::optional<velocypack::SharedSlice> snippets;
  std::optional<velocypack::SharedSlice> traverserEngines;
  std::optional<bool> fastPathLocking;

  template<typename Inspector>
  inline friend auto inspect(Inspector& f, AqlQueryActivityData& d) {
    return f.object(d).fields(
        f.field("queryId", d.id),                         //
        f.field("startTime", d.startTime),                //
        f.field("database", d.database),                  //
        f.field("user", d.user),                          //
        f.field("queryString", d.queryString),            //
        f.field("options", d.options),                    //
        f.field("bindParameters", d.bindParameters),      //
        f.field("plan", d.plan),                          //
        f.field("querySlice", d.querySlice),              //
        f.field("collections", d.collections),            //
        f.field("variables", d.variables),                //
        f.field("snippets", d.snippets),                  //
        f.field("traverserEngines", d.traverserEngines),  //
        f.field("fastPathLocking", d.fastPathLocking));
  }
};

struct AqlQueryActivity
    : activities::GuardedActivity<AqlQueryActivity, AqlQueryActivityData> {
  AqlQueryActivity(activities::ActivityId id, activities::ActivityHandle parent,
                   AqlQueryActivityData data)
      : activities::GuardedActivity<AqlQueryActivity, AqlQueryActivityData>(
            id, parent, "AQLQuery", std::move(data)) {}
  using Data = AqlQueryActivityData;

  auto setPlanSlice(velocypack::SharedSlice slice) {
    _data.getLockedGuard()->plan = std::move(slice);
  }
  auto setQuerySlice(velocypack::SharedSlice slice) {
    _data.getLockedGuard()->querySlice = std::move(slice);
  }
  auto setCollections(velocypack::SharedSlice slice) {
    _data.getLockedGuard()->collections = std::move(slice);
  }
  auto setVariables(velocypack::SharedSlice slice) {
    _data.getLockedGuard()->variables = std::move(slice);
  }
  auto setSnippets(velocypack::SharedSlice slice) {
    _data.getLockedGuard()->snippets = std::move(slice);
  }
  auto setTraverserEngines(velocypack::SharedSlice slice) {
    _data.getLockedGuard()->traverserEngines = std::move(slice);
  }
  auto setFastPathLocking(bool locking) {
    _data.getLockedGuard()->fastPathLocking = locking;
  }

  auto setUser(std::string user) { _data.getLockedGuard()->user = user; }
};
}  // namespace arangodb::aql::query_activity
