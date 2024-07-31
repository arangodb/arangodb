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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBComparator.h"

#include "Basics/system-compiler.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBPrefixExtractor.h"
#include "RocksDBEngine/RocksDBTypes.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

namespace {

template<arangodb::basics::VelocyPackHelper::SortingMethod sortingMethod>
int compareIndexedValues(arangodb::velocypack::Slice const& lhs,
                         arangodb::velocypack::Slice const& rhs) {
  TRI_ASSERT(lhs.isArray());
  TRI_ASSERT(rhs.isArray());

  arangodb::velocypack::ArrayIterator lhsIter(lhs);
  arangodb::velocypack::ArrayIterator rhsIter(rhs);

  do {
    bool lhsValid = lhsIter.valid();
    bool rhsValid = rhsIter.valid();

    if (!lhsValid && !rhsValid) {
      return static_cast<int>(lhsIter.size() - rhsIter.size());
    }

    int res;
    if constexpr (sortingMethod ==
                  arangodb::basics::VelocyPackHelper::SortingMethod::Legacy) {
      res = arangodb::basics::VelocyPackHelper::compareLegacy(
          (lhsValid ? *lhsIter : VPackSlice::noneSlice()),
          (rhsValid ? *rhsIter : VPackSlice::noneSlice()), true);
    } else {
      res = arangodb::basics::VelocyPackHelper::compare(
          (lhsValid ? *lhsIter : VPackSlice::noneSlice()),
          (rhsValid ? *rhsIter : VPackSlice::noneSlice()), true);
    }
    if (res != 0) {
      return res;
    }

    ++lhsIter;
    ++rhsIter;
  } while (true);
}

}  // namespace

namespace arangodb {

template<arangodb::basics::VelocyPackHelper::SortingMethod sortingMethod>
int RocksDBVPackComparator<sortingMethod>::compareIndexValues(
    rocksdb::Slice const& lhs, rocksdb::Slice const& rhs) const {
  constexpr size_t objectIDLength = RocksDBKey::objectIdSize();

  int r = memcmp(lhs.data(), rhs.data(), objectIDLength);

  if (r != 0) {
    // different object ID
    return r;
  }

  if (ADB_UNLIKELY(lhs.size() == objectIDLength ||
                   rhs.size() == objectIDLength)) {
    if (lhs.size() == rhs.size()) {
      return 0;
    }
    return ((lhs.size() < rhs.size()) ? -1 : 1);
  }

  TRI_ASSERT(lhs.size() > sizeof(uint64_t));
  TRI_ASSERT(rhs.size() > sizeof(uint64_t));

  VPackSlice const lSlice = VPackSlice(
      reinterpret_cast<uint8_t const*>(lhs.data()) + sizeof(uint64_t));
  VPackSlice const rSlice = VPackSlice(
      reinterpret_cast<uint8_t const*>(rhs.data()) + sizeof(uint64_t));

  r = ::compareIndexedValues<sortingMethod>(lSlice, rSlice);

  if (r != 0) {
    // comparison of index values produced an unambiguous result
    return r;
  }

  // index values were identical. now compare the leftovers (which is the
  // LocalDocumentId for non-unique indexes)
  // For the mdi, there is also the curve data, following the vpack.

  constexpr size_t offset = sizeof(uint64_t);
  size_t const lOffset = offset + static_cast<size_t>(lSlice.byteSize());
  size_t const rOffset = offset + static_cast<size_t>(rSlice.byteSize());
  size_t const lSize = lhs.size() - lOffset;
  size_t const rSize = rhs.size() - rOffset;
  size_t minSize = std::min(lSize, rSize);

  if (minSize > 0) {
    char const* lBase = lhs.data() + lOffset;
    char const* rBase = rhs.data() + rOffset;
    r = memcmp(lBase, rBase, minSize);
    if (r != 0) {
      return r;
    }
  }

  return static_cast<int>(lSize - rSize);
}

// Now explicitly instantiate the two cases:
template class RocksDBVPackComparator<
    arangodb::basics::VelocyPackHelper::SortingMethod::Legacy>;
template class RocksDBVPackComparator<
    arangodb::basics::VelocyPackHelper::SortingMethod::Correct>;

}  // namespace arangodb
