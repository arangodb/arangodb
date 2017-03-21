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
#include "RocksDBEngine/RocksDBEntry.h"

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

  RocksDBEntry::Type type = static_cast<RocksDBEntry::Type>(lhs[0]);
  switch (type) {
    case RocksDBEntry::Type::Database: {
      return compareDatabases(lhs, rhs);
    }
    case RocksDBEntry::Type::Collection: {
      return compareCollections(lhs, rhs);
    }
    case RocksDBEntry::Type::Index: {
      return compareIndices(lhs, rhs);
    }
    case RocksDBEntry::Type::Document: {
      return compareDocuments(lhs, rhs);
    }
    case RocksDBEntry::Type::IndexValue: {
      return compareIndexValues(lhs, rhs);
    }
    case RocksDBEntry::Type::UniqueIndexValue: {
      return compareUniqueIndexValues(lhs, rhs);
    }
    case RocksDBEntry::Type::View: {
      return compareViews(lhs, rhs);
    }
    case RocksDBEntry::Type::CrossReference: {
      return compareCrossReferences(lhs, rhs);
    }
    default: { return compareLexicographic(lhs, rhs); }
  }
}

int RocksDBComparator::compareType(rocksdb::Slice const& lhs,
                                   rocksdb::Slice const& rhs) const {
  return lhs[0] - rhs[0];
}

int RocksDBComparator::compareDatabases(rocksdb::Slice const& lhs,
                                        rocksdb::Slice const& rhs) const {
  return memcmp((lhs.data() + sizeof(char)), (rhs.data() + sizeof(char)),
                sizeof(uint64_t));
}

int RocksDBComparator::compareCollections(rocksdb::Slice const& lhs,
                                          rocksdb::Slice const& rhs) const {
  size_t offset = sizeof(char);
  return memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
}

int RocksDBComparator::compareIndices(rocksdb::Slice const& lhs,
                                      rocksdb::Slice const& rhs) const {
  size_t offset = sizeof(char);
  return memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
}

int RocksDBComparator::compareDocuments(rocksdb::Slice const& lhs,
                                        rocksdb::Slice const& rhs) const {
  size_t offset = sizeof(char);
  int result =
      memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
  if (result != 0) {
    return result;
  }

  offset += sizeof(uint64_t);
  return memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
}

int RocksDBComparator::compareIndexValues(rocksdb::Slice const& lhs,
                                          rocksdb::Slice const& rhs) const {
  size_t offset = sizeof(char);
  int result =
      memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
  if (result != 0) {
    return result;
  }

  VPackSlice const lSlice = extractIndexedValues(lhs);
  VPackSlice const rSlice = extractIndexedValues(rhs);

  result = compareIndexedValues(lSlice, rSlice);
  if (result != 0) {
    return result;
  }

  char const* lBase = lhs.data() + (lhs.size() - sizeof(uint64_t));
  char const* rBase = rhs.data() + (rhs.size() - sizeof(uint64_t));
  return memcmp(lBase, rBase, sizeof(uint64_t));
}

int RocksDBComparator::compareUniqueIndexValues(
    rocksdb::Slice const& lhs, rocksdb::Slice const& rhs) const {
  size_t offset = sizeof(char);
  int result =
      memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
  if (result != 0) {
    return result;
  }

  VPackSlice const lSlice = extractIndexedValues(lhs);
  VPackSlice const rSlice = extractIndexedValues(rhs);

  return compareIndexedValues(lSlice, rSlice);
}

int RocksDBComparator::compareViews(rocksdb::Slice const& lhs,
                                    rocksdb::Slice const& rhs) const {
  size_t offset = sizeof(char);
  return memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
}

int RocksDBComparator::compareCrossReferences(rocksdb::Slice const& lhs,
                                              rocksdb::Slice const& rhs) const {
  size_t offset = sizeof(char);
  int result = lhs[offset] - rhs[offset];
  if (result != 0) {
    return result;
  }

  RocksDBEntry::Type subtype = static_cast<RocksDBEntry::Type>(lhs[offset]);
  switch (subtype) {
    case RocksDBEntry::Type::Collection:
      return compareCrossReferenceCollections(lhs, rhs);
    case RocksDBEntry::Type::Index:
      return compareCrossReferenceIndices(lhs, rhs);
    case RocksDBEntry::Type::View:
      return compareCrossReferenceViews(lhs, rhs);
    default:
      return compareLexicographic(lhs, rhs);
  }
}

int RocksDBComparator::compareCrossReferenceCollections(
    rocksdb::Slice const& lhs, rocksdb::Slice const& rhs) const {
  size_t offset = 2 * sizeof(char);
  int result =
      memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
  if (result != 0) {
    return result;
  }

  offset += sizeof(uint64_t);
  return memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
}

int RocksDBComparator::compareCrossReferenceIndices(
    rocksdb::Slice const& lhs, rocksdb::Slice const& rhs) const {
  size_t offset = 2 * sizeof(char);
  int result =
      memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
  if (result != 0) {
    return result;
  }

  offset += sizeof(uint64_t);
  result =
      memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
  if (result != 0) {
    return result;
  }

  offset += sizeof(uint64_t);
  return memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
}

int RocksDBComparator::compareCrossReferenceViews(
    rocksdb::Slice const& lhs, rocksdb::Slice const& rhs) const {
  size_t offset = 2 * sizeof(char);
  int result =
      memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
  if (result != 0) {
    return result;
  }

  offset += sizeof(uint64_t);
  return memcmp((lhs.data() + offset), (rhs.data() + offset), sizeof(uint64_t));
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

VPackSlice RocksDBComparator::extractIndexedValues(
    rocksdb::Slice const& key) const {
  size_t offset = sizeof(char) + sizeof(uint64_t);
  return VPackSlice(*reinterpret_cast<VPackSlice const*>(key.data() + offset));
}
