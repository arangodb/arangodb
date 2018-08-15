////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_IRESEARCH__IRESEARCH_LINK_COORDINATOR_H
#define ARANGODB_IRESEARCH__IRESEARCH_LINK_COORDINATOR_H 1

#include "Indexes/Index.h"
#include "IResearchLinkMeta.h"

namespace arangodb {
namespace iresearch {

class IResearchViewCoordinator;

////////////////////////////////////////////////////////////////////////////////
/// @brief common base class for functionality required to link an ArangoDB
///        LogicalCollection with an IResearchView on a coordinator in cluster
////////////////////////////////////////////////////////////////////////////////
class IResearchLinkCoordinator final: public arangodb::Index {
 public:
  DECLARE_SPTR(Index);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief destructor
  ////////////////////////////////////////////////////////////////////////////////
  virtual ~IResearchLinkCoordinator() = default;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief does this IResearch Link reference the supplied view
  ////////////////////////////////////////////////////////////////////////////////
  bool operator==(LogicalView const& view) const noexcept;
  bool operator!=(LogicalView const& view) const noexcept {
    return !(*this == view);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief does this iResearch Link match the meta definition
  ////////////////////////////////////////////////////////////////////////////////
  bool operator==(IResearchLinkMeta const& meta) const noexcept {
    return _meta == meta;
  }

  bool operator!=(IResearchLinkMeta const& meta) const noexcept {
    return !(*this == meta);
  }

  virtual void batchInsert(
    transaction::Methods* trx,
    std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& documents,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue
  ) override {
    TRI_ASSERT(false); // should not be called
  }

  virtual bool canBeDropped() const override { return true; }

  virtual int drop() override { return TRI_ERROR_NO_ERROR; }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief finds first link between specified collection and view
  ////////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<IResearchLinkCoordinator> find(
    LogicalCollection const& collection,
    LogicalView const& view
  );

  virtual bool hasBatchInsert() const override { return true; }

  // selectivity can only be determined per query since multiple fields are indexed
  virtual bool hasSelectivityEstimate() const override { return false; }

  virtual arangodb::Result insert(
    transaction::Methods* trx,
    LocalDocumentId const& documentId,
    VPackSlice const& doc,
    OperationMode mode
  ) override {
    TRI_ASSERT(false); // should not be called
    return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual bool isPersistent() const override { return true; }

  // IResearch does not provide a fixed default sort order
  virtual bool isSorted() const override { return false; }

  virtual void load() override { /* NOOP */ }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create and initialize an iResearch View Link instance
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
  ) const override;

  virtual size_t memory() const override { return _meta.memory(); }

  arangodb::Result remove(
    transaction::Methods* trx,
    LocalDocumentId const& documentId,
    VPackSlice const& doc,
    OperationMode mode
  ) override {
    TRI_ASSERT(false); // should not be called
    return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLink object
  /// @param withFigures output 'figures' section with e.g. memory size
  ////////////////////////////////////////////////////////////////////////////////
  using Index::toVelocyPack; // for Index::toVelocyPack(bool, bool)
  virtual void toVelocyPack(
    arangodb::velocypack::Builder& builder,
    bool withFigures,
    bool forPeristence
  ) const override;

  virtual IndexType type() const override {
    return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
  }

  virtual char const* typeName() const override;

  virtual void unload() override { /* NOOP */ }

 private:

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief construct an uninitialized IResearch link, must call init(...) after
  ////////////////////////////////////////////////////////////////////////////////
  IResearchLinkCoordinator(
    TRI_idx_iid_t id,
    LogicalCollection& collection
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize from the specified definition
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  bool init(VPackSlice definition);

  IResearchLinkMeta _meta; // how this collection should be indexed
  std::shared_ptr<IResearchViewCoordinator> _view; // effectively the IResearch view itself (nullptr == not associated)
}; // IResearchLinkCoordinator

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_VIEW_LINK_COORDINATOR_H 
