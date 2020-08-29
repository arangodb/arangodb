////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "GeneralServer/GeneralServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestUploadHandler::RestUploadHandler(application_features::ApplicationServer& server,
                                     GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestUploadHandler::~RestUploadHandler() = default;

RestStatus RestUploadHandler::execute() {
  // extract the request type
  auto const type = _request->requestType();
  if (type != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);

    return RestStatus::DONE;
  }

  std::string filename;
  {
    std::string errorMessage;
    long systemError;

    if (TRI_GetTempName("uploads", filename, false, systemError, errorMessage) != TRI_ERROR_NO_ERROR) {
      errorMessage = "could not generate temp file: " + errorMessage;
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL, errorMessage);
      return RestStatus::DONE;
    }
  }

  std::string relativeString = TRI_GetFilename(filename);
  arangodb::velocypack::StringRef bodyStr = _request->rawPayload();
  char const* body = bodyStr.data();
  size_t bodySize = bodyStr.size();

  LOG_TOPIC("bbab9", TRACE, arangodb::Logger::FIXME)
      << "saving uploaded file of length " << bodySize << " in file '"
      << filename << "', relative '" << relativeString << "'";

  bool found;
  std::string const& value = _request->value("multipart", found);

  if (found) {
    bool multiPart = arangodb::basics::StringUtils::boolean(value);

    if (multiPart) {
      if (!parseMultiPart(body, bodySize)) {
        generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                      "invalid multipart request");
        return RestStatus::DONE;
      }
    }
  }

  try {
    FileUtils::spit(filename, body, bodySize);
  } catch (...) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "could not save file");
    return RestStatus::DONE;
  }

  std::string fullName = basics::FileUtils::buildFilename("uploads", relativeString);

  // create the response
  resetResponse(rest::ResponseCode::CREATED);

  VPackBuilder b;

  b.add(VPackValue(VPackValueType::Object));
  b.add("filename", VPackValue(fullName));
  b.close();

  VPackSlice s = b.slice();

  generateResult(rest::ResponseCode::CREATED, s);

  // success
  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a multi-part request body and determines the boundaries of
/// its first element
////////////////////////////////////////////////////////////////////////////////

bool RestUploadHandler::parseMultiPart(char const*& body, size_t& length) {
  if (_request->transportType() != Endpoint::TransportType::HTTP) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid request type");
  }

  VPackStringRef bodyPtr = _request->rawPayload();
  char const* beg = bodyPtr.data();
  char const* end = beg + bodyPtr.size();

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
    char const* p =
        TRI_IsContainedMemory(ptr, end - ptr, delimiter.c_str(), delimiter.size());
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
        // cppcheck-suppress nullPointerArithmeticRedundantCheck
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
