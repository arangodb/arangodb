////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "RestUploadHandler.h"
#include "Basics/FileUtils.h"
#include "Basics/files.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "GeneralServer/GeneralServer.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestUploadHandler::RestUploadHandler(GeneralRequest* request,
                                     GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestUploadHandler::~RestUploadHandler() {}

RestHandler::status RestUploadHandler::execute() {
  // cast is ok because http requst is required
  HttpRequest* request = dynamic_cast<HttpRequest*>(_request.get());

  if (request == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  // extract the request type
  auto const type = request->requestType();

  if (type != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);

    return status::DONE;
  }

  char* filename = nullptr;
  std::string errorMessage;
  long systemError;

  if (TRI_GetTempName("uploads", &filename, false, systemError, errorMessage) !=
      TRI_ERROR_NO_ERROR) {
    errorMessage = "could not generate temp file: " + errorMessage;
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_INTERNAL, errorMessage);
    return status::FAILED;
  }

  char* relative = TRI_GetFilename(filename);

  std::string const& bodyStr = request->body();
  char const* body = bodyStr.c_str();
  size_t bodySize = bodyStr.size();

  LOG(TRACE) << "saving uploaded file of length " << bodySize << " in file '"
             << filename << "', relative '" << relative << "'";

  bool found;
  std::string const& value = request->value("multipart", found);

  if (found) {
    bool multiPart = arangodb::basics::StringUtils::boolean(value);

    if (multiPart) {
      if (!parseMultiPart(body, bodySize)) {
        TRI_Free(TRI_CORE_MEM_ZONE, relative);
        TRI_Free(TRI_CORE_MEM_ZONE, filename);
        generateError(rest::ResponseCode::SERVER_ERROR,
                      TRI_ERROR_INTERNAL, "invalid multipart request");
        return status::FAILED;
      }
    }
  }

  try {
    FileUtils::spit(std::string(filename), body, bodySize);
    TRI_Free(TRI_CORE_MEM_ZONE, filename);
  } catch (...) {
    TRI_Free(TRI_CORE_MEM_ZONE, relative);
    TRI_Free(TRI_CORE_MEM_ZONE, filename);
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_INTERNAL, "could not save file");
    return status::FAILED;
  }

  char* fullName = TRI_Concatenate2File("uploads", relative);
  TRI_Free(TRI_CORE_MEM_ZONE, relative);

  // create the response
  resetResponse(rest::ResponseCode::CREATED);

  VPackBuilder b;

  b.add(VPackValue(VPackValueType::Object));
  b.add("filename", VPackValue(fullName));
  TRI_Free(TRI_CORE_MEM_ZONE, fullName);
  b.close();

  VPackSlice s = b.slice();

  generateResult(rest::ResponseCode::CREATED, s);

  // success
  return status::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a multi-part request body and determines the boundaries of
/// its first element
////////////////////////////////////////////////////////////////////////////////

bool RestUploadHandler::parseMultiPart(char const*& body, size_t& length) {
  // cast is ok because http requst is required
  HttpRequest* request = dynamic_cast<HttpRequest*>(_request.get());

  if (request == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  std::string const& bodyStr = request->body();
  char const* beg = bodyStr.c_str();
  char const* end = beg + bodyStr.size();

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
    char const* p = TRI_IsContainedMemory(ptr, end - ptr, delimiter.c_str(),
                                          delimiter.size());
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
