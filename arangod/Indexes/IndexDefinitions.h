////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "Indexes/IndexDefinition.h"

namespace arangodb {

struct IVectorIndexProvider;

struct EdgeIndexDefinition : public IndexDefinition {
  EdgeIndexDefinition() : IndexDefinition(IndexType::Edge) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const override;
};

struct FulltextIndexDefinition : public IndexDefinition {
  FulltextIndexDefinition() : IndexDefinition(IndexType::Fulltext) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const override {
    return factory.createFulltext(collection, definition, id,
                                  isClusterConstructor);
  }
};

struct GeoIndexDefinition : public IndexDefinition {
  GeoIndexDefinition() : IndexDefinition(IndexType::Geo) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const override {
    return factory.createGeo(collection, definition, id, isClusterConstructor);
  }
};

struct Geo1IndexDefinition : public IndexDefinition {
  Geo1IndexDefinition() : IndexDefinition(IndexType::Geo1) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const override {
    return factory.createGeo1(collection, definition, id, isClusterConstructor);
  }
};

struct Geo2IndexDefinition : public IndexDefinition {
  Geo2IndexDefinition() : IndexDefinition(IndexType::Geo2) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const override {
    return factory.createGeo2(collection, definition, id, isClusterConstructor);
  }
};

// shared by Hash/Persistent/Skiplist: identical normalize() rules, but each
// one is a different RocksDB/ClusterIndex class, so create() still needs to
// pick the right createXxx() by the instance's own _type
struct SecondaryIndexDefinition : public IndexDefinition {
  explicit SecondaryIndexDefinition(IndexType type) : IndexDefinition(type) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const override;
};

// shared by Zkd/MDI for the same reason as SecondaryIndexDefinition
struct MdiIndexDefinition : public IndexDefinition {
  explicit MdiIndexDefinition(IndexType type) : IndexDefinition(type) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const override;
};

struct MdiPrefixedIndexDefinition : public IndexDefinition {
  MdiPrefixedIndexDefinition() : IndexDefinition(IndexType::MDIPrefixed) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const override {
    return factory.createMdiPrefixed(collection, definition, id,
                                     isClusterConstructor);
  }
};

struct VectorIndexDefinition : public IndexDefinition {
  VectorIndexDefinition(IndexType type,
                        IVectorIndexProvider const& vectorIndexProvider)
      : IndexDefinition(type), _vectorIndexProvider(vectorIndexProvider) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const override {
    return factory.createVector(collection, definition, id,
                                isClusterConstructor);
  }

 protected:
  IVectorIndexProvider const& _vectorIndexProvider;
};

struct TtlIndexDefinition : public IndexDefinition {
  explicit TtlIndexDefinition(IndexType type) : IndexDefinition(type) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const override {
    return factory.createTtl(collection, definition, id, isClusterConstructor);
  }
};

struct PrimaryIndexDefinition : public IndexDefinition {
  PrimaryIndexDefinition() : IndexDefinition(IndexType::Primary) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const override;
};

}  // namespace arangodb
