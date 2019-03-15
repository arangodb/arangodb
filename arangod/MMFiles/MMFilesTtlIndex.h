////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_MMFILES_TTL_INDEX_H
#define ARANGOD_MMFILES_TTL_INDEX_H 1

#include "Basics/Common.h"
#include "MMFiles/MMFilesSkiplistIndex.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/vocbase.h"

namespace arangodb {
namespace transaction {
class Methods;
}

namespace velocypack {
class Slice;
}

class MMFilesTtlIndex final : public MMFilesSkiplistIndex {
 public:
  MMFilesTtlIndex() = delete;

  MMFilesTtlIndex(
    TRI_idx_iid_t iid,
    LogicalCollection& collection,
    arangodb::velocypack::Slice const& info
  );

  ~MMFilesTtlIndex();

  IndexType type() const override { return Index::TRI_IDX_TYPE_TTL_INDEX; }

  char const* typeName() const override { return "ttl"; }
  
  void toVelocyPack(arangodb::velocypack::Builder& builder,
                    std::underlying_type<Index::Serialize>::type flags) const override;

 protected:
  // special override method that extracts a timestamp value from the index attribute
  Result insert(transaction::Methods& trx,
                LocalDocumentId const& documentId,
                velocypack::Slice const& doc, Index::OperationMode mode) override;

  // special override method that extracts a timestamp value from the index attribute
  Result remove(transaction::Methods& trx, 
                LocalDocumentId const& documentId,
                velocypack::Slice const& doc, Index::OperationMode mode) override;
 
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
