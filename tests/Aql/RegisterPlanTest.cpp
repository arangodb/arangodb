////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Mocks/Servers.h"

#include "Aql/ExecutionNode.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.cpp"
#include "Aql/VarUsageFinder.cpp"
#include "Aql/VarUsageFinder.h"
#include "Aql/types.h"
#include "Basics/StringUtils.h"

#include <optional>
#include <unordered_set>
#include <vector>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

struct PlanMiniMock {
  PlanMiniMock(ExecutionNode::NodeType expectedType)
      : _expectedType(expectedType) {}

  auto increaseCounter(ExecutionNode::NodeType type) {
    // This is no longer true for subqueries because reasons, i.e. subqueries
    // are planned multiple times
    // TODO: refactor subquery planning?
    // EXPECT_FALSE(_called) << "Only count every node once per run";
    EXPECT_EQ(_expectedType, type) << "Count the correct type";
  }

  ExecutionNode::NodeType _expectedType;
};

struct ExecutionNodeMock {
  ExecutionNodeMock(ExecutionNode::NodeType type, std::vector<Variable const*> input,
                    std::vector<Variable const*> output,
                    std::vector<ExecutionNodeMock>* subquery = nullptr)
      : _type(type), _input(), _output(), _plan(type), _subquery(subquery) {
    for (auto const& v : input) {
      _input.emplace(v);
    }
    for (auto const& v : output) {
      _output.emplace(v);
    }
  }

  auto plan() -> PlanMiniMock* { return &_plan; }

  auto id() -> ExecutionNodeId { return ExecutionNodeId{0}; }

  auto isIncreaseDepth() -> bool {
    return ExecutionNode::isIncreaseDepth(getType());
  }

  auto alwaysCopiesRows() -> bool {
    return ExecutionNode::alwaysCopiesRows(getType());
  }

  auto getType() -> ExecutionNode::NodeType { return _type; }

  auto getVarsUsedLater() -> VarSet const& { return _usedLaterStack.back(); }

  auto getVariablesUsedHere(VarSet& res) const -> void {
    for (auto const v : _input) {
      res.emplace(v);
    }
  }

  auto setVarsUsedLater(VarSetStack& s) -> void { _usedLaterStack = s; }

  auto invalidateVarUsage() -> void {
    _usedLaterStack.clear();
    _varsValidStack.clear();
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

  auto getVariablesSetHere() const -> std::vector<Variable const*> {
    std::vector<Variable const*> result;
    std::copy(_output.begin(), _output.end(), std::back_inserter(result));
    return result;
  }

  auto setRegsToClear(RegIdSet toClear) -> void {
    _regsToClear = std::move(toClear);
  }

  auto getTypeString() const -> std::string const& {
    return ExecutionNode::getTypeString(_type);
  }

  auto setVarsValid(VarSetStack varsValidStack) -> void {
    _varsValidStack = std::move(varsValidStack);
  }
  auto getVarsValid() const -> VarSet const&;
  auto getVarsValidStack() const -> VarSetStack const&;

  void setRegsToKeep(RegIdSetStack regsToKeep) {
    _regsToKeep = std::move(regsToKeep);
  }

  auto getRegsToKeep() -> RegIdSetStack const& { return _regsToKeep; }

  auto walk(WalkerWorker<ExecutionNodeMock, WalkerUniqueness::NonUnique>& worker) -> bool {
    if (worker.before(this)) {
      return true;
    }

    if (_dependency != nullptr) {
      _dependency->walk(worker);
    }

    if (getType() == ExecutionNode::SUBQUERY) {
      auto&& subquery = getSubquery();
      TRI_ASSERT(!subquery.empty());
      if (worker.enterSubquery(this, &subquery.back())) {
        bool shouldAbort = subquery.back().walk(worker);
        worker.leaveSubquery(this, &subquery.back());
        if (shouldAbort) {
          return true;
        }
      }
    }

    worker.after(this);

    return false;
  }

  auto getVarsUsedLaterStack() const noexcept -> VarSetStack const& {
    return _usedLaterStack;
  }

  std::vector<ExecutionNodeMock>& getSubquery() { return *_subquery; }

  void setDependency(ExecutionNodeMock* ptr) { _dependency = ptr; }

  // Will be modified by walker worker
  unsigned int _depth = 0;
  std::shared_ptr<RegisterPlanT<ExecutionNodeMock>> _registerPlan = nullptr;

 private:
  ExecutionNode::NodeType _type;
  VarSet _input;
  VarSet _output;
  VarSetStack _usedLaterStack;
  VarSetStack _varsValidStack;
  RegIdSetStack _regsToKeep;
  RegIdSet _regsToClear;
  bool _varUsageValid{false};
  PlanMiniMock _plan;
  std::vector<ExecutionNodeMock>* _subquery;
  ExecutionNodeMock* _dependency = nullptr;
};

auto ExecutionNodeMock::getVarsValid() const -> VarSet const& {
  return _varsValidStack.back();
}
auto ExecutionNodeMock::getVarsValidStack() const -> VarSetStack const& {
  return _varsValidStack;
}

class RegisterPlanTest : public ::testing::Test {
 protected:
  RegisterPlanTest() {}

