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
#include "Aql/VarUsageFinder.cpp"

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
  ExecutionNodeMock(ExecutionNode::NodeType type, bool isPassthrough,
                    std::vector<Variable const*> input, std::vector<Variable const*> output)
      : _type(type), _isPassthrough(isPassthrough), _input(), _output(), _plan(type) {
    for (auto const& v : input) {
      _input.emplace(v);
    }
    for (auto const& v : output) {
      _output.emplace(v);
    }
  }

  auto plan() -> PlanMiniMock* { return &_plan; }

  auto id() -> ExecutionNodeId { return ExecutionNodeId{0}; }

  auto isPassthrough() -> bool { return _isPassthrough; }

  auto getType() -> ExecutionNode::NodeType { return _type; }

  auto getVarsUsedLater() -> VarSet const& { return _usedLater; }

  auto getVariablesUsedHere(VarSet& res) const -> void {
    for (auto const v : _input) {
      res.emplace(v);
    }
  }

  auto getVariablesUsedHere(std::unordered_set<Variable const*>& res) const -> void {
    for (auto const v : _input) {
      res.emplace(v);
    }
  }

  auto setVarsUsedLater(VarSet& v) -> void { _usedLater = v; }
  auto setVarsUsedLater(VarUsageFinder<ExecutionNodeMock>::Stack& s) -> void { _usedLaterStack = s; }

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

  auto getTypeString() const -> std::string const& {
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
  bool _isPassthrough;
  VarSet _input;
  VarSet _output;
  VarSet _usedLater;
  VarUsageFinder<ExecutionNodeMock>::Stack _usedLaterStack;
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

  auto assertPlanKeepsAllVariables(std::shared_ptr<RegisterPlanT<ExecutionNodeMock>> plan,
                                   std::vector<ExecutionNodeMock>& nodes) {
    if (nodes.empty()) {
      // Empty plan is valid.
      return;
    }

    // This test tries to do a bookkeeping of which variable is placed into
    // which register. This is done on "requirement" base, if a Node requires a
    // variable it will be added to to the bookkeeping, it can only be removed
    // by the Node claiming to produce it. If there is already a variable at
    // this position in the bookkeeping, The plan is invalid.

    std::vector<std::optional<Variable const*>> varsRequiredHere;

    auto total = plan->getTotalNrRegs();
    varsRequiredHere.reserve(total);
    for (size_t i = 0; i < total; ++i) {
      varsRequiredHere.emplace_back(std::nullopt);
    }

    // As we may have output variables these are added initially
    {
      auto final = nodes.back();
      auto setHere = final.getVariablesSetHere();
      for (auto const& v : setHere) {
        auto info = plan->varInfo.find(v->id);
        ASSERT_NE(info, plan->varInfo.end())
            << "variable " << v->name << " of node " << final.getTypeString()
            << " not planned ";
        auto regId = info->second.registerId;
        varsRequiredHere.at(regId) = v;
      }
    }

    auto checkProducedVariables = [&](ExecutionNodeMock const& n) {
      for (auto const& v : n.getVariablesSetHere()) {
        auto info = plan->varInfo.find(v->id);
        ASSERT_NE(info, plan->varInfo.end()) << "variable " << v->name << " of node "
                                             << n.getTypeString() << " not planned ";
        auto regId = info->second.registerId;
        ASSERT_LT(regId, total) << "Planned Register out of bounds";
        ASSERT_TRUE(varsRequiredHere.at(regId).has_value())
            << "Writing variable " << v->name << " to register " << regId
            << " where it is not expected";
        auto expected = varsRequiredHere.at(regId).value();
        ASSERT_EQ(v, expected)
            << "register " << regId << " used twice, content of "
            << expected->name << " expected while writing " << v->name;
        // Variable is produced here, cannot be required before
        varsRequiredHere.at(regId) = std::nullopt;
      }
    };

    auto insertRequiredVariables = [&](ExecutionNodeMock const& n) {
      VarSet requestedHere{};
      n.getVariablesUsedHere(requestedHere);
      for (auto const& v : requestedHere) {
        auto info = plan->varInfo.find(v->id);
        ASSERT_NE(info, plan->varInfo.end())
            << "variable " << v->name << " required by node "
            << n.getTypeString() << " not planned ";
        auto regId = info->second.registerId;
        ASSERT_LT(regId, total) << "Planned Register out of bounds";
        auto target = varsRequiredHere.at(regId);
        if (!target.has_value()) {
          // This register is free, claim it!
          varsRequiredHere.at(regId) = v;
        } else {
          ASSERT_EQ(target.value(), v)
              << "register " << regId << " used twice, content of "
              << target.value()->name << " still expected while also expecting "
              << v->name;
        }
      }
    };

    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
      checkProducedVariables(*it);
      insertRequiredVariables(*it);
    }
  }

  auto getVarUsage(std::vector<ExecutionNodeMock>& nodes) {
    std::unordered_map<VariableId, ExecutionNodeMock*> varSetBy;
    ::VarUsageFinder finder(&varSetBy);
    applyWalkerToNodes(nodes, finder);
    return std::move(finder.usedLaterStack);
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
      ExecutionNodeMock{ExecutionNode::SINGLETON, false, {}, {&vars[0]}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  EXPECT_EQ(plan->getTotalNrRegs(), 1);
  assertVariableInRegister(plan, vars[0], 0);
  assertPlanKeepsAllVariables(plan, myList);
}

TEST_F(RegisterPlanTest, planRegisters_should_append_variables_if_all_are_needed) {
  auto vars = generateVars(2);
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, false, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, false, {}, {&vars[0]}},
      ExecutionNodeMock{ExecutionNode::INDEX, false, {&vars[0]}, {&vars[1]}},
      ExecutionNodeMock{ExecutionNode::RETURN, false, {&vars[0], &vars[1]}, {}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  EXPECT_EQ(plan->getTotalNrRegs(), 2);
  assertVariableInRegister(plan, vars[0], 0);
  assertVariableInRegister(plan, vars[1], 1);
  assertPlanKeepsAllVariables(plan, myList);
}

TEST_F(RegisterPlanTest, planRegisters_should_reuse_register_if_possible) {
  auto vars = generateVars(2);
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, false, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, false, {}, {&vars[0]}},
      ExecutionNodeMock{ExecutionNode::INDEX, false, {&vars[0]}, {&vars[1]}},
      ExecutionNodeMock{ExecutionNode::RETURN, false, {&vars[1]}, {}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  EXPECT_EQ(plan->getTotalNrRegs(), 1);
  assertVariableInRegister(plan, vars[0], 0);
  assertVariableInRegister(plan, vars[1], 0);
  assertPlanKeepsAllVariables(plan, myList);
}

TEST_F(RegisterPlanTest, planRegisters_should_not_reuse_register_if_block_is_passthrough) {
  auto vars = generateVars(2);
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, false, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, false, {}, {&vars[0]}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, true, {&vars[0]}, {&vars[1]}},
      ExecutionNodeMock{ExecutionNode::RETURN, false, {&vars[1]}, {}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  EXPECT_EQ(plan->getTotalNrRegs(), 2);
  assertVariableInRegister(plan, vars[0], 0);
  assertVariableInRegister(plan, vars[1], 1);
  assertPlanKeepsAllVariables(plan, myList);
}

TEST_F(RegisterPlanTest, planRegisters_should_reuse_register_after_passthrough) {
  auto vars = generateVars(5);
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, false, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, false, {}, {&vars[0]}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, true, {&vars[0]}, {&vars[1]}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, true, {&vars[1]}, {&vars[2]}},
      ExecutionNodeMock{ExecutionNode::INDEX, false, {&vars[2]}, {&vars[3]}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, true, {&vars[3]}, {&vars[4]}},
      ExecutionNodeMock{ExecutionNode::RETURN, false, {&vars[4]}, {}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  EXPECT_EQ(plan->getTotalNrRegs(), 2);
  assertVariableInRegister(plan, vars[0], 0);
  assertVariableInRegister(plan, vars[1], 1);
  assertVariableInRegister(plan, vars[2], 0);
  assertVariableInRegister(plan, vars[3], 0);
  assertVariableInRegister(plan, vars[4], 1);
  assertPlanKeepsAllVariables(plan, myList);
}

TEST_F(RegisterPlanTest, variable_usage) {
  auto vars = generateVars(5);
  std::vector<ExecutionNodeMock> nodes{
      ExecutionNodeMock{ExecutionNode::SINGLETON, false, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, false, {}, {&vars[0]}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, true, {&vars[0]}, {&vars[1]}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, true, {&vars[1]}, {&vars[2]}},
      ExecutionNodeMock{ExecutionNode::INDEX, false, {&vars[2]}, {&vars[3]}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, true, {&vars[3]}, {&vars[4]}},
      ExecutionNodeMock{ExecutionNode::RETURN, false, {&vars[4]}, {}}};
  auto const usageStack = getVarUsage(nodes);
  EXPECT_EQ(usageStack.size(), 1);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
