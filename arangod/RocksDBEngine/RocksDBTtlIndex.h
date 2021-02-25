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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_TTL_INDEX_H
#define ARANGOD_ROCKSDB_ROCKSDB_TTL_INDEX_H 1

#include "RocksDBEngine/RocksDBSkiplistIndex.h"
#include "VocBase/Identifiers/IndexId.h"

namespace arangodb {
class LogicalCollection;
class RocksDBMethods;

namespace velocypack {
class Builder;
class Slice;
}

class RocksDBTtlIndex final : public RocksDBSkiplistIndex {
 public:
  RocksDBTtlIndex(IndexId iid, LogicalCollection& coll,
                  arangodb::velocypack::Slice const& info);

  IndexType type() const override { return Index::TRI_IDX_TYPE_TTL_INDEX; }

  char const* typeName() const override { return "rocksdb-ttl"; }
  
  bool matchesDefinition(VPackSlice const&) const override;
  
  void toVelocyPack(arangodb::velocypack::Builder& builder,
                    std::underlying_type<Index::Serialize>::type flags) const override;
  
 protected:
  // special override method that extracts a timestamp value from the index attribute
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId, velocypack::Slice doc,
                OperationOptions const& options) override;

  // special override method that extracts a timestamp value from the index attribute
  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId const& documentId,
                velocypack::Slice doc) override;

 private:
  /// @brief extract a timestamp value from the index attribute value
  /// returns a negative timestamp if the index attribute value is not convertible
  /// properly into a timestamp
  double getTimestamp(arangodb::velocypack::Slice const& doc) const;
  
 private:
  double const _expireAfter;
};

}

#endif
