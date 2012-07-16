////////////////////////////////////////////////////////////////////////////////
/// @brief batch request handler
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestBatchHandler.h"

#include "Basics/StringUtils.h"
#include "Basics/MutexLocker.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "VocBase/vocbase.h"
#include "GeneralServer/GeneralCommTask.h"
#include "GeneralServer/GeneralServerJob.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpServer.h"
#include "ProtocolBuffers/HttpRequestProtobuf.h"

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

RestBatchHandler::RestBatchHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase),
    _missingResponses(0),
    _outputMessages(new PB_ArangoMessage) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RestBatchHandler::~RestBatchHandler () {
  // delete protobuf message
  delete _outputMessages;
 
  // clear all handlers that still exist
  for (size_t i = 0; i < _handlers.size(); ++i) {
    HttpHandler* handler = _handlers[i];

    if (handler != 0) {
      // TODO
//      _task->getServer()->destroyHandler(handler);
      _handlers[i] = 0;
    }
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

bool RestBatchHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestBatchHandler::queue () {
  static string const client = "STANDARD";

  return client;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job* RestBatchHandler::createJob (AsyncJobServer* server) {
  HttpServer* httpServer = dynamic_cast<HttpServer*>(server);

std::cout << "createJob()\n";

  if (httpServer == 0) {
    LOGGER_WARNING << "cannot convert AsyncJobServer into a HttpServer";
    return 0;
  }

  BatchJob* b = new BatchJob(httpServer, this);

  return b;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////
        
vector<HttpHandler*> RestBatchHandler::subhandlers () {
std::cout << "subhandlers()\n";
  return _handlers;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestBatchHandler::execute () {
std::cout << "execute()\n";
  // extract the request type
  HttpRequest::HttpRequestType type = _request->requestType();
  string contentType = StringUtils::tolower(StringUtils::trim(_request->header("content-type")));
  
  if (type != HttpRequest::HTTP_REQUEST_POST || contentType != getContentType()) {
    generateNotImplemented("ILLEGAL " + BATCH_PATH);
    return HANDLER_DONE;
  }

  PB_ArangoMessage inputMessages;

  bool result = inputMessages.ParseFromArray(_request->body(), _request->bodySize());
  if (!result) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid protobuf message");
    return HANDLER_DONE;
  }

  bool failed = false;

  // loop over the input messages once to set up the output structures without concurrency
  for (int i = 0; i < inputMessages.messages_size(); ++i) {
    _outputMessages->add_messages();
  
    // create a handler for each input part 
    const PB_ArangoBatchMessage inputMessage = inputMessages.messages(i);
    HttpRequestProtobuf* request = new HttpRequestProtobuf(inputMessage);
    HttpHandler* handler = _server->createHandler(request);

    if (!handler) {
      failed = true;
      break;
    }
    else {
      _handlers.push_back(handler);
      ++_missingResponses;
    }
  }

  if (failed) {
    // TODO: handle error!
    std::cout << "SOMETHING FAILED--------------------------------------------------------------------------\n";
    return Handler::HANDLER_DONE;
  }
  
  // we have async jobs
  return Handler::HANDLER_DETACH;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a single handler response to the output array
////////////////////////////////////////////////////////////////////////////////
  
void RestBatchHandler::addResponse (HttpHandler* handler) {
std::cout << "addResponse\n";
  for (size_t i = 0; i < _handlers.size(); ++i) {
    if (_handlers[i] == handler) {
      // avoid concurrent modifications to the structure
      MUTEX_LOCKER(_handlerLock);
      PB_ArangoBatchMessage* batch = _outputMessages->mutable_messages(i);

      handler->getResponse()->write(batch);
      // delete the handler
//      _task->getServer()->destroyHandler(handler);
//      _handlers[i] = 0;
      return;
    }
  }
  
  // handler not found
  LOGGER_WARNING << "handler not found. this should not happen."; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an overall protobuf response from the array of responses
////////////////////////////////////////////////////////////////////////////////

void RestBatchHandler::assembleResponse () {
std::cout << "assembleResponse\n";
  assert(_missingResponses == 0);

  _response = new HttpResponse(HttpResponse::OK);
  _response->setContentType(getContentType());

  string data;
  if (!_outputMessages->SerializeToString(&data)) {
    // TODO
  }
  _response->body().appendText(data);
}
/*
////////////////////////////////////////////////////////////////////////////////
/// @brief convert a Job* to a GeneralServerJob* 
////////////////////////////////////////////////////////////////////////////////

GeneralServerJob<HttpServer, HttpHandlerFactory::GeneralHandler>* RestBatchHandler::toServerJob(Job* job) {
  return dynamic_cast<GeneralServerJob<HttpServer, HttpHandlerFactory::GeneralHandler> * >(job);
}
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief return the required content type string
////////////////////////////////////////////////////////////////////////////////

string const& RestBatchHandler::getContentType () {
  static string const contentType = "application/x-protobuf"; 
  
  return contentType;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
