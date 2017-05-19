////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBComparator.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBPrefixExtractor.h"
#include "RocksDBEngine/RocksDBTypes.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

RocksDBComparator::RocksDBComparator() {}

RocksDBComparator::~RocksDBComparator() {}

inline static VPackSlice indexedVPack(rocksdb::Slice const& slice) {
  TRI_ASSERT(slice.size() > (sizeof(char) + sizeof(uint64_t)));
  return VPackSlice(slice.data() + sizeof(char) + sizeof(uint64_t));
}

int RocksDBComparator::compareIndexValues(rocksdb::Slice const& lhs,
                                          rocksdb::Slice const& rhs) const {
  TRI_ASSERT(RocksDBKey::type(lhs) == RocksDBEntryType::IndexValue
             || RocksDBKey::type(lhs) == RocksDBEntryType::UniqueIndexValue);
  TRI_ASSERT(RocksDBKey::type(rhs) == RocksDBKey::type(rhs));
  
  constexpr size_t prefixLength = RocksDBPrefixExtractor::getIndexPrefixLength();
  int r = memcmp(lhs.data(), rhs.data(), prefixLength);
  if (r != 0) {
    return r;
  }
  if (lhs.size() == prefixLength || rhs.size() == prefixLength) {
    if (lhs.size() == rhs.size()) {
      return 0;
    }

    return ((lhs.size() < rhs.size()) ? -1 : 1);
  }

  TRI_ASSERT(lhs.size() > sizeof(char) + sizeof(uint64_t));
  TRI_ASSERT(rhs.size() > sizeof(char) + sizeof(uint64_t));

  VPackSlice const lSlice = indexedVPack(lhs);
  VPackSlice const rSlice = indexedVPack(rhs);

  r = compareIndexedValues(lSlice, rSlice);
  if (r != 0) {
    return r;
  }

  constexpr size_t offset = sizeof(char) + sizeof(uint64_t);
  size_t lOffset = offset + static_cast<size_t>(lSlice.byteSize());
  size_t rOffset = offset + static_cast<size_t>(rSlice.byteSize());
  char const* lBase = lhs.data() + lOffset;
  char const* rBase = rhs.data() + rOffset;
  size_t lSize = lhs.size() - lOffset;
  size_t rSize = rhs.size() - rOffset;
  size_t minSize = ((lSize <= rSize) ? lSize : rSize);

  if (minSize > 0) {
    r = memcmp(lBase, rBase, minSize);
    if (r != 0) {
      return r;
    }
  }

  if (lSize == rSize) {
    return 0;
  }

  return ((lSize < rSize) ? -1 : 1);
}

int RocksDBComparator::compareIndexedValues(VPackSlice const& lhs,
                                            VPackSlice const& rhs) const {
  TRI_ASSERT(lhs.isArray());
  TRI_ASSERT(rhs.isArray());

  VPackArrayIterator lhsIter(lhs);
  VPackArrayIterator rhsIter(rhs);
  size_t const lLength = lhsIter.size();
  size_t const rLength = rhsIter.size();
  
  while (lhsIter.valid() || rhsIter.valid()) {
    size_t i = lhsIter.index();
    int res = arangodb::basics::VelocyPackHelper::compare(
                                                          (i < lLength ? *lhsIter : VPackSlice::noneSlice()),
                                                          (i < rLength ? *rhsIter : VPackSlice::noneSlice()), true);
    if (res != 0) {
      return res;
    }
    ++lhsIter;
    ++rhsIter;
  }

  if (lLength != rLength) {
    return lLength < rLength ? -1 : 1;
  }

  return 0;
}
