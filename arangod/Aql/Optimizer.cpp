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
#include "Aql/ExecutionEngine.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"

using namespace arangodb::aql;

// @brief constructor, this will initialize the rules database
Optimizer::Optimizer(size_t maxNumberOfPlans)
    : _maxNumberOfPlans(maxNumberOfPlans),
      _runOnlyRequiredRules(false) {}
  
bool Optimizer::runOnlyRequiredRules(size_t extraPlans) const {
  return (_runOnlyRequiredRules ||
          (_newPlans.size() + _plans.size() + extraPlans >= _maxNumberOfPlans));
}
  
void Optimizer::disableRule(int rule) {
  _disabledIds.emplace(rule);
}

// @brief add a plan to the optimizer
void Optimizer::addPlan(std::unique_ptr<ExecutionPlan> plan, OptimizerRule const* rule, bool wasModified,
                        int newLevel) {
  TRI_ASSERT(plan != nullptr);

  if (newLevel <= 0) {
    // use rule's level
    newLevel = rule->level;
    // else use user-specified new level
  }

  if (wasModified) {
    if (!rule->isHidden) {
      // register which rules modified / created the plan
      // hidden rules are excluded here
      plan->addAppliedRule(static_cast<int>(rule->level));
    }

    plan->clearVarUsageComputed();
    plan->invalidateCost();
    plan->findVarUsage();
  }
  
  // hand over ownership
  _newPlans.push_back(plan.get(), newLevel);
  plan.release();
  
  // stop adding new plans in case we already have enough
  if (_newPlans.size() + _plans.size() >= _maxNumberOfPlans) {
    _runOnlyRequiredRules = true;
  }
}

// @brief the actual optimization
int Optimizer::createPlans(ExecutionPlan* plan,
                           std::vector<std::string> const& rulesSpecification,
                           bool inspectSimplePlans) {
  _runOnlyRequiredRules = false;
  // _plans contains the previous optimization result
  _plans.clear();
    
  try {
    _plans.push_back(plan, 0);
  } catch (...) {
    delete plan;
    throw;
  }
  
  if (!inspectSimplePlans &&
      !arangodb::ServerState::instance()->isCoordinator() &&
      plan->isDeadSimple()) {
    // the plan is so simple that any further optimizations would probably cost
    // more than simply executing the plan
    estimatePlans();

    return TRI_ERROR_NO_ERROR;
  }
  int leastDoneLevel = 0;

  TRI_ASSERT(!OptimizerRulesFeature::_rules.empty());
  int maxRuleLevel = OptimizerRulesFeature::_rules.rbegin()->first;

  // which optimizer rules are disabled?
  _disabledIds = OptimizerRulesFeature::getDisabledRuleIds(rulesSpecification);

  _newPlans.clear();
        
  while (leastDoneLevel < maxRuleLevel) {
    // std::cout << "Have " << _plans.size() << " plans:" << std::endl;
    // for (auto const& p : _plans.list) {
    //   p->show();
    //   std::cout << std::endl;
    // }

    // int count = 0;

    // For all current plans:
    while (_plans.size() > 0) {
      int level;
      std::unique_ptr<ExecutionPlan> p(_plans.pop_front(level));

      if (level >= maxRuleLevel) {
        _newPlans.push_back(p.get(), level);  // nothing to do, just keep it
        p.release();
      } else {                                // find next rule
        auto it = OptimizerRulesFeature::_rules.upper_bound(level);
        TRI_ASSERT(it != OptimizerRulesFeature::_rules.end());

        level = (*it).first;
        auto& rule = (*it).second;

        // skip over rules if we should
        // however, we don't want to skip those rules that will not create
        // additional plans
        if ((_runOnlyRequiredRules && rule.canCreateAdditionalPlans && rule.canBeDisabled) ||
            _disabledIds.find(level) != _disabledIds.end()) {
          // we picked a disabled rule or we have reached the max number of
          // plans and just skip this rule

          _newPlans.push_back(p.get(), level);  // nothing to do, just keep it
          p.release();

          if (!rule.isHidden) {
            ++_stats.rulesSkipped;
          }
          
          // now try next
          continue;
        }

        TRI_IF_FAILURE("Optimizer::createPlansOom") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        if (!p->varUsageComputed()) {
          p->findVarUsage();
        }

        // all optimizer rule functions must obey the following guidelines:
        // - the original plan passed to the rule function must be deleted if
        // and only
        //   if it has not been added (back) to the optimizer (using addPlan).
        // - if the rule throws, then the original plan will be deleted by the
        // optimizer.
        //   thus the rule must not have deleted the plan itself or add it
        //   back to the
        //   optimizer
        rule.func(this, std::move(p), &rule);

        if (!rule.isHidden) {
          ++_stats.rulesExecuted;
        }
      }

      // future optimization: abort early here if we found a good-enough plan
      // a good-enough plan is probably every plan with costs below some
      // defined threshold. this requires plan costs to be calculated here
    }
    
    _plans.steal(_newPlans);
    leastDoneLevel = maxRuleLevel;
    for (auto const& l : _plans.levelDone) {
      if (l < leastDoneLevel) {
        leastDoneLevel = l;
      }
    }
  }

  _stats.plansCreated = _plans.size();

  TRI_ASSERT(_plans.size() >= 1);

  estimatePlans();
  if (_plans.size() > 1) {
    sortPlans();
  }

  LOG_TOPIC(TRACE, Logger::FIXME) << "optimization ends with " << _plans.size() << " plans";

  return TRI_ERROR_NO_ERROR;
}

/// @brief estimatePlans
void Optimizer::estimatePlans() {
  for (auto& p : _plans.list) {
    if (!p->varUsageComputed()) {
      p->findVarUsage();
    }
    p->getCost();
    // this value is cached in the plan, so formally this step is
    // unnecessary, but for the sake of cleanliness...
  }
}

/// @brief sortPlans
void Optimizer::sortPlans() {
  std::sort(_plans.list.begin(), _plans.list.end(),
            [](ExecutionPlan* const& a, ExecutionPlan* const& b)
                -> bool { return a->getCost() < b->getCost(); });
}
