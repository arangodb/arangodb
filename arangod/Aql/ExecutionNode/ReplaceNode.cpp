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

#include "ReplaceNode.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/ModificationExecutorHelpers.h"
#include "Aql/Executor/ModificationExecutorInfos.h"
#include "Aql/ModificationExecutorFlags.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Variable.h"

using namespace arangodb::aql;

namespace arangodb::aql {

using SingleRowUpdateReplaceExecutionBlock =
    ExecutionBlockImpl<ModificationExecutor<
        SingleRowFetcher<BlockPassthrough::Disable>, UpdateReplaceModifier>>;

ReplaceNode::ReplaceNode(ExecutionPlan* plan,
                         arangodb::velocypack::Slice const& base)
    : UpdateReplaceNode(plan, base) {}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> ReplaceNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  RegisterId inDocRegister = variableToRegisterId(_inDocVariable);

  RegisterId inKeyRegister = variableToRegisterOptionalId(_inKeyVariable);

  RegisterId outputNew = variableToRegisterOptionalId(_outVariableNew);

  RegisterId outputOld = variableToRegisterOptionalId(_outVariableOld);

  auto readableInputRegisters = RegIdSet{inDocRegister};
  if (inKeyRegister.isValid()) {
    readableInputRegisters.emplace(inKeyRegister);
  }
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

  auto executorInfos = ModificationExecutorInfos(
      &engine, inDocRegister, inKeyRegister, RegisterPlan::MaxRegisterId,
      outputNew, outputOld, RegisterPlan::MaxRegisterId /*output*/,
      _plan->getAst()->query(), std::move(options), collection(),
      ExecutionBlock::DefaultBatchSize, ProducesResults(producesResults()),
      ConsultAqlWriteFilter(_options.consultAqlWriteFilter),
      IgnoreErrors(_options.ignoreErrors), DoCount(countStats()),
      IsReplace(true), IgnoreDocumentNotFound(_options.ignoreDocumentNotFound));
  return std::make_unique<SingleRowUpdateReplaceExecutionBlock>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* ReplaceNode::clone(ExecutionPlan* plan,
                                  bool withDependencies) const {
  auto c = std::make_unique<ReplaceNode>(plan, _id, collection(), _options,
                                         _inDocVariable, _inKeyVariable,
                                         _outVariableOld, _outVariableNew);
  ModificationNode::cloneCommon(c.get());

  return cloneHelper(std::move(c), withDependencies);
}

size_t ReplaceNode::getMemoryUsedBytes() const { return sizeof(*this); }

}  // namespace arangodb::aql
