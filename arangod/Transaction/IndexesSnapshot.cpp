////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "IndexesSnapshot.h"

#include "Indexes/Index.h"
#include "StorageEngine/PhysicalCollection.h"

namespace arangodb {

IndexesSnapshot::IndexesSnapshot(PhysicalCollection& collection)
    : _locker(collection._indexesLock, collection._indexesLockWriteOwner,
              __FILE__, __LINE__),
      _valid(true) {
  _indexes.reserve(collection._indexes.size());
  for (auto const& idx : collection._indexes) {
    _indexes.emplace_back(idx);
  }
  // in unit tests, there can be 0 indexes in a snapshot...
  TRI_ASSERT(_indexes.empty() ||
             _indexes[0]->type() == Index::TRI_IDX_TYPE_PRIMARY_INDEX);
}

IndexesSnapshot::~IndexesSnapshot() = default;

std::vector<std::shared_ptr<Index>> const& IndexesSnapshot::getIndexes()
    const noexcept {
  TRI_ASSERT(_valid);
  return _indexes;
}

void IndexesSnapshot::release() {
  // once we call this function, we give up all our locks, and we shouldn't
  // access the list of indexes anymore
  _valid = false;
  _locker.unlock();
}

bool IndexesSnapshot::hasSecondaryIndex() const noexcept {
  TRI_ASSERT(_valid);
  return _indexes.size() > 1;
}

}  // namespace arangodb
