////////////////////////////////////////////////////////////////////////////////
/// @brief upload request handler
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestUploadHandler.h"

#include "Basics/FileUtils.h"
#include "BasicsC/files.h"
#include "Logger/Logger.h"
#include "HttpServer/HttpServer.h"
#include "Rest/HttpRequest.h"

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

RestUploadHandler::RestUploadHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RestUploadHandler::~RestUploadHandler () {
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

bool RestUploadHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestUploadHandler::queue () {
  static string const client = "STANDARD";

  return client;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Handler::status_e RestUploadHandler::execute() {
  // extract the request type
  const HttpRequest::HttpRequestType type = _request->requestType();

  if (type != HttpRequest::HTTP_REQUEST_POST) { 
    generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);

    return Handler::HANDLER_DONE;
  }

  char* filename = NULL;
  
  if (TRI_GetTempName("uploads", &filename, true) != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, "could not generate temp file");
    return Handler::HANDLER_FAILED;
  }

  char* relative = TRI_GetRelativePath(filename);

  try {
    FileUtils::spit(string(filename), _request->body(), _request->bodySize());
  }
  catch (...) {
    TRI_Free(TRI_CORE_MEM_ZONE, filename);
    TRI_Free(TRI_CORE_MEM_ZONE, relative);
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, "could not save file");
    return Handler::HANDLER_FAILED;
  }
 
  // create the response
  _response = createResponse(HttpResponse::CREATED);
  _response->setContentType("application/json; charset=utf-8");
  _response->body().appendText("{\"filename\":\"uploads/").appendText(relative).appendText("\"}");
    
  TRI_Free(TRI_CORE_MEM_ZONE, filename);
  TRI_Free(TRI_CORE_MEM_ZONE, relative);

  // success
  return Handler::HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
