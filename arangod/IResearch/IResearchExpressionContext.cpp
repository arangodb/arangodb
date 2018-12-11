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

#include "Aql/AqlItemBlock.h"
#include "IResearch/IResearchExpressionContext.h"
#include "IResearch/IResearchViewNode.h"

namespace {

inline arangodb::aql::RegisterId getRegister(
    arangodb::aql::Variable const& var,
    arangodb::aql::ExecutionNode const& node
) noexcept {
  auto const& vars = node.getRegisterPlan()->varInfo;
  auto const it = vars.find(var.id);

  return vars.end() == it
    ? arangodb::aql::ExecutionNode::MaxRegisterId
    : it->second.registerId;
  }

}

namespace arangodb {
namespace iresearch {

using namespace arangodb::aql;

// -----------------------------------------------------------------------------
// --SECTION--                              ViewExpressionContext implementation
// -----------------------------------------------------------------------------

size_t ViewExpressionContext::numRegisters() const {
  return _data->getNrRegs();
}

AqlValue ViewExpressionContext::getVariableValue(
    Variable const* var, bool doCopy, bool& mustDestroy
) const {
  TRI_ASSERT(var);

  if (var == &_node->outVariable()) {
    // self-reference
    if (_expr) {
      std::string expr;

      try {
        expr = _expr->toString();
      } catch (...) { }

      if (!expr.empty()) {
        THROW_ARANGO_EXCEPTION_FORMAT(
          TRI_ERROR_NOT_IMPLEMENTED,
          "Unable to evaluate loop variable '%s' as a part of ArangoSearch noncompliant expression '%s'",
          var->name.c_str(),
          expr.c_str()
        );
      }
    }

    THROW_ARANGO_EXCEPTION_FORMAT(
      TRI_ERROR_NOT_IMPLEMENTED,
      "Unable to evaluate loop variable '%s' as a part of ArangoSearch noncompliant expression",
      var->name.c_str()
    );
  }

  mustDestroy = false;
  auto const reg = getRegister(*var, *_node);

  if (reg == arangodb::aql::ExecutionNode::MaxRegisterId) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  auto& value = _data->getValueReference(_pos, reg);

  if (doCopy) {
    mustDestroy = true;
    return value.clone();
  }

  return value;
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
