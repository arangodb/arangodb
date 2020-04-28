#include "Aql/VarUsageFinder.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include <Logger/LogMacros.h>

using namespace arangodb::aql;

template <class T>
bool VarUsageFinderT<T>::before(T* en) {
  // count the type of node found
  en->plan()->increaseCounter(en->getType());

  en->invalidateVarUsage();
  en->setVarsUsedLater(_usedLaterStack);
  switch (en->getType()) {
    case ExecutionNode::SUBQUERY_END: {
      _usedLaterStack.emplace_back(std::unordered_set<Variable const*>{});
      break;
    }

    case ExecutionNode::SUBQUERY_START: {
      auto top = std::move(_usedLaterStack.back());
      _usedLaterStack.pop_back();
      TRI_ASSERT(!_usedLaterStack.empty());
      _usedLaterStack.back().merge(top);
      break;
    }

    default:
      break;
  }

  // Add variables used here to _usedLater:
  en->getVariablesUsedHere(_usedLaterStack.back());

  return false;
}

template <class T>
void VarUsageFinderT<T>::after(T* en) {
  switch (en->getType()) {
    case ExecutionNode::SUBQUERY_START: {
      _varsValidStack.emplace_back(_varsValidStack.back());
      break;
    }

    case ExecutionNode::SUBQUERY_END: {
      _varsValidStack.pop_back();
      break;
    }

    default:
      break;
  }

  // Add variables set here to _valid:
  for (auto const& v : en->getVariablesSetHere()) {
    _varsValidStack.back().emplace(v);
    _varSetBy->insert({v->id, en});
  }

  en->setVarsValid(_varsValidStack);
  en->setVarUsageValid();
}

template struct arangodb::aql::VarUsageFinderT<ExecutionNode>;
