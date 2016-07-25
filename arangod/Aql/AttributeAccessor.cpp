////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "AttributeAccessor.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Utils/Transaction.h"

using namespace arangodb::aql;

/// @brief create the accessor
AttributeAccessor::AttributeAccessor(
    std::vector<std::string>&& attributeParts, Variable const* variable)
    : _attributeParts(attributeParts),
      _variable(variable),
      _type(EXTRACT_MULTI) {

  TRI_ASSERT(_variable != nullptr);
  TRI_ASSERT(!_attributeParts.empty());

  // determine accessor type
  if (_attributeParts.size() == 1) {
    if (attributeParts[0] == StaticStrings::KeyString) {
      _type = EXTRACT_KEY;
    } else if (attributeParts[0] == StaticStrings::IdString) {
      _type = EXTRACT_ID;
    } else if (attributeParts[0] == StaticStrings::FromString) {
      _type = EXTRACT_FROM;
    } else if (attributeParts[0] == StaticStrings::ToString) {
      _type = EXTRACT_TO;
    } else {
      _type = EXTRACT_SINGLE;
    }
  }
}
  
/// @brief replace the variable in the accessor
void AttributeAccessor::replaceVariable(std::unordered_map<VariableId, Variable const*> const& replacements) {
  for (auto const& it : replacements) {
    if (it.first == _variable->id) {
      _variable = it.second;
      return;
    }
  }
}

/// @brief execute the accessor
AqlValue AttributeAccessor::get(arangodb::Transaction* trx,
                                ExpressionContext* context, bool& mustDestroy) {
  AqlValue const& value = context->getVariableValue(_variable, false, mustDestroy);
  // get the AQL value
  switch (_type) {
    case EXTRACT_KEY:
      return value.getKeyAttribute(trx, mustDestroy, true);
    case EXTRACT_ID:
      return value.getIdAttribute(trx, mustDestroy, true);
    case EXTRACT_FROM:
      return value.getFromAttribute(trx, mustDestroy, true);
    case EXTRACT_TO:
      return value.getToAttribute(trx, mustDestroy, true);
    case EXTRACT_SINGLE:
      // use optimized version for single attribute (e.g. variable.attr)
      return value.get(trx, _attributeParts[0], mustDestroy, true);
    case EXTRACT_MULTI:
      // use general version for multiple attributes (e.g. variable.attr.subattr)
      return value.get(trx, _attributeParts, mustDestroy, true);
      // fall-through intentional
  }

  mustDestroy = false;
  return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
}

