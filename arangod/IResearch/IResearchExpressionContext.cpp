////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchExpressionContext.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/IResearchViewNode.h"
#include "Basics/StaticStrings.h"

namespace arangodb {
namespace iresearch {

using namespace arangodb::aql;

// -----------------------------------------------------------------------------
// --SECTION--                              ViewExpressionContext implementation
// -----------------------------------------------------------------------------

size_t ViewExpressionContext::numRegisters() const {
  return _data->getNrRegs();
}

AqlValue ViewExpressionContext::getVariableValue(Variable const* var, bool doCopy,
                                                 bool& mustDestroy) const {
  TRI_ASSERT(var);

  if (var == &_node->outVariable()) {
    // self-reference
    if (_expr) {
      std::string expr;

      try {
        expr = _expr->toString();
      } catch (...) {
      }

      if (!expr.empty()) {
        THROW_ARANGO_EXCEPTION_FORMAT(
            TRI_ERROR_NOT_IMPLEMENTED,
            "Unable to evaluate loop variable '%s' as a part of ArangoSearch "
            "noncompliant expression '%s'",
            var->name.c_str(), expr.c_str());
      }
    }

    THROW_ARANGO_EXCEPTION_FORMAT(
        TRI_ERROR_NOT_IMPLEMENTED,
        "Unable to evaluate loop variable '%s' as a part of ArangoSearch "
        "noncompliant expression",
        var->name.c_str());
  }

  mustDestroy = false;

  auto const& vars = _node->getRegisterPlan()->varInfo;
  auto const it = vars.find(var->id);

  if (vars.end() == it) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  auto const& varInfo = it->second;

  if (varInfo.depth > decltype(varInfo.depth)(_node->getDepth())) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                  "Variable '%s' is used before being assigned",
                                  var->name.c_str());
  }

  AqlValue const* value;

  if (_data == nullptr && _inputRow.isInitialized()) {
    // TODO the other cases shall be removed when everything is done
    value = &_inputRow.getValue(varInfo.registerId);
  } else if (_data != nullptr && !_inputRow.isInitialized()) {
    // TODO old case, shall be removed lated
    value = &_data->getValueReference(_pos, varInfo.registerId);
  } else {
    TRI_ASSERT(false);
  }

  if (doCopy) {
    mustDestroy = true;
    return value->clone();
  }

  return *value;
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
