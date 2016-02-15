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

#include "Basics/JsonHelper.h"
#include "Aql/Graphs.h"
#include "VocBase/Graphs.h"
#include "Utils/transactions.h"
#include "Cluster/ClusterMethods.h"
using namespace arangodb::basics;

std::string const graphs = "_graphs";

////////////////////////////////////////////////////////////////////////////////
/// @brief Load a graph from the _graphs collection; local and coordinator way
////////////////////////////////////////////////////////////////////////////////

arangodb::aql::Graph* arangodb::lookupGraphByName(TRI_vocbase_t* vocbase,
                                                  std::string const& name) {
  if (ServerState::instance()->isCoordinator()) {
    arangodb::rest::HttpResponse::HttpResponseCode responseCode;
    auto headers = std::make_unique<std::map<std::string, std::string>>();
    std::map<std::string, std::string> resultHeaders;
    std::string resultBody;

    TRI_voc_rid_t rev = 0;

    int error = arangodb::getDocumentOnCoordinator(
        vocbase->_name, graphs, name, rev, headers, true, responseCode,
        resultHeaders, resultBody);

    if (error != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(error,
                                    "while fetching _graph['%s'] entry: `%s`",
                                    name.c_str(), resultBody.c_str());
    }

    auto json = JsonHelper::fromString(resultBody);

    if (json == nullptr) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_HTTP_CORRUPTED_JSON,
                                    "while fetching _graph['%s'] entry: `%s`",
                                    name.c_str(), resultBody.c_str());
    }

    return new arangodb::aql::Graph(Json(TRI_UNKNOWN_MEM_ZONE, json));
  }

  CollectionNameResolver resolver(vocbase);
  auto collName = resolver.getCollectionId(graphs);

  SingleCollectionReadOnlyTransaction trx(new StandaloneTransactionContext(),
                                          vocbase, collName);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up graph '%s' in _graphs",
                                  name.c_str());
  }

  TRI_doc_mptr_copy_t mptr;
  res = trx.document(&mptr, name);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up graph '%s' in _graphs",
                                  name.c_str());
  }
  TRI_shaped_json_t document;
  TRI_EXTRACT_SHAPED_JSON_MARKER(document, mptr.getDataPtr());
  auto shaper = trx.trxCollection()->_collection->_collection->getShaper();

  auto j = TRI_JsonShapedJson(shaper, &document);
  trx.finish(res);

  if (j == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_SHAPER_FAILED,
                                  "while accessing shape for graph '%s'",
                                  name.c_str());
  }

  return new arangodb::aql::Graph(Json(TRI_UNKNOWN_MEM_ZONE, j));
}
