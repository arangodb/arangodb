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

namespace arangodb::aql::optimizer2::plan {

struct VerbosePlan {
  VPackBuilder plan;
  bool cacheable;
  std::vector<AttributeTypes::String> warnings;
  bool error;
  AttributeTypes::Numeric code;
};

template<class Inspector>
auto inspect(Inspector& f, VerbosePlan& v) {
  return f.object(v).fields(f.field("plan", v.plan),
                            f.field("cacheable", v.cacheable),
                            f.field("warnings", v.warnings),
                            f.field("error", v.error), f.field("code", v.code));
}

}  // namespace arangodb::aql::optimizer2::plan