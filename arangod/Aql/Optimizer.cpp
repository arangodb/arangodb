////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Optimizer.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OptimizerRule.h"
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/QueryOptions.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

using namespace arangodb::aql;

Optimizer::Optimizer(size_t maxNumberOfPlans)
    : _maxNumberOfPlans(maxNumberOfPlans), _runOnlyRequiredRules(false) {}

void Optimizer::disableRules(ExecutionPlan* plan,
                             std::function<bool(OptimizerRule const&)> const& predicate) {
  for (auto& it : _rules) {
    auto const& rule = OptimizerRulesFeature::ruleByIndex(it);
    if (predicate(rule)) {
      plan->disableRule(rule.level);
    }
  }
}

bool Optimizer::runOnlyRequiredRules(size_t extraPlans) const {
  return (_runOnlyRequiredRules ||
          (_newPlans.size() + _plans.size() + extraPlans >= _maxNumberOfPlans));
}

// @brief add a plan to the optimizer
void Optimizer::addPlan(std::unique_ptr<ExecutionPlan> plan,
                        OptimizerRule const& rule, bool wasModified) {

  auto it = _currentRule;
  TRI_ASSERT(it != _rules.end());
  // move it to the next rule to be processed in the next iteration
  addPlanInternal(std::move(plan), rule, wasModified, ++it);
}

void Optimizer::addPlanAndRerun(std::unique_ptr<ExecutionPlan> plan,
                                OptimizerRule const& rule, bool wasModified) {
  auto it = _rules.begin() + OptimizerRulesFeature::ruleIndex(rule.level);
  TRI_ASSERT(it != _rules.end());
  addPlanInternal(std::move(plan), rule, wasModified, it);
}


#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
class NoSubqueryChecker : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  bool before(ExecutionNode* node) override {
    TRI_ASSERT(node->getType() != ExecutionNode::SUBQUERY);
    return false;
  }
};

// Check the plan for inconsistencies, like more than one parent or dependency,
// or mismatching parents and dependencies in adjacent nodes.
class PlanChecker : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
 public:
  explicit PlanChecker(ExecutionPlan& plan) : _plan{plan} {}

  bool before(ExecutionNode* node) override {
    bool ok = true;
    std::vector<std::stringstream> errors;

    switch (node->getType()) {
      case ExecutionNode::RETURN:
        if (node->getParents().size() != 0) {
          errors.emplace_back() << "#parents == " << node->getParents().size() << " at [" << node->id() << "] " << node->getTypeString();
          ok = false;
        }
        break;
      case ExecutionNode::INSERT:
      case ExecutionNode::UPDATE:
      case ExecutionNode::REPLACE:
      case ExecutionNode::UPSERT:
      case ExecutionNode::REMOVE:
      case ExecutionNode::GATHER:
      case ExecutionNode::REMOTESINGLE:
        if (node->getParents().size() > 1) {
          errors.emplace_back() << "#parents == " << node->getParents().size() << " at [" << node->id() << "] " << node->getTypeString();
          ok = false;
        }
        break;
      default:
        if (node->getParents().size() != 1) {
          errors.emplace_back() << "#parents == " << node->getParents().size() << " at [" << node->id() << "] " << node->getTypeString();
          ok = false;
        }
        break;
    }
    switch (node->getType()) {
      case ExecutionNode::SINGLETON:
        if (node->getDependencies().size() != 0) {
          errors.emplace_back() << "#dependencies == " << node->getDependencies().size() << " at [" << node->id() << "] " << node->getTypeString();
          ok = false;
        }
        break;
      default:
        if (node->getDependencies().size() != 1) {
          errors.emplace_back() << "#dependencies == " << node->getDependencies().size() << " at [" << node->id() << "] " << node->getTypeString();
          ok = false;
        }
        break;
    }

    auto isDepOf = [](ExecutionNode const* node, ExecutionNode const* parent) {
      auto const& deps = parent->getDependencies();
      return std::any_of(deps.begin(), deps.end(), [&](auto it) { return it == node; });
    };
    auto isParentOf = [](ExecutionNode const* node, ExecutionNode const* dep) {
      auto const& parents = dep->getParents();
      return std::any_of(parents.begin(), parents.end(), [&](auto it) { return it == node; });
    };

    for (auto const& parent : node->getParents()) {
      if (!isDepOf(node, parent)) {
        errors.emplace_back() << "!isDepOf(" << node->id() << ", " << parent->id() << ")";
        errors.emplace_back() << "  node is a " << node->getTypeString();
        errors.emplace_back() << "  parent is a " << parent->getTypeString();
        ok = false;
      }
    }
    for (auto const& dep : node->getDependencies()) {
      if (!isParentOf(node, dep)) {
        errors.emplace_back() << "!isParentOf(" << dep->id() << ", " << node->id() << ")";
        errors.emplace_back() << "  dependency is a " << dep->getTypeString();
        errors.emplace_back() << "  node is a " << node->getTypeString();
        ok = false;
      }
    }

    if (!ok) {
      LOG_TOPIC("d45f8", ERR, arangodb::Logger::AQL) << "Inconsistent plan:";
      _plan.show();
      LOG_TOPIC("c14a2", ERR, arangodb::Logger::AQL) << "encountered the following error(s):";
      for (auto const& err : errors) {
        LOG_TOPIC("17a18", ERR, arangodb::Logger::AQL) << err.str();
      }
    }
    TRI_ASSERT(ok);

    return false;
  }

 private:
  ExecutionPlan& _plan;
};

