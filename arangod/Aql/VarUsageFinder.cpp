#include <Containers/HashSet.h>

#include "Aql/VarUsageFinder.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"


#include <Logger/LogMacros.h>

using namespace arangodb::aql;
using namespace arangodb::containers;

// TODO Subqueries have their own SubqueryVarUsageFinder, which is called in
//      getVariablesUsedHere(), and do a recursive walk to get the variables.
//      Then, we need to make another recursive walk of VarUsageFinder in
//      enterSubquery().
//      For nested subqueries, this is very inefficient. Plus we have some
//      duplicate logic which is harder to understand and maintain. We should
//      implement a (non-WalkerWorker-based) walk that fits the need here.
//      That is, walk upwards first, and also immediately recursively for
//      subqueries. After that walk downwards, and process subqueries downwards
//      when hitting them.

namespace {

/// @brief Merge everything from source into target. (emilib::HashSet does not
///        have a merge() method, nor extract()).
///        This behaves like
///          target.merge(source);
///        .
auto mergeInto(VarSet& target, VarSet const& source) {
  for (auto varPtr : source) {
    target.emplace(varPtr);
  }
}
}

template <class T>
bool VarUsageFinderT<T>::before(T* en) {
  // count the type of node found
  en->plan()->increaseCounter(en->getType());

  en->invalidateVarUsage();
  en->setVarsUsedLater(_usedLaterStack);
  switch (en->getType()) {
    case ExecutionNode::SUBQUERY_END: {
      _usedLaterStack.emplace_back(VarSet{});
      break;
    }

    case ExecutionNode::SUBQUERY_START: {
      auto top = std::move(_usedLaterStack.back());
      _usedLaterStack.pop_back();
      TRI_ASSERT(!_usedLaterStack.empty());
      // mergeInto is a replacement for merge(), as in
      //   _usedLaterStack.back().merge(top);
      // with similar behaviour, because emilib::HashSet does not have a merge
      // method.
      mergeInto(_usedLaterStack.back(), top);
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
      auto top = _varsValidStack.back();
      _varsValidStack.emplace_back(std::move(top));
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

template <class T>
bool VarUsageFinderT<T>::enterSubquery(T*, T* subqueryRootNode) {
  VarUsageFinderT subfinder(_varSetBy);
  // The subquery needs only the topmost varsValid entry, it must not see
  // the entries of outer (sub)queries.
  subfinder._varsValidStack = VarSetStack{_varsValidStack.back()};
  subqueryRootNode->walk(subfinder);

  return false;
}

template struct arangodb::aql::VarUsageFinderT<ExecutionNode>;
