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
  if (_outputMessages) {
    // delete protobuf message
    delete _outputMessages;
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

  if (httpServer == 0) {
    LOGGER_WARNING << "cannot convert AsyncJobServer into a HttpServer";
    return 0;
  }

  BatchJob* batchJob = new BatchJob(httpServer, this);

  return batchJob;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////
        
vector<HttpHandler*> RestBatchHandler::subhandlers () {
  return _handlers;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Handler::status_e RestBatchHandler::execute() {
  // extract the request type
  HttpRequest::HttpRequestType type = _request->requestType();
  string contentType = StringUtils::tolower(StringUtils::trim(_request->header("content-type")));
  
  if (type != HttpRequest::HTTP_REQUEST_POST || contentType != getContentType()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid request data sent");

    return Handler::HANDLER_FAILED;
  }

  if (! _inputMessages.ParseFromArray(_request->body(), _request->bodySize())) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid protobuf input message");

    return Handler::HANDLER_FAILED;
  }


  // loop over the input messages once to set up the output structures without concurrency
  for (int i = 0; i < _inputMessages.messages_size(); ++i) {
    _outputMessages->add_messages();
  
    // create a handler for each input part 
    const PB_ArangoBatchMessage* inputMessage = _inputMessages.mutable_messages(i);
    HttpRequestProtobuf* request = new HttpRequestProtobuf(*inputMessage);

    if (! request) {
      return Handler::HANDLER_FAILED;
    }

    // handler is responsible for freeing the request later
    HttpHandler* handler = _server->createHandler(request);

    if (! handler) {
      delete request;

      return Handler::HANDLER_FAILED;
    }
    
    ++_missingResponses;
    _handlers.push_back(handler);
  }

  // success
  return Handler::HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a single handler response to the output array
////////////////////////////////////////////////////////////////////////////////
  
void RestBatchHandler::addResponse (HttpHandler* handler) {
  for (size_t i = 0; i < _handlers.size(); ++i) {
    if (_handlers[i] == handler) {

      // avoid concurrent modifications to the structure
      MUTEX_LOCKER(_handlerLock);
      PB_ArangoBatchMessage* batch = _outputMessages->mutable_messages(i);

      handler->getResponse()->write(batch);
      if (--_missingResponses == 0) {
        assembleResponse();
      }
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
  assert(_missingResponses == 0);

  _response = new HttpResponse(HttpResponse::OK);
  _response->setContentType(getContentType());

  string data;
  if (!_outputMessages->SerializeToString(&data)) {
    delete _response;

    generateError(HttpResponse::SERVER_ERROR,
                  TRI_ERROR_OUT_OF_MEMORY,
                  "out of memory");
    return;

  }

  _response->body().appendText(data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the required content type string
////////////////////////////////////////////////////////////////////////////////

string const& RestBatchHandler::getContentType () {
  static string const contentType = "application/x-protobuf"; 
  
  return contentType;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief destroy handlers in case setup went wrong
////////////////////////////////////////////////////////////////////////////////
  
void RestBatchHandler::destroyHandlers () {
  for (size_t i = 0; i < _handlers.size(); ++i) {
    HttpHandler* handler = _handlers[i];

    delete handler;
  }

  _handlers.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
