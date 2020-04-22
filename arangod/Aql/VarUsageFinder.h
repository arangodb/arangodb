////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_VAR_USAGE_FINDER_H
#define ARANGOD_AQL_VAR_USAGE_FINDER_H 1

#include "Aql/WalkerWorker.h"

namespace arangodb::aql {

/// @brief helper struct for findVarUsage

template <class T>
struct VarUsageFinder final : public WalkerWorker<T> {
  ::arangodb::containers::HashSet<Variable const*> _usedLater;
  ::arangodb::containers::HashSet<Variable const*> _valid;
  std::unordered_map<VariableId, T*>* _varSetBy;
  bool const _ownsVarSetBy;

  VarUsageFinder(VarUsageFinder const&) = delete;
  VarUsageFinder& operator=(VarUsageFinder const&) = delete;

  VarUsageFinder() : _varSetBy(nullptr), _ownsVarSetBy(true) {
    _varSetBy = new std::unordered_map<VariableId, T*>();
  }

  explicit VarUsageFinder(std::unordered_map<VariableId, T*>* varSetBy)
      : _varSetBy(varSetBy), _ownsVarSetBy(false) {
    TRI_ASSERT(_varSetBy != nullptr);
  }

  ~VarUsageFinder() {
    if (_ownsVarSetBy) {
      TRI_ASSERT(_varSetBy != nullptr);
      delete _varSetBy;
    }
  }

  bool before(T* en) override final {
    // count the type of node found
    en->plan()->increaseCounter(en->getType());

    en->invalidateVarUsage();
    en->setVarsUsedLater(_usedLater);
    // Add variables used here to _usedLater:

    en->getVariablesUsedHere(_usedLater);

    return false;
  }

  void after(T* en) override final {
    // Add variables set here to _valid:
    for (auto const& v : en->getVariablesSetHere()) {
      _valid.insert(v);
      _varSetBy->insert({v->id, en});
    }

    en->setVarsValid(_valid);
    en->setVarUsageValid();
  }

  bool enterSubquery(T*, T* sub) override final {
    VarUsageFinder subfinder(_varSetBy);
    subfinder._valid = _valid;  // need a copy for the subquery!
    sub->walk(subfinder);

    // we've fully processed the subquery
    return false;
  }
};

}  // namespace arangodb::aql

#endif