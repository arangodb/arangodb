////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBAqlFunctions.h"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Function.h"
#include "RocksDBEngine/RocksDBFulltextIndex.h"
#include "RocksDBEngine/RocksDBGeoIndex.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "StorageEngine/DocumentIdentifierToken.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

static ExecutionCondition const NotInCoordinator = [] {
  return !arangodb::ServerState::instance()->isRunningInCluster() ||
         !arangodb::ServerState::instance()->isCoordinator();
};

/// @brief function FULLTEXT
AqlValue RocksDBAqlFunctions::Fulltext(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "FULLTEXT", 3, 4);

  AqlValue collectionValue = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!collectionValue.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }

  std::string const collectionName(collectionValue.slice().copyString());

  AqlValue attribute = ExtractFunctionParameterValue(trx, parameters, 1);

  if (!attribute.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }

  std::string attributeName(attribute.slice().copyString());

  AqlValue queryValue = ExtractFunctionParameterValue(trx, parameters, 2);

  if (!queryValue.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }

  std::string queryString = queryValue.slice().copyString();

  size_t maxResults = 0;  // 0 means "all results"
  if (parameters.size() >= 4) {
    AqlValue limit = ExtractFunctionParameterValue(trx, parameters, 3);
    if (!limit.isNull(true) && !limit.isNumber()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
    }
    if (limit.isNumber()) {
      int64_t value = limit.toInt64(trx);
      if (value > 0) {
        maxResults = static_cast<size_t>(value);
      }
    }
  }

  auto resolver = trx->resolver();
  TRI_voc_cid_t cid = resolver->getCollectionIdLocal(collectionName);
  trx->addCollectionAtRuntime(cid, collectionName);

  LogicalCollection* collection = trx->documentCollection(cid);

  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, "",
                                  collectionName.c_str());
  }

  // NOTE: The shared_ptr is protected by trx lock.
  // It is save to use the raw pointer directly.
  // We are NOT allowed to delete the index.
  arangodb::RocksDBFulltextIndex* fulltextIndex = nullptr;

  // split requested attribute name on '.' character to create a proper
  // vector of AttributeNames
  std::vector<std::vector<arangodb::basics::AttributeName>> search;
  search.emplace_back();
  for (auto const& it : basics::StringUtils::split(attributeName, '.')) {
    search.back().emplace_back(it, false);
  }

  for (auto const& idx : collection->getIndexes()) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
      // test if index is on the correct field
      if (arangodb::basics::AttributeName::isIdentical(idx->fields(), search,
                                                       false)) {
        // match!
        fulltextIndex = static_cast<arangodb::RocksDBFulltextIndex*>(idx.get());
        break;
      }
    }
  }

  if (fulltextIndex == nullptr) {
    // fiddle collection name into error message
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING,
                                  collectionName.c_str());
  }
  // do we need this in rocksdb?
  trx->pinData(cid);

  transaction::BuilderLeaser builder(trx);
  FulltextQuery parsedQuery;
  Result res = fulltextIndex->parseQueryString(queryString, parsedQuery);
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
  }
  res = fulltextIndex->executeQuery(trx, parsedQuery, maxResults,
                                    *(builder.get()));
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
  }
  return AqlValue(builder.get());
}

/// @brief Load geoindex for collection name
static arangodb::RocksDBGeoIndex* getGeoIndex(
    transaction::Methods* trx, TRI_voc_cid_t const& cid,
    std::string const& collectionName) {
  // NOTE:
  // Due to trx lock the shared_index stays valid
  // as long as trx stays valid.
  // It is save to return the Raw pointer.
  // It can only be used until trx is finished.
  trx->addCollectionAtRuntime(cid, collectionName);
  Result res = trx->state()->ensureCollections();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
  }

  auto document = trx->documentCollection(cid);
  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, "'%s'",
                                  collectionName.c_str());
  }

  arangodb::RocksDBGeoIndex* index = nullptr;
  for (auto const& idx : document->getIndexes()) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX ||
        idx->type() == arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX) {
      index = static_cast<arangodb::RocksDBGeoIndex*>(idx.get());
      break;
    }
  }

  if (index == nullptr) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_GEO_INDEX_MISSING,
                                  collectionName.c_str());
  }

  trx->pinData(cid);
  return index;
}

