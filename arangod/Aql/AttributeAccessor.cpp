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
#include "Aql/Variable.h"
#include "Basics/VelocyPackHelper.h"
#include "Utils/AqlTransaction.h"

using namespace arangodb::aql;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the accessor
////////////////////////////////////////////////////////////////////////////////

AttributeAccessor::AttributeAccessor(
    std::vector<char const*> const& attributeParts, Variable const* variable)
    : _attributeParts(attributeParts),
      _combinedName(),
      _variable(variable) {
  TRI_ASSERT(_variable != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the accessor
////////////////////////////////////////////////////////////////////////////////

AqlValue$ AttributeAccessor::get(arangodb::AqlTransaction* trx,
                                 AqlItemBlock const* argv, size_t startPos,
                                 std::vector<Variable const*> const& vars,
                                 std::vector<RegisterId> const& regs) {
  size_t i = 0;
  for (auto it = vars.begin(); it != vars.end(); ++it, ++i) {
    if ((*it)->id == _variable->id) {
      // get the AQL value
      return AqlValue$(argv->getValueReference(startPos, regs[i]), trx, nullptr).get(_attributeParts);
    }
    // fall-through intentional
  }

  return AqlValueReference(arangodb::basics::VelocyPackHelper::NullValue());
}

