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
#include "Basics/MutexLocker.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <deque>

namespace arangodb {
namespace aql {

class Optimizer {
 public:
  /// @brief optimizer statistics
  struct Stats {
    int64_t rulesExecuted = 0;
    int64_t rulesSkipped = 0;
    int64_t plansCreated = 1;  // 1 for the initial plan

    std::shared_ptr<VPackBuilder> toVelocyPack() const {
      auto result = std::make_shared<VPackBuilder>();
      {
        VPackObjectBuilder b(result.get());
        result->add("rulesExecuted", VPackValue(rulesExecuted));
        result->add("rulesSkipped", VPackValue(rulesSkipped));
        result->add("plansCreated", VPackValue(plansCreated));
      }
      return result;
    }
  };

  /// @brief optimizer rules
  enum RuleLevel : int {
    // List all the rules in the system here:
    // lower level values mean earlier rule execution

    // note that levels must be unique

    // "Pass 1": moving nodes "up" (potentially outside loops):
    pass1 = 100,

    // determine the "right" type of CollectNode and
    // add a sort node for each COLLECT (may be removed later)
    specializeCollectRule_pass1 = 105,
    
    inlineSubqueriesRule_pass1 = 106,

    // split and-combined filters into multiple smaller filters
    splitFiltersRule_pass1 = 110,

    // move calculations up the dependency chain (to pull them out of
    // inner loops etc.)
    moveCalculationsUpRule_pass1 = 120,

    // move filters up the dependency chain (to make result sets as small
    // as possible as early as possible)
    moveFiltersUpRule_pass1 = 130,

    // remove calculations that are repeatedly used in a query
    removeRedundantCalculationsRule_pass1 = 140,

    /// "Pass 2": try to remove redundant or unnecessary nodes
    pass2 = 200,
    // remove filters from the query that are not necessary at all
    // filters that are always true will be removed entirely
    // filters that are always false will be replaced with a NoResults node
    removeUnnecessaryFiltersRule_pass2 = 210,

    // remove calculations that are never necessary
    removeUnnecessaryCalculationsRule_pass2 = 220,

    // remove redundant sort blocks
    removeRedundantSortsRule_pass2 = 230,

    /// "Pass 3": interchange EnumerateCollection nodes in all possible ways
    ///           this is level 500, please never let new plans from higher
    ///           levels go back to this or lower levels!
    pass3 = 500,
    interchangeAdjacentEnumerationsRule_pass3 = 510,

    // "Pass 4": moving nodes "up" (potentially outside loops) (second try):
    pass4 = 600,
    // move calculations up the dependency chain (to pull them out of
    // inner loops etc.)
    moveCalculationsUpRule_pass4 = 610,

    // move filters up the dependency chain (to make result sets as small
    // as possible as early as possible)
    moveFiltersUpRule_pass4 = 620,

    /// "Pass 5": try to remove redundant or unnecessary nodes (second try)
    // remove filters from the query that are not necessary at all
    // filters that are always true will be removed entirely
    // filters that are always false will be replaced with a NoResults node
    pass5 = 700,

    // remove redundant sort blocks
    removeRedundantSortsRule_pass5 = 710,

    // remove SORT RAND() if appropriate
    removeSortRandRule_pass5 = 720,

    // remove INTO for COLLECT if appropriate
    removeCollectVariablesRule_pass5 = 740,

    // propagate constant attributes in FILTERs
    propagateConstantAttributesRule_pass5 = 750,

    // remove unused out variables for data-modification queries
    removeDataModificationOutVariablesRule_pass5 = 760,

    /// "Pass 6": use indexes if possible for FILTER and/or SORT nodes
    pass6 = 800,

    // replace simple OR conditions with IN
    replaceOrWithInRule_pass6 = 810,

    // remove redundant OR conditions
    removeRedundantOrRule_pass6 = 820,

    useIndexesRule_pass6 = 830,

    // try to remove filters covered by index ranges
    removeFiltersCoveredByIndexRule_pass6 = 840,

    removeUnnecessaryFiltersRule_pass6 = 850,

    // try to find sort blocks which are superseeded by indexes
    useIndexForSortRule_pass6 = 860,

    // sort values used in IN comparisons of remaining filters
    sortInValuesRule_pass6 = 865,

    // remove calculations that are never necessary
    removeUnnecessaryCalculationsRule_pass6 = 870,

    // merge filters into graph traversals
    optimizeTraversalsRule_pass6 = 880,

    /// Pass 9: push down calculations beyond FILTERs and LIMITs
    moveCalculationsDownRule_pass9 = 900,

    /// Pass 9: patch update statements
    patchUpdateStatementsRule_pass9 = 902,

    /// "Pass 10": final transformations for the cluster
    // make operations on sharded collections use distribute
    distributeInClusterRule_pass10 = 1000,

    // make operations on sharded collections use scatter / gather / remote
    scatterInClusterRule_pass10 = 1010,

    // move FilterNodes & Calculation nodes in between
    // scatter(remote) <-> gather(remote) so they're
    // distributed to the cluster nodes.
    distributeFilternCalcToClusterRule_pass10 = 1020,

    // move SortNodes into the distribution.
    // adjust gathernode to also contain the sort criteria.
    distributeSortToClusterRule_pass10 = 1030,

    // try to get rid of a RemoteNode->ScatterNode combination which has
    // only a SingletonNode and possibly some CalculationNodes as dependencies
    removeUnnecessaryRemoteScatterRule_pass10 = 1040,

