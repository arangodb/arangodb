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
#include "Basics/StringUtils.h"
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
// --SECTION--                                               HttpHandler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestUploadHandler::execute () {
  // extract the request type
  const HttpRequest::HttpRequestType type = _request->requestType();

  if (type != HttpRequest::HTTP_REQUEST_POST) {
    generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);

    return status_t(HttpHandler::HANDLER_DONE);
  }

  char* filename = nullptr;
  std::string errorMessage;
  long systemError;

  if (TRI_GetTempName("uploads", &filename, false, systemError, errorMessage) != TRI_ERROR_NO_ERROR) {
    errorMessage = "could not generate temp file: " + errorMessage;
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, errorMessage);
    return status_t(HttpHandler::HANDLER_FAILED);
  }

  char* relative = TRI_GetFilename(filename);

  LOG_TRACE("saving uploaded file of length %llu in file '%s', relative '%s'",
            (unsigned long long) _request->bodySize(),
            filename,
            relative);

  char const* body = _request->body();
  size_t bodySize  = _request->bodySize();

  bool found;
  char const* value = _request->value("multipart", found);

  if (found) {
    bool multiPart = triagens::basics::StringUtils::boolean(value);

    if (multiPart) {
      if (! parseMultiPart(body, bodySize)) {
        TRI_Free(TRI_CORE_MEM_ZONE, relative);
        TRI_Free(TRI_CORE_MEM_ZONE, filename);
        generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, "invalid multipart request");
        return status_t(HttpHandler::HANDLER_FAILED);
      }
    }
  }

  try {
    FileUtils::spit(string(filename), body, bodySize);
    TRI_Free(TRI_CORE_MEM_ZONE, filename);
  }
  catch (...) {
    TRI_Free(TRI_CORE_MEM_ZONE, relative);
    TRI_Free(TRI_CORE_MEM_ZONE, filename);
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, "could not save file");
    return status_t(HttpHandler::HANDLER_FAILED);
  }

  char* fullName = TRI_Concatenate2File("uploads", relative);
  TRI_Free(TRI_CORE_MEM_ZONE, relative);

  // create the response
  _response = createResponse(HttpResponse::CREATED);
  _response->setContentType("application/json; charset=utf-8");

  TRI_json_t json;

  TRI_InitObjectJson(TRI_UNKNOWN_MEM_ZONE, &json);
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, &json, "filename", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, fullName, strlen(fullName)));
  TRI_Free(TRI_CORE_MEM_ZONE, fullName);

  generateResult(HttpResponse::CREATED, &json);
  TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, &json);

  // success
  return status_t(HttpHandler::HANDLER_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a multi-part request body and determines the boundaries of
/// its first element
////////////////////////////////////////////////////////////////////////////////

bool RestUploadHandler::parseMultiPart (char const*& body,
                                        size_t& length) {
  char const* beg = _request->body();
  char const* end = beg + _request->bodySize();

  while (beg < end && (*beg == '\r' || *beg == '\n' || *beg == ' ')) {
    ++beg;
  }

  // find delimiter
  char const* ptr = beg;
  while (ptr < end && *ptr == '-') {
    ++ptr;
  }

  while (ptr < end && *ptr != '\r' && *ptr != '\n') {
    ++ptr;
  }
  if (ptr == beg) {
    // oops
    return false;
  }

  std::string const delimiter(beg, ptr - beg);
  if (ptr < end && *ptr == '\r') {
    ++ptr;
  }
  if (ptr < end && *ptr == '\n') {
    ++ptr;
  }

  std::vector<std::pair<char const*, size_t>> parts;

  while (ptr < end) {
    char const* p = TRI_IsContainedMemory(ptr, end - ptr, delimiter.c_str(), delimiter.size());
    if (p == nullptr || p + delimiter.size() + 2 >= end || p - 2 <= ptr) {
      return false;
    }

    char const* q = p;
    if (*(q - 1) == '\n') {
      --q;
    }
    if (*(q - 1) == '\r') {
      --q;
    }
       
    parts.push_back(std::make_pair(ptr, q - ptr));
    ptr = p + delimiter.size();
    if (*ptr == '-' && *(ptr + 1) == '-') {
      // eom
      break;
    }
    if (*ptr == '\r') {
      ++ptr;
    }
    if (ptr < end && *ptr == '\n') {
      ++ptr;
    }
  }

  for (auto& part : parts) {
    auto ptr = part.first;
    auto end = part.first + part.second;
    char const* data = nullptr;

    while (ptr < end) {
      while (ptr < end && *ptr == ' ') {
        ++ptr;
      }
      if (ptr < end && (*ptr == '\r' || *ptr == '\n')) {
        // end of headers
        if (*ptr == '\r') {
          ++ptr;
        }
        if (ptr < end && *ptr == '\n') {
          ++ptr;
        }
        data = ptr;
        break;
      }

      // header line
      char const* eol = TRI_IsContainedMemory(ptr, end - ptr, "\r\n", 2);

      if (eol == nullptr) {
        eol = TRI_IsContainedMemory(ptr, end - ptr, "\n", 1);
      }

      if (eol == nullptr) {
        return false;
      }

      char const* colon = TRI_IsContainedMemory(ptr, end - ptr, ":", 1);
      if (colon == nullptr) {
        return false;
      }

      char const* p = colon; 
      while (p > ptr && *(p - 1) == ' ') {
        --p; 
      }

      ++colon;
      while (colon < eol && *colon == ' ') {
        ++colon;     
      }

      char const* q = eol;
      while (q > ptr && *(q - 1) == ' ') {
        --q; 
      }

      ptr = eol;
      if (*ptr == '\r') {
        ++ptr;
      }
      if (ptr < end && *ptr == '\n') {
        ++ptr;
      }
    }

    if (data == nullptr) {
      return false;
    }

    body = data;
    length = static_cast<size_t>(end - data);

    // stop after the first found element
    break;
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
