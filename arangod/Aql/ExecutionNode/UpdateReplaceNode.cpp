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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "UpdateReplaceNode.h"

#include "Aql/Collection.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/ModificationExecutor.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"

using namespace arangodb::aql;

namespace arangodb::aql {

UpdateReplaceNode::UpdateReplaceNode(ExecutionPlan* plan, ExecutionNodeId id,
                                     Collection const* collection,
                                     ModificationOptions const& options,
                                     Variable const* inDocVariable,
                                     Variable const* inKeyVariable,
                                     Variable const* outVariableOld,
                                     Variable const* outVariableNew)
    : ModificationNode(plan, id, collection, options, outVariableOld,
                       outVariableNew),
      _inDocVariable(inDocVariable),
      _inKeyVariable(inKeyVariable) {
  TRI_ASSERT(_inDocVariable != nullptr);
  // _inKeyVariable might be a nullptr
}

UpdateReplaceNode::UpdateReplaceNode(ExecutionPlan* plan,
                                     arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inDocVariable(
          Variable::varFromVPack(plan->getAst(), base, "inDocVariable")),
      _inKeyVariable(Variable::varFromVPack(plan->getAst(), base,
                                            "inKeyVariable", true)) {}

void UpdateReplaceNode::doToVelocyPack(VPackBuilder& nodes,
                                       unsigned flags) const {
  ModificationNode::doToVelocyPack(nodes, flags);
  nodes.add(VPackValue("inDocVariable"));
  _inDocVariable->toVelocyPack(nodes);

  // inKeyVariable might be empty
  if (_inKeyVariable != nullptr) {
    nodes.add(VPackValue("inKeyVariable"));
    _inKeyVariable->toVelocyPack(nodes);
  }
}

/// @brief getVariablesUsedHere, modifying the set in-place
void UpdateReplaceNode::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_inDocVariable);

  if (_inKeyVariable != nullptr) {
    vars.emplace(_inKeyVariable);
  }
}

void UpdateReplaceNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  if (_inDocVariable != nullptr) {
    _inDocVariable = Variable::replace(_inDocVariable, replacements);
  }
  if (_inKeyVariable != nullptr) {
    _inKeyVariable = Variable::replace(_inKeyVariable, replacements);
  }
}

void UpdateReplaceNode::replaceAttributeAccess(
    ExecutionNode const* self, Variable const* searchVariable,
    std::span<std::string_view> attribute, Variable const* replaceVariable,
    size_t /*index*/) {
  auto replace = [&](Variable const*& variable) {
    if (variable != nullptr && searchVariable == variable &&
        attribute.size() == 1 && attribute[0] == StaticStrings::KeyString) {
      // replace the following patterns:
      // FOR doc IN collection LET #x = doc._key (projection)
      //   UPDATE|REPLACE doc._key WITH ... INTO collection
      // with
      //   UPDATE|REPLACE #x WITH ... INTO collection
      // doc._id does not need to be supported for the lookup value here,
      // as using `_id` for the lookup value is not supported.
      variable = replaceVariable;
    }
  };

  replace(_inKeyVariable);
  replace(_inDocVariable);
}

}  // namespace arangodb::aql
