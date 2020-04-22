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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Mocks/Servers.h"

#include "Aql/ExecutionNode.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.cpp"
#include "Aql/VarUsageFinder.h"
#include "Basics/StringUtils.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

typedef ::arangodb::containers::HashSet<Variable const*> VarSet;

struct PlanMiniMock {
  PlanMiniMock(ExecutionNode::NodeType expectedType)
      : _expectedType(expectedType) {}

  auto increaseCounter(ExecutionNode::NodeType type) {
    EXPECT_FALSE(_called) << "Only count every node once per run";
    _called = true;
    EXPECT_EQ(_expectedType, type) << "Count the correct type";
  }

  bool _called{false};
  ExecutionNode::NodeType _expectedType;
};

struct ExecutionNodeMock {
  ExecutionNodeMock(ExecutionNode::NodeType type, bool isIncreaseDepth,
                    std::vector<Variable const*> input, std::vector<Variable const*> output)
      : _type(type), _isIncreaseDepth(isIncreaseDepth), _input(), _output(), _plan(type) {
    for (auto const& v : input) {
      _input.emplace(v);
    }
    for (auto const& v : output) {
      _output.emplace(v);
    }
  }

  auto plan() -> PlanMiniMock* { return &_plan; }

  auto id() -> ExecutionNodeId { return ExecutionNodeId{0}; }

  auto isIncreaseDepth() -> bool { return _isIncreaseDepth; }

  auto getType() -> ExecutionNode::NodeType { return _type; }

  auto getVarsUsedLater() -> VarSet const& { return _usedLater; }

  auto getVariablesUsedHere(VarSet& res) const -> void {
    for (auto const v : _input) {
      res.emplace(v);
    }
  }

  auto setVarsUsedLater(VarSet& v) -> void { _usedLater = v; }

  auto invalidateVarUsage() -> void {
    _usedLater.clear();
    _varsValid.clear();
    _varUsageValid = false;
  }

  auto setVarUsageValid() -> void { _varUsageValid = true; }

  auto getOutputVariables() const -> VariableIdSet {
    VariableIdSet res;
    for (auto const& v : _output) {
      res.insert(v->id);
    }
    return res;
  }

  auto getVariablesSetHere() const -> VarSet { return _output; }

  auto setRegsToClear(std::unordered_set<RegisterId>&& toClear) -> void {}

  auto getTypeString() -> std::string const& {
    return ExecutionNode::getTypeString(_type);
  }

  auto setVarsValid(VarSet const& v) -> void { _varsValid = v; }

  auto walk(WalkerWorker<ExecutionNodeMock>& worker) -> bool {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  // Will be modified by walker worker
  unsigned int _depth = 0;
  std::shared_ptr<RegisterPlanT<ExecutionNodeMock>> _registerPlan = nullptr;

 private:
  ExecutionNode::NodeType _type;
  bool _isIncreaseDepth;
  VarSet _input;
  VarSet _output;
  VarSet _usedLater;
  VarSet _varsValid{};
  bool _varUsageValid{false};
  PlanMiniMock _plan;
};

class RegisterPlanTest : public ::testing::Test {
 protected:
  RegisterPlanTest() {}

  auto walk(std::vector<ExecutionNodeMock>& nodes)
      -> std::shared_ptr<RegisterPlanT<ExecutionNodeMock>> {
    // Compute the variable usage for nodes.
    std::unordered_map<VariableId, ExecutionNodeMock*> varSetBy;
    ::VarUsageFinder finder(&varSetBy);
    applyWalkerToNodes(nodes, finder);

    auto registerPlan = std::make_shared<RegisterPlanT<ExecutionNodeMock>>();
    RegisterPlanWalkerT<ExecutionNodeMock> worker(registerPlan);
    applyWalkerToNodes(nodes, worker);

    return registerPlan;
  }

  auto generateVars(size_t amount) -> std::vector<Variable> {
    std::vector<Variable> res;
    for (size_t i = 0; i < amount; ++i) {
      res.emplace_back("var" + arangodb::basics::StringUtils::itoa(i), i);
    }
    return res;
  }

  auto assertVariableInRegister(std::shared_ptr<RegisterPlanT<ExecutionNodeMock>> plan,
                                Variable const& v, RegisterId r) {
    auto it = plan->varInfo.find(v.id);
    ASSERT_NE(it, plan->varInfo.end());
    EXPECT_EQ(it->second.registerId, r);
  }

 private:
  template <class Walker>
  auto applyWalkerToNodes(std::vector<ExecutionNodeMock>& nodes, Walker& worker) -> void {
    for (auto it = nodes.rbegin(); it < nodes.rend(); ++it) {
      worker.before(&(*it));
    }

    for (auto& n : nodes) {
      worker.after(&n);
    }
  }
};

TEST_F(RegisterPlanTest, walker_should_plan_registers) {
  auto vars = generateVars(1);
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, true, {}, {&vars[0]}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  assertVariableInRegister(plan, vars[0], 0);
}

TEST_F(RegisterPlanTest, planRegisters_should_append_variables_if_all_are_needed) {
  auto vars = generateVars(2);
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, true, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, true, {}, {&vars[0]}},
      ExecutionNodeMock{ExecutionNode::INDEX, true, {&vars[0]}, {&vars[1]}},
      ExecutionNodeMock{ExecutionNode::RETURN, true, {&vars[0], &vars[1]}, {}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  assertVariableInRegister(plan, vars[0], 0);
  assertVariableInRegister(plan, vars[1], 1);
}

TEST_F(RegisterPlanTest, planRegisters_should_reuse_register_if_possible) {
  auto vars = generateVars(2);
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, true, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, true, {}, {&vars[0]}},
      ExecutionNodeMock{ExecutionNode::INDEX, true, {&vars[0]}, {&vars[1]}},
      ExecutionNodeMock{ExecutionNode::RETURN, true, {&vars[1]}, {}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  assertVariableInRegister(plan, vars[0], 0);
  assertVariableInRegister(plan, vars[1], 0);
}

TEST_F(RegisterPlanTest, planRegisters_should_not_reuse_register_if_block_is_passthrough) {
  auto vars = generateVars(2);
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, true, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, true, {}, {&vars[0]}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, true, {&vars[0]}, {&vars[1]}},
      ExecutionNodeMock{ExecutionNode::RETURN, true, {&vars[1]}, {}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  assertVariableInRegister(plan, vars[0], 0);
  assertVariableInRegister(plan, vars[1], 1);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb