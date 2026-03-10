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

namespace arangodb::aql::query::activity {

struct AQLQueryActivityData {
  std::string queryString;
  double startTime;
  std::optional<velocypack::SharedSlice> bindParameters;

  template<typename Inspector>
  inline friend auto inspect(Inspector& f, AQLQueryActivityData& d) {
    return f.object(d).fields(f.field("queryString", d.queryString),  //
                              f.field("startTime", d.startTime),      //
                              f.field("bindParameters", d.bindParameters));
  }
};

struct AQLQueryActivity
    : activities::GuardedActivity<AQLQueryActivity, AQLQueryActivityData> {
  AQLQueryActivity(activities::ActivityId id, activities::ActivityHandle parent,
                   AQLQueryActivityData data)
      : activities::GuardedActivity<AQLQueryActivity, AQLQueryActivityData>(
            id, parent, "AQLQueryActivity", std::move(data)) {}
  using Data = AQLQueryActivityData;
};
}  // namespace arangodb::aql::query::activity