  auto walk(std::vector<ExecutionNodeMock>& nodes)
      -> std::shared_ptr<RegisterPlanT<ExecutionNodeMock>> {
    // Compute the variable usage for nodes.
    std::unordered_map<VariableId, ExecutionNodeMock*> varSetBy;
    ::VarUsageFinderT finder(&varSetBy);
    applyWalkerToNodes(nodes, finder);

    auto registerPlan = std::make_shared<RegisterPlanT<ExecutionNodeMock>>();
    RegisterPlanWalkerT<ExecutionNodeMock> worker(registerPlan);
    applyWalkerToNodes(nodes, worker);

    return registerPlan;
  }

  template <size_t amount>
  auto generateVars()
      -> std::pair<std::vector<Variable>, std::array<Variable*, amount>> {
    std::vector<Variable> vars;
    vars.reserve(amount);
    std::array<Variable*, amount> ptrs{};
    for (size_t i = 0; i < amount; ++i) {
      vars.emplace_back("var" + arangodb::basics::StringUtils::itoa(i),
                        static_cast<VariableId>(i), false);
      ptrs[i] = &vars[i];
    }

    return {std::move(vars), ptrs};
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

    auto total = *std::max_element(plan->nrRegs.cbegin(), plan->nrRegs.cend());
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
        varsRequiredHere.at(regId.value()) = v;
      }
    }

