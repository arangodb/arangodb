////////////////////////////////////////////////////////////////////////////////
/// @brief admin log request handler
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
/// @author Achim Brandt
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminLogHandler.h"

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Variant/VariantArray.h"
#include "Variant/VariantString.h"
#include "Variant/VariantUInt32.h"
#include "Variant/VariantUInt64.h"
#include "Variant/VariantVector.h"

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
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestAdminLogHandler::execute () {

  // .............................................................................
  // check the maximal log level to report
  // .............................................................................

  bool found;
  string upto = StringUtils::tolower(request->value("upto", found));
  TRI_log_level_e ul;
  
  if (found) {
    if (upto == "fatal" || upto == "0") {
      ul = TRI_LOG_LEVEL_FATAL;
    }
    else if (upto == "error" || upto == "1") {
      ul = TRI_LOG_LEVEL_ERROR;
    }
    else if (upto == "warning" || upto == "2") {
      ul = TRI_LOG_LEVEL_WARNING;
    }
    else if (upto == "info" || upto == "3") {
      ul = TRI_LOG_LEVEL_INFO;
    }
    else if (upto == "debug" || upto == "4") {
      ul = TRI_LOG_LEVEL_DEBUG;
    }
    else if (upto == "trace" || upto == "5") {
      ul = TRI_LOG_LEVEL_TRACE;
    }
    else {
      generateError(HttpResponse::BAD, 
                    TRI_REST_ERROR_BAD_PARAMETER,
                    "unknown 'upto' log level: '" + upto + "'");
      return HANDLER_DONE;
    }
  }
  else {
    ul = TRI_LOG_LEVEL_INFO;
  }
  
  // .............................................................................
  // check the starting position
  // .............................................................................
  
  uint64_t start = 0;
  
  string s = request->value("start", found);
  
  if (found) {
    start = StringUtils::uint64(s);
  }
  
  // .............................................................................
  // check the offset
  // .............................................................................
  
  uint64_t offset = 0;
  
  s = request->value("offset", found);
  
  if (found) {
    offset = StringUtils::uint64(s);
  }
  
  // .............................................................................
  // check the size
  // .............................................................................
  
  uint64_t size = (uint64_t) -1;
  
  s = request->value("size", found);
  
  if (found) {
    size = StringUtils::uint64(s);
  }
  
  // .............................................................................
  // check the sort direction
  // .............................................................................
  
  bool sortAscending = false;
  bool sortDescending = false;
  
  string sortdir = StringUtils::tolower(request->value("sort", found));
  
  if (found) {
    if (sortdir == "asc") {
      sortAscending = true;
    }
    else if (sortdir == "desc") {
      sortDescending = true;
    }
    else {
      LOGGER_DEBUG << "unknown sort direction '" << sortdir << "'";
    }
  }
  
  // .............................................................................
  // check the search criteria
  // .............................................................................
  
  bool search = false;
  string searchString = StringUtils::tolower(request->value("search", search));
  
  // .............................................................................
  // generate result
  // .............................................................................
  
  VariantArray* result = new VariantArray();
  
  VariantVector* lid = new VariantVector();
  result->add("lid", lid);
  
  VariantVector* level = new VariantVector();
  result->add("level", level);
  
  VariantVector* timestamp = new VariantVector();
  result->add("timestamp", timestamp);
  
  VariantVector* text = new VariantVector();
  result->add("text", text);
  
  TRI_vector_t * logs = TRI_BufferLogging(ul, start);
  TRI_vector_t clean;
  
  TRI_InitVector(&clean, sizeof(TRI_log_buffer_t));
  
  for (size_t i = 0;  i < logs->_length;  ++i) {
    TRI_log_buffer_t* buf = (TRI_log_buffer_t*) TRI_AtVector(logs, i);
    
    if (search) {
      string text = buf->_text;
      
      if (text.find(searchString) == string::npos) {
        continue;
      }
    }
    
    TRI_PushBackVector(&clean, buf);
  }
  
  result->add("totalAmount", new VariantUInt64(clean._length));
  
  size_t length = clean._length;
  
  if (offset >= length) {
    length = 0;
    offset = 0;
  }
  else if (offset > 0) {
    length -= offset;
  }
  else if (length > size) {
    length = size;
  }
  
  if (sortAscending) {
    qsort(((char*) TRI_BeginVector(&clean)) + offset * sizeof(TRI_log_buffer_t),
          length,
          sizeof(TRI_log_buffer_t),
          LidCompareAsc);
  }
  else if (sortDescending) {
    qsort(((char*) TRI_BeginVector(&clean)) + offset * sizeof(TRI_log_buffer_t),
          length,
          sizeof(TRI_log_buffer_t),
          LidCompareDesc);
  }
  
  for (size_t i = 0;  i < length;  ++i) {
    TRI_log_buffer_t* buf = (TRI_log_buffer_t*) TRI_AtVector(logs, offset + i);
    uint32_t l = 0;
    
    switch (buf->_level) {
      case TRI_LOG_LEVEL_FATAL:  l = 0; break;
      case TRI_LOG_LEVEL_ERROR:  l = 1; break;
      case TRI_LOG_LEVEL_WARNING:  l = 2; break;
      case TRI_LOG_LEVEL_INFO:  l = 3; break;
      case TRI_LOG_LEVEL_DEBUG:  l = 4; break;
      case TRI_LOG_LEVEL_TRACE:  l = 5; break;
    }
    
    lid->add(new VariantUInt64(buf->_lid));
    level->add(new VariantUInt32(l));
    timestamp->add(new VariantUInt32(buf->_timestamp));
    text->add(new VariantString(buf->_text));
  }
  
  TRI_FreeBufferLogging(logs);
  
  generateResult(result);
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
