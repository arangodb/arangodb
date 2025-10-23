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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ResourceUsage.h"
#include "Basics/StringHeap.h"
#include "Containers/FlatHashSet.h"

#include <velocypack/HashedStringRef.h>

namespace arangodb::graph {

/// Small wrapper around the actual datastore in
/// which edges and vertices are stored.
/// This db server variant can just work with raw
/// document tokens and retrieve documents as needed

class RefactoredTraverserCache {
 public:
  RefactoredTraverserCache(arangodb::ResourceMonitor& resourceMonitor);

  ~RefactoredTraverserCache();

  RefactoredTraverserCache(RefactoredTraverserCache const&) = delete;
  RefactoredTraverserCache(RefactoredTraverserCache&&) = default;

  RefactoredTraverserCache& operator=(RefactoredTraverserCache const&) = delete;

  /// @brief clears all allocated memory in the underlying StringHeap
  void clear();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Persist the given id string. The return value is guaranteed to
  ///        stay valid as long as this cache is valid
  //////////////////////////////////////////////////////////////////////////////
  arangodb::velocypack::HashedStringRef persistString(
      arangodb::velocypack::HashedStringRef idString);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Stringheap to take care of _id strings, s.t. they stay valid
  ///        during the entire traversal.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::StringHeap _stringHeap;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Set of all strings persisted in the stringHeap. So we can save
  /// some
  ///        memory by not storing them twice.
  //////////////////////////////////////////////////////////////////////////////
  containers::FlatHashSet<arangodb::velocypack::HashedStringRef>
      _persistedStrings;

  arangodb::ResourceMonitor& _resourceMonitor;
};

}  // namespace arangodb::graph
