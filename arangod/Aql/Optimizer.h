////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_OPTIMIZER_H
#define ARANGOD_AQL_OPTIMIZER_H 1

#include "Basics/Common.h"
#include "Aql/ExecutionPlan.h"
#include "Basics/RollingVector.h"

#include <velocypack/Builder.h>

namespace arangodb {
namespace aql {
struct OptimizerRule;
class OptimizerRulesFeature;

class Optimizer {
 public:
  /// @brief optimizer statistics
  struct Stats {
    int64_t rulesExecuted = 0;
    int64_t rulesSkipped = 0;
    int64_t plansCreated = 1;  // 1 for the initial plan

    void toVelocyPack(velocypack::Builder& b) const {
      velocypack::ObjectBuilder guard(&b, true);
      b.add("rulesExecuted", velocypack::Value(rulesExecuted));
      b.add("rulesSkipped", velocypack::Value(rulesSkipped));
      b.add("plansCreated", velocypack::Value(plansCreated));
    }
  };

 public:

  /// @brief the following struct keeps a list (deque) of ExecutionPlan*
  /// and has some automatic convenience functions.
  struct PlanList {
    RollingVector<ExecutionPlan*> list;
    RollingVector<int> levelDone;

    PlanList() {
      list.reserve(8);
      levelDone.reserve(8);
    }

    /// @brief constructor with a plan
    PlanList(ExecutionPlan* p, int level) { push_back(p, level); }

    /// @brief destructor, deleting contents
    ~PlanList() {
      for (auto& p : list) {
        delete p;
      }
    }

    /// @brief check if a plan is contained in the list
    bool isContained(ExecutionPlan* plan) const {
      for (auto const& p : list) {
        if (p == plan) {
          return true;
        }
      }
      return false;
    }

    /// @brief get number of plans contained
    size_t size() const { return list.size(); }

    /// @brief check if empty
    bool empty() const { return list.empty(); }

    /// @brief pop the first one
    ExecutionPlan* pop_front(int& levelDoneOut) {
      auto p = list.front();
      levelDoneOut = levelDone.front();
      list.pop_front();
      levelDone.pop_front();
      return p;
    }

    /// @brief push_back
    void push_back(ExecutionPlan* p, int level) {
      list.push_back(p);
      try {
        levelDone.push_back(level);
      } catch (...) {
        list.pop_back();
        throw;
      }
    }

    /// @brief steals all the plans in b and clears b at the same time
    void steal(PlanList& b) {
      list = std::move(b.list);
      levelDone = std::move(b.levelDone);
    }

    /// @brief appends all the plans to the target and clears *this at the same
    /// time
    void appendTo(PlanList& target) {
      while (list.size() > 0) {
        auto p = list.front();
        int level = levelDone.front();
        list.pop_front();
        levelDone.pop_front();
        try {
          target.push_back(p, level);
        } catch (...) {
          delete p;
          throw;
        }
      }
    }

    /// @brief clear, deletes all plans contained
    void clear() {
      for (auto& p : list) {
        delete p;
      }
      list.clear();
      levelDone.clear();
    }
  };

 public:
  /// @brief constructor, this will initialize the rules database
  /// the .cpp file includes Aql/OptimizerRules.h
  /// and add all methods there to the rules database
  explicit Optimizer(size_t);

  ~Optimizer() {}

 public:
  /// @brief do the optimization, this does the optimization, the resulting
  /// plans are all estimated, sorted by that estimate and can then be got
  /// by getPlans, until the next initialize is called. Note that the optimizer
  /// object takes ownership of the execution plan and will delete it
  /// automatically on destruction. It will also have ownership of all the
  /// newly created plans it recalls and will automatically delete them.
  /// If you need to extract the plans from the optimizer use stealBest or
  /// stealPlans.
  int createPlans(ExecutionPlan* p, std::vector<std::string> const& rulesSpecification, 
                  bool inspectSimplePlans, bool estimateAllPlans);

  size_t hasEnoughPlans(size_t extraPlans) const;

  /// @brief add a plan to the optimizer
  void addPlan(std::unique_ptr<ExecutionPlan>, OptimizerRule const*, bool, int newLevel = 0);

  void disableRule(int rule);

  /// @brief getBest, ownership of the plan remains with the optimizer
  ExecutionPlan* getBest() {
    if (_plans.empty()) {
      return nullptr;
    }
    return _plans.list.front();
  }

  /// @brief getPlans, ownership of the plans remains with the optimizer
  RollingVector<ExecutionPlan*>& getPlans() { return _plans.list; }

  /// @brief stealBest, ownership of the plan is handed over to the caller,
  /// all other plans are deleted
  ExecutionPlan* stealBest() {
    if (_plans.empty()) {
      return nullptr;
    }
    auto res = _plans.list.front();
    for (size_t i = 1; i < _plans.size(); i++) {
      delete _plans.list[i];
    }
    _plans.list.clear();
    _plans.levelDone.clear();

    return res;
  }

  bool runOnlyRequiredRules() const { return _runOnlyRequiredRules; }

  /// @brief numberOfPlans, returns the current number of plans in the system
  /// this should be called from rules, it will consider those that the
  /// current rules has already added
  size_t numberOfPlans() { return _plans.size() + _newPlans.size() + 1; }

  /// @brief stealPlans, ownership of the plans is handed over to the caller,
  /// the optimizer will forget about them!
  RollingVector<ExecutionPlan*> stealPlans() {
    RollingVector<ExecutionPlan*> res(std::move(_plans.list));
    _plans.levelDone.clear();
    return res;
  }

 public:
  /// @brief optimizer statistics
  Stats _stats;

 private:
  /// @brief the current set of plans to be optimized
  PlanList _plans;

  /// @brief current list of plans (while applying optimizer rules)
  PlanList _newPlans;
  
  // which optimizer rules are disabled?
  std::unordered_set<int> _disabledIds;

  /// @brief maximal number of plans to produce
  size_t const _maxNumberOfPlans;
  
  /// @brief run only the required optimizer rules
  bool _runOnlyRequiredRules;

  /// @brief default value for maximal number of plans to produce
  static constexpr size_t defaultMaxNumberOfPlans = 192;
};

}  // namespace aql
}  // namespace arangodb
#endif
