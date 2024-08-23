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

#include "CalculationNode.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/CalculationExecutor.h"
#include "Aql/Executor/IdExecutor.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Variable.h"

using namespace arangodb;
using namespace arangodb::aql;

CalculationNode::CalculationNode(ExecutionPlan* plan,
                                 arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _outVariable(Variable::varFromVPack(plan->getAst(), base, "outVariable")),
      _expression(std::make_unique<Expression>(plan->getAst(), base)) {}

CalculationNode::CalculationNode(ExecutionPlan* plan, ExecutionNodeId id,
                                 std::unique_ptr<Expression> expr,
                                 Variable const* outVariable)
    : ExecutionNode(plan, id),
      _outVariable(outVariable),
      _expression(std::move(expr)) {
  TRI_ASSERT(_expression != nullptr);
  TRI_ASSERT(_outVariable != nullptr);
}

CalculationNode::~CalculationNode() = default;

/// @brief doToVelocyPack, for CalculationNode
void CalculationNode::doToVelocyPack(velocypack::Builder& nodes,
                                     unsigned flags) const {
  nodes.add(VPackValue("expression"));
  _expression->toVelocyPack(nodes, flags);

  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  nodes.add("canThrow", VPackValue(false));

  nodes.add("expressionType", VPackValue(_expression->typeString()));

  if ((flags & SERIALIZE_FUNCTIONS) && _expression->node() != nullptr) {
    auto root = _expression->node();
    if (root != nullptr) {
      // enumerate all used functions, but report each function only once
      std::unordered_set<std::string> functionsSeen;
      nodes.add("functions", VPackValue(VPackValueType::Array));

      Ast::traverseReadOnly(
          root, [&functionsSeen, &nodes](AstNode const* node) -> bool {
            if (node->type == NODE_TYPE_FCALL) {
              auto func = static_cast<Function const*>(node->getData());
              if (functionsSeen.insert(func->name).second) {
                // built-in function, not seen before
                nodes.openObject();
                nodes.add("name", VPackValue(func->name));
                nodes.add(
                    "isDeterministic",
                    VPackValue(func->hasFlag(Function::Flags::Deterministic)));
                nodes.add("canAccessDocuments",
                          VPackValue(func->hasFlag(
                              Function::Flags::CanReadDocuments)));
                nodes.add("canRunOnDBServerCluster",
                          VPackValue(func->hasFlag(
                              Function::Flags::CanRunOnDBServerCluster)));
                nodes.add("canRunOnDBServerOneShard",
                          VPackValue(func->hasFlag(
                              Function::Flags::CanRunOnDBServerOneShard)));
                nodes.add(
                    "cacheable",
                    VPackValue(func->hasFlag(Function::Flags::Cacheable)));
                nodes.add("usesV8", VPackValue(node->willUseV8()));
                // deprecated
                nodes.add("canRunOnDBServer",
                          VPackValue(func->hasFlag(
                              Function::Flags::CanRunOnDBServerCluster)));
                nodes.close();
              }
            } else if (node->type == NODE_TYPE_FCALL_USER) {
              auto func = node->getString();
              if (functionsSeen.insert(func).second) {
                // user defined function, not seen before
                nodes.openObject();
                nodes.add("name", VPackValue(func));
                nodes.add("isDeterministic", VPackValue(false));
                nodes.add("canRunOnDBServer", VPackValue(false));
                nodes.add("usesV8", VPackValue(true));
                nodes.close();
              }
            }
            return true;
          });

      nodes.close();
    }
  }
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> CalculationNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  RegisterId outputRegister = variableToRegisterId(_outVariable);
  TRI_ASSERT((_outVariable->type() == Variable::Type::Const) ==
             outputRegister.isConstRegister());

  VarSet inVars;
  _expression->variables(inVars);

  std::vector<std::pair<VariableId, RegisterId>> expInVarsToRegs;
  expInVarsToRegs.reserve(inVars.size());

  auto inputRegisters = RegIdSet{};

  for (auto const& var : inVars) {
    TRI_ASSERT(var != nullptr);
    auto regId = variableToRegisterId(var);
    expInVarsToRegs.emplace_back(var->id, regId);
    inputRegisters.emplace(regId);
  }

  bool const isReference = (expression()->node()->type == NODE_TYPE_REFERENCE);
  if (isReference) {
    TRI_ASSERT(expInVarsToRegs.size() == 1);
  }
#ifdef USE_V8
  bool const willUseV8 = expression()->willUseV8();
#endif

  TRI_ASSERT(expression() != nullptr);

  auto registerInfos = createRegisterInfos(std::move(inputRegisters),
                                           outputRegister.isRegularRegister()
                                               ? RegIdSet{outputRegister}
                                               : RegIdSet{});

  if (_outVariable->type() == Variable::Type::Const) {
    // we have a const variable, so we can simply use an IdExector to forward
    // the rows.
    auto executorInfos = IdExecutorInfos(false, outputRegister);
    return std::make_unique<ExecutionBlockImpl<
        IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  }

  auto executorInfos = CalculationExecutorInfos(
      outputRegister,
      engine.getQuery() /* used for v8 contexts and in expression */,
      *expression(),
      std::move(expInVarsToRegs)); /* required by expression.execute */

  if (isReference) {
    return std::make_unique<
        ExecutionBlockImpl<CalculationExecutor<CalculationType::Reference>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
#ifdef USE_V8
  } else if (willUseV8) {
    return std::make_unique<
        ExecutionBlockImpl<CalculationExecutor<CalculationType::V8Condition>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
#endif
  } else {
    return std::make_unique<
        ExecutionBlockImpl<CalculationExecutor<CalculationType::Condition>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  }
}

ExecutionNode* CalculationNode::clone(ExecutionPlan* plan,
                                      bool withDependencies) const {
  return cloneHelper(
      std::make_unique<CalculationNode>(
          plan, _id, _expression->clone(plan->getAst(), true), _outVariable),
      withDependencies);
}

/// @brief estimateCost
CostEstimate CalculationNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

AsyncPrefetchEligibility CalculationNode::canUseAsyncPrefetching()
    const noexcept {
  // we cannot use prefetching if the calculation employs V8, because the
  // Query object only has a single V8 context, which it can enter and exit.
  // with prefetching, multiple threads can execute calculations in the same
  // Query instance concurrently, and when using V8, they could try to
  // enter/exit the V8 context of the query concurrently. this is currently
  // not thread-safe, so we don't use prefetching.
  // the constraint for determinism is there because we could produce
  // different query results when prefetching is enabled, at least in
  // streaming queries.
  return expression()->isDeterministic() && !expression()->willUseV8()
             ? AsyncPrefetchEligibility::kEnableForNode
             : AsyncPrefetchEligibility::kDisableForNode;
}

ExecutionNode::NodeType CalculationNode::getType() const { return CALCULATION; }

size_t CalculationNode::getMemoryUsedBytes() const { return sizeof(*this); }

Variable const* CalculationNode::outVariable() const { return _outVariable; }

Expression* CalculationNode::expression() const {
  TRI_ASSERT(_expression != nullptr);
  return _expression.get();
}

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void CalculationNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  VarSet variables;
  expression()->variables(variables);

  // check if the calculation uses any of the variables that we want to
  // replace
  for (auto const& it : variables) {
    if (replacements.contains(it->id)) {
      // calculation uses a to-be-replaced variable
      expression()->replaceVariables(replacements);
      return;
    }
  }
}

void CalculationNode::replaceAttributeAccess(
    ExecutionNode const* self, Variable const* searchVariable,
    std::span<std::string_view> attribute, Variable const* replaceVariable,
    size_t /*index*/) {
  expression()->replaceAttributeAccess(searchVariable, attribute,
                                       replaceVariable);
}

void CalculationNode::getVariablesUsedHere(VarSet& vars) const {
  expression()->variables(vars);
}

std::vector<Variable const*> CalculationNode::getVariablesSetHere() const {
  return std::vector<Variable const*>{_outVariable};
}

bool CalculationNode::isDeterministic() {
  return _expression->isDeterministic();
}
