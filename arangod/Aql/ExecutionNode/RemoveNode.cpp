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

#include "RemoveNode.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/ModificationExecutor.h"
#include "Aql/Executor/ModificationExecutorHelpers.h"
#include "Aql/ModificationExecutorFlags.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"

using namespace arangodb::aql;

namespace arangodb::aql {

using SingleRowRemoveExecutionBlock = ExecutionBlockImpl<ModificationExecutor<
    SingleRowFetcher<BlockPassthrough::Disable>, RemoveModifier>>;

RemoveNode::RemoveNode(ExecutionPlan* plan,
                       arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inVariable(Variable::varFromVPack(plan->getAst(), base, "inVariable")) {}

void RemoveNode::doToVelocyPack(VPackBuilder& nodes, unsigned flags) const {
  ModificationNode::doToVelocyPack(nodes, flags);
  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> RemoveNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();

  TRI_ASSERT(previousNode != nullptr);

  RegisterId inDocRegister = variableToRegisterId(_inVariable);
  RegisterId outputNew = variableToRegisterOptionalId(_outVariableNew);
  RegisterId outputOld = variableToRegisterOptionalId(_outVariableOld);

  OperationOptions options = ModificationExecutorHelpers::convertOptions(
      _options, _outVariableNew, _outVariableOld);

  auto readableInputRegisters = RegIdSet{inDocRegister};
  auto writableOutputRegisters = RegIdSet{};
  if (outputNew.isValid()) {
    writableOutputRegisters.emplace(outputNew);
  }
  if (outputOld.isValid()) {
    writableOutputRegisters.emplace(outputOld);
  }
  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writableOutputRegisters));

  auto executorInfos = ModificationExecutorInfos(
      &engine, inDocRegister, RegisterPlan::MaxRegisterId,
      RegisterPlan::MaxRegisterId, outputNew, outputOld,
      RegisterPlan::MaxRegisterId /*output*/, _plan->getAst()->query(),
      std::move(options), collection(), ExecutionBlock::DefaultBatchSize,
      ProducesResults(producesResults()),
      ConsultAqlWriteFilter(_options.consultAqlWriteFilter),
      IgnoreErrors(_options.ignoreErrors), DoCount(countStats()),
      IsReplace(false) /*(needed by upsert)*/,
      IgnoreDocumentNotFound(_options.ignoreDocumentNotFound));
  return std::make_unique<SingleRowRemoveExecutionBlock>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* RemoveNode::clone(ExecutionPlan* plan,
                                 bool withDependencies) const {
  auto c = std::make_unique<RemoveNode>(plan, _id, collection(), _options,
                                        _inVariable, _outVariableOld);
  ModificationNode::cloneCommon(c.get());

  return cloneHelper(std::move(c), withDependencies);
}

void RemoveNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _inVariable = Variable::replace(_inVariable, replacements);
}

void RemoveNode::replaceAttributeAccess(ExecutionNode const* self,
                                        Variable const* searchVariable,
                                        std::span<std::string_view> attribute,
                                        Variable const* replaceVariable,
                                        size_t /*index*/) {
  if (_inVariable != nullptr && searchVariable == _inVariable &&
      attribute.size() == 1 && attribute[0] == StaticStrings::KeyString) {
    // replace the following patterns:
    // FOR doc IN collection LET #x = doc._key (projection)
    //   REMOVE doc._key WITH ... INTO collection
    // with
    //   REMOVE #x WITH ... INTO collection
    // doc._id does not need to be supported for the lookup value here,
    // as using `_id` for the lookup value is not supported.
    _inVariable = replaceVariable;
  }
}

size_t RemoveNode::getMemoryUsedBytes() const { return sizeof(*this); }

}  // namespace arangodb::aql
