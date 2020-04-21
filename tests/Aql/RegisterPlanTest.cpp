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
#include "Basics/StringUtils.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

struct ExecutionNodeMock {
  ExecutionNodeMock(ExecutionNode::NodeType type, bool isIncreaseDepth,
                    std::vector<Variable const*> input, std::vector<Variable const*> output)
      : _type(type), _isIncreaseDepth(isIncreaseDepth), _input(), _output() {
    for (auto const& v : input) {
      _input.emplace(v);
    }
    for (auto const& v : output) {
      _output.emplace(v);
    }
  }

  auto id() -> ExecutionNodeId { return ExecutionNodeId{0}; }

  auto isIncreaseDepth() -> bool { return _isIncreaseDepth; }

  auto getType() -> ExecutionNode::NodeType { return _type; }

  auto getVarsUsedLater() -> ::arangodb::containers::HashSet<Variable const*> const& {
    return {};
  }

  auto getVariablesUsedHere(::arangodb::containers::HashSet<Variable const*>& res) const
      -> void {
    for (auto const v : _input) {
      res.emplace(v);
    }
  }

  auto getOutputVariables() const -> VariableIdSet {
    VariableIdSet res;
    for (auto const& v : _output) {
      res.insert(v->id);
    }
    return res;
  }

  auto setRegsToClear(std::unordered_set<RegisterId>&& toClear) -> void {}

  auto getTypeString() -> std::string const& {
    return ExecutionNode::getTypeString(_type);
  }

  // Will be modified by walker worker
  unsigned int _depth = 0;
  std::shared_ptr<RegisterPlanT<ExecutionNodeMock>> _registerPlan = nullptr;

 private:
  ExecutionNode::NodeType _type;
  bool _isIncreaseDepth;
  ::arangodb::containers::HashSet<Variable const*> _input;
  ::arangodb::containers::HashSet<Variable const*> _output;
};

class RegisterPlanTest : public ::testing::Test {
 protected:
  RegisterPlanTest() {}

  auto walk(std::vector<ExecutionNodeMock>& nodes)
      -> std::shared_ptr<RegisterPlanT<ExecutionNodeMock>> {
    auto registerPlan = std::make_shared<RegisterPlanT<ExecutionNodeMock>>();
    RegisterPlanWalkerT<ExecutionNodeMock> worker(registerPlan);

    for (auto& n : nodes) {
      worker.before(&n);
    }
    for (auto it = nodes.rbegin(); it < nodes.rend(); ++it) {
      worker.after(&(*it));
    }

    return registerPlan;
  }

  auto generateVars(size_t amount) -> std::vector<Variable> {
    std::vector<Variable> res;
    for (size_t i = 0; i < amount; ++i) {
      res.emplace_back("var" + arangodb::basics::StringUtils::itoa(i), i);
    }
    return res;
  }
};

TEST_F(RegisterPlanTest, planRegisters_should_add_registerPlan) {
  auto vars = generateVars(1);
  std::vector<ExecutionNodeMock> myList{
      ExecutionNodeMock{ExecutionNode::SINGLETON, true, {}, {&vars[0]}}};
  auto plan = walk(myList);
  ASSERT_NE(plan, nullptr);
  LOG_DEVEL << "Plan: " << *plan;
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb