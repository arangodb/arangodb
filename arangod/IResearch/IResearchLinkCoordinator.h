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

#include "ClusterEngine/ClusterIndex.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearchLink.h"
#include "Indexes/IndexFactory.h"
#include "VocBase/Identifiers/IndexId.h"

namespace arangodb {

struct IndexTypeFactory;  // forward declaration

}  // namespace arangodb
namespace arangodb::iresearch {

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

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize from the specified definition used in make(...)
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  Result init(velocypack::Slice definition);

  bool canBeDropped() const final { return IResearchLink::canBeDropped(); }

  arangodb::Result drop() final { return IResearchLink::drop(); }

  bool hasSelectivityEstimate() const final {
    return IResearchLink::hasSelectivityEstimate();
  }

  bool isHidden() const final {
    return true;  // always hide links
  }

  // IResearch does not provide a fixed default sort order
  bool isSorted() const final { return IResearchLink::isSorted(); }

  void load() final { IResearchLink::load(); }

  bool matchesDefinition(arangodb::velocypack::Slice const& slice) const final {
    return IResearchLink::matchesDefinition(slice);
  }

  Stats stats() const final;

  size_t memory() const final { return stats().indexSize; }
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLink object
  /// @param withFigures output 'figures' section with e.g. memory size
  ////////////////////////////////////////////////////////////////////////////////
  using Index::toVelocyPack;  // for std::shared_ptr<Builder>
                              // Index::toVelocyPack(bool, Index::Serialize)
  void toVelocyPack(
      arangodb::velocypack::Builder& builder,
      std::underlying_type<arangodb::Index::Serialize>::type flags) const final;

  void toVelocyPackFigures(velocypack::Builder& builder) const final {
    IResearchDataStore::toVelocyPackStats(builder);
  }

  IndexType type() const final { return IResearchLink::type(); }

  char const* typeName() const final { return IResearchLink::typeName(); }

  void unload() final {
    auto res = IResearchLink::unload();

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearchLinkCoordinator-specific implementation of an
  ///        IndexTypeFactory
  ////////////////////////////////////////////////////////////////////////////////
  class IndexFactory final : public IndexTypeFactory {
    friend class IResearchLinkCoordinator;

   public:
    explicit IndexFactory(ArangodServer& server);

    [[nodiscard]] bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
                             std::string const& dbname) const final;

    std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                       velocypack::Slice definition, IndexId id,
                                       bool isClusterConstructor) const final;

    Result normalize(velocypack::Builder& normalized,
                     velocypack::Slice definition, bool isCreation,
                     TRI_vocbase_t const& vocbase) const final;
  };

  static std::shared_ptr<IndexFactory> createFactory(ArangodServer&);
};

}  // namespace arangodb::iresearch
