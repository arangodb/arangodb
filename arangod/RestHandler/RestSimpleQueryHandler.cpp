////////////////////////////////////////////////////////////////////////////////
/// @brief simple query handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestSimpleQueryHandler.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/VelocyPackHelper.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "V8Server/ApplicationV8.h"

using namespace triagens::arango;
using namespace triagens::rest;


////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestSimpleQueryHandler::RestSimpleQueryHandler(
    HttpRequest* request, std::pair<triagens::arango::ApplicationV8*,
                                    triagens::aql::QueryRegistry*>* pair)
    : RestCursorHandler(request, pair) {}


////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestSimpleQueryHandler::execute() {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  if (type == HttpRequest::HTTP_REQUEST_PUT) {
    char const* prefix = _request->requestPath();

    if (strcmp(prefix, RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH.c_str()) ==
        0) {
      // all query
      allDocuments();
      return status_t(HANDLER_DONE);
    }
  }

  generateError(HttpResponse::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return status_t(HANDLER_DONE);
}


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_all
/// @brief returns all documents of a collection
///
/// @RESTHEADER{PUT /_api/simple/all, Return all documents}
///
/// @RESTALLBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// Returns all documents of a collections. The call expects a JSON object
/// as body with the following attributes:
///
/// - *collection*: The name of the collection to query.
///
/// - *skip*: The number of documents to skip in the query (optional).
///
/// - *limit*: The maximal amount of documents to return. The *skip*
///   is applied before the *limit* restriction. (optional)
///
/// Returns a cursor containing the result, see [Http
/// Cursor](../HttpAqlQueryCursor/README.md) for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the query was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// Limit the amount of documents using *limit*
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleAllSkipLimit}
///     var cn = "products";
///     db._drop(cn);
///     var collection = db._create(cn);
///     collection.save({"Hello1" : "World1" });
///     collection.save({"Hello2" : "World2" });
///     collection.save({"Hello3" : "World3" });
///     collection.save({"Hello4" : "World4" });
///     collection.save({"Hello5" : "World5" });
///
///     var url = "/_api/simple/all";
///     var body = '{ "collection": "products", "skip": 2, "limit" : 2 }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using a *batchSize* value
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleAllBatch}
///     var cn = "products";
///     db._drop(cn);
///     var collection = db._create(cn);
///     collection.save({"Hello1" : "World1" });
///     collection.save({"Hello2" : "World2" });
///     collection.save({"Hello3" : "World3" });
///     collection.save({"Hello4" : "World4" });
///     collection.save({"Hello5" : "World5" });
///
///     var url = "/_api/simple/all";
///     var body = '{ "collection": "products", "batchSize" : 3 }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestSimpleQueryHandler::allDocuments() {
  try {
    bool parseSuccess = true;
    VPackOptions options;
    std::shared_ptr<VPackBuilder> parsedBody =
        parseVelocyPackBody(&options, parseSuccess);

    if (!parseSuccess) {
      return;
    }
    VPackSlice body = parsedBody.get()->slice();

    VPackSlice const value = body.get("collection");

    if (!value.isString()) {
      generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting string for <collection>");
      return;
    }
    std::string collectionName = value.copyString();

    if (!collectionName.empty()) {
      auto const* col =
          TRI_LookupCollectionByNameVocBase(_vocbase, collectionName.c_str());

      if (col != nullptr && collectionName.compare(col->_name) != 0) {
        // user has probably passed in a numeric collection id.
        // translate it into a "real" collection name
        collectionName = std::string(col->_name);
      }
    }

    VPackBuilder bindVars;
    bindVars.openObject();
    bindVars.add("@collection", VPackValue(collectionName));

    std::string aql("FOR doc IN @@collection ");

    VPackSlice const skip = body.get("skip");
    VPackSlice const limit = body.get("limit");
    if (skip.isNumber() || limit.isNumber()) {
      aql.append("LIMIT @skip, @limit ");

      if (skip.isNumber()) {
        bindVars.add("skip", skip);
      } else {
        bindVars.add("skip", VPackValue(VPackValueType::Null));
      }

      if (limit.isNumber()) {
        bindVars.add("limit", limit);
      } else {
        bindVars.add("limit", VPackValue(VPackValueType::Null));
      }
    }
    bindVars.close();
    aql.append("RETURN doc");

    VPackBuilder data;
    data.openObject();
    data.add("query", VPackValue(aql));
    data.add("bindVars", bindVars.slice());
    data.add("count", VPackValue(true));

    // pass on standard options
    {
      VPackSlice ttl = body.get("ttl");
      if (!ttl.isNone()) {
        data.add("ttl", ttl);
      }

      VPackSlice batchSize = body.get("batchSize");
      if (!batchSize.isNone()) {
        data.add("batchSize", batchSize);
      }
    }
    data.close();

    VPackSlice s = data.slice();
    // now run the actual query and handle the result
    processQuery(s);
  } catch (triagens::basics::Exception const& ex) {
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  } catch (std::bad_alloc const&) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& ex) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
}


