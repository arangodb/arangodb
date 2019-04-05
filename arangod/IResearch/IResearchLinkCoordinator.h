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

#include "ClusterEngine/ClusterIndex.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearchLink.h"

namespace arangodb {

struct IndexTypeFactory;  // forward declaration
}

namespace arangodb {
namespace iresearch {

class IResearchViewCoordinator;

////////////////////////////////////////////////////////////////////////////////
/// @brief common base class for functionality required to link an ArangoDB
///        LogicalCollection with an IResearchView on a coordinator in cluster
////////////////////////////////////////////////////////////////////////////////
class IResearchLinkCoordinator final : public arangodb::ClusterIndex, public IResearchLink {
 public:
  virtual void batchInsert(
      transaction::Methods& trx,
      std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& documents,
      std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) override {
    TRI_ASSERT(false);  // should not be called
  }

  virtual bool canBeDropped() const override {
    return IResearchLink::canBeDropped();
  }

  virtual arangodb::Result drop() override { return IResearchLink::drop(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the factory for this type of index
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::IndexTypeFactory const& factory();

  virtual bool hasSelectivityEstimate() const override {
    return IResearchLink::hasSelectivityEstimate();
  }

  bool isHidden() const override {
    return true;  // always hide links
  }

  // IResearch does not provide a fixed default sort order
  virtual bool isSorted() const override { return IResearchLink::isSorted(); }

  virtual arangodb::IndexIterator* iteratorForCondition(
      arangodb::transaction::Methods* trx, 
      arangodb::aql::AstNode const* condNode, arangodb::aql::Variable const* var,
      arangodb::IndexIteratorOptions const& opts) override {
    TRI_ASSERT(false);  // should not be called
    return nullptr;
  }

  virtual void load() override { IResearchLink::load(); }

  virtual bool matchesDefinition(arangodb::velocypack::Slice const& slice) const override {
    return IResearchLink::matchesDefinition(slice);
  }

  virtual size_t memory() const override { return IResearchLink::memory(); }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLink object
  /// @param withFigures output 'figures' section with e.g. memory size
  ////////////////////////////////////////////////////////////////////////////////
  using Index::toVelocyPack; // for std::shared_ptr<Builder> Index::toVelocyPack(bool, Index::Serialize)
  virtual void toVelocyPack(arangodb::velocypack::Builder& builder,
                            std::underlying_type<arangodb::Index::Serialize>::type flags) const override;

  virtual IndexType type() const override { return IResearchLink::type(); }

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
  struct IndexFactory;  // forward declaration

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief construct an uninitialized IResearch link, must call init(...)
  /// after
  ////////////////////////////////////////////////////////////////////////////////
  IResearchLinkCoordinator(TRI_idx_iid_t id, arangodb::LogicalCollection& collection);
};  // IResearchLinkCoordinator

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGODB_IRESEARCH__IRESEARCH_VIEW_LINK_COORDINATOR_H