static AqlValue buildGeoResult(transaction::Methods* trx,
                               LogicalCollection* collection,
                               arangodb::aql::Query* query,
                               rocksdbengine::GeoCoordinates* cors,
                               TRI_voc_cid_t const& cid,
                               std::string const& attributeName) {
  if (cors == nullptr) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  size_t const nCoords = cors->length;
  if (nCoords == 0) {
    GeoIndex_CoordinatesFree(cors);
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  struct geo_coordinate_distance_t {
    geo_coordinate_distance_t(double distance, RocksDBToken token)
        : _distance(distance), _token(token) {}
    double _distance;
    RocksDBToken _token;
  };

  std::vector<geo_coordinate_distance_t> distances;

  try {
    distances.reserve(nCoords);

    for (size_t i = 0; i < nCoords; ++i) {
      distances.emplace_back(geo_coordinate_distance_t(
          cors->distances[i], RocksDBToken(cors->coordinates[i].data)));
    }
  } catch (...) {
    GeoIndex_CoordinatesFree(cors);
    throw;
  }

  GeoIndex_CoordinatesFree(cors);

  // sort result by distance
  std::sort(distances.begin(), distances.end(),
            [](geo_coordinate_distance_t const& left,
               geo_coordinate_distance_t const& right) {
              return left._distance < right._distance;
            });

  ManagedDocumentResult mmdr;
  transaction::BuilderLeaser builder(trx);
  builder->openArray();
  if (!attributeName.empty()) {
    // We have to copy the entire document
    for (auto& it : distances) {
      VPackObjectBuilder docGuard(builder.get());
      builder->add(attributeName, VPackValue(it._distance));
      if (collection->readDocument(trx, it._token, mmdr)) {
        VPackSlice doc(mmdr.vpack());
        for (auto const& entry : VPackObjectIterator(doc)) {
          std::string key = entry.key.copyString();
          if (key != attributeName) {
            builder->add(key, entry.value);
          }
        }
      }
    }

  } else {
    for (auto& it : distances) {
      if (collection->readDocument(trx, it._token, mmdr)) {
        mmdr.addToBuilder(*builder.get(), false);
      }
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function NEAR
AqlValue RocksDBAqlFunctions::Near(arangodb::aql::Query* query,
                                   transaction::Methods* trx,
                                   VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "NEAR", 3, 5);

  AqlValue collectionValue = ExtractFunctionParameterValue(trx, parameters, 0);
  if (!collectionValue.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
  }

  std::string const collectionName(collectionValue.slice().copyString());

  AqlValue latitude = ExtractFunctionParameterValue(trx, parameters, 1);
  AqlValue longitude = ExtractFunctionParameterValue(trx, parameters, 2);

  if (!latitude.isNumber() || !longitude.isNumber()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
  }

  // extract limit
  int64_t limitValue = 100;

  if (parameters.size() > 3) {
    AqlValue limit = ExtractFunctionParameterValue(trx, parameters, 3);

    if (limit.isNumber()) {
      limitValue = limit.toInt64(trx);
    } else if (!limit.isNull(true)) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
    }
  }

  std::string attributeName;
  if (parameters.size() > 4) {
    // have a distance attribute
    AqlValue distanceValue = ExtractFunctionParameterValue(trx, parameters, 4);

    if (!distanceValue.isNull(true) && !distanceValue.isString()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
    }

    if (distanceValue.isString()) {
      attributeName = distanceValue.slice().copyString();
    }
  }

  TRI_voc_cid_t cid = trx->resolver()->getCollectionIdLocal(collectionName);
  arangodb::RocksDBGeoIndex* index = getGeoIndex(trx, cid, collectionName);

  TRI_ASSERT(index != nullptr);
  TRI_ASSERT(trx->isPinned(cid));

  rocksdbengine::GeoCoordinates* cors =
      index->nearQuery(trx, latitude.toDouble(trx), longitude.toDouble(trx),
                       static_cast<size_t>(limitValue));

  return buildGeoResult(trx, index->collection(), query, cors, cid,
                        attributeName);
}

/// @brief function WITHIN
AqlValue RocksDBAqlFunctions::Within(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "WITHIN", 4, 5);

  AqlValue collectionValue = ExtractFunctionParameterValue(trx, parameters, 0);

  if (!collectionValue.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
  }

  std::string const collectionName(collectionValue.slice().copyString());

  AqlValue latitudeValue = ExtractFunctionParameterValue(trx, parameters, 1);
  AqlValue longitudeValue = ExtractFunctionParameterValue(trx, parameters, 2);
  AqlValue radiusValue = ExtractFunctionParameterValue(trx, parameters, 3);

  if (!latitudeValue.isNumber() || !longitudeValue.isNumber() ||
      !radiusValue.isNumber()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
  }

  std::string attributeName;
  if (parameters.size() > 4) {
    // have a distance attribute
    AqlValue distanceValue = ExtractFunctionParameterValue(trx, parameters, 4);

    if (!distanceValue.isNull(true) && !distanceValue.isString()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
    }

    if (distanceValue.isString()) {
      attributeName = distanceValue.slice().copyString();
    }
  }

  TRI_voc_cid_t cid = trx->resolver()->getCollectionIdLocal(collectionName);
  arangodb::RocksDBGeoIndex* index = getGeoIndex(trx, cid, collectionName);

  TRI_ASSERT(index != nullptr);
  TRI_ASSERT(trx->isPinned(cid));

  rocksdbengine::GeoCoordinates* cors = index->withinQuery(
      trx, latitudeValue.toDouble(trx), longitudeValue.toDouble(trx),
      radiusValue.toDouble(trx));

  return buildGeoResult(trx, index->collection(), query, cors, cid,
                        attributeName);
}

void RocksDBAqlFunctions::registerResources() {
  auto functions = AqlFunctionFeature::AQLFUNCTIONS;
  TRI_ASSERT(functions != nullptr);

  // fulltext functions
  functions->add({"FULLTEXT", ".h,.,.|.", false, true,
                  false, true, &RocksDBAqlFunctions::Fulltext,
                  NotInCoordinator});
  functions->add({"NEAR", ".h,.,.|.,.", false, true, false,
                  true, &RocksDBAqlFunctions::Near, NotInCoordinator});
  functions->add({"WITHIN", ".h,.,.,.|.", false, true,
                  false, true, &RocksDBAqlFunctions::Within, NotInCoordinator});
}