    auto checkProducedVariables = [&](ExecutionNodeMock const& n) {
      for (auto const& v : n.getVariablesSetHere()) {
        auto info = plan->varInfo.find(v->id);
        ASSERT_NE(info, plan->varInfo.end()) << "variable " << v->name << " of node "
                                             << n.getTypeString() << " not planned ";
        auto regId = info->second.registerId;
        ASSERT_LT(regId, total) << "Planned Register out of bounds";
        ASSERT_TRUE(varsRequiredHere.at(regId.value()).has_value())
            << "Writing variable " << v->name << " to register " << regId.value()
            << " where it is not expected";
        auto expected = varsRequiredHere.at(regId.value()).value();
        ASSERT_EQ(v, expected)
            << "register " << regId.value() << " used twice, content of "
            << expected->name << " expected while writing " << v->name;
        // Variable is produced here, cannot be required before
        varsRequiredHere.at(regId.value()) = std::nullopt;
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
        auto target = varsRequiredHere.at(regId.value());
        if (!target.has_value()) {
          // This register is free, claim it!
          varsRequiredHere.at(regId.value()) = v;
        } else {
          ASSERT_EQ(target.value(), v)
              << "register " << regId.value() << " used twice, content of "
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
    ::VarUsageFinderT finder(&varSetBy);
    applyWalkerToNodes(nodes, finder);
  }

 private:
  void fixDependencies(std::vector<ExecutionNodeMock>& nodes) {
    for (auto it = nodes.begin(); it < nodes.end(); ++it) {
      if (it->getType() == ExecutionNode::SUBQUERY) {
        fixDependencies(it->getSubquery());
      }

      if (std::next(it) != nodes.end()) {
        std::next(it)->setDependency(&(*it));
      }
    }
  }

  template <class Walker>
  auto applyWalkerToNodes(std::vector<ExecutionNodeMock>& nodes, Walker& worker) -> void {
    // fix dependencies
    fixDependencies(nodes);
    nodes.back().walk(worker);
  }
};

TEST_F(RegisterPlanTest, walker_should_plan_registers) {
  auto&& [vars, ptrs] = generateVars<1>();
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {&vars[0]}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  EXPECT_EQ(plan->nrRegs.back(), 1);
  assertVariableInRegister(plan, vars[0], 0);
  assertPlanKeepsAllVariables(plan, myList);
}

TEST_F(RegisterPlanTest, planRegisters_should_append_variables_if_all_are_needed) {
  auto&& [vars, ptrs] = generateVars<2>();
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {&vars[0]}},
      ExecutionNodeMock{ExecutionNode::INDEX, {&vars[0]}, {&vars[1]}},
      ExecutionNodeMock{ExecutionNode::RETURN, {&vars[0], &vars[1]}, {}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  EXPECT_EQ(plan->nrRegs.back(), 2);
  assertVariableInRegister(plan, vars[0], 0);
  assertVariableInRegister(plan, vars[1], 1);
  assertPlanKeepsAllVariables(plan, myList);
}

TEST_F(RegisterPlanTest, planRegisters_should_reuse_register_if_possible) {
  auto&& [vars, ptrs] = generateVars<2>();
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {&vars[0]}},
      ExecutionNodeMock{ExecutionNode::INDEX, {&vars[0]}, {&vars[1]}},
      ExecutionNodeMock{ExecutionNode::RETURN, {&vars[1]}, {}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  EXPECT_EQ(plan->nrRegs.back(), 1);
  assertVariableInRegister(plan, vars[0], 0);
  assertVariableInRegister(plan, vars[1], 0);
  assertPlanKeepsAllVariables(plan, myList);
}

TEST_F(RegisterPlanTest, planRegisters_should_not_reuse_register_if_block_is_passthrough) {
  auto&& [vars, ptrs] = generateVars<2>();
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {&vars[0]}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {&vars[0]}, {&vars[1]}},
      ExecutionNodeMock{ExecutionNode::RETURN, {&vars[1]}, {}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  EXPECT_EQ(plan->nrRegs.back(), 2);
  assertVariableInRegister(plan, vars[0], 0);
  assertVariableInRegister(plan, vars[1], 1);
  assertPlanKeepsAllVariables(plan, myList);
}

TEST_F(RegisterPlanTest, planRegisters_should_reuse_register_after_passthrough) {
  auto&& [vars, ptrs] = generateVars<5>();
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {&vars[0]}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {&vars[0]}, {&vars[1]}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {&vars[1]}, {&vars[2]}},
      ExecutionNodeMock{ExecutionNode::INDEX, {&vars[2]}, {&vars[3]}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {&vars[3]}, {&vars[4]}},
      ExecutionNodeMock{ExecutionNode::RETURN, {&vars[4]}, {}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  EXPECT_EQ(plan->nrRegs.back(), 2);
  assertVariableInRegister(plan, vars[0], 0);
  assertVariableInRegister(plan, vars[1], 1);
  assertVariableInRegister(plan, vars[2], 0);
  assertVariableInRegister(plan, vars[3], 0);
  assertVariableInRegister(plan, vars[4], 1);
  assertPlanKeepsAllVariables(plan, myList);
}

TEST_F(RegisterPlanTest, variable_usage) {
  auto&& [vars, ptrs] = generateVars<5>();
  auto [nicole, doris, shawn, ronald, maria] = ptrs;
  std::vector<ExecutionNodeMock> nodes{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {nicole}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {nicole}, {doris}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {doris}, {shawn}},
      ExecutionNodeMock{ExecutionNode::INDEX, {shawn}, {ronald}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {ronald}, {maria}},
      ExecutionNodeMock{ExecutionNode::RETURN, {maria}, {}}};
  getVarUsage(nodes);

  {  // Check varsUsedLater

    EXPECT_TRUE((VarSetStack{VarSet{nicole, doris, shawn, ronald, maria}}) == nodes[0].getVarsUsedLaterStack());

    // SINGLETON
    EXPECT_EQ((VarSetStack{VarSet{nicole, doris, shawn, ronald, maria}}),
              nodes[0].getVarsUsedLaterStack());
    // ENUMERATE_COLLECTION
    EXPECT_EQ((VarSetStack{VarSet{nicole, doris, shawn, ronald, maria}}),
              nodes[1].getVarsUsedLaterStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{doris, shawn, ronald, maria}}),
              nodes[2].getVarsUsedLaterStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{shawn, ronald, maria}}), nodes[3].getVarsUsedLaterStack());
    // INDEX
    EXPECT_EQ((VarSetStack{VarSet{ronald, maria}}), nodes[4].getVarsUsedLaterStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{maria}}), nodes[5].getVarsUsedLaterStack());
    // RETURN
    EXPECT_EQ((VarSetStack{VarSet{}}), nodes[6].getVarsUsedLaterStack());
  }

  {  // Check varsValid

    // SINGLETON
    EXPECT_EQ((VarSetStack{VarSet{}}), nodes[0].getVarsValidStack());
    // ENUMERATE_COLLECTION
    EXPECT_EQ((VarSetStack{VarSet{nicole}}), nodes[1].getVarsValidStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{nicole, doris}}), nodes[2].getVarsValidStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{nicole, doris, shawn}}), nodes[3].getVarsValidStack());
    // INDEX
    EXPECT_EQ((VarSetStack{VarSet{nicole, doris, shawn, ronald}}), nodes[4].getVarsValidStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{nicole, doris, shawn, ronald, maria}}),
              nodes[5].getVarsValidStack());
    // RETURN
    EXPECT_EQ((VarSetStack{VarSet{nicole, doris, shawn, ronald, maria}}),
              nodes[6].getVarsValidStack());
  }
}

