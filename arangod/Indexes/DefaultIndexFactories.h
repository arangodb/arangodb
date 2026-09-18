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

#include "Basics/Exceptions.h"
#include "Indexes/IndexFactory.h"

#include <velocypack/Slice.h>

namespace arangodb {

struct IVectorIndexProvider;

// equal()/normalize() need no storage engine, unlike instantiate()
struct DefaultIndexTypeDefinition : public IndexTypeFactory {
  IndexType const _type;

  DefaultIndexTypeDefinition(application_features::ApplicationServer& server,
                             IndexType type)
      : IndexTypeFactory(server), _type(type) {}

  bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
             std::string const&) const override {
    return IndexTypeFactory::equal(_type, lhs, rhs, true);
  }

  // overridden by the RocksDB-specific subclass; never called on this one
  std::shared_ptr<Index> instantiate(LogicalCollection&, velocypack::Slice,
                                     IndexId, bool) const override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
};

struct EdgeIndexDefinition : public DefaultIndexTypeDefinition {
  explicit EdgeIndexDefinition(application_features::ApplicationServer& server)
      : DefaultIndexTypeDefinition(server, IndexType::Edge) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct FulltextIndexDefinition : public DefaultIndexTypeDefinition {
  explicit FulltextIndexDefinition(
      application_features::ApplicationServer& server)
      : DefaultIndexTypeDefinition(server, IndexType::Fulltext) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct GeoIndexDefinition : public DefaultIndexTypeDefinition {
  explicit GeoIndexDefinition(application_features::ApplicationServer& server)
      : DefaultIndexTypeDefinition(server, IndexType::Geo) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct Geo1IndexDefinition : public DefaultIndexTypeDefinition {
  explicit Geo1IndexDefinition(application_features::ApplicationServer& server)
      : DefaultIndexTypeDefinition(server, IndexType::Geo) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct Geo2IndexDefinition : public DefaultIndexTypeDefinition {
  explicit Geo2IndexDefinition(application_features::ApplicationServer& server)
      : DefaultIndexTypeDefinition(server, IndexType::Geo) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct SecondaryIndexDefinition : public DefaultIndexTypeDefinition {
  SecondaryIndexDefinition(application_features::ApplicationServer& server,
                           IndexType type)
      : DefaultIndexTypeDefinition(server, type) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct MdiIndexDefinition : public DefaultIndexTypeDefinition {
  MdiIndexDefinition(application_features::ApplicationServer& server,
                     IndexType type)
      : DefaultIndexTypeDefinition(server, type) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct MdiPrefixedIndexDefinition : public DefaultIndexTypeDefinition {
  explicit MdiPrefixedIndexDefinition(
      application_features::ApplicationServer& server)
      : DefaultIndexTypeDefinition(server, IndexType::MDIPrefixed) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct VectorIndexDefinition : public DefaultIndexTypeDefinition {
  VectorIndexDefinition(application_features::ApplicationServer& server,
                        IndexType type,
                        IVectorIndexProvider const& vectorIndexProvider)
      : DefaultIndexTypeDefinition(server, type),
        _vectorIndexProvider(vectorIndexProvider) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

 protected:
  IVectorIndexProvider const& _vectorIndexProvider;
};

struct TtlIndexDefinition : public DefaultIndexTypeDefinition {
  TtlIndexDefinition(application_features::ApplicationServer& server,
                     IndexType type)
      : DefaultIndexTypeDefinition(server, type) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct PrimaryIndexDefinition : public DefaultIndexTypeDefinition {
  explicit PrimaryIndexDefinition(
      application_features::ApplicationServer& server)
      : DefaultIndexTypeDefinition(server, IndexType::Primary) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

}  // namespace arangodb
