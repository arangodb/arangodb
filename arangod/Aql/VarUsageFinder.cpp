#include "Aql/VarUsageFinder.h"
#include "Aql/ExecutionNode.h"
#include <Logger/LogMacros.h>

using namespace arangodb::aql;

template <class T>
bool VarUsageFinder<T>::before(T* en) {
  // count the type of node found
  en->plan()->increaseCounter(en->getType());

  en->invalidateVarUsage();
  en->setVarsUsedLater(usedLaterStack);
  //en->setVarsUsedLater(_usedLater);

  switch (en->getType()) {
    case ExecutionNode::SUBQUERY_END: {
      // auto* node  = ExecutionNode::castTo<SubqueryEndNode*>(en);
      usedLaterStack.emplace_back(std::unordered_set<Variable const*>{});
      break;
    }

    case ExecutionNode::SUBQUERY_START: {
      // auto* node  = ExecutionNode::castTo<SubqueryStartNode*>(en);

      auto top = std::move(usedLaterStack.back());
      usedLaterStack.pop_back();
      TRI_ASSERT(!usedLaterStack.empty());
      usedLaterStack.back().merge(top);
      break;
    }

    default:
      break;
  }

  LOG_DEVEL "VarUsageFinder::before " << en->getTypeString() << ":"
                                      << en->id() << " vars used later: ";
  for (auto const [idx, level] : enumerate(usedLaterStack)) {
    LOG_DEVEL << "Level " << idx;
    for (auto const& var : level) {
      LOG_DEVEL << "Variable " << var->id << " name = " << var->name;
    }
  }

  // Add variables used here to _usedLater:
  en->getVariablesUsedHere(usedLaterStack.back());
  /*en->getVariablesUsedHere(_usedLater);
  {
    ::arangodb::containers::HashSet<Variable const*> dummy;
    en->getVariablesUsedHere(dummy);
    for (auto const var : dummy) {
      usedLaterStack.back().emplace(var);
    }
  }*/

  return false;
}

template <class T>
void VarUsageFinder<T>::after(T* en) {
  LOG_DEVEL "VarUsageFinder::after " << en->getTypeString() << ":" << en->id()
                                     << " vars valid here: ";

  // Add variables set here to _valid:
  for (auto const& v : en->getVariablesSetHere()) {
    _valid.insert(v);
    _varSetBy->insert({v->id, en});
  }

  for (auto const var : _valid) {
    LOG_DEVEL << var->id << " name = " << var->name;
  }

  en->setVarsValid(_valid);
  en->setVarUsageValid();
}
