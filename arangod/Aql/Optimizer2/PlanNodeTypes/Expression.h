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

#include "velocypack/Builder.h"

namespace arangodb::aql::optimizer2::types {

struct Expression {
  AttributeTypes::String type;
  AttributeTypes::Numeric typeID;
  AttributeTypes::Numeric value;
  AttributeTypes::String vType;
  AttributeTypes::Numeric vTypeID;
  AttributeTypes::String expressionType;
  std::optional<bool> excludesNull;
  std::optional<std::vector<Expression>> subNodes;
};

template<class Inspector>
auto inspect(Inspector& f, Expression& v) {
  return f.object(v).fields(
      f.field("type", v.type), f.field("typeID", v.typeID),
      f.field("value", v.value), f.field("vType", v.vType),
      f.field("vTypeID", v.vTypeID),
      f.field("expressionType", v.expressionType),
      f.field("excludesNull", v.excludesNull), f.field("subNodes", v.subNodes));
}

}  // namespace arangodb::aql::optimizer2::types