TEST_F(RegisterPlanTest, variable_usage_with_spliced_subquery) {
  auto&& [vars, ptrs] = generateVars<5>();
  auto [mark, debra, tina, mary, jesse] = ptrs;
  std::vector<ExecutionNodeMock> nodes{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {mark}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {mark}, {debra}},
      ExecutionNodeMock{ExecutionNode::SUBQUERY_START, {}, {}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {debra}, {tina}},
      ExecutionNodeMock{ExecutionNode::SUBQUERY_END, {tina}, {mary}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {mark, mary}, {jesse}},
      ExecutionNodeMock{ExecutionNode::RETURN, {jesse}, {}}};
  getVarUsage(nodes);

  {  // Check varsUsedLater

    // SINGLETON
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark, tina, debra}}),
              nodes[0].getVarsUsedLaterStack());
    // ENUMERATE_COLLECTION
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark, tina, debra}}),
              nodes[1].getVarsUsedLaterStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark, tina, debra}}),
              nodes[2].getVarsUsedLaterStack());
    // SUBQUERY_START
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark}, VarSet{tina, debra}}),
              nodes[3].getVarsUsedLaterStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark}, VarSet{tina}}), nodes[4].getVarsUsedLaterStack());
    // SUBQUERY_END
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark}}), nodes[5].getVarsUsedLaterStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{jesse}}), nodes[6].getVarsUsedLaterStack());
    // RETURN
    EXPECT_EQ((VarSetStack{VarSet{}}), nodes[7].getVarsUsedLaterStack());
  }

  {  // Check varsValid
    // SINGLETON
    EXPECT_EQ((VarSetStack{VarSet{}}), nodes[0].getVarsValidStack());
    // ENUMERATE_COLLECTION
    EXPECT_EQ((VarSetStack{VarSet{mark}}), nodes[1].getVarsValidStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{mark, debra}}), nodes[2].getVarsValidStack());
    // SUBQUERY_START
    EXPECT_EQ((VarSetStack{VarSet{mark, debra}, VarSet{mark, debra}}), nodes[3].getVarsValidStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{mark, debra}, VarSet{mark, debra, tina}}),
              nodes[4].getVarsValidStack());
    // SUBQUERY_END
    EXPECT_EQ((VarSetStack{VarSet{mark, debra, mary}}), nodes[5].getVarsValidStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{mark, debra, mary, jesse}}), nodes[6].getVarsValidStack());
    // RETURN
    EXPECT_EQ((VarSetStack{VarSet{mark, debra, mary, jesse}}), nodes[7].getVarsValidStack());
  }
}

