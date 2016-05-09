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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "HttpResponse.h"

#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"

using namespace arangodb;
using namespace arangodb::basics;

std::string const HttpResponse::BATCH_ERROR_HEADER = "x-arango-errors";
bool HttpResponse::HIDE_PRODUCT_HEADER = false;

HttpResponse::HttpResponse(ResponseCode code)
    : GeneralResponse(code),
      _isHeadResponse(false),
      _isChunked(false),
      _body(TRI_UNKNOWN_MEM_ZONE, false),
      _bodySize(0) {
  if (!HIDE_PRODUCT_HEADER) {
    _headers[StaticStrings::Server] = "ArangoDB";
  }

  _headers[StaticStrings::Connection] = StaticStrings::KeepAlive;
  _headers[StaticStrings::ContentTypeHeader] = StaticStrings::MimeTypeText;
}

void HttpResponse::setCookie(std::string const& name, std::string const& value,
                             int lifeTimeSeconds, std::string const& path,
                             std::string const& domain, bool secure,
                             bool httpOnly) {
  std::unique_ptr<StringBuffer> buffer =
      std::make_unique<StringBuffer>(TRI_UNKNOWN_MEM_ZONE);

  std::string tmp = StringUtils::trim(name);
  buffer->appendText(tmp);
  buffer->appendChar('=');

  tmp = StringUtils::urlEncode(value);
  buffer->appendText(tmp);

  if (lifeTimeSeconds != 0) {
    time_t rawtime;

    time(&rawtime);
    if (lifeTimeSeconds > 0) {
      rawtime += lifeTimeSeconds;
    } else {
      rawtime = 1;
    }

    if (rawtime > 0) {
      struct tm* timeinfo;
      char buffer2[80];

      timeinfo = gmtime(&rawtime);
      strftime(buffer2, 80, "%a, %d-%b-%Y %H:%M:%S %Z", timeinfo);
      buffer->appendText(TRI_CHAR_LENGTH_PAIR("; expires="));
      buffer->appendText(buffer2);
    }
  }

  if (!path.empty()) {
    buffer->appendText(TRI_CHAR_LENGTH_PAIR("; path="));
    buffer->appendText(path);
  }

  if (!domain.empty()) {
    buffer->appendText(TRI_CHAR_LENGTH_PAIR("; domain="));
    buffer->appendText(domain);
  }

  if (secure) {
    buffer->appendText(TRI_CHAR_LENGTH_PAIR("; secure"));
  }

  if (httpOnly) {
    buffer->appendText(TRI_CHAR_LENGTH_PAIR("; HttpOnly"));
  }

  _cookies.emplace_back(buffer->c_str());
}

void HttpResponse::headResponse(size_t size) {
  _body.clear();
  _isHeadResponse = true;
  _bodySize = size;
}

size_t HttpResponse::bodySize() const {
  if (_isHeadResponse) {
    return _bodySize;
  }
  return _body.length();
}

void HttpResponse::writeHeader(StringBuffer* output) {
  bool const capitalizeHeaders = true; 

  output->appendText(TRI_CHAR_LENGTH_PAIR("HTTP/1.1 "));
  output->appendText(responseString(_responseCode));
  output->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));

  bool seenTransferEncoding = false;
  std::string transferEncoding;

  for (auto const& it : _headers) {
    std::string const& key = it.first;
    size_t const keyLength = key.size();

    // ignore content-length
    if (keyLength == 14 && key[0] == 'c' &&
        memcmp(key.c_str(), "content-length", keyLength) == 0) {
      continue;
    }

    // save transfer encoding
    if (keyLength == 17 && key[0] == 't' &&
        memcmp(key.c_str(), "transfer-encoding", keyLength) == 0) {
      seenTransferEncoding = true;
      transferEncoding = it.second;
      continue;
    }

    if (capitalizeHeaders) {
      char const* p = key.c_str();
      char const* end = p + keyLength;
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
      output->appendText(key);
    }

    output->appendText(TRI_CHAR_LENGTH_PAIR(": "));
    output->appendText(it.second);
    output->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
  }

  for (auto const& it : _cookies) {
    if (capitalizeHeaders) {
      output->appendText(TRI_CHAR_LENGTH_PAIR("Set-Cookie: "));
    } else {
      output->appendText(TRI_CHAR_LENGTH_PAIR("set-cookie: "));
    }

    output->appendText(it);
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

void HttpResponse::checkHeader(std::string const& key,
                               std::string const& value) {
  if (key[0] == 't' && key == "transfer-encoding") {
    if (TRI_CaseEqualString(value.c_str(), "chunked")) {
      _isChunked = true;
    } else {
      _isChunked = false;
    }
  }
}
