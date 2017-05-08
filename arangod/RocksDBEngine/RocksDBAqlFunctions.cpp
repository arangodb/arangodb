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
#include "StorageEngine/DocumentIdentifierToken.h"
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

/// @brief function NEAR
AqlValue RocksDBAqlFunctions::Near(arangodb::aql::Query* query,
                                   transaction::Methods* trx,
                                   VPackFunctionParameters const& parameters) {
  // TODO: obi
  THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_GEO_INDEX_MISSING, "NEAR");
}

/// @brief function WITHIN
AqlValue RocksDBAqlFunctions::Within(
    arangodb::aql::Query* query, transaction::Methods* trx,
    VPackFunctionParameters const& parameters) {
  // TODO: obi
  THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_GEO_INDEX_MISSING, "Within");
}

void RocksDBAqlFunctions::registerResources() {
  auto functions = AqlFunctionFeature::AQLFUNCTIONS;
  TRI_ASSERT(functions != nullptr);

  // fulltext functions
  functions->add({"FULLTEXT", "AQL_FULLTEXT", "hs,s,s|n", true, false, true,
                  false, true, &RocksDBAqlFunctions::Fulltext,
                  NotInCoordinator});
  functions->add({"NEAR", "AQL_NEAR", "hs,n,n|nz,s", true, false, true, false,
                  true, &RocksDBAqlFunctions::Near, NotInCoordinator});
  functions->add({"WITHIN", "AQL_WITHIN", "hs,n,n,n|s", true, false, true,
                  false, true, &RocksDBAqlFunctions::Within, NotInCoordinator});
}
