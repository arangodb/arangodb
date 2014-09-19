////////////////////////////////////////////////////////////////////////////////
/// @brief AQL query/cursor request handler
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestAqlHandler.h"

#include "Basics/ConditionLocker.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

#include "HttpServer/HttpServer.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "GeneralServer/GeneralServerJob.h"
#include "GeneralServer/GeneralServer.h"

using namespace std;
using namespace triagens::arango;
using namespace triagens::rest;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the queue
////////////////////////////////////////////////////////////////////////////////

const string RestAqlHandler::QUEUE_NAME = "STANDARD";

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestAqlHandler::RestAqlHandler (triagens::rest::HttpRequest* request,
                   std::pair<ApplicationV8*, QueryRegistry*>* pair)
  : RestBaseHandler(request),
    _applicationV8(pair->first),
    _context(static_cast<VocbaseContext*>(request->getRequestContext())),
    _vocbase(_context->getVocbase()),
    _queryRegistry(pair->second) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestAqlHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestAqlHandler::queue () const {
  return QUEUE_NAME;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void RestAqlHandler::createQuery () {
  _response = createResponse(triagens::rest::HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  _response->body().appendText("{\"a\":12}");

}

void RestAqlHandler::deleteQuery () {
  _response = createResponse(triagens::rest::HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  _response->body().appendText("{\"b\":12}");
}

void RestAqlHandler::useQuery () {
  _response = createResponse(triagens::rest::HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  _response->body().appendText("{\"c\":12}");
}

void RestAqlHandler::getInfoQuery () {
  _response = createResponse(triagens::rest::HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  _response->body().appendText("{\"d\":12}");
}

triagens::rest::HttpHandler::status_t RestAqlHandler::execute () {
  auto context = _applicationV8->enterContext("STANDARD", _vocbase, nullptr,
                                              false, false);
  if (nullptr == context) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot enter V8 context");
  }
  else {
    // extract the sub-request type
    HttpRequest::HttpRequestType type = _request->requestType();

    // execute one of the CRUD methods
    switch (type) {
      case HttpRequest::HTTP_REQUEST_POST: {
        createQuery(); 
        break;
      }
      case HttpRequest::HTTP_REQUEST_DELETE: {
        deleteQuery();
        break;
      }
      case HttpRequest::HTTP_REQUEST_PUT: {
        useQuery();
        break;
      }
      case HttpRequest::HTTP_REQUEST_GET: {
        getInfoQuery();
        break;
      }
      case HttpRequest::HTTP_REQUEST_HEAD: {
        _response = createResponse(triagens::rest::HttpResponse::METHOD_NOT_ALLOWED);
        break;
      }
      case HttpRequest::HTTP_REQUEST_PATCH:
      case HttpRequest::HTTP_REQUEST_OPTIONS:
      case HttpRequest::HTTP_REQUEST_ILLEGAL: {
        generateError(HttpResponse::METHOD_NOT_ALLOWED, 
                      TRI_ERROR_NOT_IMPLEMENTED,
                      "illegal method for /_api/aql");
        break;
      }
    }

  }

  _applicationV8->exitContext(context);

  return status_t(HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
