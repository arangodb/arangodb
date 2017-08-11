////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_ROCKSDB_LINK_H
#define ARANGOD_IRESEARCH__IRESEARCH_ROCKSDB_LINK_H 1

//#include "IResearchLink.h"

#include "RocksDBEngine/RocksDBIndex.h"

namespace arangodb {
namespace iresearch {

class IResearchLink; // forward declaration

class IResearchRocksDBLink final: public arangodb::RocksDBIndex {
 public:
  typedef std::shared_ptr<Index> ptr;

  virtual ~IResearchRocksDBLink();

  virtual bool allowExpansion() const override;
  virtual bool canBeDropped() const override;
  virtual bool hasSelectivityEstimate() const override;

  virtual arangodb::Result insertInternal(
      transaction::Methods* trx,
      arangodb::RocksDBMethods*,
      TRI_voc_rid_t revisionId,
      const arangodb::velocypack::Slice& doc
  ) override;

  virtual bool isSorted() const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create and initialize a RocksDB IResearch View Link instance
  /// @return nullptr on failure
  ////////////////////////////////////////////////////////////////////////////////
  static ptr make(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection* collection,
    VPackSlice const& definition
  ) noexcept;

  virtual arangodb::Result removeInternal(
      transaction::Methods* trx,
      arangodb::RocksDBMethods*,
      TRI_voc_rid_t revisionId,
      const arangodb::velocypack::Slice& doc
  ) override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief iResearch Link index type enum value
  ////////////////////////////////////////////////////////////////////////////////
  virtual IndexType type() const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief iResearch Link index type string value
  ////////////////////////////////////////////////////////////////////////////////
  virtual char const* typeName() const override;

 private:
  std::shared_ptr<IResearchLink> _link;

  IResearchRocksDBLink(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection* collection
  );
};

} // iresearch
} // arangodb
#endif