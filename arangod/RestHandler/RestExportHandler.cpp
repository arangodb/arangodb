////////////////////////////////////////////////////////////////////////////////
/// @brief export request handler
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

#include "RestExportHandler.h"
#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/MutexLocker.h"
#include "Utils/CollectionExport.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "Wal/LogfileManager.h"

using namespace triagens::arango;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestExportHandler::RestExportHandler (HttpRequest* request) 
  : RestVocbaseBaseHandler(request) {

}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestExportHandler::execute () {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  if (type == HttpRequest::HTTP_REQUEST_POST) {
    createCursor();
    return status_t(HANDLER_DONE);
  }

  generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED); 
  return status_t(HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief build options for the query as JSON
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json RestExportHandler::buildOptions (TRI_json_t const* json) {
  auto getAttribute = [&json] (char const* name) {
    return TRI_LookupObjectJson(json, name);
  };

  triagens::basics::Json options(triagens::basics::Json::Object);

  auto attribute = getAttribute("count");
  options.set("count", triagens::basics::Json(TRI_IsBooleanJson(attribute) ? attribute->_value._boolean : false));

  attribute = getAttribute("batchSize");
  options.set("batchSize", triagens::basics::Json(TRI_IsNumberJson(attribute) ? attribute->_value._number : 1000.0));

  if (TRI_IsNumberJson(attribute) && static_cast<size_t>(attribute->_value._number) == 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TYPE_ERROR, "expecting non-zero value for <batchSize>");
  }

  if (! options.has("ttl")) {
    attribute = getAttribute("ttl");
    options.set("ttl", triagens::basics::Json(TRI_IsNumberJson(attribute) ? attribute->_value._number : 30.0));
  }

  return options;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_export
/// @brief export all documents from a collection, using a cursor
///
/// @RESTHEADER{POST /_api/export, Create export cursor}
///
/// @RESTBODYPARAM{options,json,optional}
/// A JSON object with export options.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The name of the collection to export.
///
/// @RESTDESCRIPTION
/// A call to this method creates a cursor containing all documents in the 
/// specified collection. In contrast to other data-producing APIs, the internal
/// data structures produced by the export API are more lightweight, so it is
/// the preferred way to retrieve all documents from a collection.
///
/// The following attributes can be used inside the JSON object:
///
/// - *count*: boolean flag that indicates whether the number of documents
///   in the result set should be returned in the "count" attribute of the result (optional).
///   Calculating the "count" attribute might in the future have a performance
///   impact so this option is turned off by default, and "count" is only returned 
///   when requested.
///
/// - *batchSize*: maximum number of result documents to be transferred from
///   the server to the client in one roundtrip (optional). If this attribute is
///   not set, a server-controlled default value will be used.
///
/// - *ttl*: an optional time-to-live for the cursor (in seconds). The cursor will be
///   removed on the server automatically after the specified amount of time. This
///   is useful to ensure garbage collection of cursors that are not fully fetched
///   by clients. If not set, a server-defined value will be used.
///
/// If the result set can be created by the server, the server will respond with
/// *HTTP 201*. The body of the response will contain a JSON object with the
/// result set.
///
/// The returned JSON object has the following properties:
///
/// - *error*: boolean flag to indicate that an error occurred (*false*
///   in this case)
///
/// - *code*: the HTTP status code
///
/// - *result*: an array of result documents (might be empty if the collection was empty)
///
/// - *hasMore*: a boolean indicator whether there are more results
///   available for the cursor on the server
///
/// - *count*: the total number of result documents available (only
///   available if the query was executed with the *count* attribute set)
///
/// - *id*: id of temporary cursor created on the server (optional, see above)
///
/// If the JSON representation is malformed or the query specification is
/// missing from the request, the server will respond with *HTTP 400*.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - *error*: boolean flag to indicate that an error occurred (*true* in this case)
///
/// - *code*: the HTTP status code
///
/// - *errorNum*: the server error number
///
/// - *errorMessage*: a descriptive error message
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the result set can be created by the server.
///
/// @RESTRETURNCODE{400}
/// is returned if the JSON representation is malformed or the query specification is
/// missing from the request.
///
/// @RESTRETURNCODE{404}
/// The server will respond with *HTTP 404* in case a non-existing collection is
/// accessed in the query.
///
/// @RESTRETURNCODE{405}
/// The server will respond with *HTTP 405* if an unsupported HTTP method is used.
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestExportHandler::createCursor () {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/export");
    return;
  }
  
  // extract the cid
  bool found;
  char const* name = _request->value("collection", found);

  if (! found || *name == '\0') {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + EXPORT_PATH + "?collection=<identifier>");
    return;
  }

  try { 
    std::unique_ptr<TRI_json_t> json(parseJsonBody());
 
    triagens::basics::Json options;

    if (json.get() != nullptr) {
      if (! TRI_IsObjectJson(json.get())) {
        generateError(HttpResponse::BAD, TRI_ERROR_QUERY_EMPTY);
        return;
      }
    
      options = buildOptions(json.get());
    }
    else {
      // create an empty options object
      options = triagens::basics::Json(triagens::basics::Json::Object);
    }
      
    bool flush = triagens::basics::JsonHelper::getBooleanValue(options.json(), "flush", true);

    if (flush) {
      // flush the logfiles so the export can fetch all documents
      int res = triagens::wal::LogfileManager::instance()->flush(true, true, false);

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    // this may throw!
    std::unique_ptr<CollectionExport> collectionExport(new CollectionExport(_vocbase, name));
    collectionExport->run();

    { 
      _response = createResponse(HttpResponse::CREATED);
      _response->setContentType("application/json; charset=utf-8");

      size_t batchSize = triagens::basics::JsonHelper::getNumericValue<size_t>(options.json(), "batchSize", 1000);
      double ttl = triagens::basics::JsonHelper::getNumericValue<double>(options.json(), "ttl", 30);
      bool count = triagens::basics::JsonHelper::getBooleanValue(options.json(), "count", false);

      auto cursors = static_cast<triagens::arango::CursorRepository*>(_vocbase->_cursorRepository);
      TRI_ASSERT(cursors != nullptr);
      
      // create a cursor from the result
      triagens::arango::Cursor* cursor = cursors->createFromExport(collectionExport.get(), batchSize, ttl, count); 
      collectionExport.release();

      try {
        cursor->dump(_response->body());
        cursors->release(cursor);
      }
      catch (...) {
        cursors->release(cursor);
        throw;
      }
    }
  }  
  catch (triagens::basics::Exception const& ex) {
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
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