TEST_F(RegisterPlanTest, variable_usage_with_spliced_subquery2) {
  auto&& [vars, ptrs] = generateVars<5>();
  auto [mark, debra, tina, mary, jesse] = ptrs;
  std::vector<ExecutionNodeMock> nodes{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {mark}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {mark}, {debra}},
      ExecutionNodeMock{ExecutionNode::SUBQUERY_START, {}, {}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {mark}, {tina}},
      ExecutionNodeMock{ExecutionNode::SUBQUERY_END, {tina}, {mary}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {mary, debra}, {jesse}},
      ExecutionNodeMock{ExecutionNode::RETURN, {jesse}, {}}};
  getVarUsage(nodes);

  {  // Check varsUsedLater

    // SINGLETON
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark, tina, debra}}),
              nodes[0].getVarsUsedLaterStack());
    // ENUMERATE_COLLECTION
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark, tina, debra}}),
              nodes[1].getVarsUsedLaterStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark, tina, debra}}),
              nodes[2].getVarsUsedLaterStack());
    // SUBQUERY_START
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, debra}, VarSet{mark, tina}}),
              nodes[3].getVarsUsedLaterStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, debra}, VarSet{tina}}), nodes[4].getVarsUsedLaterStack());
    // SUBQUERY_END
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, debra}}), nodes[5].getVarsUsedLaterStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{jesse}}), nodes[6].getVarsUsedLaterStack());
    // RETURN
    EXPECT_EQ((VarSetStack{VarSet{}}), nodes[7].getVarsUsedLaterStack());
  }

  {  // Check varsValid
    // SINGLETON
    EXPECT_EQ((VarSetStack{VarSet{}}), nodes[0].getVarsValidStack());
    // ENUMERATE_COLLECTION
    EXPECT_EQ((VarSetStack{VarSet{mark}}), nodes[1].getVarsValidStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{mark, debra}}), nodes[2].getVarsValidStack());
    // SUBQUERY_START
    EXPECT_EQ((VarSetStack{VarSet{mark, debra}, VarSet{mark, debra}}), nodes[3].getVarsValidStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{mark, debra}, VarSet{mark, debra, tina}}),
              nodes[4].getVarsValidStack());
    // SUBQUERY_END
    EXPECT_EQ((VarSetStack{VarSet{mark, debra, mary}}), nodes[5].getVarsValidStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{mark, debra, mary, jesse}}), nodes[6].getVarsValidStack());
    // RETURN
    EXPECT_EQ((VarSetStack{VarSet{mark, debra, mary, jesse}}), nodes[7].getVarsValidStack());
  }
}

