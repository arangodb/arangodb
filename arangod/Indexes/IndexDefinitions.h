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

#include "Indexes/IndexDefinition.h"

namespace arangodb {

struct IVectorIndexProvider;

struct EdgeIndexDefinition : public IndexDefinition {
  EdgeIndexDefinition() : IndexDefinition(IndexType::Edge) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct FulltextIndexDefinition : public IndexDefinition {
  FulltextIndexDefinition() : IndexDefinition(IndexType::Fulltext) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct GeoIndexDefinition : public IndexDefinition {
  GeoIndexDefinition() : IndexDefinition(IndexType::Geo) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct Geo1IndexDefinition : public IndexDefinition {
  Geo1IndexDefinition() : IndexDefinition(IndexType::Geo) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct Geo2IndexDefinition : public IndexDefinition {
  Geo2IndexDefinition() : IndexDefinition(IndexType::Geo) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct SecondaryIndexDefinition : public IndexDefinition {
  explicit SecondaryIndexDefinition(IndexType type) : IndexDefinition(type) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct MdiIndexDefinition : public IndexDefinition {
  explicit MdiIndexDefinition(IndexType type) : IndexDefinition(type) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct MdiPrefixedIndexDefinition : public IndexDefinition {
  MdiPrefixedIndexDefinition() : IndexDefinition(IndexType::MDIPrefixed) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct VectorIndexDefinition : public IndexDefinition {
  VectorIndexDefinition(IndexType type,
                        IVectorIndexProvider const& vectorIndexProvider)
      : IndexDefinition(type), _vectorIndexProvider(vectorIndexProvider) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;

 protected:
  IVectorIndexProvider const& _vectorIndexProvider;
};

struct TtlIndexDefinition : public IndexDefinition {
  explicit TtlIndexDefinition(IndexType type) : IndexDefinition(type) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

struct PrimaryIndexDefinition : public IndexDefinition {
  PrimaryIndexDefinition() : IndexDefinition(IndexType::Primary) {}

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override;
};

}  // namespace arangodb
