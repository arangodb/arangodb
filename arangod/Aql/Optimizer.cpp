////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/EnumeratePathsNode.h"
#include "Aql/ExecutionNode/InsertNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/ShortestPathNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionNode/UpdateNode.h"
#include "Aql/ExecutionNode/UpsertNode.h"
#include "Aql/IndexHint.h"
#include "Aql/OptimizerRule.h"
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/ProfileLevel.h"
#include "Aql/QueryOptions.h"
#include "Basics/debugging.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"

#include <absl/strings/str_cat.h>

using namespace arangodb::aql;

Optimizer::Optimizer(ResourceMonitor& resourceMonitor, size_t maxNumberOfPlans)
    : _plans(resourceMonitor),
      _newPlans(resourceMonitor),
      _maxNumberOfPlans(maxNumberOfPlans),
      _runOnlyRequiredRules(false) {}

void Optimizer::toVelocyPack(velocypack::Builder& b) const {
  _stats.toVelocyPack(b);
}

void Optimizer::disableRules(
    ExecutionPlan* plan,
    std::function<bool(OptimizerRule const&)> const& predicate) {
  for (auto& it : _rules) {
    auto const& rule = OptimizerRulesFeature::ruleByIndex(it);
    if (predicate(rule)) {
      plan->disableRule(rule.level);
    }
  }
}

bool Optimizer::runOnlyRequiredRules() const noexcept {
  return (_runOnlyRequiredRules ||
          (_newPlans.size() + _plans.size() + 1 >= _maxNumberOfPlans));
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
class NoSubqueryChecker
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  bool before(ExecutionNode* node) override {
    TRI_ASSERT(node->getType() != ExecutionNode::SUBQUERY);
    return false;
  }
};

