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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_MMFILES_LINK_H
#define ARANGOD_IRESEARCH__IRESEARCH_MMFILES_LINK_H 1

#include "IResearchLink.h"

#include "Indexes/Index.h"

namespace arangodb {

struct IndexTypeFactory; // forward declaration

}

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

class IResearchMMFilesLink final
  : public arangodb::Index, public IResearchLink {
 public:
  virtual ~IResearchMMFilesLink();

  void afterTruncate(TRI_voc_tick_t /*tick*/) override {
    IResearchLink::afterTruncate();
  };

  virtual void batchInsert(
    arangodb::transaction::Methods& trx,
    std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& documents,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue
  ) override {
    IResearchLink::batchInsert(trx, documents, queue);
  }

  virtual bool canBeDropped() const override {
    return IResearchLink::canBeDropped();
  }

  virtual arangodb::Result drop() override { return IResearchLink::drop(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the factory for this type of index
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::IndexTypeFactory const& factory();

  virtual bool hasBatchInsert() const override {
    return IResearchLink::hasBatchInsert();
  }

  virtual bool hasSelectivityEstimate() const override {
    return IResearchLink::hasSelectivityEstimate();
  }

  virtual arangodb::Result insert(
    arangodb::transaction::Methods& trx,
    arangodb::LocalDocumentId const& documentId,
    arangodb::velocypack::Slice const& doc,
    arangodb::Index::OperationMode mode
  ) override {
    return IResearchLink::insert(trx, documentId, doc, mode);
  }

  virtual bool isPersistent() const override {
    return IResearchLink::isPersistent();
  }

  virtual bool isSorted() const override {
    return IResearchLink::isSorted();
  }

  virtual arangodb::IndexIterator* iteratorForCondition(
    arangodb::transaction::Methods* trx,
    arangodb::ManagedDocumentResult* result,
    arangodb::aql::AstNode const* condNode,
    arangodb::aql::Variable const* var,
    arangodb::IndexIteratorOptions const& opts
  ) override {
    TRI_ASSERT(false); // should not be called
    return nullptr;
  }

  virtual void load() override {
    IResearchLink::load();
  }

  virtual bool matchesDefinition(
    arangodb::velocypack::Slice const& slice
  ) const override {
    return IResearchLink::matchesDefinition(slice);
  }

  virtual size_t memory() const override {
    return IResearchLink::memory();
  }

  arangodb::Result remove(
    transaction::Methods& trx,
    arangodb::LocalDocumentId const& documentId,
    VPackSlice const& doc,
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
    std::underlying_type<arangodb::Index::Serialize>::type
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
  struct IndexFactory; // forward declaration

  IResearchMMFilesLink(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection
  );
};

NS_END // iresearch
NS_END // arangodb

#endif