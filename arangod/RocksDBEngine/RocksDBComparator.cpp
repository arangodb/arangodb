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

#include "RocksDBEngine/RocksDBComparator.h"
#include "Basics/VelocyPackHelper.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBTypes.h"

using namespace arangodb;
using namespace arangodb::velocypack;

RocksDBComparator::RocksDBComparator() : _name("ArangoRocksDBComparator") {}

RocksDBComparator::~RocksDBComparator() {}

const char* RocksDBComparator::Name() const { return _name.data(); };

int RocksDBComparator::Compare(rocksdb::Slice const& lhs,
                               rocksdb::Slice const& rhs) const {
  int result = compareType(lhs, rhs);
  if (result != 0) {
    return result;
  }

  RocksDBEntryType type = RocksDBKey::type(lhs);
  switch (type) {
    case RocksDBEntryType::IndexValue: {
      return compareIndexValues(lhs, rhs);
    }
    case RocksDBEntryType::UniqueIndexValue: {
      return compareUniqueIndexValues(lhs, rhs);
    }
    default: { return compareLexicographic(lhs, rhs); }
  }
}

int RocksDBComparator::compareType(rocksdb::Slice const& lhs,
                                   rocksdb::Slice const& rhs) const {
  return lhs[0] - rhs[0];  // type is first byte of every key
}

int RocksDBComparator::compareLexicographic(rocksdb::Slice const& lhs,
                                            rocksdb::Slice const& rhs) const {
  size_t minLength = (lhs.size() <= rhs.size()) ? lhs.size() : rhs.size();

  int result = memcmp(lhs.data(), rhs.data(), minLength);
  if (result != 0 || lhs.size() == rhs.size()) {
    return result;
  }

  return ((lhs.size() < rhs.size()) ? -1 : 1);
}

int RocksDBComparator::compareIndexValues(rocksdb::Slice const& lhs,
                                          rocksdb::Slice const& rhs) const {
  size_t offset = sizeof(char);
  int result =
      memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
  if (result != 0) {
    return result;
  }

  VPackSlice const lSlice = RocksDBKey::indexedVPack(lhs);
  VPackSlice const rSlice = RocksDBKey::indexedVPack(rhs);

  result = compareIndexedValues(lSlice, rSlice);
  if (result != 0) {
    return result;
  }

  offset += sizeof(uint64_t);
  size_t lOffset = offset + static_cast<size_t>(lSlice.byteSize());
  size_t rOffset = offset + static_cast<size_t>(rSlice.byteSize());
  char const* lBase = lhs.data() + lOffset;
  char const* rBase = rhs.data() + rOffset;
  size_t lSize = lhs.size() - lOffset;
  size_t rSize = rhs.size() - rOffset;
  size_t minSize = ((lSize <= rSize) ? lSize : rSize);

  result = memcmp(lBase, rBase, minSize);
  if (result != 0) {
    return result;
  }

  if (lSize == rSize) {
    return 0;
  }

  return ((lSize < rSize) ? -1 : 1);
}

int RocksDBComparator::compareUniqueIndexValues(
    rocksdb::Slice const& lhs, rocksdb::Slice const& rhs) const {
  size_t offset = sizeof(char);
  int result =
      memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
  if (result != 0) {
    return result;
  }

  VPackSlice const lSlice = RocksDBKey::indexedVPack(lhs);
  VPackSlice const rSlice = RocksDBKey::indexedVPack(rhs);

  return compareIndexedValues(lSlice, rSlice);
}

int RocksDBComparator::compareIndexedValues(VPackSlice const& lhs,
                                            VPackSlice const& rhs) const {
  TRI_ASSERT(lhs.isArray());
  TRI_ASSERT(rhs.isArray());

  size_t const lLength = lhs.length();
  size_t const rLength = rhs.length();
  size_t const n = lLength < rLength ? lLength : rLength;

  for (size_t i = 0; i < n; ++i) {
    int res = arangodb::basics::VelocyPackHelper::compare(
        (i < lLength ? lhs[i] : VPackSlice::noneSlice()),
        (i < rLength ? rhs[i] : VPackSlice::noneSlice()), true);

    if (res != 0) {
      return res;
    }
  }

  if (lLength != rLength) {
    return lLength < rLength ? -1 : 1;
  }

  return 0;
}
