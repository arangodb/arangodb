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

#include "Aql/ExecutionPlan.h"
#include "Basics/Common.h"
#include "Containers/RollingVector.h"

#include <velocypack/Builder.h>
#include <velocypack/StringRef.h>
#include <vector>

namespace arangodb {
namespace aql {
struct OptimizerRule;
struct QueryOptions;

class Optimizer {
 private:
  /// @brief this stored the positions of rules in OptimizerRulesFeature::_rules
  using RuleDatabase = std::vector<int>; 

  /// @brief the following struct keeps a list (deque) of ExecutionPlan*
  /// and has some automatic convenience functions.
  struct PlanList {
    using Entry = std::pair<std::unique_ptr<ExecutionPlan>, RuleDatabase::iterator>;

    ::arangodb::containers::RollingVector<Entry> list;

    PlanList() { list.reserve(8); }

    /// @brief constructor with a plan
    PlanList(std::unique_ptr<ExecutionPlan> p, RuleDatabase::iterator rule) {
      push_back(std::move(p), rule);
    }

    /// @brief destructor, deleting contents
    ~PlanList() = default;

    /// @brief get number of plans contained
    size_t size() const { return list.size(); }

    /// @brief check if empty
    bool empty() const { return list.empty(); }

    /// @brief pop the first one
    Entry pop_front() {
      auto p = std::move(list.front());
      list.pop_front();
      return p;
    }

    /// @brief push_back
    void push_back(std::unique_ptr<ExecutionPlan> p, RuleDatabase::iterator rule) {
      list.push_back({std::move(p), rule});
    }

    /// @brief swaps the two lists
    void swap(PlanList& b) { list.swap(b.list); }

    /// @brief appends all the plans to the target and clears *this at the same
    /// time
    void appendTo(PlanList& target) {
      while (list.size() > 0) {
        auto p = std::move(list.front());
        list.pop_front();
        target.push_back(std::move(p.first), p.second);
      }
    }

    /// @brief clear, deletes all plans contained
    void clear() { list.clear(); }
  };

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

  /// @brief constructor, this will initialize the rules database
  /// the .cpp file includes Aql/OptimizerRules.h
  /// and add all methods there to the rules database
  explicit Optimizer(size_t maxNumberOfPlans);

  ~Optimizer() = default;

  /// @brief disable rules in the given plan, using the predicate function
  void disableRules(ExecutionPlan* plan,
                    std::function<bool(OptimizerRule const&)> const& predicate);

  /// @brief do the optimization, this does the optimization, the resulting
  /// plans are all estimated, sorted by that estimate and can then be got
  /// by getPlans, until the next initialize is called. Note that the optimizer
  /// object takes ownership of the execution plan and will delete it
  /// automatically on destruction. It will also have ownership of all the
  /// newly created plans it recalls and will automatically delete them.
  /// If you need to extract the plans from the optimizer use stealBest or
  /// stealPlans.
  void createPlans(std::unique_ptr<ExecutionPlan> p,
                   QueryOptions const& queryOptions, bool estimateAllPlans);

  /// @brief add a plan to the optimizer
  void addPlan(std::unique_ptr<ExecutionPlan>, OptimizerRule const&, bool wasModified);
  
  /// @brief add a plan to the optimizer and makes it rerun the current rule again 
  void addPlanAndRerun(std::unique_ptr<ExecutionPlan>, OptimizerRule const&, bool wasModified);

  /// @brief getPlans, ownership of the plans remains with the optimizer
  ::arangodb::containers::RollingVector<PlanList::Entry>& getPlans() {
    return _plans.list;
  }

  /// @brief stealBest, ownership of the plan is handed over to the caller,
  /// all other plans are deleted
  std::unique_ptr<ExecutionPlan> stealBest() {
    if (_plans.empty()) {
      return nullptr;
    }
    auto res = std::move(_plans.list.front());
    _plans.list.clear();
    return std::move(res.first);
  }

  bool runOnlyRequiredRules(size_t extraPlans) const;

  /// @brief numberOfPlans, returns the current number of plans in the system
  /// this should be called from rules, it will consider those that the
  /// current rules has already added
  size_t numberOfPlans() { return _plans.size() + _newPlans.size() + 1; }
  
 private:
  /// @brief disable a specific rule
  void disableRule(ExecutionPlan* plan, int level);
  
  /// @brief disable a specific rule, by name
  void disableRule(ExecutionPlan* plan, velocypack::StringRef name); 
  
  /// @brief enable a specific rule
  void enableRule(ExecutionPlan* plan, int level);
  
  /// @brief enable a specific rule, by name
  void enableRule(ExecutionPlan* plan, velocypack::StringRef name);

  /// @brief adds a plan, internal worker method
  void addPlanInternal(std::unique_ptr<ExecutionPlan> plan,
                       OptimizerRule const& rule, bool wasModified,
                       RuleDatabase::iterator const& nextRule);

 public:
  /// @brief optimizer statistics
  Stats _stats;

 private:
  /// @brief the current set of plans to be optimized
  PlanList _plans;

  /// @brief current list of plans (while applying optimizer rules)
  PlanList _newPlans;

  /// @brief the rule that is currently getting applied
  /// (while applying optimizer rules in createPlans)
  RuleDatabase::iterator _currentRule;

  /// @brief list of optimizer rules to be applied
  RuleDatabase _rules;

  /// @brief maximal number of plans to produce
  size_t const _maxNumberOfPlans;

  /// @brief run only the required optimizer rules
  bool _runOnlyRequiredRules;
};

}  // namespace aql
}  // namespace arangodb
#endif
