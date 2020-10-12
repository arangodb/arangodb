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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_ROCKSDB_LINK_H
#define ARANGOD_IRESEARCH__IRESEARCH_ROCKSDB_LINK_H 1

#include "IResearchLink.h"

#include "Indexes/IndexFactory.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/Identifiers/IndexId.h"

namespace arangodb {

struct IndexTypeFactory;  // forward declaration
}

namespace arangodb {
namespace iresearch {

class IResearchRocksDBLink final : public arangodb::RocksDBIndex, public IResearchLink {
 public:
  IResearchRocksDBLink(IndexId iid, arangodb::LogicalCollection& collection);

  void afterTruncate(TRI_voc_tick_t tick,
                     arangodb::transaction::Methods* trx) override {
    IResearchLink::afterTruncate(tick, trx);
  }

  bool canBeDropped() const override {
    return IResearchLink::canBeDropped();
  }

  arangodb::Result drop() override { return IResearchLink::drop(); }

  bool hasSelectivityEstimate() const override {
    return IResearchLink::hasSelectivityEstimate();
  }

  arangodb::Result insert(arangodb::transaction::Methods& trx,
                          arangodb::RocksDBMethods* methods,
                          arangodb::LocalDocumentId const& documentId,
                          arangodb::velocypack::Slice const doc,
                          arangodb::OperationOptions& /*options*/) override {
    return IResearchLink::insert(trx, documentId, doc);
  }

  bool isSorted() const override { return IResearchLink::isSorted(); }

  bool isHidden() const override { return IResearchLink::isHidden(); }

  bool needsReversal() const override { return true; } 
  
  void load() override { IResearchLink::load(); }

  bool matchesDefinition(arangodb::velocypack::Slice const& slice) const override {
    return IResearchLink::matchesDefinition(slice);
  }

  size_t memory() const override {
    // FIXME return in memory size
    return stats().indexSize;
  }

  arangodb::Result remove(arangodb::transaction::Methods& trx,
                                  arangodb::RocksDBMethods*,
                                  arangodb::LocalDocumentId const& documentId,
                                  arangodb::velocypack::Slice const doc) override {
    return IResearchLink::remove(trx, documentId, doc);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLink object
  /// @param withFigures output 'figures' section with e.g. memory size
  ////////////////////////////////////////////////////////////////////////////////
  using Index::toVelocyPack; // for std::shared_ptr<Builder> Index::toVelocyPack(bool, Index::Serialize)
  void toVelocyPack(arangodb::velocypack::Builder& builder,
                    std::underlying_type<arangodb::Index::Serialize>::type flags) const override;

  void toVelocyPackFigures(velocypack::Builder& builder) const override {
    IResearchLink::toVelocyPackStats(builder);
  }

  IndexType type() const override { return IResearchLink::type(); }

  char const* typeName() const override {
    return IResearchLink::typeName();
  }

  void unload() override {
    auto res = IResearchLink::unload();

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearchRocksDBLink-specific implementation of an IndexTypeFactory
  ////////////////////////////////////////////////////////////////////////////////
  struct IndexFactory : public arangodb::IndexTypeFactory {
    friend class IResearchRocksDBLink;

   private:
    IndexFactory(arangodb::application_features::ApplicationServer& server);

   public:
    bool equal(arangodb::velocypack::Slice const& lhs,
               arangodb::velocypack::Slice const& rhs,
               std::string const& dbname) const override;

    std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                                 arangodb::velocypack::Slice const& definition,
                                                 IndexId id,
                                                 bool /*isClusterConstructor*/) const override;

    virtual arangodb::Result normalize(             // normalize definition
        arangodb::velocypack::Builder& normalized,  // normalized definition (out-param)
        arangodb::velocypack::Slice definition,  // source definition
        bool isCreation,              // definition for index creation
        TRI_vocbase_t const& vocbase  // index vocbase
        ) const override;
  };

  static std::shared_ptr<IndexFactory> createFactory(application_features::ApplicationServer&);
};

}  // namespace iresearch
}  // namespace arangodb

#endif
