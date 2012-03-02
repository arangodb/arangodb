////////////////////////////////////////////////////////////////////////////////
/// @brief action request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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

#include "Basics/StringUtils.h"
#include "Rest/HttpRequest.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::avocado;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestActionHandler::RestActionHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase),
    _action(0) {
  _action = TRI_LookupActionVocBase(request);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestActionHandler::queue () {
  return _action->_queue;
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

  // extract the sub-request type
  if (_action != 0) {
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
        generateNotImplemented("ILLEGAL METHODS");
        break;
    }
  }
  else {
    generateNotImplemented("ILLEGAL PATH" + _action->_url);
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
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

bool RestActionHandler::executeAction () {
  response = TRI_ExecuteActionVocBase(_vocbase, _action, request);

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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
