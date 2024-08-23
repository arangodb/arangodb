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

#include "UpsertNode.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/ModificationExecutor.h"
#include "Aql/Executor/ModificationExecutorHelpers.h"
#include "Aql/ModificationExecutorFlags.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/UpsertModifier.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"

using namespace arangodb::aql;

namespace arangodb::aql {

using SingleRowUpsertExecutionBlock = ExecutionBlockImpl<ModificationExecutor<
    SingleRowFetcher<BlockPassthrough::Disable>, UpsertModifier>>;

UpsertNode::UpsertNode(
    ExecutionPlan* plan, ExecutionNodeId id, Collection const* collection,
    ModificationOptions const& options, Variable const* inDocVariable,
    Variable const* insertVariable, Variable const* updateVariable,
    Variable const* outVariableNew, bool isReplace, bool canReadOwnWrites)
    : ModificationNode(plan, id, collection, options, nullptr, outVariableNew),
      _inDocVariable(inDocVariable),
      _insertVariable(insertVariable),
      _updateVariable(updateVariable),
      _isReplace(isReplace),
      _canReadOwnWrites(canReadOwnWrites) {
  TRI_ASSERT(_inDocVariable != nullptr);
  TRI_ASSERT(_insertVariable != nullptr);
  TRI_ASSERT(_updateVariable != nullptr);

  TRI_ASSERT(_outVariableOld == nullptr);
}

UpsertNode::UpsertNode(ExecutionPlan* plan,
                       arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inDocVariable(
          Variable::varFromVPack(plan->getAst(), base, "inDocVariable")),
      _insertVariable(
          Variable::varFromVPack(plan->getAst(), base, "insertVariable")),
      _updateVariable(
          Variable::varFromVPack(plan->getAst(), base, "updateVariable")),
      _isReplace(base.get("isReplace").isTrue()),
      _canReadOwnWrites(true) {
  if (auto s = base.get(StaticStrings::ReadOwnWrites); !s.isNone()) {
    // "readOwnWrites" attribute was introduced in 3.12.
    // older coordinators will not send it. thus we make it default
    // to true.
    _canReadOwnWrites = s.isTrue();
  }
}

/// @brief toVelocyPack
void UpsertNode::doToVelocyPack(VPackBuilder& nodes, unsigned flags) const {
  ModificationNode::doToVelocyPack(nodes, flags);

  nodes.add(VPackValue("inDocVariable"));
  _inDocVariable->toVelocyPack(nodes);
  nodes.add(VPackValue("insertVariable"));
  _insertVariable->toVelocyPack(nodes);
  nodes.add(VPackValue("updateVariable"));
  _updateVariable->toVelocyPack(nodes);
  nodes.add("isReplace", VPackValue(_isReplace));
  nodes.add(StaticStrings::ReadOwnWrites, VPackValue(_canReadOwnWrites));
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> UpsertNode::createBlock(
    ExecutionEngine& engine) const {
  using namespace arangodb::aql;

  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  RegisterId inDoc = variableToRegisterId(_inDocVariable);
  RegisterId insert = variableToRegisterId(_insertVariable);
  RegisterId update = variableToRegisterId(_updateVariable);

  RegisterId outputNew = variableToRegisterOptionalId(_outVariableNew);

  RegisterId outputOld = variableToRegisterOptionalId(_outVariableOld);

  auto readableInputRegisters = RegIdSet{inDoc, insert, update};
  auto writableOutputRegisters = RegIdSet{};
  if (outputNew.isValid()) {
    writableOutputRegisters.emplace(outputNew);
  }
  if (outputOld.isValid()) {
    writableOutputRegisters.emplace(outputOld);
  }
  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writableOutputRegisters));

  OperationOptions options = ModificationExecutorHelpers::convertOptions(
      _options, _outVariableNew, _outVariableOld);
  // We must not disable indexing for UPSERTs because the subquery might rely on
  // a non-unique secondary index
  options.canDisableIndexing = false;

  // if we do not need to observe our own writes, we can turn on batching
  // for the UpsertNode. otherwise, the Upsert will execute with a batch
  // size of just 1.
  size_t batchSize = 1;
  if (!_canReadOwnWrites) {
    batchSize = ExecutionBlock::DefaultBatchSize;
  }

  auto executorInfos = ModificationExecutorInfos(
      &engine, inDoc, insert, update, outputNew, outputOld,
      RegisterPlan::MaxRegisterId /*output*/, _plan->getAst()->query(),
      std::move(options), collection(), batchSize,
      ProducesResults(producesResults()),
      ConsultAqlWriteFilter(_options.consultAqlWriteFilter),
      IgnoreErrors(_options.ignoreErrors), DoCount(countStats()),
      IsReplace(_isReplace) /*(needed by upsert)*/,
      IgnoreDocumentNotFound(_options.ignoreDocumentNotFound));
  return std::make_unique<SingleRowUpsertExecutionBlock>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* UpsertNode::clone(ExecutionPlan* plan,
                                 bool withDependencies) const {
  auto c = std::make_unique<UpsertNode>(
      plan, _id, collection(), _options, _inDocVariable, _insertVariable,
      _updateVariable, _outVariableNew, _isReplace, _canReadOwnWrites);
  ModificationNode::cloneCommon(c.get());

  return cloneHelper(std::move(c), withDependencies);
}

void UpsertNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  if (_inDocVariable != nullptr) {
    _inDocVariable = Variable::replace(_inDocVariable, replacements);
  }
  if (_insertVariable != nullptr) {
    _insertVariable = Variable::replace(_insertVariable, replacements);
  }
  if (_updateVariable != nullptr) {
    _updateVariable = Variable::replace(_updateVariable, replacements);
  }
}

void UpsertNode::replaceAttributeAccess(ExecutionNode const* self,
                                        Variable const* searchVariable,
                                        std::span<std::string_view> attribute,
                                        Variable const* replaceVariable,
                                        size_t /*index*/) {
  auto replace = [&](Variable const*& variable) {
    if (variable != nullptr && searchVariable == variable &&
        attribute.size() == 1 && attribute[0] == StaticStrings::KeyString) {
      variable = replaceVariable;
    }
  };

  replace(_inDocVariable);
  replace(_insertVariable);
  replace(_updateVariable);
}

size_t UpsertNode::getMemoryUsedBytes() const { return sizeof(*this); }

}  // namespace arangodb::aql
