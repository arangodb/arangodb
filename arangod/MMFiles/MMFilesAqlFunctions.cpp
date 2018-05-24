////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesAqlFunctions.h"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Function.h"
#include "Aql/Query.h"
#include "MMFiles/MMFilesFulltextIndex.h"
#include "MMFiles/MMFilesGeoIndex.h"
#include "MMFiles/mmfiles-fulltext-index.h"
#include "MMFiles/mmfiles-fulltext-query.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LocalDocumentId.h"
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

/// @brief Load geoindex for collection name
static arangodb::MMFilesGeoIndex* getGeoIndex(
    transaction::Methods* trx, TRI_voc_cid_t const& cid,
    std::string const& collectionName) {
  // NOTE:
  // Due to trx lock the shared_index stays valid
  // as long as trx stays valid.
  // It is save to return the Raw pointer.
  // It can only be used until trx is finished.
  trx->addCollectionAtRuntime(cid, collectionName);

  arangodb::MMFilesGeoIndex* index = nullptr;
  auto indexes = trx->indexesForCollection(collectionName);
  for (auto const& idx : indexes) {
    if (arangodb::Index::isGeoIndex(idx->type())) {
      index = static_cast<arangodb::MMFilesGeoIndex*>(idx.get());
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

/// @brief function FULLTEXT
AqlValue MMFilesAqlFunctions::Fulltext(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "FULLTEXT", 3, 4);

  AqlValue collectionValue = ExtractFunctionParameterValue(parameters, 0);
  if (!collectionValue.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }
  std::string const cname(collectionValue.slice().copyString());

  AqlValue attribute = ExtractFunctionParameterValue(parameters, 1);
  if (!attribute.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }
  std::string const attributeName(attribute.slice().copyString());

  AqlValue queryValue = ExtractFunctionParameterValue(parameters, 2);
  if (!queryValue.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
  }
  std::string const queryString = queryValue.slice().copyString();

  size_t maxResults = 0;  // 0 means "all results"
  if (parameters.size() >= 4) {
    AqlValue limit = ExtractFunctionParameterValue(parameters, 3);
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

  TRI_voc_cid_t cid = trx->resolver()->getCollectionIdLocal(cname);
  if (cid == 0) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, "%s",
                                  cname.c_str());
  }
  // add the collection to the query for proper cache handling
  query->collections()->add(cname, AccessMode::Type::READ);
  trx->addCollectionAtRuntime(cid, cname, AccessMode::Type::READ);
  LogicalCollection const* collection = trx->documentCollection(cid);
  TRI_ASSERT(collection != nullptr);

  // split requested attribute name on '.' character to create a proper
  // vector of AttributeNames
  std::vector<std::vector<arangodb::basics::AttributeName>> search;
  search.emplace_back();
  for (auto const& it : basics::StringUtils::split(attributeName, '.')) {
    search.back().emplace_back(it, false);
  }

  // NOTE: The shared_ptr is protected by trx lock.
  // It is safe to use the raw pointer directly.
  // We are NOT allowed to delete the index.
  arangodb::MMFilesFulltextIndex* fulltextIndex = nullptr;

  auto indexes = collection->getIndexes();
  for (auto const& idx : indexes) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
      // test if index is on the correct field
      if (arangodb::basics::AttributeName::isIdentical(idx->fields(), search,
                                                       false)) {
        // match!
        fulltextIndex = static_cast<arangodb::MMFilesFulltextIndex*>(idx.get());
        break;
      }
    }
  }

  if (fulltextIndex == nullptr) {
    // fiddle collection name into error message
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING,
                                  cname.c_str());
  }

  trx->pinData(cid);

  TRI_fulltext_query_t* ft = TRI_CreateQueryMMFilesFulltextIndex(
      TRI_FULLTEXT_SEARCH_MAX_WORDS, maxResults);

  if (ft == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  bool isSubstringQuery = false;
  int res = TRI_ParseQueryMMFilesFulltextIndex(ft, queryString.c_str(),
                                               &isSubstringQuery);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeQueryMMFilesFulltextIndex(ft);
    THROW_ARANGO_EXCEPTION(res);
  }

  // note: the following call will free "ft"!
  std::set<TRI_voc_rid_t> queryResult =
      TRI_QueryMMFilesFulltextIndex(fulltextIndex->internals(), ft);

  TRI_ASSERT(trx->isPinned(cid));

  transaction::BuilderLeaser builder(trx);
  builder->openArray();

  ManagedDocumentResult mmdr;
  for (auto const& it : queryResult) {
    if (collection->readDocument(trx, LocalDocumentId{it}, mmdr)) {
      mmdr.addToBuilder(*builder.get(), true);
    }
  }
  builder->close();
  return AqlValue(builder.get());
}

