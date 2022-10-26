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

// TODO: Think about "cleaning this up" later. Currently unclear how those types
// do "mix" each other
// TODO HINT: "Expression" aka. "Condition" aka. "(partial) AstNode*?"
// TODO: This will need some cleanup, there are too many optionals IMHO.
struct Expression {
  // now finally those (type, typeID) are optional as well, as we also handle
  // empty expressions like: " { } "
  std::optional<AttributeTypes::String> type;
  std::optional<AttributeTypes::Numeric> typeID;

  std::optional<VPackBuilder> value;
  std::optional<AttributeTypes::String> name;
  std::optional<AttributeTypes::Numeric> id;
  // if value, then also vType and vTypeID must be set
  std::optional<AttributeTypes::String> vType;
  std::optional<AttributeTypes::Numeric> vTypeID;
  std::optional<AttributeTypes::String> expressionType;
  std::optional<std::vector<Expression>> subNodes;
  std::optional<AttributeTypes::Numeric>
      quantifier;  // Quantifier::Type (1,2,3,4)
  std::optional<AttributeTypes::Numeric> levels;
  std::optional<bool> booleanize;
  std::optional<bool> excludesNull;
  std::optional<bool> sorted;

  bool operator==(Expression const& other) const {
    bool valueEquals = false;

    if (this->value.has_value() && other.value.has_value()) {
      if (this->value.value().slice().binaryEquals(
              other.value.value().slice())) {
        // TODO: Check if binaryEquals is the method we want here.
        valueEquals = true;
      }
    }

    return (valueEquals && this->type == other.type &&
            this->typeID == other.typeID && this->name == other.name &&
            this->id == other.id && this->vType == other.vType &&
            this->vTypeID == other.vTypeID &&
            this->expressionType == other.expressionType &&
            this->subNodes == other.subNodes &&
            this->quantifier == other.quantifier &&
            this->levels == other.levels &&
            this->booleanize == other.booleanize &&
            this->excludesNull == other.excludesNull &&
            this->sorted == other.sorted);
  };
};  // namespace arangodb::aql::optimizer2::types

template<class Inspector>
auto inspect(Inspector& f, Expression& v) {
  return f.object(v).fields(
      f.field("type", v.type), f.field("name", v.name), f.field("id", v.id),
      f.field("typeID", v.typeID), f.field("value", v.value),
      f.field("vType", v.vType), f.field("vTypeID", v.vTypeID),
      f.field("expressionType", v.expressionType),
      f.field("excludesNull", v.excludesNull), f.field("subNodes", v.subNodes),
      f.field("quantifier", v.quantifier), f.field("levels", v.levels),
      f.field("booleanize", v.booleanize),
      f.field("excludesNull", v.excludesNull), f.field("sorted", v.sorted));
}

}  // namespace arangodb::aql::optimizer2::types
