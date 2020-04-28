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

#include <vector>
#include <unordered_set>
#include <Containers/Enumerate.h>
#include "Aql/Variable.h"
#include "Aql/WalkerWorker.h"

namespace arangodb::aql {

/// @brief helper struct for findVarUsage

template <class T>
struct VarUsageFinder final : public WalkerWorker<T> {
  using Stack = std::vector<std::unordered_set<Variable const*>>;

  Stack _usedLaterStack = Stack{{}};
  Stack _varsValidStack = Stack{{}};
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

  bool before(T* en) override final;

  /*
   * o  set: x, z   valid = x, z  usedLater = (z, x)
   * |\
   * | \ clear z
   * |  o set: y    valid = x, y, z   usedLater = (z, x)
   * |cx| clear y
   * |  |
   * |  o used: x   valid = x, y, z   usedLater = ((), ())
   * | / clear x
   * |/
   * o used: z   valid = x, z  usedLater = (z)
   * |
   * o used: z   usedLater = (...)
   */

  void after(T* en) override final;

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
