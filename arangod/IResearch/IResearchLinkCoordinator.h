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

#include "IResearchLink.h"
#include "ClusterEngine/ClusterIndex.h"
#include "IResearch/IResearchLinkMeta.h"

namespace arangodb {

struct IndexTypeFactory; // forward declaration

}

namespace arangodb {
namespace iresearch {

class IResearchViewCoordinator;

////////////////////////////////////////////////////////////////////////////////
/// @brief common base class for functionality required to link an ArangoDB
///        LogicalCollection with an IResearchView on a coordinator in cluster
////////////////////////////////////////////////////////////////////////////////
class IResearchLinkCoordinator final
  : public arangodb::ClusterIndex, public IResearchLink {
 public:

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
    transaction::Methods& trx,
    std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& documents,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue
  ) override {
    TRI_ASSERT(false); // should not be called
  }

  virtual bool canBeDropped() const override { return true; }

  virtual arangodb::Result drop() override { return arangodb::Result(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the factory for this type of index
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::IndexTypeFactory const& factory();

  virtual bool hasBatchInsert() const override { return true; }

  // selectivity can only be determined per query since multiple fields are indexed
  virtual bool hasSelectivityEstimate() const override { return false; }

  virtual arangodb::Result insert(
    transaction::Methods& trx,
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

  virtual void load() override { /* NOOP */ }

  virtual bool matchesDefinition(
    arangodb::velocypack::Slice const& slice
  ) const override;

  virtual size_t memory() const override { return _meta.memory(); }

  arangodb::Result remove(
    transaction::Methods& trx,
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
  using Index::toVelocyPack; // for Index::toVelocyPack(bool, unsigned)
  virtual void toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<arangodb::Index::Serialize>::type flags
  ) const override;

  virtual IndexType type() const override {
    return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
  }

  virtual char const* typeName() const override;

  virtual void unload() override { /* NOOP */ }

 private:
  struct IndexFactory; // forward declaration

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief construct an uninitialized IResearch link, must call init(...) after
  ////////////////////////////////////////////////////////////////////////////////
  IResearchLinkCoordinator(
    TRI_idx_iid_t id,
    arangodb::LogicalCollection& collection
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize from the specified definition
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result init(arangodb::velocypack::Slice const& definition);

  IResearchLinkMeta _meta; // how this collection should be indexed
  std::shared_ptr<IResearchViewCoordinator> _view; // effectively the IResearch view itself (nullptr == not associated)
}; // IResearchLinkCoordinator

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_VIEW_LINK_COORDINATOR_H 
