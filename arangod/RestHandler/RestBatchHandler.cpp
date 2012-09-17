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
  return true;
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
                  "invalid request message data sent");

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

    Handler::status_e status = Handler::HANDLER_FAILED;
    do {
      try {
        status = handler->execute();
      }
      catch (triagens::basics::TriagensError const& ex) {
        handler->handleError(ex);
      }
      catch (std::exception const& ex) {
        triagens::basics::InternalError err(ex, __FILE__, __LINE__);

        handler->handleError(err);
      }
      catch (...) {
        triagens::basics::InternalError err("executeDirectHandler", __FILE__, __LINE__);
        handler->handleError(err);
      }
    }
    while (status == Handler::HANDLER_REQUEUE);
   
    if (status == Handler::HANDLER_DONE) {
      PB_ArangoBatchMessage* batch = _outputMessages->mutable_messages(i);
      handler->getResponse()->write(batch);
    }

    delete handler;

    if (status == Handler::HANDLER_FAILED) {
      return Handler::HANDLER_FAILED;
    }
  }
  
  size_t messageSize = _outputMessages->ByteSize();
  
  // allocate output buffer
  char* output = (char*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(char) * messageSize, false);
  if (output == NULL) {
    generateError(HttpResponse::SERVER_ERROR,
                  TRI_ERROR_OUT_OF_MEMORY,
                  "out of memory");
    return Handler::HANDLER_FAILED;
  }

  _response = new HttpResponse(HttpResponse::OK);
  _response->setContentType(getContentType());

  // content of message is binary, cannot use null-terminated std::string
  if (!_outputMessages->SerializeToArray(output, messageSize)) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, output);
    delete _response;

    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY, "out of memory");
    return Handler::HANDLER_FAILED;
  }
  
  _response->body().appendText(output, messageSize);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, output);

  // success
  return Handler::HANDLER_DONE;
}

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
