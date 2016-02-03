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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminLogHandler.h"
#include "Basics/logging.h"
#include "Basics/StringUtils.h"
#include "Rest/HttpRequest.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief sort ascending
////////////////////////////////////////////////////////////////////////////////

static bool LidCompareAsc(void const* l, void const* r) {
  TRI_log_buffer_t const* left = (TRI_log_buffer_t const*)l;
  TRI_log_buffer_t const* right = (TRI_log_buffer_t const*)r;

  return left->_lid < right->_lid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort descending
////////////////////////////////////////////////////////////////////////////////

static bool LidCompareDesc(void const* l, void const* r) {
  TRI_log_buffer_t const* left = (TRI_log_buffer_t const*)l;
  TRI_log_buffer_t const* right = (TRI_log_buffer_t const*)r;

  return right->_lid < left->_lid;
}

RestAdminLogHandler::RestAdminLogHandler(rest::HttpRequest* request)
    : RestBaseHandler(request) {}

bool RestAdminLogHandler::isDirect() const { return true; }

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_admin_modules_flush
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestAdminLogHandler::execute() {
  // "/log" can only be called for the _system database
  if (_request->databaseName() != "_system") {
    generateError(HttpResponse::FORBIDDEN,
                  TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
    return status_t(HANDLER_DONE);
  }

  // .............................................................................
  // check the maximal log level to report
  // .............................................................................

  bool found1;
  std::string upto = StringUtils::tolower(_request->value("upto", found1));

  bool found2;
  std::string lvl = StringUtils::tolower(_request->value("level", found2));

  TRI_log_level_e ul = TRI_LOG_LEVEL_INFO;
  bool useUpto = true;
  std::string logLevel;

  // prefer level over upto
  if (found2) {
    logLevel = lvl;
    useUpto = false;
  } else if (found1) {
    logLevel = upto;
    useUpto = true;
  }

  if (found1 || found2) {
    if (logLevel == "fatal" || logLevel == "0") {
      ul = TRI_LOG_LEVEL_FATAL;
    } else if (logLevel == "error" || logLevel == "1") {
      ul = TRI_LOG_LEVEL_ERROR;
    } else if (logLevel == "warning" || logLevel == "2") {
      ul = TRI_LOG_LEVEL_WARNING;
    } else if (logLevel == "info" || logLevel == "3") {
      ul = TRI_LOG_LEVEL_INFO;
    } else if (logLevel == "debug" || logLevel == "4") {
      ul = TRI_LOG_LEVEL_DEBUG;
    } else if (logLevel == "trace" || logLevel == "5") {
      ul = TRI_LOG_LEVEL_TRACE;
    } else {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    std::string("unknown '") + (found2 ? "level" : "upto") +
                        "' log level: '" + logLevel + "'");
      return status_t(HANDLER_DONE);
    }
  }

  // .............................................................................
  // check the starting position
  // .............................................................................

  uint64_t start = 0;

  bool found;
  std::string s = _request->value("start", found);

  if (found) {
    start = StringUtils::uint64(s);
  }

  // .............................................................................
  // check the offset
  // .............................................................................

  uint64_t offset = 0;

  s = _request->value("offset", found);

  if (found) {
    offset = StringUtils::uint64(s);
  }

  // .............................................................................
  // check the size
  // .............................................................................

  uint64_t size = (uint64_t)-1;

  s = _request->value("size", found);

  if (found) {
    size = StringUtils::uint64(s);
  }

  // .............................................................................
  // check the sort direction
  // .............................................................................

  bool sortAscending = true;

  std::string sortdir = StringUtils::tolower(_request->value("sort", found));

  if (found) {
    if (sortdir == "desc") {
      sortAscending = false;
    }
  }

  // .............................................................................
  // check the search criteria
  // .............................................................................

  bool search = false;
  std::string searchString =
      StringUtils::tolower(_request->value("search", search));

  // .............................................................................
  // generate result
  // .............................................................................

  TRI_vector_t* logs = TRI_BufferLogging(ul, start, useUpto);

  if (logs == nullptr) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return status_t(HANDLER_DONE);
  }

  std::vector<TRI_log_buffer_t*> clean;
  for (size_t i = 0; i < TRI_LengthVector(logs); ++i) {
    TRI_log_buffer_t* buf = static_cast<TRI_log_buffer_t*>(TRI_AtVector(logs, i));

    if (search) {
      std::string text = StringUtils::tolower(buf->_text);

      if (text.find(searchString) == std::string::npos) {
        continue;
      }
    }
    clean.emplace_back(buf);
  }

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  size_t length = clean.size();
  try {
    result.add("totalAmount", VPackValue(length));

    if (offset >= length) {
      length = 0;
      offset = 0;
    } else if (offset > 0) {
      length -= static_cast<size_t>(offset);
    }

    // restrict to at most <size> elements
    if (length > size) {
      length = static_cast<size_t>(size);
    }

    std::sort(clean.begin() + offset, clean.begin() + offset + length,
              (sortAscending ? LidCompareAsc : LidCompareDesc));

    // For now we build the arrays one ofter the other
    // first lid
    result.add("lid", VPackValue(VPackValueType::Array));
    for (size_t i = 0; i < length; ++i) {
      TRI_log_buffer_t* buf = clean.at(i + static_cast<size_t>(offset));
      result.add(VPackValue(buf->_lid));
    }
    result.close();

    // second level
    result.add("level", VPackValue(VPackValueType::Array));
    for (size_t i = 0; i < length; ++i) {
      TRI_log_buffer_t* buf = clean.at(i + static_cast<size_t>(offset));
      uint32_t l = 0;

      switch (buf->_level) {
        case TRI_LOG_LEVEL_FATAL:
          l = 0;
          break;
        case TRI_LOG_LEVEL_ERROR:
          l = 1;
          break;
        case TRI_LOG_LEVEL_WARNING:
          l = 2;
          break;
        case TRI_LOG_LEVEL_INFO:
          l = 3;
          break;
        case TRI_LOG_LEVEL_DEBUG:
          l = 4;
          break;
        case TRI_LOG_LEVEL_TRACE:
          l = 5;
          break;
      }
      result.add(VPackValue(l));
    }
    result.close();

    // third timestamp
    result.add("timestamp", VPackValue(VPackValueType::Array));
    for (size_t i = 0; i < length; ++i) {
      TRI_log_buffer_t* buf = clean.at(i + static_cast<size_t>(offset));
      result.add(VPackValue(static_cast<size_t>(buf->_timestamp)));
    }
    result.close();

    // fourth text
    result.add("text", VPackValue(VPackValueType::Array));
    for (size_t i = 0; i < length; ++i) {
      TRI_log_buffer_t* buf = clean.at(i + static_cast<size_t>(offset));
      result.add(VPackValue(buf->_text));
    }
    result.close();

    result.close();  // Close the result object

    TRI_FreeBufferLogging(logs);

    VPackSlice slice = result.slice();
    generateResult(slice);
  } catch (...) {
    // Not Enough memory to build everything up
    // Has been ignored thus far
    // So ignore again
  }

  return status_t(HANDLER_DONE);
}
