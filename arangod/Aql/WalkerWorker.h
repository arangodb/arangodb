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

#ifndef ARANGOD_AQL_WALKER_WORKER_H
#define ARANGOD_AQL_WALKER_WORKER_H 1

#include "Aql/types.h"
#include "Basics/Common.h"
#include "Basics/debugging.h"
#include "Containers/HashSet.h"

namespace arangodb {
namespace aql {

enum class WalkerUniqueness : std::uint8_t { Unique, NonUnique };

/// @brief base interface to walk an execution plan recursively.
template <class T>
class WalkerWorkerBase {
 public:
  WalkerWorkerBase() = default;

  virtual ~WalkerWorkerBase() = default;

  /// @brief return true to abort walking, false otherwise
  virtual bool before(T*) {
    return false;  // true to abort the whole walking process
  }

  virtual void after(T*) {}

  /// @brief return true to enter subqueries, false otherwise
  virtual bool enterSubquery(T* /*super*/, T* /*sub*/) { return true; }

  virtual void leaveSubquery(T* /*super*/, T* /*sub*/) {}

  virtual bool done(T* /*en*/) { return false; }
};

/// @brief functionality to walk an execution plan recursively.
/// if template parameter `unique == true`, this will visit each node once, even
/// if multiple paths lead to the same node. no assertions are raised if
/// multiple paths lead to the same node
template <class T, WalkerUniqueness U>
class WalkerWorker : public WalkerWorkerBase<T> {
 public:
  virtual bool done([[maybe_unused]] T* en) override {
    if constexpr (U == WalkerUniqueness::Unique) {
      return !_done.emplace(en).second;
    }

    // this is a no-op in non-maintainer mode
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // make sure a node is only processed once
    if (_done.emplace(en).second) {
      return false;
    }

    // should never happen
    TRI_ASSERT(false);

    return true;
#else
    return false;
#endif
  }

  void reset() {
    if constexpr (U == WalkerUniqueness::Unique) {
      _done.clear();
      return;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _done.clear();
#endif
  }

 private:
  ::arangodb::containers::HashSet<T*> _done;
};


}  // namespace aql
}  // namespace arangodb

#endif
