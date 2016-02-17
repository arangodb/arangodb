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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestEdgeHandler.h"
#include "Basics/conversions.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Rest/HttpRequest.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief free a string if defined, nop otherwise
////////////////////////////////////////////////////////////////////////////////

#define FREE_STRING(zone, what) \
  if (what != 0) {              \
    TRI_Free(zone, what);       \
    what = 0;                   \
  }

RestEdgeHandler::RestEdgeHandler(HttpRequest* request)
    : RestDocumentHandler(request) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock API_EDGE_CREATE
////////////////////////////////////////////////////////////////////////////////

bool RestEdgeHandler::createDocument() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (!suffix.empty()) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + EDGE_PATH +
                      "?collection=<identifier>");
    return false;
  }

  // extract the from
  bool found;
  char const* from = _request->value("from", found);
  if (!found || *from == '\0') {
    generateError(
        HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "'from' is missing, expecting " + EDGE_PATH +
            "?collection=<identifier>&from=<from-handle>&to=<to-handle>");
    return false;
  }

  // extract the to
  char const* to = _request->value("to", found);

  if (!found || *to == '\0') {
    generateError(
        HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "'to' is missing, expecting " + EDGE_PATH +
            "?collection=<identifier>&from=<from-handle>&to=<to-handle>");
    return false;
  }

  // extract the cid
  std::string const collection = _request->value("collection", found);

  if (!found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  /* TODO
  if (!checkCreateCollection(collection, getCollectionType())) {
    return false;
  }
  */

  try {
    bool parseSuccess = true;
    VPackOptions options;
    options.keepTopLevelOpen = true; // We need to insert _from and _to
    std::shared_ptr<VPackBuilder> parsedBody =
        parseVelocyPackBody(&options, parseSuccess);

    if (!parseSuccess) {
      return false;
    }

    if (!parsedBody->isOpenObject()) {
      generateTransactionError(collection,
                               TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
      return false;
    }

    // Add _from and _to
    parsedBody->add(TRI_VOC_ATTRIBUTE_FROM, VPackValue(from));
    parsedBody->add(TRI_VOC_ATTRIBUTE_TO, VPackValue(to));

    parsedBody->close();

    VPackSlice body = parsedBody->slice();

    // find and load collection given by name or identifier
    SingleCollectionWriteTransaction<1> trx(new StandaloneTransactionContext(),
                                            _vocbase, collection);

    // .............................................................................
    // inside write transaction
    // .............................................................................

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      generateTransactionError(collection, res);
      return false;
    }

    // TODO Test if is edge collection
    /*
    TRI_document_collection_t* document = trx.documentCollection();

    if (document->_info.type() != TRI_COL_TYPE_EDGE) {
      // check if we are inserting with the EDGE handler into a non-EDGE
      // collection
      generateError(HttpResponse::BAD,
                    TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
      return false;
    }
    */

    arangodb::OperationOptions opOptions;
    opOptions.waitForSync = extractWaitForSync();


    arangodb::OperationResult result = trx.insert(collection, body, opOptions);


    // Will commit if no error occured.
    // or abort if an error occured.
    // result stays valid!
    res = trx.finish(result.code);

    // .............................................................................
    // outside write transaction
    // .............................................................................

    if (result.failed()) {
      // TODO correct errors for _from or _to invalid.
      /* wrongPart is either from or to
      if (res == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        generateError(HttpResponse::NOT_FOUND, res,
                      wrongPart + " does not point to a valid collection");
      } else {
        generateError(HttpResponse::BAD, res,
                      wrongPart + " is not a document handle");
      }
      */
      generateTransactionError(collection, result.code);
      return false;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      generateTransactionError(collection, res);
      return false;
    }

    generateSaved(result, collection, TRI_COL_TYPE_EDGE);

    return true;
  } catch (arangodb::basics::Exception const& ex) {
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  } catch (std::bad_alloc const&) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& ex) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
  // Only in error case
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document (an edge), coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

bool RestEdgeHandler::createDocumentCoordinator(std::string const& collname,
                                                bool waitForSync,
                                                VPackSlice const& document,
                                                char const* from,
                                                char const* to) {
  std::string const& dbname = _request->databaseName();

  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  std::unique_ptr<TRI_json_t> json(
      arangodb::basics::VelocyPackHelper::velocyPackToJson(document));
  int error = arangodb::createEdgeOnCoordinator(dbname, collname, waitForSync,
                                                json, from, to, responseCode,
                                                resultHeaders, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collname.c_str(), error);

    return false;
  }
  // Essentially return the response we got from the DBserver, be it
  // OK or an error:
  createResponse(responseCode);
  arangodb::mergeResponseHeaders(_response, resultHeaders);
  _response->body().appendText(resultBody.c_str(), resultBody.size());

  return responseCode >= arangodb::rest::HttpResponse::BAD;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock API_EDGE_READ
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock API_EDGE_READ_ALL
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock API_EDGE_READ_HEAD
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock API_EDGE_REPLACE
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock API_EDGE_UPDATES
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock API_EDGE_DELETE
////////////////////////////////////////////////////////////////////////////////
