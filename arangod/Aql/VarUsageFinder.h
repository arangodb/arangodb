////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_VAR_USAGE_FINDER_H
#define ARANGOD_AQL_VAR_USAGE_FINDER_H 1

#include "Aql/Variable.h"
#include "Aql/WalkerWorker.h"

#include <Containers/Enumerate.h>

#include <vector>
#include <unordered_set>

namespace arangodb::aql {

class ExecutionNode;

/// @brief helper struct for findVarUsage

template <class T>
struct VarUsageFinderT;

using VarUsageFinder = VarUsageFinderT<ExecutionNode>;

template <class T>
struct VarUsageFinderT final : public WalkerWorker<T, WalkerUniqueness::NonUnique> {
  VarSetStack _usedLaterStack{VarSet{}};
  VarSetStack _varsValidStack{VarSet{}};

  std::unordered_map<VariableId, T*>* _varSetBy;
  bool const _ownsVarSetBy;

  VarUsageFinderT(VarUsageFinderT const&) = delete;
  VarUsageFinderT& operator=(VarUsageFinderT const&) = delete;

  VarUsageFinderT() : _varSetBy(nullptr), _ownsVarSetBy(true) {
    _varSetBy = new std::unordered_map<VariableId, T*>();
  }

  explicit VarUsageFinderT(std::unordered_map<VariableId, T*>* varSetBy)
      : _varSetBy(varSetBy), _ownsVarSetBy(false) {
    TRI_ASSERT(_varSetBy != nullptr);
  }

  ~VarUsageFinderT() {
    if (_ownsVarSetBy) {
      TRI_ASSERT(_varSetBy != nullptr);
      delete _varSetBy;
    }
  }

  bool before(T* en) final;

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

  bool enterSubquery(T*, T*) final;
};

}  // namespace arangodb::aql

#endif
