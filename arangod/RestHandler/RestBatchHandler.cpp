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
#include "BasicsC/string-buffer.h"
#include "Rest/HttpRequest.h"
#include "VocBase/vocbase.h"
//#include "ProtocolBuffers/arangodb.pb.h"

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
  : RestVocbaseBaseHandler(request, vocbase) {
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

HttpHandler::status_e RestBatchHandler::execute () {
  return HANDLER_DONE;
}

/*
  bool found;
  char const* valueStr = request->value("value", found);

  if (found) {
    sleep(::atoi(valueStr));
  }
  else {
    sleep(5);
  }
  // extract the request type
  HttpRequest::HttpRequestType type = request->requestType();
  string contentType = StringUtils::tolower(StringUtils::trim(request->header("content-type")));

  if (type != HttpRequest::HTTP_REQUEST_POST || contentType != "application/x-protobuf") {
    generateNotImplemented("ILLEGAL " + BATCH_PATH);
    return HANDLER_DONE;
  }

  LOGGER_INFO << "body size: " << request->bodySize();
  FILE* fp = fopen("got","w");
  fwrite(request->body(), request->bodySize(), 1, fp);
  fclose(fp);
  */
/*
  PB_ArangoMessage messages;
  bool result = messages.ParseFromArray(request->body(), request->bodySize());
  if (!result) {
    LOGGER_INFO << "invalid message";
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "invalid protobuf message");
    return HANDLER_DONE;
  }
  response = new HttpResponse(HttpResponse::CREATED);

  response->setContentType("application/x-protobuf");

  response->body().appendText("hihi");

        string contentType = StringUtils::tolower(StringUtils::trim(request->header("content-type")));
        if (request->requestType() == HttpRequest::HTTP_REQUEST_POST && contentType == "application/x-protobuf") {
          return handleProtobufRequest();
        } 

    HttpRequest::HttpRequestType getRequestTypeFromProtoBuf(const PB_ArangoRequestType type) {
      switch (type) {
        case PB_REQUEST_TYPE_DELETE:
          return HttpRequest::HTTP_REQUEST_DELETE;
        case PB_REQUEST_TYPE_GET:
          return HttpRequest::HTTP_REQUEST_GET;
        case PB_REQUEST_TYPE_HEAD:
          return HttpRequest::HTTP_REQUEST_HEAD;
        case PB_REQUEST_TYPE_POST:
          return HttpRequest::HTTP_REQUEST_POST;
        case PB_REQUEST_TYPE_PUT:
          return HttpRequest::HTTP_REQUEST_PUT;
      }
    }

    
    bool HttpCommTask::handleProtobufRequest () {
      PB_ArangoMessage messages;
      bool result = messages.ParseFromArray(request->body(), request->bodySize());
  
      if (!result) {
        LOGGER_INFO << "invalid message";
        generateError(HttpResponse::BAD,
                      TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                      "invalid protobuf message");
        return HANDLER_DONE;
      }

      for (int i = 0; i < messages.messages_size(); ++i) {
        PB_ArangoBatchMessage* message = messages(i);

        assert(message->type() == PB_BLOB_REQUEST);
  
        PB_ArangoBlobRequest* blob = message->request();
        HttpRequest::HttpRequestType requestType = getRequestTypeFromProtoBuf(blob->requesttype());

        const string url = blob->url();
      }    
      PB_ArangoKeyValue* kv;

      return HANDLER_DONE;
    }
*/

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
