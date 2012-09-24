////////////////////////////////////////////////////////////////////////////////
/// @brief http response, part of a multipart message
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2008-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpResponsePart.h"

#include "BasicsC/strings.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new http response part
////////////////////////////////////////////////////////////////////////////////

HttpResponsePart::HttpResponsePart (HttpResponseCode code) 
  : HttpResponse() {
    _code = code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a http response part
////////////////////////////////////////////////////////////////////////////////

HttpResponsePart::~HttpResponsePart () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief writes the header
////////////////////////////////////////////////////////////////////////////////

void HttpResponsePart::writeHeader (StringBuffer* output) {
  output->appendText("HTTP/1.1 ");
  output->appendText(responseString(_code));
  output->appendText("\r\n");
  
  basics::Dictionary<char const*>::KeyValue const* begin;
  basics::Dictionary<char const*>::KeyValue const* end;
  
  bool seenTransferEncoding = false;
  string transferEncoding;
  
  for (_headers.range(begin, end);  begin < end;  ++begin) {
    char const* key = begin->_key;
    
    if (key == 0) {
      continue;
    }
    
    // ignore content-length
    if (*key == 'c' && TRI_EqualString(key, "content-length")) {
      continue;
    }
    
    if (*key == 't' && TRI_EqualString(key, "transfer-encoding")) {
      seenTransferEncoding = true;
      transferEncoding = begin->_value;
      continue;
    }
    
    char const* value = begin->_value;
    
    output->appendText(key);
    output->appendText(": ");
    output->appendText(value);
    output->appendText("\r\n");
  }
  
  if (seenTransferEncoding && transferEncoding == "chunked") {
    output->appendText("transfer-encoding: chunked\r\n\r\n");
  }
  else {
    if (seenTransferEncoding) {
      output->appendText("transfer-encoding: ");
      output->appendText(transferEncoding);
      output->appendText("\r\n");
    }
   
    output->appendText("content-length: ");
    
    if (_isHeadResponse) {
      output->appendInteger(_bodySize);
    }
    else {
      output->appendInteger(_body.length());
    }
    
    output->appendText("\r\n\r\n");
  }
  // end of header, body to follow
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
