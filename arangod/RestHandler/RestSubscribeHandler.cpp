////////////////////////////////////////////////////////////////////////////////
/// @brief document changes subscription handler
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestSubscribeHandler.h"
#include "Rest/HttpRequest.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestSubscribeHandler::RestSubscribeHandler (HttpRequest* request)
  : RestVocbaseBaseHandler(request) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestSubscribeHandler::execute () {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  switch (type) {
    case HttpRequest::HTTP_REQUEST_POST: addSubscription(); break;

    case HttpRequest::HTTP_REQUEST_ILLEGAL:
    default: {
      generateNotImplemented("ILLEGAL " + DOCUMENT_PATH);
      break;
    }
  }

  // this handler is done
  return status_t(HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

void RestSubscribeHandler::addSubscription () {
  std::unique_ptr<TRI_json_t> json(parseJsonBody());

  if (json == nullptr) {
    return;
  }

  if (json->_type != TRI_JSON_OBJECT) {
    generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR, "expecting object in body");
    return;
  }

  std::vector<TRI_voc_cid_t> collections;

  auto const* value = TRI_LookupObjectJson(json.get(), "collections");

  if (TRI_IsArrayJson(value)) {
    for (size_t i = 0; i < TRI_LengthArrayJson(value); ++i) {
      auto n = static_cast<TRI_json_t const*>(TRI_AtVector(&value->_value._objects, i));

      if (! TRI_IsStringJson(n)) {
        generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR, "expecting array of strings for 'collections'");
        return;
      }
 
      TRI_vocbase_col_t const* collection = TRI_LookupCollectionByNameVocBase(_vocbase, n->_value._string.data);

      if (collection == nullptr) {
        generateError(HttpResponse::NOT_FOUND, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
        return;
      }

      collections.emplace_back(collection->_cid);
    }
  }

  if (collections.empty()) {
    generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR, "expecting array of strings for 'collections'");
    return;
  }

  _response = createResponse(HttpResponse::OK);
  _response->setHeader("transfer-encoding", "chunked");
  _response->body().appendText("ok");

  AddChangeListeners(_request->clientTaskId(), collections); 
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