/// @brief function NEAR
AqlValue MMFilesAqlFunctions::Near(arangodb::aql::Query* query,
                                   transaction::Methods* trx,
                                   VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "NEAR", 3, 5);

  AqlValue collectionValue = ExtractFunctionParameterValue(parameters, 0);
  if (!collectionValue.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
  }

  std::string const collectionName(collectionValue.slice().copyString());

  AqlValue latitude = ExtractFunctionParameterValue(parameters, 1);
  AqlValue longitude = ExtractFunctionParameterValue(parameters, 2);

  if (!latitude.isNumber() || !longitude.isNumber()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
  }

  // extract limit
  int64_t limitValue = 100;

  if (parameters.size() > 3) {
    AqlValue limit = ExtractFunctionParameterValue(parameters, 3);

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
    AqlValue distanceValue = ExtractFunctionParameterValue(parameters, 4);

    if (!distanceValue.isNull(true) && !distanceValue.isString()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "NEAR");
    }

    if (distanceValue.isString()) {
      attributeName = distanceValue.slice().copyString();
    }
  }

  TRI_voc_cid_t cid = trx->resolver()->getCollectionIdLocal(collectionName);
  arangodb::MMFilesGeoIndex* index = getGeoIndex(trx, cid, collectionName);

  TRI_ASSERT(index != nullptr);
  TRI_ASSERT(trx->isPinned(cid));

  try {
    transaction::BuilderLeaser builder(trx);
    builder->openArray();
    index->nearQuery(trx, latitude.toDouble(trx), longitude.toDouble(trx),
                     static_cast<size_t>(limitValue), attributeName,
                     *builder.get());
    builder->close();

    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief function WITHIN
AqlValue MMFilesAqlFunctions::Within(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  ValidateParameters(parameters, "WITHIN", 4, 5);

  AqlValue collectionValue = ExtractFunctionParameterValue(parameters, 0);

  if (!collectionValue.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
  }

  std::string const collectionName(collectionValue.slice().copyString());

  AqlValue latitudeValue = ExtractFunctionParameterValue(parameters, 1);
  AqlValue longitudeValue = ExtractFunctionParameterValue(parameters, 2);
  AqlValue radiusValue = ExtractFunctionParameterValue(parameters, 3);

  if (!latitudeValue.isNumber() || !longitudeValue.isNumber() ||
      !radiusValue.isNumber()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
  }

  std::string attributeName;
  if (parameters.size() > 4) {
    // have a distance attribute
    AqlValue distanceValue = ExtractFunctionParameterValue(parameters, 4);

    if (!distanceValue.isNull(true) && !distanceValue.isString()) {
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "WITHIN");
    }

    if (distanceValue.isString()) {
      attributeName = distanceValue.slice().copyString();
    }
  }

  TRI_voc_cid_t cid = trx->resolver()->getCollectionIdLocal(collectionName);
  arangodb::MMFilesGeoIndex* index = getGeoIndex(trx, cid, collectionName);

  TRI_ASSERT(index != nullptr);
  TRI_ASSERT(trx->isPinned(cid));

  try {
    transaction::BuilderLeaser builder(trx);
    builder->openArray();
    index->withinQuery(trx, latitudeValue.toDouble(trx),
                       longitudeValue.toDouble(trx), radiusValue.toDouble(trx),
                       attributeName, *builder.get());
    builder->close();

    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

void MMFilesAqlFunctions::registerResources() {
  auto functions = AqlFunctionFeature::AQLFUNCTIONS;
  TRI_ASSERT(functions != nullptr);

  functions->add({"FULLTEXT", ".h,.,.|.", false, true,
                 false, &MMFilesAqlFunctions::Fulltext,
                 NotInCoordinator});
  functions->add({"NEAR", ".h,.,.|.,.", false, true, false,
                  &MMFilesAqlFunctions::Near, NotInCoordinator});
  functions->add({"WITHIN", ".h,.,.,.|.", false, true,
                  false, &MMFilesAqlFunctions::Within, NotInCoordinator});
}