// Check the plan for inconsistencies, like more than one parent or dependency,
// or mismatching parents and dependencies in adjacent nodes.
class PlanChecker
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
 public:
  explicit PlanChecker(ExecutionPlan& plan) : _plan{plan} {}

  bool before(ExecutionNode* node) override {
    bool ok = true;
    std::vector<std::stringstream> errors;

    switch (node->getType()) {
      case ExecutionNode::RETURN:
        if (node->getParents().size() != 0) {
          errors.emplace_back()
              << "#parents == " << node->getParents().size() << " at ["
              << node->id() << "] " << node->getTypeString();
          ok = false;
        }
        break;
      case ExecutionNode::INSERT:
      case ExecutionNode::UPDATE:
      case ExecutionNode::REPLACE:
      case ExecutionNode::UPSERT:
      case ExecutionNode::REMOVE:
      case ExecutionNode::GATHER:
      case ExecutionNode::REMOTE_SINGLE:
      case ExecutionNode::REMOTE_MULTIPLE:
        if (node->getParents().size() > 1) {
          errors.emplace_back()
              << "#parents == " << node->getParents().size() << " at ["
              << node->id() << "] " << node->getTypeString();
          ok = false;
        }
        break;
      default:
        if (node->getParents().size() != 1) {
          errors.emplace_back()
              << "#parents == " << node->getParents().size() << " at ["
              << node->id() << "] " << node->getTypeString();
          ok = false;
        }
        break;
    }
    switch (node->getType()) {
      case ExecutionNode::SINGLETON:
        if (node->getDependencies().size() != 0) {
          errors.emplace_back()
              << "#dependencies == " << node->getDependencies().size()
              << " at [" << node->id() << "] " << node->getTypeString();
          ok = false;
        }
        break;
      default:
        if (node->getDependencies().size() != 1) {
          errors.emplace_back()
              << "#dependencies == " << node->getDependencies().size()
              << " at [" << node->id() << "] " << node->getTypeString();
          ok = false;
        }
        break;
    }

    auto isDepOf = [](ExecutionNode const* node, ExecutionNode const* parent) {
      auto const& deps = parent->getDependencies();
      return std::any_of(deps.begin(), deps.end(),
                         [&](auto it) { return it == node; });
    };
    auto isParentOf = [](ExecutionNode const* node, ExecutionNode const* dep) {
      auto const& parents = dep->getParents();
      return std::any_of(parents.begin(), parents.end(),
                         [&](auto it) { return it == node; });
    };

    for (auto const& parent : node->getParents()) {
      if (!isDepOf(node, parent)) {
        errors.emplace_back()
            << "!isDepOf(" << node->id() << ", " << parent->id() << ")";
        errors.emplace_back() << "  node is a " << node->getTypeString();
        errors.emplace_back() << "  parent is a " << parent->getTypeString();
        ok = false;
      }
    }
    for (auto const& dep : node->getDependencies()) {
      if (!isParentOf(node, dep)) {
        errors.emplace_back()
            << "!isParentOf(" << dep->id() << ", " << node->id() << ")";
        errors.emplace_back() << "  dependency is a " << dep->getTypeString();
        errors.emplace_back() << "  node is a " << node->getTypeString();
        ok = false;
      }
    }

    if (!ok) {
      LOG_TOPIC("d45f8", ERR, arangodb::Logger::AQL) << "Inconsistent plan:";
      _plan.show();
      LOG_TOPIC("c14a2", ERR, arangodb::Logger::AQL)
          << "encountered the following error(s):";
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

#endif  // ARANGODB_ENABLE_MAINTAINER_MODE

// @brief add a plan to the optimizer
void Optimizer::addPlanInternal(std::unique_ptr<ExecutionPlan> plan,
                                OptimizerRule const& rule, bool wasModified,
                                RuleDatabase::iterator const& nextRule) {
  TRI_ASSERT(plan != nullptr);
  TRI_ASSERT(!_rules.empty());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto checker = PlanChecker{*plan};
  plan->root()->walk(checker);
#endif  // ARANGODB_ENABLE_MAINTAINER_MODE

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

void Optimizer::initializeRules(ExecutionPlan* plan,
                                QueryOptions const& queryOptions) {
  if (ADB_LIKELY(_rules.empty())) {
    auto const& rules = OptimizerRulesFeature::rules();
    _rules.reserve(rules.size());

    TRI_ASSERT(
        std::is_sorted(rules.begin(), rules.end(),
                       [](OptimizerRule const& lhs, OptimizerRule const& rhs) {
                         return lhs.level < rhs.level;
                       }));

    int index = -1;
    for (auto const& rule : rules) {
      // insert position of rule inside OptimizerRulesFeature::_rules
      _rules.emplace_back(++index);
      if (rule.isDisabledByDefault() || plan->isDisabledRule(rule.level)) {
        disableRule(plan, rule.level);
      }
    }

    // enable/disable rules as per user request
    for (auto const& name : queryOptions.optimizerRules) {
      if (name.empty()) {
        continue;
      }
      if (name[0] == '-') {
        disableRule(plan, name);
      } else {
        enableRule(plan, name);
      }
    }
  }
}

// @brief the actual optimization
void Optimizer::createPlans(std::unique_ptr<ExecutionPlan> plan,
                            QueryOptions const& queryOptions) {
  _runOnlyRequiredRules = false;
  ExecutionPlan* initialPlan = plan.get();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto checker = PlanChecker{*plan};
  plan->root()->walk(checker);
#endif  // ARANGODB_ENABLE_MAINTAINER_MODE

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
        auto ruleDisabled = p->isDisabledRule(rule.level);
        if (ruleDisabled ||
            (_runOnlyRequiredRules && rule.canCreateAdditionalPlans() &&
             rule.canBeDisabled())) {
          // we picked a disabled rule or we have reached the max number of
          // plans and just skip this rule
          ++it;  // move it to the next rule to be processed in the next
                 // iteration
          _newPlans.push_back(std::move(p), it);  // nothing to do, just keep it

          if (!rule.isHidden() &&
              !(rule.isDisabledByDefault() && ruleDisabled)) {
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

        size_t numberOfPlansBeforeRule = _newPlans.size();
        if (queryOptions.getProfileLevel() >= ProfileLevel::Blocks) {
          // run rule with tracing optimizer rule execution time
          if (_stats.executionTimes == nullptr) {
            // allocate the map lazily, so we can save the initial memory
            // allocation in case tracing is disabled.
            _stats.executionTimes =
                std::make_unique<std::unordered_map<int, double>>();
          }
          TRI_ASSERT(_stats.executionTimes != nullptr);

          double time = TRI_microtime();
          rule.func(this, std::move(p), rule);
          time = TRI_microtime() - time;
          auto [it, inserted] =
              _stats.executionTimes->try_emplace(rule.level, time);
          if (!inserted) {
            // a rule may have been executed before already. in this case, just
            // add to the already tracked time
            (*it).second += time;
          }
        } else {
          // run rule without tracing optimizer rules
          rule.func(this, std::move(p), rule);
        }

        // we should have at least one more plan than before
        TRI_ASSERT(_newPlans.size() > numberOfPlansBeforeRule);
        // if the rule is marked to create additional plans, it can create
        // an arbitrary number of plans. otherwise it must create exactly
        // one plan (which may be the same as its input plan).
        TRI_ASSERT(rule.canCreateAdditionalPlans() ||
                   _newPlans.size() == numberOfPlansBeforeRule + 1)
            << "optimizer rule " << rule.name
            << " executed additional plans although it is not marked as such";

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

  estimateCosts(queryOptions);

  // Best plan should not have forced hints left.
  // note: this method will throw in case the plan still contains
  // unsatisfied index hints that have their force flag set
  checkForcedIndexHints();

  TRI_ASSERT(!_plans.empty());
  LOG_TOPIC("5b5f6", TRACE, Logger::FIXME)
      << "optimization ends with " << _plans.size() << " plans";
}

void Optimizer::finalizePlans() {
  for (auto& plan : _plans.list) {
    insertDistributeInputCalculation(*plan.first);
    activateCallstackSplit(*plan.first);

    plan.first->findVarUsage();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    NoSubqueryChecker checker;
    plan.first->root()->walk(checker);
#endif  // ARANGODB_ENABLE_MAINTAINER_MODE
  }
}

void Optimizer::checkForcedIndexHints() {
  while (true) {
    auto& bestPlan = _plans.list.front().first;

    IndexHint hint = bestPlan->firstUnsatisfiedForcedIndexHint();
    if (!hint.isSet()) {
      // all index hints satisified in current best plan
      break;
    }

    TRI_ASSERT(hint.isForced());
    if (_plans.size() == 1) {
      // we are the last plan and cannot satisfy the index hint -> fail
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE,
          absl::StrCat("could not use index hint to serve query; ",
                       hint.toString()));
    }

    // there are more plans left to try.
    // discard the current plan and continue with the next-best plan.
    _plans.list.pop_front();
  }
  TRI_ASSERT(!_plans.list.empty());
}

void Optimizer::estimateCosts(QueryOptions const& queryOptions) {
  // Invalidate all plans and get new estimates before doing anything
  for (auto& plan : _plans.list) {
    plan.first->invalidateCost();
    plan.first->getCost();
  }

  std::sort(_plans.list.begin(), _plans.list.end(),
            [](PlanList::Entry const& a, PlanList::Entry const& b) -> bool {
              return a.first->getCost().estimatedCost <
                     b.first->getCost().estimatedCost;
            });
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

void Optimizer::disableRule(ExecutionPlan* plan, std::string_view name) {
  if (name.starts_with('-')) {
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

void Optimizer::enableRule(ExecutionPlan* plan, std::string_view name) {
  if (name.starts_with('+')) {
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
  TRI_ASSERT(b.isOpenObject());
  b.add("rulesExecuted", velocypack::Value(rulesExecuted));
  b.add("rulesSkipped", velocypack::Value(rulesSkipped));
  b.add("plansCreated", velocypack::Value(plansCreated));

  if (executionTimes != nullptr) {
    b.add("rules", velocypack::Value(velocypack::ValueType::Object));
    for (auto const& it : *executionTimes) {
      b.add(OptimizerRulesFeature::translateRule(it.first),
            velocypack::Value(it.second));
    }
    b.close();
  } else {
    b.add("rules", velocypack::Slice::emptyObjectSlice());
  }
}

void Optimizer::Stats::toVelocyPackForCachedPlan(velocypack::Builder& b) {
  TRI_ASSERT(b.isOpenObject());
  b.add("rulesExecuted", velocypack::Value(0));
  b.add("rulesSkipped", velocypack::Value(0));
  b.add("plansCreated", velocypack::Value(1));
  // note: "rules" object must be present because otherwise the explainer
  // will not show all plan details
  b.add("rules", velocypack::Slice::emptyObjectSlice());
}

enum class DistributeType { DOCUMENT, TRAVERSAL, PATH };
using EN = arangodb::aql::ExecutionNode;

namespace {

bool willUseV8(arangodb::aql::ExecutionPlan const& plan) {
  struct V8Checker
      : arangodb::aql::WalkerWorkerBase<arangodb::aql::ExecutionNode> {
    bool before(arangodb::aql::ExecutionNode* n) override {
      if (n->getType() == arangodb::aql::ExecutionNode::CALCULATION &&
          static_cast<arangodb::aql::CalculationNode*>(n)
              ->expression()
              ->willUseV8()) {
        result = true;
        return true;
      }
      return false;
    }
    bool result{false};
  } walker{};
  plan.root()->walk(walker);
  return walker.result;
}

}  // namespace

void arangodb::aql::activateCallstackSplit(ExecutionPlan& plan) {
  if (willUseV8(plan)) {
    // V8 requires thread local context configuration, so we cannot
    // use our thread based split solution...
    return;
  }

  auto const& options = plan.getAst()->query().queryOptions();
  struct CallstackSplitter : WalkerWorkerBase<ExecutionNode> {
    explicit CallstackSplitter(size_t maxNodes)
        : maxNodesPerCallstack(maxNodes) {}
    bool before(ExecutionNode* n) override {
      // This rule must be executed after subquery splicing, so we must not see
      // any subqueries here!
      TRI_ASSERT(n->getType() != EN::SUBQUERY);

      if (n->getType() == EN::REMOTE) {
        // RemoteNodes provide a natural split in the callstack, so we can reset
        // the counter here!
        count = 0;
      } else if (++count >= maxNodesPerCallstack) {
        count = 0;
        n->enableCallstackSplit();
      }
      return false;
    }
    size_t maxNodesPerCallstack;
    size_t count = 0;
  };

  CallstackSplitter walker(options.maxNodesPerCallstack);
  plan.root()->walk(walker);
}

void arangodb::aql::insertDistributeInputCalculation(ExecutionPlan& plan) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan.findNodesOfType(nodes, ExecutionNode::DISTRIBUTE, true);

  for (auto const& n : nodes) {
    auto* distributeNode = ExecutionNode::castTo<DistributeNode*>(n);
    auto* targetNode =
        plan.getNodesById().at(distributeNode->getTargetNodeId());
    TRI_ASSERT(targetNode != nullptr);

    auto collection = static_cast<Collection const*>(nullptr);
    auto inputVariable = static_cast<Variable const*>(nullptr);
    auto alternativeVariable = static_cast<Variable const*>(nullptr);

    auto createKeys = bool{false};
    auto allowKeyConversionToObject = bool{false};
    auto allowSpecifiedKeys = bool{false};
    bool canProjectOnlyId{false};

    DistributeType fixupGraphInput = DistributeType::DOCUMENT;

    std::function<void(Variable * variable)> setInVariable;
    std::function<void(Variable * variable)> setTargetVariable;
    std::function<void(Variable * variable)> setDistributeVariable;
    bool ignoreErrors = false;

    // TODO: this seems a bit verbose, but is at least local & simple
    //       the modification nodes are all collectionaccessing, the graph nodes
    //       are currently assumed to be disjoint, and hence smart, so all
    //       collections are sharded the same way!
    switch (targetNode->getType()) {
      case ExecutionNode::INSERT: {
        auto* insertNode = ExecutionNode::castTo<InsertNode*>(targetNode);
        collection = insertNode->collection();
        inputVariable = insertNode->inVariable();
        createKeys = true;
        allowKeyConversionToObject = true;
        setInVariable = [insertNode](Variable* var) {
          insertNode->setInVariable(var);
        };

        alternativeVariable = insertNode->oldSmartGraphVariable();
        if (alternativeVariable != nullptr) {
          canProjectOnlyId = true;
        }

      } break;
      case ExecutionNode::REMOVE: {
        auto* removeNode = ExecutionNode::castTo<RemoveNode*>(targetNode);
        collection = removeNode->collection();
        inputVariable = removeNode->inVariable();
        createKeys = false;
        allowKeyConversionToObject = true;
        ignoreErrors = removeNode->getOptions().ignoreErrors;
        setInVariable = [removeNode](Variable* var) {
          removeNode->setInVariable(var);
        };
      } break;
      case ExecutionNode::UPDATE:
      case ExecutionNode::REPLACE: {
        auto* updateReplaceNode =
            ExecutionNode::castTo<UpdateReplaceNode*>(targetNode);
        collection = updateReplaceNode->collection();
        ignoreErrors = updateReplaceNode->getOptions().ignoreErrors;
        if (updateReplaceNode->inKeyVariable() != nullptr) {
          inputVariable = updateReplaceNode->inKeyVariable();
          // This is the _inKeyVariable! This works, since we use default
          // sharding!
          allowKeyConversionToObject = true;
          setInVariable = [updateReplaceNode](Variable* var) {
            updateReplaceNode->setInKeyVariable(var);
          };
        } else {
          inputVariable = updateReplaceNode->inDocVariable();
          allowKeyConversionToObject = false;
          setInVariable = [updateReplaceNode](Variable* var) {
            updateReplaceNode->setInDocVariable(var);
          };
        }
        createKeys = false;
      } break;
      case ExecutionNode::UPSERT: {
        // an UPSERT node has two input variables!
        auto* upsertNode = ExecutionNode::castTo<UpsertNode*>(targetNode);
        collection = upsertNode->collection();
        inputVariable = upsertNode->inDocVariable();
        alternativeVariable = upsertNode->insertVariable();
        ignoreErrors = upsertNode->getOptions().ignoreErrors;
        allowKeyConversionToObject = true;
        createKeys = true;
        allowSpecifiedKeys = true;
        setInVariable = [upsertNode](Variable* var) {
          upsertNode->setInsertVariable(var);
        };
      } break;
      case ExecutionNode::TRAVERSAL: {
        auto* traversalNode = ExecutionNode::castTo<TraversalNode*>(targetNode);
        TRI_ASSERT(traversalNode->isDisjoint());
        collection = traversalNode->collection();
        inputVariable = traversalNode->inVariable();
        allowKeyConversionToObject = true;
        createKeys = false;
        fixupGraphInput = DistributeType::TRAVERSAL;
        setInVariable = [traversalNode](Variable* var) {
          traversalNode->setInVariable(var);
        };
      } break;
      case ExecutionNode::ENUMERATE_PATHS: {
        auto* pathsNode =
            ExecutionNode::castTo<EnumeratePathsNode*>(targetNode);
        TRI_ASSERT(pathsNode->isDisjoint());
        collection = pathsNode->collection();
        // Subtle: EnumeratePathsNode uses a reference when returning
        // startInVariable
        TRI_ASSERT(pathsNode->usesStartInVariable());
        inputVariable = &pathsNode->startInVariable();
        TRI_ASSERT(pathsNode->usesTargetInVariable());
        alternativeVariable = &pathsNode->targetInVariable();
        allowKeyConversionToObject = true;
        createKeys = false;
        fixupGraphInput = DistributeType::PATH;
        setInVariable = [pathsNode](Variable* var) {
          pathsNode->setStartInVariable(var);
        };
        setTargetVariable = [pathsNode](Variable* var) {
          pathsNode->setTargetInVariable(var);
        };
        setDistributeVariable = [pathsNode](Variable* var) {
          pathsNode->setDistributeVariable(var);
        };
      } break;
      case ExecutionNode::SHORTEST_PATH: {
        auto* shortestPathNode =
            ExecutionNode::castTo<ShortestPathNode*>(targetNode);
        TRI_ASSERT(shortestPathNode->isDisjoint());
        collection = shortestPathNode->collection();
        TRI_ASSERT(shortestPathNode->usesStartInVariable());
        inputVariable = shortestPathNode->startInVariable();
        TRI_ASSERT(shortestPathNode->usesTargetInVariable());
        alternativeVariable = shortestPathNode->targetInVariable();
        allowKeyConversionToObject = true;
        createKeys = false;
        fixupGraphInput = DistributeType::PATH;
        setInVariable = [shortestPathNode](Variable* var) {
          shortestPathNode->setStartInVariable(var);
        };
        setTargetVariable = [shortestPathNode](Variable* var) {
          shortestPathNode->setTargetInVariable(var);
        };
        setDistributeVariable = [shortestPathNode](Variable* var) {
          shortestPathNode->setDistributeVariable(var);
        };
        break;
      }
      default: {
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, absl::StrCat("Cannot distribute ",
                                             targetNode->getTypeString(), "."));
      }
    }
    TRI_ASSERT(inputVariable != nullptr);
    TRI_ASSERT(collection != nullptr);
    // allowSpecifiedKeys can only be true for UPSERT
    TRI_ASSERT(targetNode->getType() == ExecutionNode::UPSERT ||
               !allowSpecifiedKeys);
    // createKeys can only be true for INSERT/UPSERT
    TRI_ASSERT((targetNode->getType() == ExecutionNode::INSERT ||
                targetNode->getType() == ExecutionNode::UPSERT) ||
               !createKeys);

    CalculationNode* calcNode = nullptr;
    auto setter = plan.getVarSetBy(inputVariable->id);
    if (setter == nullptr ||  // this can happen for $smartHandOver
        setter->getType() == EN::ENUMERATE_COLLECTION ||
        setter->getType() == EN::INDEX) {
      // If our input variable is set by a collection/index enumeration, it is
      // guaranteed to be an object with a _key attribute, so we don't need to
      // do anything.
      if (!createKeys || collection->usesDefaultSharding()) {
        // no need to insert an extra calculation node in this case.
        return;
      }
      // in case we have a collection that is not sharded by _key,
      // the keys need to be created/validated by the coordinator.
    }

    auto* ast = plan.getAst();
    auto args = ast->createNodeArray();
    char const* function;
    args->addMember(ast->createNodeReference(inputVariable));
    switch (fixupGraphInput) {
      case DistributeType::TRAVERSAL:
      case DistributeType::PATH: {
        function = "MAKE_DISTRIBUTE_GRAPH_INPUT";
        break;
      }
      case DistributeType::DOCUMENT: {
        if (createKeys) {
          function = "MAKE_DISTRIBUTE_INPUT_WITH_KEY_CREATION";
          if (alternativeVariable) {
            args->addMember(ast->createNodeReference(alternativeVariable));
          } else {
            args->addMember(ast->createNodeValueNull());
          }
          auto flags = ast->createNodeObject();
          flags->addMember(ast->createNodeObjectElement(
              "allowSpecifiedKeys",
              ast->createNodeValueBool(allowSpecifiedKeys)));
          flags->addMember(ast->createNodeObjectElement(
              "ignoreErrors", ast->createNodeValueBool(ignoreErrors)));
          flags->addMember(ast->createNodeObjectElement(
              "projectOnlyId", ast->createNodeValueBool(canProjectOnlyId)));
          auto const& collectionName = collection->name();
          flags->addMember(ast->createNodeObjectElement(
              "collection",
              ast->createNodeValueString(collectionName.c_str(),
                                         collectionName.length())));

          args->addMember(flags);
        } else {
          function = "MAKE_DISTRIBUTE_INPUT";
          auto flags = ast->createNodeObject();
          flags->addMember(ast->createNodeObjectElement(
              "allowKeyConversionToObject",
              ast->createNodeValueBool(allowKeyConversionToObject)));
          flags->addMember(ast->createNodeObjectElement(
              "ignoreErrors", ast->createNodeValueBool(ignoreErrors)));
          bool canUseCustomKey =
              collection->getCollection()->usesDefaultShardKeys() ||
              allowSpecifiedKeys;
          flags->addMember(ast->createNodeObjectElement(
              "canUseCustomKey", ast->createNodeValueBool(canUseCustomKey)));

          args->addMember(flags);
        }
      }
    }

    if (fixupGraphInput == DistributeType::PATH) {
      // We need to insert two additional calculation nodes
      // on for source, one for target.
      // Both nodes are then piped into the SelectSmartDistributeGraphInput
      // which selects the smart input side.

      Variable* sourceVariable =
          plan.getAst()->variables()->createTemporaryVariable();
      auto sourceExpr = std::make_unique<Expression>(
          ast, ast->createNodeFunctionCall(function, args, true));
      auto sourceCalcNode = plan.createNode<CalculationNode>(
          &plan, plan.nextId(), std::move(sourceExpr), sourceVariable);

      Variable* targetVariable =
          plan.getAst()->variables()->createTemporaryVariable();
      auto targetArgs = ast->createNodeArray();
      TRI_ASSERT(alternativeVariable != nullptr);
      targetArgs->addMember(ast->createNodeReference(alternativeVariable));
      TRI_ASSERT(args->numMembers() == targetArgs->numMembers());
      auto targetExpr = std::make_unique<Expression>(
          ast, ast->createNodeFunctionCall(function, targetArgs, true));
      auto targetCalcNode = plan.createNode<CalculationNode>(
          &plan, plan.nextId(), std::move(targetExpr), targetVariable);

      // update the target node with in and out variables
      setInVariable(sourceVariable);
      setTargetVariable(targetVariable);

      auto selectInputArgs = ast->createNodeArray();
      selectInputArgs->addMember(ast->createNodeReference(sourceVariable));
      selectInputArgs->addMember(ast->createNodeReference(targetVariable));

      Variable* variable =
          plan.getAst()->variables()->createTemporaryVariable();
      auto expr = std::make_unique<Expression>(
          ast,
          ast->createNodeFunctionCall("SELECT_SMART_DISTRIBUTE_GRAPH_INPUT",
                                      selectInputArgs, true));
      calcNode = plan.createNode<CalculationNode>(&plan, plan.nextId(),
                                                  std::move(expr), variable);
      distributeNode->setVariable(variable);
      setDistributeVariable(variable);
      // Inject the calculations before the distributeNode
      plan.insertBefore(distributeNode, sourceCalcNode);
      plan.insertBefore(distributeNode, targetCalcNode);
    } else {
      // We insert an additional calculation node to create the input for our
      // distribute node.
      Variable* variable =
          plan.getAst()->variables()->createTemporaryVariable();

      // update the targetNode so that it uses the same input variable as our
      // distribute node
      setInVariable(variable);

      auto expr = std::make_unique<Expression>(
          ast, ast->createNodeFunctionCall(function, args, true));
      calcNode = plan.createNode<CalculationNode>(&plan, plan.nextId(),
                                                  std::move(expr), variable);
      distributeNode->setVariable(variable);
    }

    plan.insertBefore(distributeNode, calcNode);
    plan.clearVarUsageComputed();
    plan.findVarUsage();
  }
}