#endif // ARANGODB_ENABLE_MAINTAINER_MODE

  // @brief add a plan to the optimizer
void Optimizer::addPlanInternal(std::unique_ptr<ExecutionPlan> plan,
                                OptimizerRule const& rule, bool wasModified,
                                RuleDatabase::iterator const& nextRule) {
  TRI_ASSERT(plan != nullptr);
  TRI_ASSERT(!_rules.empty());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto checker = PlanChecker{*plan};
  plan->root()->walk(checker);
#endif // ARANGODB_ENABLE_MAINTAINER_MODE

  plan->setValidity(true);

  if (wasModified) {
    if (!rule.isHidden()) {
      // register which rules modified / created the plan
      // hidden rules are excluded here
      plan->addAppliedRule(static_cast<int>(rule.level));
    }

    plan->clearVarUsageComputed();
    plan->findVarUsage();
  }

  // hand over ownership
  _newPlans.push_back(std::move(plan), nextRule);

  // stop adding new plans in case we already have enough
  if (_newPlans.size() + _plans.size() >= _maxNumberOfPlans) {
    _runOnlyRequiredRules = true;
  }
}

void Optimizer::initializeRules(ExecutionPlan* plan, QueryOptions const& queryOptions) {
  if (ADB_LIKELY(_rules.empty())) {
    auto const& rules = OptimizerRulesFeature::rules();
    _rules.reserve(rules.size());

    TRI_ASSERT(std::is_sorted(rules.begin(), rules.end(), [](OptimizerRule const& lhs, OptimizerRule const& rhs) {
      return lhs.level < rhs.level;
    }));

    int index = -1;
    for (auto& rule : OptimizerRulesFeature::rules()) {
      // insert position of rule inside OptimizerRulesFeature::_rules
      _rules.emplace_back(++index);
      if (rule.isDisabledByDefault()) {
        disableRule(plan, rule.level);
      }
    }
  }
  
  // enable/disable rules as per user request
  for (auto const& name : queryOptions.optimizerRules) {
    if (name.empty()) {
      continue;
    }
    if (name[0] == '-') {
      disableRule(plan, arangodb::velocypack::StringRef(name));
    } else {
      enableRule(plan, arangodb::velocypack::StringRef(name));
    }
  }
}

// @brief the actual optimization
void Optimizer::createPlans(std::unique_ptr<ExecutionPlan> plan,
                            QueryOptions const& queryOptions, bool estimateAllPlans) {
  _runOnlyRequiredRules = false;
  ExecutionPlan* initialPlan = plan.get();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto checker = PlanChecker{*plan};
  plan->root()->walk(checker);
#endif // ARANGODB_ENABLE_MAINTAINER_MODE

  initializeRules(initialPlan, queryOptions);

  TRI_ASSERT(!_rules.empty());

  // _plans contains the previous optimization result
  _plans.clear();
  _plans.push_back(std::move(plan), _rules.begin());

  _newPlans.clear();

  while (true) {
    // std::cout << "Have " << _plans.size() << " plans:" << std::endl;
    // for (auto const& p : _plans.list) {
    //   p->show();
    //   std::cout << std::endl;
    // }

    // int count = 0;

    // For all current plans:
    while (!_plans.empty()) {
      std::unique_ptr<ExecutionPlan> p;
      std::tie(p, _currentRule) = _plans.pop_front();

      if (_currentRule == _rules.end()) {
        // nothing to do, just keep it
        _newPlans.push_back(std::move(p), _currentRule);
      } else {
        // find next rule
        auto it = _currentRule;
        TRI_ASSERT(it != _rules.end());

        auto const& rule = OptimizerRulesFeature::ruleByIndex(*it);

        // skip over rules if we should
        // however, we don't want to skip those rules that will not create
        // additional plans
        if (p->isDisabledRule(rule.level) ||
            (_runOnlyRequiredRules && rule.canCreateAdditionalPlans() && rule.canBeDisabled())) {
          // we picked a disabled rule or we have reached the max number of
          // plans and just skip this rule
          ++it;  // move it to the next rule to be processed in the next
                 // iteration
          _newPlans.push_back(std::move(p), it);  // nothing to do, just keep it

          if (!rule.isHidden()) {
            ++_stats.rulesSkipped;
          }

          // now try next
          continue;
        }

        TRI_IF_FAILURE("Optimizer::createPlansOom") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        p->findVarUsage();
        p->setValidity(false);

        if (queryOptions.getProfileLevel() >= ProfileLevel::Blocks) {
          // run rule with tracing optimizer rule execution time
          if (_stats.executionTimes == nullptr) {
            // allocate the map lazily, so we can save the initial memory allocation
            // in case tracing is disabled.
            _stats.executionTimes = std::make_unique<std::unordered_map<int, double>>();
          }
          TRI_ASSERT(_stats.executionTimes != nullptr);

          double time = TRI_microtime();
          rule.func(this, std::move(p), rule);
          time = TRI_microtime() - time;
          auto [it, inserted] = _stats.executionTimes->try_emplace(rule.level, time);
          if (!inserted) {
            // a rule may have been executed before already. in this case, just add to the
            // already tracked time
            (*it).second += time;
          }
        } else {
          // run rule without tracing optimizer rules
          rule.func(this, std::move(p), rule);
        }

        if (!rule.isHidden()) {
          ++_stats.rulesExecuted;
        }
      }

      // future optimization: abort early here if we found a good-enough plan
      // a good-enough plan is probably every plan with costs below some
      // defined threshold. this requires plan costs to be calculated here
    }

    TRI_ASSERT(_plans.empty());
    // we use swap here to keep the allocated buffers of both lists so we can
    // reuse them in the next iteration
    _plans.swap(_newPlans);

    auto fullyOptimized = [this](auto const& v) {
      return v.second == _rules.end();
    };

    if (std::all_of(_plans.list.begin(), _plans.list.end(), fullyOptimized)) {
      break;
    }
  }

  _stats.plansCreated = _plans.size();

  TRI_ASSERT(_plans.size() >= 1);

  finalizePlans();
  
  estimateCosts(queryOptions, estimateAllPlans);
  
  LOG_TOPIC("5b5f6", TRACE, Logger::FIXME)
      << "optimization ends with " << _plans.size() << " plans";
}

