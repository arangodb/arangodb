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
/// @author Jan Steemann
/// @author Daniel H. Larkin
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_PREFIX_EXTRACTOR_H
#define ARANGO_ROCKSDB_ROCKSDB_PREFIX_EXTRACTOR_H 1

#include "Basics/Common.h"

#include <rocksdb/slice.h>
#include <rocksdb/slice_transform.h>

namespace arangodb {

/// @brief effectively used for the rocksdb edge index, to allow a
/// dynamically length prefix spanning the indexed _from / _to string
class RocksDBPrefixExtractor final : public rocksdb::SliceTransform {
 public:
  RocksDBPrefixExtractor() = default;
  ~RocksDBPrefixExtractor() = default;

  const char* Name() const override { return "RocksDBPrefixExtractor"; }

  rocksdb::Slice Transform(rocksdb::Slice const& key) const override {
    // 8-byte objectID + 0..n-byte string + 1-byte '\0'
    // + 8 byte revisionID + 1-byte 0xFF (these are for cut off)
    TRI_ASSERT(key.size() >= sizeof(char) + sizeof(uint64_t));
    if (key.data()[key.size() - 1] != '\0') {
      // unfortunately rocksdb seems to call Tranform(Transform(k))
      TRI_ASSERT(static_cast<uint8_t>(key.data()[key.size() - 1]) == 0xFFU);
      TRI_ASSERT(key.size() > sizeof(char) * 3 + sizeof(uint64_t) * 2);
      size_t l = key.size() - sizeof(uint64_t) - sizeof(char);
      TRI_ASSERT(key.data()[l - 1] == '\0');
      return rocksdb::Slice(key.data(), l);
    } else {
      TRI_ASSERT(key.data()[key.size() - 1] == '\0');
      return key;
    }
  }

  bool InDomain(rocksdb::Slice const& key) const override {
    // 8-byte objectID + n-byte string + 1-byte '\0' + ...
    TRI_ASSERT(key.size() >= sizeof(char) + sizeof(uint64_t));
    return key.data()[key.size() - 1] != '\0';
  }

  bool InRange(rocksdb::Slice const& dst) const override {
    TRI_ASSERT(dst.size() >= sizeof(char) + sizeof(uint64_t));
    return dst.data()[dst.size() - 1] != '\0';
  }

  bool SameResultWhenAppended(rocksdb::Slice const& prefix) const override {
    return prefix.data()[prefix.size() - 1] == '\0';
  }
};

}  // namespace arangodb

#endif