TEST_F(RegisterPlanTest, variable_usage_with_subquery) {
  auto&& [vars, ptrs] = generateVars<6>();
  auto [mark, debra, mary, jesse, paul, tobias] = ptrs;

  std::vector<ExecutionNodeMock> subquery{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {tobias}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {debra, tobias}, {paul}},
      ExecutionNodeMock{ExecutionNode::RETURN, {paul}, {}}};

  std::vector<ExecutionNodeMock> nodes{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {mark}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {mark}, {debra}},
      ExecutionNodeMock{ExecutionNode::SUBQUERY, {debra}, {mary}, &subquery},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {mark, mary}, {jesse}},
      ExecutionNodeMock{ExecutionNode::RETURN, {jesse}, {}}};
  getVarUsage(nodes);

  {
    // Check varsUsedLater

    // SINGLETON
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark, debra}}), nodes[0].getVarsUsedLaterStack());
    // ENUMERATE_COLLECTION
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark, debra}}), nodes[1].getVarsUsedLaterStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark, debra}}), nodes[2].getVarsUsedLaterStack());
    // SUBQUERY
    EXPECT_EQ((VarSetStack{VarSet{jesse, mary, mark}}), nodes[3].getVarsUsedLaterStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{jesse}}), nodes[4].getVarsUsedLaterStack());
    // RETURN
    EXPECT_EQ((VarSetStack{VarSet{}}), nodes[5].getVarsUsedLaterStack());

    // SINGLETON
    EXPECT_EQ((VarSetStack{VarSet{tobias, debra, paul}}), subquery[0].getVarsUsedLaterStack());
    // ENUMERATE_COLLECTION
    EXPECT_EQ((VarSetStack{VarSet{tobias, debra, paul}}), subquery[1].getVarsUsedLaterStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{paul}}), subquery[2].getVarsUsedLaterStack());
    // RETURN
    EXPECT_EQ((VarSetStack{VarSet{}}), subquery[3].getVarsUsedLaterStack());
  }

  {  // Check varsValid

    // SINGLETON
    EXPECT_EQ((VarSetStack{VarSet{}}), nodes[0].getVarsValidStack());
    // ENUMERATE_COLLECTION
    EXPECT_EQ((VarSetStack{VarSet{mark}}), nodes[1].getVarsValidStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{mark, debra}}), nodes[2].getVarsValidStack());
    // SUBQUERY
    EXPECT_EQ((VarSetStack{VarSet{mark, debra, mary}}), nodes[3].getVarsValidStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{mark, debra, mary, jesse}}), nodes[4].getVarsValidStack());
    // RETURN
    EXPECT_EQ((VarSetStack{VarSet{mark, debra, mary, jesse}}), nodes[5].getVarsValidStack());

    // SINGLETON
    EXPECT_EQ((VarSetStack{VarSet{mark, debra}}), subquery[0].getVarsValidStack());
    // ENUMERATE_COLLECTION
    EXPECT_EQ((VarSetStack{VarSet{mark, debra, tobias}}), subquery[1].getVarsValidStack());
    // CALCULATION
    EXPECT_EQ((VarSetStack{VarSet{mark, debra, tobias, paul}}), subquery[2].getVarsValidStack());
    // RETURN
    EXPECT_EQ((VarSetStack{VarSet{mark, debra, tobias, paul}}), subquery[3].getVarsValidStack());
  }
}

TEST_F(RegisterPlanTest, variable_usage_with_subquery_using_many_registers) {
  auto&& [vars, ptrs] = generateVars<6>();
  auto [mark, debra, mary, jesse, paul, tobias] = ptrs;
  std::vector<ExecutionNodeMock> nodes{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {mark}},
      ExecutionNodeMock{ExecutionNode::SUBQUERY_START, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {tobias}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {mark, tobias}, {paul}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {mark, paul}, {debra}},
      ExecutionNodeMock{ExecutionNode::SUBQUERY_END, {debra}, {mary}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {mark, mary}, {jesse}},
      ExecutionNodeMock{ExecutionNode::RETURN, {jesse}, {}}};
  getVarUsage(nodes);
  auto plan = walk(nodes);
  {
    SCOPED_TRACE("while checking mark");
    assertVariableInRegister(plan, *mark, 0);
  }
  {
    SCOPED_TRACE("while checking tobias");
    assertVariableInRegister(plan, *tobias, 1);
  }
  {
    SCOPED_TRACE("while checking paul");
    assertVariableInRegister(plan, *paul, 2);
  }
  {
    SCOPED_TRACE("while checking debra");
    assertVariableInRegister(plan, *debra, 1);
  }
  {
    SCOPED_TRACE("while checking mary");
    assertVariableInRegister(plan, *mary, 1);
  }
  {
    SCOPED_TRACE("while checking jesse");
    assertVariableInRegister(plan, *jesse, 2);
  }
}

