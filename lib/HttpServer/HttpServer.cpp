////////////////////////////////////////////////////////////////////////////////
/// @brief http server implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpServer.h"

#include "HttpServer/ServiceUnavailableHandler.h"
#include "Rest/HttpRequestPlain.h"

using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new http server
////////////////////////////////////////////////////////////////////////////////

HttpServer::HttpServer (Scheduler* scheduler, Dispatcher* dispatcher)
  : GeneralServerDispatcher<HttpServer, HttpHandlerFactory, HttpCommTask>(scheduler, dispatcher),
    _closeWithoutKeepAlive(false) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return the scheduler
////////////////////////////////////////////////////////////////////////////////

Scheduler* HttpServer::getScheduler () {
  return _scheduler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the dispatcher
////////////////////////////////////////////////////////////////////////////////

Dispatcher* HttpServer::getDispatcher () {
  return _dispatcher;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if to close connection if keep-alive is missing
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::getCloseWithoutKeepAlive () const {
  return _closeWithoutKeepAlive;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close connection if keep-alive is missing
////////////////////////////////////////////////////////////////////////////////

void HttpServer::setCloseWithoutKeepAlive (bool value) {
  _closeWithoutKeepAlive = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
