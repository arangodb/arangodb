////////////////////////////////////////////////////////////////////////////////
/// @brief upload request handler
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestUploadHandler.h"

#include "Basics/FileUtils.h"
#include "Basics/files.h"
#include "Basics/logging.h"
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
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestUploadHandler::RestUploadHandler (HttpRequest* request)
  : RestVocbaseBaseHandler(request) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RestUploadHandler::~RestUploadHandler () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Handler::status_t RestUploadHandler::execute () {
  // extract the request type
  const HttpRequest::HttpRequestType type = _request->requestType();

  if (type != HttpRequest::HTTP_REQUEST_POST) {
    generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);

    return status_t(Handler::HANDLER_DONE);
  }

  char* filename = NULL;

  if (TRI_GetTempName("uploads", &filename, false) != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, "could not generate temp file");
    return status_t(Handler::HANDLER_FAILED);
  }

  char* relative = TRI_GetFilename(filename);

  LOG_TRACE("saving uploaded file of length %llu in file '%s', relative '%s'",
            (unsigned long long) _request->bodySize(),
            filename,
            relative);

  try {
    FileUtils::spit(string(filename), _request->body(), _request->bodySize());
    TRI_Free(TRI_CORE_MEM_ZONE, filename);
  }
  catch (...) {
    TRI_Free(TRI_CORE_MEM_ZONE, relative);
    TRI_Free(TRI_CORE_MEM_ZONE, filename);
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, "could not save file");
    return status_t(Handler::HANDLER_FAILED);
  }

  char* fullName = TRI_Concatenate2File("uploads", relative);
  TRI_Free(TRI_CORE_MEM_ZONE, relative);

  // create the response
  _response = createResponse(HttpResponse::CREATED);
  _response->setContentType("application/json; charset=utf-8");

  TRI_json_t json;

  TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &json);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &json, "filename", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, fullName));

  generateResult(HttpResponse::CREATED, &json);
  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &json);

  // success
  return status_t(Handler::HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