void Optimizer::finalizePlans() {
  for (auto& plan : _plans.list) {
    insertDistributeInputCalculation(*plan.first);
    
    plan.first->findVarUsage();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    NoSubqueryChecker checker;
    plan.first->root()->walk(checker);
#endif // ARANGODB_ENABLE_MAINTAINER_MODE
  }
}

void Optimizer::estimateCosts(QueryOptions const& queryOptions, bool estimateAllPlans) {
  if (estimateAllPlans || _plans.size() > 1 || queryOptions.profile >= ProfileLevel::Blocks) {
    // if profiling is turned on, we must do the cost estimation here
    // because the cost estimation must be done while the transaction
    // is still running
    for (auto& plan : _plans.list) {
      plan.first->invalidateCost();
      plan.first->getCost();
      // this value is cached in the plan, so formally this step is
      // unnecessary, but for the sake of cleanliness...
    }

    if (_plans.size() > 1) {
      // only sort plans when necessary
      std::sort(_plans.list.begin(), _plans.list.end(),
                [](PlanList::Entry const& a, PlanList::Entry const& b) -> bool {
                  return a.first->getCost().estimatedCost < b.first->getCost().estimatedCost;
                });
    }
  }
}

void Optimizer::disableRule(ExecutionPlan* plan, int level) {
  if (level <= 0) {
    // invalid rule
    return;
  }

  auto const& rule = OptimizerRulesFeature::ruleByLevel(level);
  if (rule.canBeDisabled()) {
    plan->disableRule(level);
  }
}

void Optimizer::disableRule(ExecutionPlan* plan, arangodb::velocypack::StringRef name) {
  if (!name.empty() && name[0] == '-') {
    name = name.substr(1);
  }

  if (name == "all") {
    // disable all rules
    for (auto& r : _rules) {
      auto const& rule = OptimizerRulesFeature::ruleByIndex(r);
      disableRule(plan, rule.level);
    }
  } else {
    disableRule(plan, OptimizerRulesFeature::translateRule(name));
  }
}

void Optimizer::enableRule(ExecutionPlan* plan, int level) {
  if (level <= 0) {
    // invalid rule
    return;
  }
  plan->enableRule(level);
}

void Optimizer::enableRule(ExecutionPlan* plan, arangodb::velocypack::StringRef name) {
  if (!name.empty() && name[0] == '+') {
    name = name.substr(1);
  }

  if (name == "all") {
    // enable all rules
    for (auto& r : _rules) {
      auto const& rule = OptimizerRulesFeature::ruleByIndex(r);
      if (!rule.isDisabledByDefault()) {
        enableRule(plan, rule.level);
      }
    }
  } else {
    enableRule(plan, OptimizerRulesFeature::translateRule(name));
  }
}
    
void Optimizer::Stats::toVelocyPack(velocypack::Builder& b) const {
  velocypack::ObjectBuilder guard(&b, true);
  b.add("rulesExecuted", velocypack::Value(rulesExecuted));
  b.add("rulesSkipped", velocypack::Value(rulesSkipped));
  b.add("plansCreated", velocypack::Value(plansCreated));

  if (executionTimes != nullptr) {
    b.add("rules", velocypack::Value(velocypack::ValueType::Object));
    for (auto const& it : *executionTimes) {
      b.add(OptimizerRulesFeature::translateRule(it.first), velocypack::Value(it.second));
    }
    b.close();
  }
}