// The current register planning isn't optimal enough to satisfy this test.
TEST_F(RegisterPlanTest, DISABLED_multiple_spliced_subqueries) {
  auto&& [vars, ptrs] = generateVars<10>();
  auto [maria, andrew, douglas, christopher, patricia, betty, doris, christine,
        wanda, ronald] = ptrs;
  std::vector<ExecutionNodeMock> nodes{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {maria}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {maria}, {andrew}},

      ExecutionNodeMock{ExecutionNode::SUBQUERY_START, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {douglas}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {douglas, andrew}, {christopher}},
      ExecutionNodeMock{ExecutionNode::SUBQUERY_END, {christopher}, {patricia}},

      ExecutionNodeMock{ExecutionNode::CALCULATION, {maria, patricia}, {betty}},

      ExecutionNodeMock{ExecutionNode::SUBQUERY_START, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {doris}},
      ExecutionNodeMock{ExecutionNode::CALCULATION, {doris}, {christine}},
      ExecutionNodeMock{ExecutionNode::SUBQUERY_END, {christine}, {wanda}},

      ExecutionNodeMock{ExecutionNode::CALCULATION, {betty, wanda}, {ronald}},

      ExecutionNodeMock{ExecutionNode::RETURN, {ronald}, {}}};
  getVarUsage(nodes);
  auto plan = walk(nodes);
  {
    SCOPED_TRACE("while checking maria");
    assertVariableInRegister(plan, *maria, 0);
  }
  {
    SCOPED_TRACE("while checking andrew");
    assertVariableInRegister(plan, *andrew, 1);
  }
  {
    SCOPED_TRACE("while checking douglas");
    assertVariableInRegister(plan, *douglas, 0);
  }
  {
    SCOPED_TRACE("while checking christopher");
    assertVariableInRegister(plan, *christopher, 2);
  }
  {
    SCOPED_TRACE("while checking patricia");
    assertVariableInRegister(plan, *patricia, 1);
  }
  {
    SCOPED_TRACE("while checking betty");
    assertVariableInRegister(plan, *betty, 2);
  }
  {
    SCOPED_TRACE("while checking doris");
    assertVariableInRegister(plan, *doris, 0);
  }
  {
    SCOPED_TRACE("while checking christine");
    assertVariableInRegister(plan, *christine, 1);
  }
  {
    SCOPED_TRACE("while checking wanda");
    assertVariableInRegister(plan, *wanda, 0);
  }
  {
    SCOPED_TRACE("while checking ronald");
    assertVariableInRegister(plan, *ronald, 1);
  }
}

// The current register planning cannot reuse registers that are never used.
// Also see the comment on "brenda".
TEST_F(RegisterPlanTest, DISABLED_reuse_unused_register) {
  auto&& [vars, ptrs] = generateVars<2>();
  auto [howard, brenda] = ptrs;
  std::vector<ExecutionNodeMock> nodes{
      ExecutionNodeMock{ExecutionNode::SINGLETON, {}, {}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {howard}},
      ExecutionNodeMock{ExecutionNode::ENUMERATE_COLLECTION, {}, {brenda}},
      ExecutionNodeMock{ExecutionNode::RETURN, {brenda}, {}}};
  getVarUsage(nodes);
  auto plan = walk(nodes);
  {
    SCOPED_TRACE("while checking howard");
    assertVariableInRegister(plan, *howard, 0);
  }
  {
    // As howard is never used, we could reuse register 0 immediately.
    // However, the current register planning does not do that.
    SCOPED_TRACE("while checking steve");
    assertVariableInRegister(plan, *brenda, 0);
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
