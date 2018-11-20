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

#include "IResearchLink.h"

#include "RocksDBEngine/RocksDBIndex.h"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

class IResearchRocksDBLink final
  : public arangodb::RocksDBIndex, public IResearchLink {
 public:
  DECLARE_SHARED_PTR(Index);

  virtual ~IResearchRocksDBLink();

  virtual void afterTruncate(TRI_voc_tick_t/*tick*/) override {
    IResearchLink::afterTruncate();
  };

  virtual void batchInsert(
    transaction::Methods* trx,
    std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& documents,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue
  ) override {
    IResearchLink::batchInsert(trx, documents, queue);
  }

  virtual bool canBeDropped() const override {
    return IResearchLink::canBeDropped();
  }

  virtual int drop() override {
    writeRocksWalMarker();

    return IResearchLink::drop().errorNumber();
  }

  virtual bool hasBatchInsert() const override {
    return IResearchLink::hasBatchInsert();
  }

  virtual bool hasSelectivityEstimate() const override {
    return IResearchLink::hasSelectivityEstimate();
  }

  virtual arangodb::Result insertInternal(
      transaction::Methods* trx,
      arangodb::RocksDBMethods*,
      LocalDocumentId const& documentId,
      const arangodb::velocypack::Slice& doc,
      OperationMode mode
  ) override {
    return IResearchLink::insert(trx, documentId, doc, mode);
  }

  virtual bool isSorted() const override {
    return IResearchLink::isSorted();
  }

  virtual void load() override {
    IResearchLink::load();
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create and initialize a RocksDB IResearch View Link instance
  /// @return nullptr on failure
  ////////////////////////////////////////////////////////////////////////////////
  static ptr make(
    arangodb::LogicalCollection& collection,
    arangodb::velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  ) noexcept;

  virtual bool matchesDefinition(
    arangodb::velocypack::Slice const& slice
  ) const override {
    return IResearchLink::matchesDefinition(slice);
  }

  virtual size_t memory() const override {
    return IResearchLink::memory();
  }

  virtual arangodb::Result removeInternal(
      transaction::Methods* trx,
      arangodb::RocksDBMethods*,
      LocalDocumentId const& documentId,
      const arangodb::velocypack::Slice& doc,
      OperationMode mode
  ) override {
    return IResearchLink::remove(trx, documentId, doc, mode);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLink object
  /// @param withFigures output 'figures' section with e.g. memory size
  ////////////////////////////////////////////////////////////////////////////////
  using Index::toVelocyPack; // for Index::toVelocyPack(bool, unsigned)
  virtual void toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<arangodb::Index::Serialize>::type flags
  ) const override;

  virtual IndexType type() const override {
    return IResearchLink::type();
  }

  virtual char const* typeName() const override {
    return IResearchLink::typeName();
  }

  virtual void unload() override {
    auto res = IResearchLink::unload();

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

 private:
  void writeRocksWalMarker();
  IResearchRocksDBLink(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection
  );
};

NS_END // iresearch
NS_END // arangodb

#endif