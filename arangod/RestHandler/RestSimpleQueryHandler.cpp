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
#include "Basics/json.h"
#include "Basics/MutexLocker.h"
#include "Basics/ScopeGuard.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "V8Server/ApplicationV8.h"

using namespace triagens::arango;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestSimpleQueryHandler::RestSimpleQueryHandler (HttpRequest* request,
                                                std::pair<triagens::arango::ApplicationV8*, triagens::aql::QueryRegistry*>* pair) 
  : RestCursorHandler(request, pair) {

}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestSimpleQueryHandler::execute () {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  if (type == HttpRequest::HTTP_REQUEST_PUT) {
    char const* prefix = _request->requestPath();

    if (strcmp(prefix, RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH.c_str()) == 0) {
      // all query
      allDocuments();
      return status_t(HANDLER_DONE);
    }
  }

  generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED); 
  return status_t(HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_all
/// @brief returns all documents of a collection
///
/// @RESTHEADER{PUT /_api/simple/all, Return all documents}
///
/// @RESTBODYPARAM{query,string,required}
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
/// Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
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

void RestSimpleQueryHandler::allDocuments () {
  try { 
    std::unique_ptr<TRI_json_t> json(parseJsonBody());

    if (json.get() == nullptr) {
      return;
    }

    auto const value = TRI_LookupObjectJson(json.get(), "collection");

    if (! TRI_IsStringJson(value)) {
      generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR, "expecting string for <collection>");
      return;
    }
    
    std::string collectionName = std::string(value->_value._string.data, value->_value._string.length - 1);

    if (! collectionName.empty()) {
      auto const* col = TRI_LookupCollectionByNameVocBase(_vocbase, collectionName.c_str());

      if (col != nullptr && collectionName.compare(col->_name) != 0) {
        // user has probably passed in a numeric collection id.
        // translate it into a "real" collection name
        collectionName = std::string(col->_name);
      }
    }

    triagens::basics::Json bindVars(triagens::basics::Json::Object, 3);
    bindVars("@collection", triagens::basics::Json(collectionName));

    std::string aql("FOR doc IN @@collection ");
      
    auto const skip  = TRI_LookupObjectJson(json.get(), "skip");
    auto const limit = TRI_LookupObjectJson(json.get(), "limit");

    if (TRI_IsNumberJson(skip) || TRI_IsNumberJson(limit)) {
      aql.append("LIMIT @skip, @limit ");

      if (TRI_IsNumberJson(skip)) {
        bindVars("skip", triagens::basics::Json(skip->_value._number));
      }
      else {
        bindVars("skip", triagens::basics::Json(triagens::basics::Json::Null));
      }

      if (TRI_IsNumberJson(limit)) {
        bindVars("limit", triagens::basics::Json(limit->_value._number));
      }
      else {
        bindVars("limit", triagens::basics::Json(triagens::basics::Json::Null));
      }
    }

    aql.append("RETURN doc");

    triagens::basics::Json data(triagens::basics::Json::Object, 5);
    data("query", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, aql));
    data("bindVars", bindVars);
    data("count", triagens::basics::Json(true));

    // pass on standard options
    {
      auto value = TRI_LookupObjectJson(json.get(), "ttl");

      if (value != nullptr) {
        data("ttl", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, value)));
      }

      value = TRI_LookupObjectJson(json.get(), "batchSize");

      if (value != nullptr) {
        data("batchSize", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, value)));
      }
    }
  
    // now run the actual query and handle the result
    processQuery(data.json());
  }
  catch (triagens::basics::Exception const& ex) {
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  }
  catch (std::bad_alloc const&) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  }
  catch (std::exception const& ex) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, ex.what());
  }
  catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
