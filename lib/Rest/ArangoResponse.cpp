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
////////////////////////////////////////////////////////////////////////////////

#include "ArangoResponse.h"
#include "Basics/logging.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"


#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace std;

////////////////////////////////////////////////////////////////////////////////
/// @brief batch error count header
////////////////////////////////////////////////////////////////////////////////

std::string const ArangoResponse::BatchErrorHeader = "X-Arango-Errors";

////////////////////////////////////////////////////////////////////////////////
/// @brief hide header "Server: ArangoDB" in HTTP/VSTREAM responses
////////////////////////////////////////////////////////////////////////////////

bool ArangoResponse::HideProductHeader = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new http/vstream response
////////////////////////////////////////////////////////////////////////////////

ArangoResponse::ArangoResponse(ResponseCode code, uint32_t apiCompatibility)
    : _code(code),
      _apiCompatibility(apiCompatibility),
      _isHeadResponse(false),
      _isChunked(false),
      _headers(6),
      _bodySize(0),
      _freeables() {
  if (!HideProductHeader) {
    _headers.insert(TRI_CHAR_LENGTH_PAIR("server"), "ArangoDB");
  }
  _headers.insert(TRI_CHAR_LENGTH_PAIR("connection"), "Keep-Alive");
  _headers.insert(TRI_CHAR_LENGTH_PAIR("content-type"),
                  "text/plain; charset=utf-8");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a http/vstream response
////////////////////////////////////////////////////////////////////////////////

ArangoResponse::~ArangoResponse() {

  for (auto& it : _freeables) {
    delete[] it;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the response code (http/vstream)
////////////////////////////////////////////////////////////////////////////////

ArangoResponse::ResponseCode ArangoResponse::responseCode() const {
  return _code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the response code (http/vstream)
////////////////////////////////////////////////////////////////////////////////

void ArangoResponse::setResponseCode(ResponseCode code) { _code = code; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the content length
////////////////////////////////////////////////////////////////////////////////

size_t ArangoResponse::contentLength() {
  if (_isHeadResponse) {
    return _bodySize;
  }

  Dictionary<char const*>::KeyValue const* kv =
      _headers.lookup(TRI_CHAR_LENGTH_PAIR("content-length"));

  if (kv == nullptr) {
    return 0;
  }

  return StringUtils::uint32(kv->_value);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief sets the content type
////////////////////////////////////////////////////////////////////////////////

void ArangoResponse::setContentType(std::string const& contentType) {
  setHeader(TRI_CHAR_LENGTH_PAIR("content-type"), contentType);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if chunked encoding is set
////////////////////////////////////////////////////////////////////////////////

bool ArangoResponse::isChunked() const { return _isChunked; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

std::string ArangoResponse::header(std::string const& key) const {
  std::string k = StringUtils::tolower(key);
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(k.c_str());

  if (kv == nullptr) {
    return "";
  }
  return kv->_value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

std::string ArangoResponse::header(char const* key, size_t keyLength) const {
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(key, keyLength);

  if (kv == nullptr) {
    return "";
  }
  return kv->_value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

std::string ArangoResponse::header(std::string const& key, bool& found) const {
  std::string k = StringUtils::tolower(key);
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(k.c_str());

  if (kv == nullptr) {
    found = false;
    return "";
  }
  found = true;
  return kv->_value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

std::string ArangoResponse::header(char const* key, size_t keyLength,
                                 bool& found) const {
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(key, keyLength);

  if (kv == nullptr) {
    found = false;
    return "";
  }
  found = true;
  return kv->_value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all header fields
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> ArangoResponse::headers() const {
  basics::Dictionary<char const*>::KeyValue const* begin;
  basics::Dictionary<char const*>::KeyValue const* end;

  std::map<std::string, std::string> result;

  for (_headers.range(begin, end); begin < end; ++begin) {
    char const* key = begin->_key;

    if (key == nullptr) {
      continue;
    }

    result[key] = begin->_value;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a header field
////////////////////////////////////////////////////////////////////////////////

void ArangoResponse::setHeader(char const* key, size_t keyLength,
                             std::string const& value) {
  if (value.empty()) {
    _headers.erase(key);
  } else {
    char const* v = StringUtils::duplicate(value);

    if (v != nullptr) {
      _headers.insert(key, keyLength, v);
      checkHeader(key, v);

      _freeables.emplace_back(v);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a header field
////////////////////////////////////////////////////////////////////////////////

void ArangoResponse::setHeader(char const* key, size_t keyLength,
                             char const* value) {
  if (*value == '\0') {
    _headers.erase(key);
  } else {
    _headers.insert(key, keyLength, value);
    checkHeader(key, value);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a header field
////////////////////////////////////////////////////////////////////////////////

void ArangoResponse::setHeader(std::string const& key, std::string const& value) {
  std::string lk = StringUtils::tolower(key);

  if (value.empty()) {
    _headers.erase(lk.c_str());
  } else {
    StringUtils::trimInPlace(lk);

    char const* k = StringUtils::duplicate(lk);
    char const* v = StringUtils::duplicate(value);

    _headers.insert(k, lk.size(), v);
    checkHeader(k, v);

    _freeables.push_back(k);
    _freeables.push_back(v);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for special headers
////////////////////////////////////////////////////////////////////////////////

void ArangoResponse::checkHeader(char const* key, char const* value) {
  if (key[0] == 't' && strcmp(key, "transfer-encoding") == 0) {
    if (TRI_CaseEqualString(value, "chunked")) {
      _isChunked = true;
    } else {
      _isChunked = false;
    }
  }
}  