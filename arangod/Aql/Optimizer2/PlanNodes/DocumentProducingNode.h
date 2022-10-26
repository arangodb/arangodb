////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Aql/Optimizer2/Types/Types.h"
#include "Aql/Optimizer2/PlanNodeTypes/Projections.h"
#include "Aql/Optimizer2/PlanNodeTypes/Expression.h"

namespace arangodb::aql::optimizer2::nodes {

struct DocumentProducingNode : optimizer2::types::Projections {
  bool count;
  bool readOwnWrites;
  bool useCache;
  bool producesResult;
  std::optional<optimizer2::types::Expression> filter;

  bool operator==(DocumentProducingNode const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, DocumentProducingNode& v) {
  return f.object(v).fields(
      f.embedFields(static_cast<optimizer2::types::Projections&>(v)),
      f.field("count", v.count), f.field("readOwnWrites", v.readOwnWrites),
      f.field("useCache", v.useCache),
      f.field("producesResult", v.producesResult), f.field("filter", v.filter));
}

}  // namespace arangodb::aql::optimizer2::nodes
