////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationServer.h"
#include "Indexes/IndexFactory.h"

namespace arangodb {

class RocksDBIndexFactory final : public IndexFactory {
 public:
  RocksDBIndexFactory(application_features::ApplicationServer&,
                      IndexTypeCatalog const&);

  // RocksDB is the only engine that persists an objectId (its RocksDB key
  // prefix) as part of the index definition
  void finalizeDefinition(velocypack::Builder& normalized,
                          velocypack::Slice definition,
                          bool isCreation) const override;

  /// @brief create initial system indexes
  void fillSystemIndexes(
      LogicalCollection& col,
      std::vector<std::shared_ptr<Index>>& systemIndexes) const override;

  /// @brief create indexes from a list of index definitions
  void prepareIndexes(
      LogicalCollection& col, velocypack::Slice indexesSlice,
      std::vector<std::shared_ptr<Index>>& indexes) const override;

  std::shared_ptr<Index> createPrimary(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool isClusterConstructor) const override;
  std::shared_ptr<Index> createEdge(LogicalCollection& collection,
                                    velocypack::Slice definition, IndexId id,
                                    bool isClusterConstructor) const override;
  std::shared_ptr<Index> createGeo(LogicalCollection& collection,
                                   velocypack::Slice definition, IndexId id,
                                   bool isClusterConstructor) const override;
  std::shared_ptr<Index> createGeo1(LogicalCollection& collection,
                                    velocypack::Slice definition, IndexId id,
                                    bool isClusterConstructor) const override;
  std::shared_ptr<Index> createGeo2(LogicalCollection& collection,
                                    velocypack::Slice definition, IndexId id,
                                    bool isClusterConstructor) const override;
  std::shared_ptr<Index> createHash(LogicalCollection& collection,
                                    velocypack::Slice definition, IndexId id,
                                    bool isClusterConstructor) const override;
  std::shared_ptr<Index> createPersistent(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool isClusterConstructor) const override;
  std::shared_ptr<Index> createSkiplist(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool isClusterConstructor) const override;
  std::shared_ptr<Index> createTtl(LogicalCollection& collection,
                                   velocypack::Slice definition, IndexId id,
                                   bool isClusterConstructor) const override;
  std::shared_ptr<Index> createFulltext(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool isClusterConstructor) const override;
  std::shared_ptr<Index> createZkd(LogicalCollection& collection,
                                   velocypack::Slice definition, IndexId id,
                                   bool isClusterConstructor) const override;
  std::shared_ptr<Index> createMdi(LogicalCollection& collection,
                                   velocypack::Slice definition, IndexId id,
                                   bool isClusterConstructor) const override;
  std::shared_ptr<Index> createMdiPrefixed(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool isClusterConstructor) const override;
  std::shared_ptr<Index> createVector(LogicalCollection& collection,
                                      velocypack::Slice definition, IndexId id,
                                      bool isClusterConstructor) const override;
  std::shared_ptr<Index> createInverted(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool isClusterConstructor) const override;
};

}  // namespace arangodb
