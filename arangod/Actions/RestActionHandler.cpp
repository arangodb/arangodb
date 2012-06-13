////////////////////////////////////////////////////////////////////////////////
/// @brief action request handler
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
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestActionHandler.h"

#include "Actions/ActionDispatcherThread.h"
#include "Actions/actions.h"
#include "Basics/StringUtils.h"
#include "Rest/HttpRequest.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestActionHandler::RestActionHandler (HttpRequest* request, action_options_t* data)
  : RestVocbaseBaseHandler(request, data->_vocbase),
    _action(0),
    _context(0),
    _queue() {
  _action = TRI_LookupActionVocBase(request);

  if (_action == 0) {
    _queue = "STANDARD";
  }
  else {
    _queue = data->_queue + _action->_type;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestActionHandler::isDirect () {
  return _action == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestActionHandler::queue () {
  return _queue;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void RestActionHandler::setDispatcherThread (DispatcherThread* thread) {
  ActionDispatcherThread* actionThread = dynamic_cast<ActionDispatcherThread*>(thread);

  if (actionThread != 0) {
    _context = actionThread->context();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestActionHandler::execute () {
  static LoggerData::Task const logExecute("ACTION [execute]");
  static LoggerData::Task const logHead("ACTION [head]");
  static LoggerData::Task const logIllegal("ACTION [illegal]");

  LoggerData::Task const * task = &logIllegal;

  bool res = false;

  // need an action
  if (_action == 0) {
    string n = request->requestPath();
    n += StringUtils::join(request->suffix(), "/");

    generateNotImplemented(n);
  }

  // need a context
  else if (_context == 0) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, "no execution context is known");
  }

  // execute
  else {

    // extract the sub-request type
    HttpRequest::HttpRequestType type = request->requestType();

    // prepare logging
    switch (type) {
      case HttpRequest::HTTP_REQUEST_DELETE: task = &logExecute; break;
      case HttpRequest::HTTP_REQUEST_GET: task = &logExecute; break;
      case HttpRequest::HTTP_REQUEST_POST: task = &logExecute; break;
      case HttpRequest::HTTP_REQUEST_PUT: task = &logExecute; break;
      case HttpRequest::HTTP_REQUEST_HEAD: task = &logHead; break;
      case HttpRequest::HTTP_REQUEST_ILLEGAL: task = &logIllegal; break;
    }

    _timing << *task;
    LOGGER_REQUEST_IN_START_I(_timing);

    // execute one of the CRUD methods
    switch (type) {
      case HttpRequest::HTTP_REQUEST_GET: res = executeAction(); break;
      case HttpRequest::HTTP_REQUEST_POST: res = executeAction(); break;
      case HttpRequest::HTTP_REQUEST_PUT: res = executeAction(); break;
      case HttpRequest::HTTP_REQUEST_DELETE: res = executeAction(); break;
      case HttpRequest::HTTP_REQUEST_HEAD: res = executeAction(); break;

      default:
        res = false;
        generateNotImplemented("METHOD");
        break;
    }
  }

  _timingResult = res ? RES_ERR : RES_OK;

  // this handler is done
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

bool RestActionHandler::executeAction () {
  response = _action->execute(_vocbase, _context, request);

  if (response == 0) {
    generateNotImplemented(_action->_url);
    return false;
  }

  return (int) response->responseCode() < 400;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