    // recognize that a RemoveNode can be moved to the shards
    undistributeRemoveAfterEnumCollRule_pass10 = 1050
  };

 public:
  struct Rule;

  /// @brief type of an optimizer rule function, the function gets an
  /// optimizer, an ExecutionPlan, and the current rule. it has
  /// to append one or more plans to the resulting deque. This must
  /// include the original plan if it ought to be kept. The rule has to
  /// set the level of the appended plan to the largest level of rule
  /// that ought to be considered as done to indicate which rule is to be
  /// applied next.
  typedef std::function<void(Optimizer*, ExecutionPlan*, Rule const*)>
      RuleFunction;

  /// @brief type of an optimizer rule
  struct Rule {
    std::string name;
    RuleFunction func;
    RuleLevel const level;
    bool const canCreateAdditionalPlans;
    bool const canBeDisabled;
    bool const isHidden;

    Rule() = delete;

    Rule(std::string const& name, RuleFunction const& func, RuleLevel level,
         bool canCreateAdditionalPlans, bool canBeDisabled, bool isHidden)
        : name(name),
          func(func),
          level(level),
          canCreateAdditionalPlans(canCreateAdditionalPlans),
          canBeDisabled(canBeDisabled),
          isHidden(isHidden) {}
  };

  /// @brief the following struct keeps a list (deque) of ExecutionPlan*
  /// and has some automatic convenience functions.
  struct PlanList {
    std::deque<ExecutionPlan*> list;
    std::deque<int> levelDone;

    PlanList() {}

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
      list.swap(b.list);
      levelDone.swap(b.levelDone);
      for (auto& p : b.list) {
        delete p;
      }
      b.list.clear();
      b.levelDone.clear();
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
  int createPlans(ExecutionPlan* p, std::vector<std::string> const&, bool);

  /// @brief add a plan to the optimizer
  /// returns false if there are already enough plans, true otherwise
  bool addPlan(ExecutionPlan*, Rule const*, bool, int newLevel = 0);

  /// @brief getBest, ownership of the plan remains with the optimizer
  ExecutionPlan* getBest() {
    if (_plans.empty()) {
      return nullptr;
    }
    return _plans.list.front();
  }

  /// @brief getPlans, ownership of the plans remains with the optimizer
  std::deque<ExecutionPlan*>& getPlans() { return _plans.list; }

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

  /// @brief numberOfPlans, returns the current number of plans in the system
  /// this should be called from rules, it will consider those that the
  /// current rules has already added
  size_t numberOfPlans() { return _plans.size() + _newPlans.size() + 1; }

  /// @brief stealPlans, ownership of the plans is handed over to the caller,
  /// the optimizer will forget about them!
  std::deque<ExecutionPlan*> stealPlans() {
    std::deque<ExecutionPlan*> res;
    res.swap(_plans.list);
    _plans.levelDone.clear();
    return res;
  }

  /// @brief translate a list of rule ids into rule name
  static std::vector<std::string> translateRules(std::vector<int> const&);

  /// @brief translate a single rule
  static char const* translateRule(int);

  /// @brief returns the previous rule (sorted by rule levels)
  static RuleLevel previousRule(RuleLevel level) {
    auto it = _rules.find(level);

    if (it == _rules.begin()) {
      // already at start
      return level;
    }

    --it;
    return (*it).second.level;
  }

 private:
  /// @brief estimatePlans
  void estimatePlans();

  /// @brief sortPlans
  void sortPlans();

  /// @brief look up the ids of all disabled rules
  std::unordered_set<int> getDisabledRuleIds(
      std::vector<std::string> const&) const;

  /// @brief register a rule
  static void registerRule(std::string const& name, RuleFunction func,
                           RuleLevel level, bool canCreateAdditionalPlans,
                           bool canBeDisabled, bool isHidden = false) {
    if (_ruleLookup.find(name) != _ruleLookup.end()) {
      // duplicate rule names are not allowed
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "duplicate optimizer rule name");
    }

    _ruleLookup.emplace(name, level);

    if (_rules.find(level) != _rules.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "duplicate optimizer rule level");
    }

    _rules.emplace(level, Rule(name, func, level, canCreateAdditionalPlans, canBeDisabled, isHidden));
  }

  /// @brief register a hidden rule
  static void registerHiddenRule(std::string const& name, RuleFunction const& func,
                                 RuleLevel level, bool canCreateAdditionalPlans, bool canBeDisabled) {
    registerRule(name, func, level, canCreateAdditionalPlans, canBeDisabled, true);
  }

  /// @brief set up the optimizer rules once and forever
  static void setupRules();

 public:
  /// @brief optimizer statistics
  Stats _stats;

 private:
  /// @brief the rules database
  static std::map<int, Rule> _rules;

  /// @brief map to look up rule id by name
  static std::unordered_map<std::string, int> _ruleLookup;

  /// @brief mutex to protect rule setup
  static arangodb::Mutex SetupLock;

  /// @brief the current set of plans to be optimized
  PlanList _plans;

  /// @brief current list of plans (while applying optimizer rules)
  PlanList _newPlans;

  /// @brief maximal number of plans to produce
  size_t const _maxNumberOfPlans;

  /// @brief default value for maximal number of plans to produce
  static size_t const DefaultMaxNumberOfPlans = 192;
};

}  // namespace aql
}  // namespace arangodb
#endif
