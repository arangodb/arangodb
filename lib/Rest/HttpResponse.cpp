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
////////////////////////////////////////////////////////////////////////////////

#include "HttpResponse.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"

#include "Rest/ArangoResponse.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new http response
////////////////////////////////////////////////////////////////////////////////

HttpResponse::HttpResponse(ArangoResponse::ResponseCode cde, uint32_t apiC)
    : ArangoResponse(cde, apiC),
      _body(TRI_UNKNOWN_MEM_ZONE) {
}

HttpResponse::~HttpResponse(){

}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes the header
////////////////////////////////////////////////////////////////////////////////

void HttpResponse::writeHeader(StringBuffer* output) {
  bool const capitalizeHeaders = (_apiCompatibility >= 20100);

  output->appendText(TRI_CHAR_LENGTH_PAIR("HTTP/1.1 "));
  output->appendText(responseString(_code));
  output->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));

  basics::Dictionary<char const*>::KeyValue const* begin;
  basics::Dictionary<char const*>::KeyValue const* end;

  bool seenTransferEncoding = false;
  std::string transferEncoding;

  for (_headers.range(begin, end); begin < end; ++begin) {
    char const* key = begin->_key;

    if (key == nullptr) {
      continue;
    }

    size_t const keyLength = strlen(key);

    // ignore content-length
    if (keyLength == 14 && *key == 'c' &&
        memcmp(key, "content-length", keyLength) == 0) {
      continue;
    }

    if (keyLength == 17 && *key == 't' &&
        memcmp(key, "transfer-encoding", keyLength) == 0) {
      seenTransferEncoding = true;
      transferEncoding = begin->_value;
      continue;
    }

    if (capitalizeHeaders) {
      char const* p = key;
      char const* end = key + keyLength;
      int capState = 1;
      while (p < end) {
        if (capState == 1) {
          // upper case
          output->appendChar(::toupper(*p));
          capState = 0;
        } else if (capState == 0) {
          // normal case
          output->appendChar(::tolower(*p));
          if (*p == '-') {
            capState = 1;
          } else if (*p == ':') {
            capState = 2;
          }
        } else {
          // output as is
          output->appendChar(*p);
        }
        ++p;
      }
    } else {
      output->appendText(key, keyLength);
    }
    output->appendText(TRI_CHAR_LENGTH_PAIR(": "));
    output->appendText(begin->_value);
    output->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
  }

  for (std::vector<char const*>::iterator iter = _cookies.begin();
       iter != _cookies.end(); ++iter) {
    if (capitalizeHeaders) {
      output->appendText(TRI_CHAR_LENGTH_PAIR("Set-Cookie: "));
    } else {
      output->appendText(TRI_CHAR_LENGTH_PAIR("set-cookie: "));
    }
    output->appendText(*iter);
    output->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
  }

  if (seenTransferEncoding && transferEncoding == "chunked") {
    if (capitalizeHeaders) {
      output->appendText(
          TRI_CHAR_LENGTH_PAIR("Transfer-Encoding: chunked\r\n\r\n"));
    } else {
      output->appendText(
          TRI_CHAR_LENGTH_PAIR("transfer-encoding: chunked\r\n\r\n"));
    }
  } else {
    if (seenTransferEncoding) {
      if (capitalizeHeaders) {
        output->appendText(TRI_CHAR_LENGTH_PAIR("Transfer-Encoding: "));
      } else {
        output->appendText(TRI_CHAR_LENGTH_PAIR("transfer-encoding: "));
      }
      output->appendText(transferEncoding);
      output->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
    }

    if (capitalizeHeaders) {
      output->appendText(TRI_CHAR_LENGTH_PAIR("Content-Length: "));
    } else {
      output->appendText(TRI_CHAR_LENGTH_PAIR("content-length: "));
    }

    if (_isHeadResponse) {
      // From http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.13
      //
      // 14.13 Content-Length
      //
      // The Content-Length entity-header field indicates the size of the
      // entity-body,
      // in decimal number of OCTETs, sent to the recipient or, in the case of
      // the HEAD method,
      // the size of the entity-body that would have been sent had the request
      // been a GET.
      output->appendInteger(_bodySize);
    } else {
      output->appendInteger(_body.length());
    }

    output->appendText(TRI_CHAR_LENGTH_PAIR("\r\n\r\n"));
  }
  // end of header, body to follow
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the body
////////////////////////////////////////////////////////////////////////////////

StringBuffer& HttpResponse::body() { return _body; }