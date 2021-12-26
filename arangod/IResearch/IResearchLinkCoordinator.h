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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ClusterEngine/ClusterIndex.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearchLink.h"
#include "Indexes/IndexFactory.h"
#include "VocBase/Identifiers/IndexId.h"

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
class IResearchLinkCoordinator final : public arangodb::ClusterIndex,
                                       public IResearchLink {
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief construct an uninitialized IResearch link, must call init(...)
  /// after
  ////////////////////////////////////////////////////////////////////////////////
  IResearchLinkCoordinator(IndexId id, arangodb::LogicalCollection& collection);

  virtual bool canBeDropped() const override {
    return IResearchLink::canBeDropped();
  }

  virtual arangodb::Result drop() override { return IResearchLink::drop(); }

  virtual bool hasSelectivityEstimate() const override {
    return IResearchLink::hasSelectivityEstimate();
  }

  bool isHidden() const override {
    return true;  // always hide links
  }

  // IResearch does not provide a fixed default sort order
  virtual bool isSorted() const override { return IResearchLink::isSorted(); }

  virtual void load() override { IResearchLink::load(); }

  virtual bool matchesDefinition(
      arangodb::velocypack::Slice const& slice) const override {
    return IResearchLink::matchesDefinition(slice);
  }

  virtual size_t memory() const override {
    // FIXME return in memory size
    return stats().indexSize;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLink object
  /// @param withFigures output 'figures' section with e.g. memory size
  ////////////////////////////////////////////////////////////////////////////////
  using Index::toVelocyPack;  // for std::shared_ptr<Builder>
                              // Index::toVelocyPack(bool, Index::Serialize)
  virtual void toVelocyPack(
      arangodb::velocypack::Builder& builder,
      std::underlying_type<arangodb::Index::Serialize>::type flags)
      const override;

  void toVelocyPackFigures(velocypack::Builder& builder) const override {
    IResearchLink::toVelocyPackStats(builder);
  }

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

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearchLinkCoordinator-specific implementation of an
  ///        IndexTypeFactory
  ////////////////////////////////////////////////////////////////////////////////
  struct IndexFactory : public IndexTypeFactory {
    friend class IResearchLinkCoordinator;

   private:
    IndexFactory(application_features::ApplicationServer& server);

   public:
    bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
               std::string const& dbname) const override;

    std::shared_ptr<Index> instantiate(
        LogicalCollection& collection, velocypack::Slice definition, IndexId id,
        bool isClusterConstructor) const override;

    virtual Result normalize(velocypack::Builder& normalized,
                             velocypack::Slice definition, bool isCreation,
                             TRI_vocbase_t const& vocbase) const override;
  };

  static std::shared_ptr<IndexFactory> createFactory(
      application_features::ApplicationServer&);
};  // IResearchLinkCoordinator

}  // namespace iresearch
}  // namespace arangodb
