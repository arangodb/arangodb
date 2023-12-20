////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearchLink.h"

#include "Indexes/IndexFactory.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/Identifiers/IndexId.h"

namespace arangodb {

struct IndexTypeFactory;  // forward declaration
}

namespace arangodb {
namespace iresearch {

class IResearchRocksDBLink final : public RocksDBIndex, public IResearchLink {
 public:
  IResearchRocksDBLink(IndexId iid, LogicalCollection& collection,
                       uint64_t objectId);

  ResultT<TruncateGuard> truncateBegin(rocksdb::WriteBatch& batch) final {
    auto r = RocksDBIndex::truncateBegin(batch);
    if (!r.ok()) {
      return r;
    }
    return IResearchDataStore::truncateBegin();
  }

  void truncateCommit(TruncateGuard&& guard, TRI_voc_tick_t tick,
                      transaction::Methods* trx) final {
    IResearchDataStore::truncateCommit(std::move(guard), tick, trx);
    guard = {};
  }

  bool canBeDropped() const override { return IResearchLink::canBeDropped(); }

  Result drop() /*noexcept*/ final { return IResearchLink::drop(); }

  bool hasSelectivityEstimate() const override {
    return IResearchLink::hasSelectivityEstimate();
  }

  void recoveryInsert(uint64_t tick, LocalDocumentId documentId,
                      VPackSlice doc) {
    IResearchDataStore::recoveryInsert<FieldIterator<FieldMeta>,
                                       IResearchLinkMeta>(tick, documentId, doc,
                                                          meta());
  }

  Result insert(transaction::Methods& trx, RocksDBMethods* /*methods*/,
                LocalDocumentId const& documentId, VPackSlice doc,
                OperationOptions const& /*options*/,
                bool /*performChecks*/) override {
    return IResearchDataStore::insert<FieldIterator<FieldMeta>,
                                      IResearchLinkMeta>(trx, documentId, doc,
                                                         meta());
  }

  Result remove(transaction::Methods& trx, RocksDBMethods* /*methods*/,
                LocalDocumentId const& documentId, VPackSlice /*doc*/,
                OperationOptions const& /*options*/) final {
    return IResearchDataStore::remove(trx, documentId);
  }

  bool isSorted() const override { return IResearchLink::isSorted(); }

  bool isHidden() const override { return IResearchLink::isHidden(); }

  bool needsReversal() const override { return true; }

  void load() override { IResearchLink::load(); }

  bool matchesDefinition(VPackSlice const& slice) const override {
    return IResearchLink::matchesDefinition(slice);
  }

  size_t memory() const override {
    // FIXME return in memory size
    return stats().indexSize;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLink object
  /// @param withFigures output 'figures' section with e.g. memory size
  ////////////////////////////////////////////////////////////////////////////////
  using Index::toVelocyPack;  // for std::shared_ptr<Builder>
                              // Index::toVelocyPack(bool, Index::Serialize)
  void toVelocyPack(
      VPackBuilder& builder,
      std::underlying_type<Index::Serialize>::type flags) const override;

  void toVelocyPackFigures(velocypack::Builder& builder) const final {
    IResearchDataStore::toVelocyPackStats(builder);
  }

  IndexType type() const override { return IResearchLink::type(); }

  char const* typeName() const override { return IResearchLink::typeName(); }

  void unload() override {
    auto res = IResearchLink::unload();

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearchRocksDBLink-specific implementation of an IndexTypeFactory
  ////////////////////////////////////////////////////////////////////////////////
  struct IndexFactory : public IndexTypeFactory {
    friend class IResearchRocksDBLink;

   private:
    IndexFactory(ArangodServer& server);

   public:
    bool equal(VPackSlice lhs, VPackSlice rhs,
               std::string const& dbname) const override;

    std::shared_ptr<Index> instantiate(
        LogicalCollection& collection, VPackSlice definition, IndexId id,
        bool /*isClusterConstructor*/) const override;

    virtual Result normalize(VPackBuilder& normalized, VPackSlice definition,
                             bool isCreation,
                             TRI_vocbase_t const& vocbase) const override;
  };

  static std::shared_ptr<IndexFactory> createFactory(ArangodServer&);
};

}  // namespace iresearch
}  // namespace arangodb
