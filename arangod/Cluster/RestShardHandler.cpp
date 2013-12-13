////////////////////////////////////////////////////////////////////////////////
/// @brief shard control request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestShardHandler.h"

#include "Cluster/ServerState.h"
#include "Dispatcher/Dispatcher.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

#include "HttpServer/HttpServer.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "GeneralServer/GeneralServerJob.h"
#include "GeneralServer/GeneralServer.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the queue
////////////////////////////////////////////////////////////////////////////////
  
const string RestShardHandler::QUEUE_NAME = "STANDARD";

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestShardHandler::RestShardHandler (triagens::rest::HttpRequest* request,
                                    void* data) 
  : RestBaseHandler(request),
    _dispatcher(0) {
  
  _dispatcher = static_cast<triagens::rest::Dispatcher*>(data);    

  assert(_dispatcher != 0);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestShardHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestShardHandler::queue () const {
  return QUEUE_NAME;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

triagens::rest::HttpHandler::status_e RestShardHandler::execute () {
  ServerState::RoleEnum role = ServerState::instance()->getRole();

  if (role == ServerState::ROLE_COORDINATOR) {
    generateError(triagens::rest::HttpResponse::BAD, 
                  (int) triagens::rest::HttpResponse::BAD, 
                  "this API is meant to be called on a coordinator node");
    return HANDLER_DONE;
  }
/*
  bool found;
  char const* coordinator = _request->header("x-arango-coordinator", found);

  if (! found) {
    generateError(triagens::rest::HttpResponse::BAD, 
                  (int) triagens::rest::HttpResponse::BAD, 
                  "header 'x-arango-coordinator' is missing");
    return HANDLER_DONE;
  }
  
  char const* operation = _request->header("x-arango-operation", found);
  if (! found) {
    generateError(triagens::rest::HttpResponse::BAD, 
                  (int) triagens::rest::HttpResponse::BAD, 
                  "header 'x-arango-operation' is missing");
  }
  
  char const* url = _request->header("x-arango-url", found);
  if (! found) {
    generateError(triagens::rest::HttpResponse::BAD, 
                  (int) triagens::rest::HttpResponse::BAD, 
                  "header 'x-arango-url' is missing");
  }
*/

/*
  triagens::rest::HttpHandler* handler = this->_server->createHandler(_request);
  triagens::rest::Job* job = new triagens::rest::GeneralServerJob<triagens::rest::HttpServer, triagens::rest::HttpHandlerFactory::GeneralHandler>(0, handler, true);
          
  _dispatcher->addJob(job);
*/  
  // respond with a 202
  _response = createResponse(triagens::rest::HttpResponse::ACCEPTED);

  return HANDLER_DONE;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
