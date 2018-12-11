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

#include "Optimizer.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/QueryOptions.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"

using namespace arangodb::aql;

// @brief constructor, this will initialize the rules database
Optimizer::Optimizer(size_t maxNumberOfPlans)
    : _maxNumberOfPlans(maxNumberOfPlans),
      _runOnlyRequiredRules(false) {
  for (auto& r : OptimizerRulesFeature::_rules) {
    _rules.emplace(r.first, Rule{r.second, true});
  }
}
  
void Optimizer::disableRule(int rule) {
  auto it = _rules.find(rule);
  TRI_ASSERT(it != _rules.end());
  it->second.enabled = false;
}
   
bool Optimizer::runOnlyRequiredRules(size_t extraPlans) const {
  return (_runOnlyRequiredRules ||
          (_newPlans.size() + _plans.size() + extraPlans >= _maxNumberOfPlans));
}

// @brief add a plan to the optimizer
void Optimizer::addPlan(std::unique_ptr<ExecutionPlan> plan, OptimizerRule const* rule, bool wasModified,
                        int newLevel) {
  TRI_ASSERT(plan != nullptr);
  TRI_ASSERT(&_currentRule->second.rule == rule);

  auto it = _currentRule;

  if (newLevel <= 0) {
    ++it; // move it to the next rule to be processed in the next iteration
  } else {
    it = _rules.upper_bound(newLevel);
  }

  if (wasModified) {
    if (!rule->isHidden) {
      // register which rules modified / created the plan
      // hidden rules are excluded here
      plan->addAppliedRule(static_cast<int>(rule->level));
    }

    plan->clearVarUsageComputed();
    plan->findVarUsage();
  }
  
  // hand over ownership
  _newPlans.push_back(std::move(plan), it);
  
  // stop adding new plans in case we already have enough
  if (_newPlans.size() + _plans.size() >= _maxNumberOfPlans) {
    _runOnlyRequiredRules = true;
  }
}

// @brief the actual optimization
int Optimizer::createPlans(std::unique_ptr<ExecutionPlan> plan,
                           QueryOptions const& queryOptions,
                           bool estimateAllPlans) {
  _runOnlyRequiredRules = false;
  ExecutionPlan* initialPlan = plan.get();

  // _plans contains the previous optimization result
  _plans.clear();
  _plans.push_back(std::move(plan), _rules.begin());
    
  if (!queryOptions.inspectSimplePlans &&
      !arangodb::ServerState::instance()->isCoordinator() &&
      initialPlan->isDeadSimple()) {
    // the plan is so simple that any further optimizations would probably cost
    // more than simply executing the plan
    initialPlan->findVarUsage();
    if (estimateAllPlans || queryOptions.profile >= PROFILE_LEVEL_BLOCKS) {
      // if profiling is turned on, we must do the cost estimation here
      // because the cost estimation must be done while the transaction
      // is still running
      initialPlan->invalidateCost();
      initialPlan->getCost();
    }
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(!_rules.empty());

  // which optimizer rules are disabled?
  for (auto rule : OptimizerRulesFeature::getDisabledRuleIds(queryOptions.optimizerRules)) {
    disableRule(rule);
  }

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
        _newPlans.push_back(std::move(p), _currentRule);  // nothing to do, just keep it
      } else {                                // find next rule
        auto it = _currentRule;
        TRI_ASSERT(it != _rules.end());

        auto& rule = it->second.rule;

        // skip over rules if we should
        // however, we don't want to skip those rules that will not create
        // additional plans
        if (!it->second.enabled || (_runOnlyRequiredRules && rule.canCreateAdditionalPlans && rule.canBeDisabled)) {
          // we picked a disabled rule or we have reached the max number of
          // plans and just skip this rule
          ++it; // move it to the next rule to be processed in the next iteration
          _newPlans.push_back(std::move(p), it);  // nothing to do, just keep it

          if (!rule.isHidden) {
            ++_stats.rulesSkipped;
          }
          
          // now try next
          continue;
        }

        TRI_IF_FAILURE("Optimizer::createPlansOom") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        p->findVarUsage();

        // all optimizer rule functions must obey the following guidelines:
        // - the original plan passed to the rule function must be deleted if
        //   and only if it has not been added (back) to the optimizer (using addPlan).
        // - if the rule throws, then the original plan will be deleted by the optimizer.
        //   thus the rule must not have deleted the plan itself or add it
        //   back to the optimizer
        rule.func(this, std::move(p), &rule);

        if (!rule.isHidden) {
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

    auto fully_optimized = [this](auto const& v){ return v.second == _rules.end(); };
    if (std::all_of(_plans.list.begin(), _plans.list.end(), fully_optimized)) {
      break;
    }
  }

  _stats.plansCreated = _plans.size();

  TRI_ASSERT(_plans.size() >= 1);

  // finalize plans  
  for (auto& plan : _plans.list) {
    plan.first->findVarUsage();
  }

  // do cost estimation
  if (estimateAllPlans || _plans.size() > 1 || queryOptions.profile >= PROFILE_LEVEL_BLOCKS) {
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
                [](PlanList::Entry const& a, PlanList::Entry const& b)
                    -> bool { return a.first->getCost().estimatedCost < b.first->getCost().estimatedCost; });
    } 
  }

  LOG_TOPIC(TRACE, Logger::FIXME) << "optimization ends with " << _plans.size() << " plans";

  return TRI_ERROR_NO_ERROR;
}
