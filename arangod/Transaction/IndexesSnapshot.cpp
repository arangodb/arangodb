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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "IndexesSnapshot.h"

#include "Indexes/Index.h"
#include "StorageEngine/PhysicalCollection.h"

namespace arangodb {

IndexesSnapshot::IndexesSnapshot(
    RecursiveReadLocker<basics::ReadWriteLock>&& locker,
    std::vector<std::shared_ptr<Index>> indexes)
    : _locker(std::move(locker)), _indexes(std::move(indexes)), _valid(true) {}

IndexesSnapshot::~IndexesSnapshot() = default;

// note: in unit tests, there can be whatever indexes (even no indexes)
// in an index snapshot
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
