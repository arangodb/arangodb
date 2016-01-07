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

#include "Basics/StringUtils.h"
#include "Basics/logging.h"
#include "Rest/HttpRequest.h"

using namespace std;
using namespace triagens;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;


////////////////////////////////////////////////////////////////////////////////
/// @brief sort ascending
////////////////////////////////////////////////////////////////////////////////

static int LidCompareAsc(void const* l, void const* r) {
  TRI_log_buffer_t const* left = (TRI_log_buffer_t const*)l;
  TRI_log_buffer_t const* right = (TRI_log_buffer_t const*)r;

  return (int)(((int64_t)left->_lid) - ((int64_t)right->_lid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort descending
////////////////////////////////////////////////////////////////////////////////

static int LidCompareDesc(void const* l, void const* r) {
  TRI_log_buffer_t const* left = (TRI_log_buffer_t const*)l;
  TRI_log_buffer_t const* right = (TRI_log_buffer_t const*)r;

  return (int)(((int64_t)right->_lid) - ((int64_t)left->_lid));
}


////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestAdminLogHandler::RestAdminLogHandler(rest::HttpRequest* request)
    : RestBaseHandler(request) {}


////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestAdminLogHandler::isDirect() const { return true; }

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_admin_modules_flush
/// @brief returns the log files
///
/// @RESTHEADER{GET /_admin/log, Read global log from the server}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{upto,string,optional}
/// Returns all log entries up to log level *upto*. Note that *upto* must be:
/// - *fatal* or *0*
/// - *error* or *1*
/// - *warning* or *2*
/// - *info* or *3*
/// - *debug*  or *4*
/// The default value is *info*.
///
/// @RESTQUERYPARAM{level,string,optional}
/// Returns all log entries of log level *level*. Note that the query parameters
/// *upto* and *level* are mutually exclusive.
///
/// @RESTQUERYPARAM{start,number,optional}
/// Returns all log entries such that their log entry identifier (*lid* value)
/// is greater or equal to *start*.
///
/// @RESTQUERYPARAM{size,number,optional}
/// Restricts the result to at most *size* log entries.
///
/// @RESTQUERYPARAM{offset,number,optional}
/// Starts to return log entries skipping the first *offset* log entries. *offset*
/// and *size* can be used for pagination.
///
/// @RESTQUERYPARAM{search,string,optional}
/// Only return the log entries containing the text specified in *search*.
///
/// @RESTQUERYPARAM{sort,string,optional}
/// Sort the log entries either ascending (if *sort* is *asc*) or descending
/// (if *sort* is *desc*) according to their *lid* values. Note that the *lid*
/// imposes a chronological order. The default value is *asc*.
///
/// @RESTDESCRIPTION
/// Returns fatal, error, warning or info log messages from the server's global log.
/// The result is a JSON object with the following attributes:
///
/// - *lid*: a list of log entry identifiers. Each log message is uniquely
///   identified by its @LIT{lid} and the identifiers are in ascending
///   order.
///
/// - *level*: a list of the log-levels for all log entries.
///
/// - *timestamp*: a list of the timestamps as seconds since 1970-01-01 for all log
///   entries.
///
/// - *text* a list of the texts of all log entries
///
/// - *totalAmount*: the total amount of log entries before pagination.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{400}
/// is returned if invalid values are specified for *upto* or *level*.
///
/// @RESTRETURNCODE{403}
/// is returned if the log is requested for any database other than *_system*.
///
/// @RESTRETURNCODE{500}
/// is returned if the server cannot generate the result due to an out-of-memory
/// error.
/// @endDocuBlock
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
  string upto = StringUtils::tolower(_request->value("upto", found1));

  bool found2;
  string lvl = StringUtils::tolower(_request->value("level", found2));

  TRI_log_level_e ul = TRI_LOG_LEVEL_INFO;
  bool useUpto = true;
  string logLevel;

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
                    string("unknown '") + (found2 ? "level" : "upto") +
                        "' log level: '" + logLevel + "'");
      return status_t(HANDLER_DONE);
    }
  }

  // .............................................................................
  // check the starting position
  // .............................................................................

  uint64_t start = 0;

  bool found;
  string s = _request->value("start", found);

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

  string sortdir = StringUtils::tolower(_request->value("sort", found));

  if (found) {
    if (sortdir == "desc") {
      sortAscending = false;
    }
  }

  // .............................................................................
  // check the search criteria
  // .............................................................................

  bool search = false;
  string searchString = StringUtils::tolower(_request->value("search", search));

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
    TRI_log_buffer_t* buf = (TRI_log_buffer_t*)TRI_AtVector(logs, i);

    if (search) {
      string text = StringUtils::tolower(buf->_text);

      if (text.find(searchString) == string::npos) {
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


