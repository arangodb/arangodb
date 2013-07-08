////////////////////////////////////////////////////////////////////////////////
/// @brief admin log request handler
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
/// @author Achim Brandt
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminLogHandler.h"

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"

using namespace triagens;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief sort ascending
////////////////////////////////////////////////////////////////////////////////

static int LidCompareAsc (void const* l, void const* r) {
  TRI_log_buffer_t const* left = (TRI_log_buffer_t const*) l;
  TRI_log_buffer_t const* right = (TRI_log_buffer_t const*) r;

  return ((int64_t) left->_lid) - ((int64_t) right->_lid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort descending
////////////////////////////////////////////////////////////////////////////////

static int LidCompareDesc (void const* l, void const* r) {
  TRI_log_buffer_t const* left = (TRI_log_buffer_t const*) l;
  TRI_log_buffer_t const* right = (TRI_log_buffer_t const*) r;

  return ((int64_t) right->_lid) - ((int64_t) left->_lid);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestAdminLogHandler::RestAdminLogHandler (rest::HttpRequest* request)
  : RestAdminBaseHandler(request) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestAdminLogHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the log files (inheritDoc)
///
/// @RESTHEADER{GET /_admin/log,reads the log information}
///
/// @REST{GET /_admin/log}
///
/// Returns all fatal, error, warning or info log messages.
///
/// The returned object contains the attributes:
///
/// - @LIT{lid}: a list of log-entry identifiers. Each log message is uniquely
///   identified by its @LIT{lid} and the identifiers are in ascending
///   order.
///
/// - @LIT{level}: a list of the log-level of the log-entry.
///
/// - @LIT{timestamp}: a list of the timestamp as seconds since
///   1970-01-01 of the log-entry.
///
/// - @LIT{text}: a list of the text of the log-entry.
///
/// - @LIT{totalAmount}: the total amount of log entries before pagination.
///
/// @REST{GET /_admin/log?upto=@FA{log-level}}
///
/// Returns all log entries upto @FA{log-level}. Note that @FA{log-level} must
/// be:
///
/// - @LIT{fatal} / @LIT{0}
/// - @LIT{error} / @LIT{1}
/// - @LIT{warning} / @LIT{2}
/// - @LIT{info} / @LIT{3}
/// - @LIT{debug} / @LIT{4}
///
/// @REST{GET /_admin/log?level=@FA{log-level}}
///
/// Returns all log entries of @FA{log-level}. Note that @LIT{level=} and
/// @LIT{upto=} are mutably exclusive.
///
/// @REST{GET /_admin/log?size=@FA{size}&offset=@FA{offset}}
///
/// Paginates the result. Skip the first @FA{offset} entries and limit the
/// number of returned log-entries to @FA{size}.
///
/// @REST{GET /_admin/log?start=@FA{lid}}
///
/// Returns all log entries such that their log-entry identifier is greater
/// or equal to @LIT{lid}.
///
/// @REST{GET /_admin/log?sort=@FA{direction}}
///
/// Sort the log-entries either ascending if @FA{direction} is @LIT{asc}, or
/// descending if it is @LIT{desc} according to their @LIT{lid}. Note
/// that the @LIT{lid} imposes a chronological order.
///
/// @REST{GET /_admin/log?search=@FA{text}}
///
/// Only return the log-entries containing the @FA{text} string.
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestAdminLogHandler::execute () {

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
  }
  else if (found1) {
    logLevel = upto;
    useUpto = true;
  }

  if (found1 || found2) {
    if (logLevel == "fatal" || logLevel == "0") {
      ul = TRI_LOG_LEVEL_FATAL;
    }
    else if (logLevel == "error" || logLevel == "1") {
      ul = TRI_LOG_LEVEL_ERROR;
    }
    else if (logLevel == "warning" || logLevel == "2") {
      ul = TRI_LOG_LEVEL_WARNING;
    }
    else if (logLevel == "info" || logLevel == "3") {
      ul = TRI_LOG_LEVEL_INFO;
    }
    else if (logLevel == "debug" || logLevel == "4") {
      ul = TRI_LOG_LEVEL_DEBUG;
    }
    else if (logLevel == "trace" || logLevel == "5") {
      ul = TRI_LOG_LEVEL_TRACE;
    }
    else {
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    string("unknown '") + (found2 ? "level" : "upto") + "' log level: '" + logLevel + "'");
      return HANDLER_DONE;
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

  uint64_t size = (uint64_t) -1;

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

  TRI_vector_t * logs = TRI_BufferLogging(ul, start, useUpto);

  if (logs == 0) {
    generateError(HttpResponse::SERVER_ERROR,
                  TRI_ERROR_OUT_OF_MEMORY);
    return HANDLER_DONE;
  }


  TRI_vector_t clean;

  TRI_InitVector(&clean, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_log_buffer_t));

  for (size_t i = 0;  i < logs->_length;  ++i) {
    TRI_log_buffer_t* buf = (TRI_log_buffer_t*) TRI_AtVector(logs, i);

    if (search) {
      string text = StringUtils::tolower(buf->_text);

      if (text.find(searchString) == string::npos) {
        continue;
      }
    }

    TRI_PushBackVector(&clean, buf);
  }


  TRI_json_t result;
  TRI_InitArrayJson(TRI_UNKNOWN_MEM_ZONE, &result);

  // create the 4 vectors for the result parts
  TRI_json_t* lid       = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_json_t* level     = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_json_t* timestamp = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_json_t* text      = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, &result, "totalAmount", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (double) clean._length));

  size_t length = clean._length;

  if (offset >= length) {
    length = 0;
    offset = 0;
  }
  else if (offset > 0) {
    length -= offset;
  }

  // restrict to at most <size> elements
  if (length > size) {
    length = size;
  }

  qsort(((char*) TRI_BeginVector(&clean)) + offset * sizeof(TRI_log_buffer_t),
        length,
        sizeof(TRI_log_buffer_t),
        (sortAscending ? LidCompareAsc : LidCompareDesc));

  for (size_t i = 0;  i < length;  ++i) {
    TRI_log_buffer_t* buf = (TRI_log_buffer_t*) TRI_AtVector(&clean, offset + i);
    uint32_t l = 0;

    switch (buf->_level) {
      case TRI_LOG_LEVEL_FATAL:    l = 0; break;
      case TRI_LOG_LEVEL_ERROR:    l = 1; break;
      case TRI_LOG_LEVEL_WARNING:  l = 2; break;
      case TRI_LOG_LEVEL_INFO:     l = 3; break;
      case TRI_LOG_LEVEL_DEBUG:    l = 4; break;
      case TRI_LOG_LEVEL_TRACE:    l = 5; break;
    }

    // put the data into the individual vectors
    if (lid != 0) {
      TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, lid, TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (double) buf->_lid));
    }

    if (level != 0) {
      TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, level, TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (double) l));
    }

    if (timestamp != 0) {
      TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, timestamp, TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (double) buf->_timestamp));
    }

    if (text != 0) {
      TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, text, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, buf->_text));
    }
  }

  TRI_FreeBufferLogging(logs);
  TRI_DestroyVector(&clean);

  // now put the 4 vectors into the result
  if (lid != 0) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, &result, "lid", lid);
  }
  if (level != 0) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, &result, "level", level);
  }
  if (timestamp != 0) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, &result, "timestamp", timestamp);
  }
  if (text != 0) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, &result, "text", text);
  }

  generateResult(&result);

  TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, &result);

  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
