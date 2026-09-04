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

#include "DefaultIndexFactories.h"

#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Indexes/IndexFactory.h"
#include "VectorIndex/IVectorIndexProvider.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;

Result EdgeIndexDefinition::normalize(velocypack::Builder& normalized,
                                      velocypack::Slice /*definition*/,
                                      bool isCreation,
                                      Database const& /*vocbase*/) const {
  if (isCreation) {
    // creating these indexes yourself is forbidden
    return TRI_ERROR_FORBIDDEN;
  }

  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(IndexType::Edge)));

  return TRI_ERROR_INTERNAL;
}

Result FulltextIndexDefinition::normalize(velocypack::Builder& normalized,
                                          velocypack::Slice definition,
                                          bool isCreation,
                                          Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(IndexType::Fulltext)));

  if (isCreation && !ServerState::instance()->isCoordinator() &&
      !definition.hasKey(StaticStrings::ObjectId)) {
    normalized.add(StaticStrings::ObjectId,
                   velocypack::Value(std::to_string(TRI_NewTickServer())));
  }

  return IndexFactory::enhanceJsonIndexFulltext(definition, normalized,
                                                isCreation);
}

Result GeoIndexDefinition::normalize(velocypack::Builder& normalized,
                                     velocypack::Slice definition,
                                     bool isCreation,
                                     Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(IndexType::Geo)));

  if (isCreation && !ServerState::instance()->isCoordinator() &&
      !definition.hasKey(StaticStrings::ObjectId)) {
    normalized.add(StaticStrings::ObjectId,
                   VPackValue(std::to_string(TRI_NewTickServer())));
  }

  return IndexFactory::enhanceJsonIndexGeo(definition, normalized, isCreation,
                                           1, 2);
}

Result Geo1IndexDefinition::normalize(velocypack::Builder& normalized,
                                      velocypack::Slice definition,
                                      bool isCreation,
                                      Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(IndexType::Geo)));

  if (isCreation && !ServerState::instance()->isCoordinator() &&
      !definition.hasKey(StaticStrings::ObjectId)) {
    normalized.add(StaticStrings::ObjectId,
                   velocypack::Value(std::to_string(TRI_NewTickServer())));
  }

  return IndexFactory::enhanceJsonIndexGeo(definition, normalized, isCreation,
                                           1, 1);
}

Result Geo2IndexDefinition::normalize(velocypack::Builder& normalized,
                                      velocypack::Slice definition,
                                      bool isCreation,
                                      Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(IndexType::Geo)));

  if (isCreation && !ServerState::instance()->isCoordinator() &&
      !definition.hasKey(StaticStrings::ObjectId)) {
    normalized.add(StaticStrings::ObjectId,
                   velocypack::Value(std::to_string(TRI_NewTickServer())));
  }

  return IndexFactory::enhanceJsonIndexGeo(definition, normalized, isCreation,
                                           1, 2);
}

Result SecondaryIndexDefinition::normalize(velocypack::Builder& normalized,
                                           velocypack::Slice definition,
                                           bool isCreation,
                                           Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(_type)));

  if (isCreation && !ServerState::instance()->isCoordinator() &&
      !definition.hasKey(StaticStrings::ObjectId)) {
    normalized.add(StaticStrings::ObjectId,
                   velocypack::Value(std::to_string(TRI_NewTickServer())));
  }
  if (isCreation) {
    bool est = basics::VelocyPackHelper::getBooleanValue(
        definition, StaticStrings::IndexEstimates, true);
    normalized.add(StaticStrings::IndexEstimates, velocypack::Value(est));
  }

  return IndexFactory::enhanceJsonIndexGeneric(definition, normalized,
                                               isCreation);
}

Result MdiIndexDefinition::normalize(velocypack::Builder& normalized,
                                     velocypack::Slice definition,
                                     bool isCreation,
                                     Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(_type)));

  if (isCreation && !ServerState::instance()->isCoordinator() &&
      !definition.hasKey(StaticStrings::ObjectId)) {
    normalized.add(
        StaticStrings::ObjectId,
        arangodb::velocypack::Value(std::to_string(TRI_NewTickServer())));
  }

  if (definition.hasKey(StaticStrings::IndexPrefixFields)) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  "`mdi` index does not support prefixed fields. use "
                  "`mdi-prefixed` as type instead.");
  }
  // a mdi never uses index estimates
  normalized.add(StaticStrings::IndexEstimates, velocypack::Value(false));

  return IndexFactory::enhanceJsonIndexMdi(definition, normalized, isCreation);
}

Result MdiPrefixedIndexDefinition::normalize(
    velocypack::Builder& normalized, velocypack::Slice definition,
    bool isCreation, Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(arangodb::StaticStrings::IndexType,
                 arangodb::velocypack::Value(
                     arangodb::Index::oldtypeName(IndexType::MDIPrefixed)));

  if (isCreation && !ServerState::instance()->isCoordinator() &&
      !definition.hasKey(StaticStrings::ObjectId)) {
    normalized.add(
        StaticStrings::ObjectId,
        arangodb::velocypack::Value(std::to_string(TRI_NewTickServer())));
  }
  if (isCreation) {
    bool est = basics::VelocyPackHelper::getBooleanValue(
        definition, StaticStrings::IndexEstimates, true);
    normalized.add(StaticStrings::IndexEstimates, velocypack::Value(est));
  }

  return IndexFactory::enhanceJsonIndexMdiPrefixed(definition, normalized,
                                                   isCreation);
}

Result VectorIndexDefinition::normalize(velocypack::Builder& normalized,
                                        velocypack::Slice definition,
                                        bool isCreation,
                                        Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());

  if (!_vectorIndexProvider.isVectorIndexEnabled()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "vector index feature is not enabled. Run ArangoDB with "
            "`--vector-index` flag turned on."};
  }

  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(_type)));

  if (isCreation && !ServerState::instance()->isCoordinator() &&
      !definition.hasKey(StaticStrings::ObjectId)) {
    normalized.add(
        StaticStrings::ObjectId,
        arangodb::velocypack::Value(std::to_string(TRI_NewTickServer())));
  }

  // a vector index never uses index estimates
  normalized.add(StaticStrings::IndexEstimates, velocypack::Value(false));

  return IndexFactory::enhanceJsonIndexVector(definition, normalized,
                                              isCreation);
}

Result TtlIndexDefinition::normalize(velocypack::Builder& normalized,
                                     velocypack::Slice definition,
                                     bool isCreation,
                                     Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(_type)));

  if (isCreation && !ServerState::instance()->isCoordinator() &&
      !definition.hasKey(StaticStrings::ObjectId)) {
    normalized.add(StaticStrings::ObjectId,
                   velocypack::Value(std::to_string(TRI_NewTickServer())));
  }
  // a TTL index never uses index estimates
  normalized.add(StaticStrings::IndexEstimates, velocypack::Value(false));

  return IndexFactory::enhanceJsonIndexTtl(definition, normalized, isCreation);
}

Result PrimaryIndexDefinition::normalize(velocypack::Builder& normalized,
                                         velocypack::Slice /*definition*/,
                                         bool isCreation,
                                         Database const& /*vocbase*/) const {
  if (isCreation) {
    // creating these indexes yourself is forbidden
    return TRI_ERROR_FORBIDDEN;
  }

  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(IndexType::Primary)));

  return TRI_ERROR_INTERNAL;
}
