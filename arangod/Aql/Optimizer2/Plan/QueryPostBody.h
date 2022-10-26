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
/// @author Heiko  Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Inspection/Types.h"
#include "Aql/Optimizer2/Types/Types.h"
#include "Aql/Optimizer2/Inspection/VPackInspection.h"

#include <velocypack/Builder.h>

namespace arangodb::aql::optimizer2::plan {

struct QueryPostBody {
  AttributeTypes::String query;
  std::optional<VPackBuilder> bindVars;
  std::optional<VPackBuilder> options;
};

template<class Inspector>
auto inspect(Inspector& f, QueryPostBody& v) {
  return f.object(v).fields(f.field("query", v.query),
                            f.field("bindVars", v.bindVars),
                            f.field("options", v.options));
}

}  // namespace arangodb::aql::optimizer2::plan