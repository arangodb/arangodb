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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_PREFIX_EXTRACTOR_H
#define ARANGO_ROCKSDB_ROCKSDB_PREFIX_EXTRACTOR_H 1

#include "Basics/Common.h"
#include "RocksDBEngine/RocksDBTypes.h"

#include <rocksdb/slice.h>
#include <rocksdb/slice_transform.h>

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

/// Default prefix extractor mostly used to fill prefix bloom filters
class RocksDBPrefixExtractor final : public rocksdb::SliceTransform {
 public:
  RocksDBPrefixExtractor();
  ~RocksDBPrefixExtractor();

  const char* Name() const;
  rocksdb::Slice Transform(rocksdb::Slice const& key) const;
  bool InDomain(rocksdb::Slice const& key) const;
  bool InRange(rocksdb::Slice const& dst) const;

  static size_t getPrefixLength(RocksDBEntryType type);
  static constexpr size_t getIndexPrefixLength() { return 9; }

 private:
  const std::string _name;
  static const size_t _prefixLength[];
};

class RocksDBEdgePrefixExtractor final : public rocksdb::SliceTransform {
 public:
  RocksDBEdgePrefixExtractor() {}
  ~RocksDBEdgePrefixExtractor(){};

  const char* Name() const { return "EdgePrefixExtractor"; }

  rocksdb::Slice Transform(rocksdb::Slice const& key) const {
    TRI_ASSERT(key.size() > sizeof(char) * 2 + sizeof(uint64_t) * 2);
    size_t length = key.size() - sizeof(uint64_t);  // just remove revision ID
    return rocksdb::Slice(key.data(), length);
  }

  bool InDomain(rocksdb::Slice const& key) const {
    TRI_ASSERT(key.size() > sizeof(char) * 2 + sizeof(uint64_t));
    return static_cast<RocksDBEntryType>(key.data()[0]) ==
           RocksDBEntryType::EdgeIndexValue;
  }

  bool InRange(rocksdb::Slice const& dst) const {
    TRI_ASSERT(dst.size() > sizeof(char) * 2 + sizeof(uint64_t));
    return static_cast<RocksDBEntryType>(dst.data()[0]) ==
           RocksDBEntryType::EdgeIndexValue;
  }

 private:
  static const size_t _prefixLength[];
};

}  // namespace arangodb

#endif
