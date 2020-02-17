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

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>
#include <time.h>

#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackDumper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/tri-strings.h"
#include "Meta/conversion.h"
#include "Rest/GeneralRequest.h"

using namespace arangodb;
using namespace arangodb::basics;

bool HttpResponse::HIDE_PRODUCT_HEADER = false;

HttpResponse::HttpResponse(ResponseCode code, uint64_t mid,
                           std::unique_ptr<basics::StringBuffer> buffer)
: GeneralResponse(code, mid),
  _body(std::move(buffer)),
  _bodySize(0) {
  _contentType = ContentType::TEXT;
    
  if (!_body) {
    _body = std::make_unique<basics::StringBuffer>(false);
  }

  if (_body->c_str() == nullptr) {
    // no buffer could be reserved. out of memory!
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

void HttpResponse::reset(ResponseCode code) {
  _responseCode = code;
  _headers.clear();
  _contentType = ContentType::TEXT;
  TRI_ASSERT(_body != nullptr);
  _body->clear();
  _bodySize = 0;
}

void HttpResponse::setCookie(std::string const& name, std::string const& value,
                             int lifeTimeSeconds, std::string const& path,
                             std::string const& domain, bool secure, bool httpOnly) {
  StringBuffer buffer(false);

  std::string tmp = StringUtils::trim(name);
  buffer.appendText(tmp);
  buffer.appendChar('=');

  tmp = StringUtils::urlEncode(value);
  buffer.appendText(tmp);

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
      buffer.appendText(TRI_CHAR_LENGTH_PAIR("; expires="));
      buffer.appendText(buffer2);
    }
  }

  if (!path.empty()) {
    buffer.appendText(TRI_CHAR_LENGTH_PAIR("; path="));
    buffer.appendText(path);
  }

  if (!domain.empty()) {
    buffer.appendText(TRI_CHAR_LENGTH_PAIR("; domain="));
    buffer.appendText(domain);
  }

  if (secure) {
    buffer.appendText(TRI_CHAR_LENGTH_PAIR("; secure"));
  }

  if (httpOnly) {
    buffer.appendText(TRI_CHAR_LENGTH_PAIR("; HttpOnly"));
  }
  // copies buffer into a std::string
  _cookies.emplace_back(buffer.data(), buffer.length());
}

void HttpResponse::headResponse(size_t size) {
  TRI_ASSERT(_body != nullptr);
  _body->clear();
  _bodySize = size;
  _generateBody = false;
}

size_t HttpResponse::bodySize() const {
  if (!_generateBody) {
    return _bodySize;
  }
  TRI_ASSERT(_body != nullptr);
  return _body->length();
}

void HttpResponse::writeHeader(StringBuffer* output) {
  output->appendText(TRI_CHAR_LENGTH_PAIR("HTTP/1.1 "));
  output->appendText(responseString(_responseCode));
  output->appendText("\r\n", 2);

  bool seenServerHeader = false;
  bool seenTransferEncodingHeader = false;
  std::string transferEncoding;

  for (auto const& it : _headers) {
    std::string const& key = it.first;
    size_t const keyLength = key.size();

    // ignore content-length
    if (keyLength == 14 && key[0] == 'c' &&
        memcmp(key.c_str(), "content-length", keyLength) == 0) {
      continue;
    } else if (keyLength == 10 && key[0] == 'c' &&
               memcmp(key.c_str(), "connection", keyLength) == 0) {
      // this ensures we don't print two "Connection" headers
      continue;
    }

    // save transfer encoding
    if (keyLength == 17 && key[0] == 't' &&
        memcmp(key.c_str(), "transfer-encoding", keyLength) == 0) {
      seenTransferEncodingHeader = true;
      transferEncoding = it.second;
      continue;
    }

    if (keyLength == 6 && key[0] == 's' && memcmp(key.c_str(), "server", keyLength) == 0) {
      // this ensures we don't print two "Server" headers
      seenServerHeader = true;
      // go on and use the user-defined "Server" header value
    }

    // reserve enough space for header name + ": " + value + "\r\n"
    if (output->reserve(keyLength + 2 + it.second.size() + 2) != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    char const* p = key.data();
    char const* end = p + keyLength;
    int capState = 1;

    while (p < end) {
      if (capState == 1) {
        // upper case
        output->appendCharUnsafe(StringUtils::toupper(*p));
        capState = 0;
      } else if (capState == 0) {
        // normal case
        output->appendCharUnsafe(StringUtils::tolower(*p));
        if (*p == '-') {
          capState = 1;
        } else if (*p == ':') {
          capState = 2;
        }
      } else {
        // output as is
        output->appendCharUnsafe(*p);
      }
      ++p;
    }

    output->appendTextUnsafe(": ", 2);
    output->appendTextUnsafe(it.second);
    output->appendTextUnsafe("\r\n", 2);
  }

  // add "Server" response header
  if (!seenServerHeader && !HIDE_PRODUCT_HEADER) {
    output->appendText(TRI_CHAR_LENGTH_PAIR("Server: ArangoDB\r\n"));
  }

  // this is just used by the batch handler, close connection
  output->appendText(TRI_CHAR_LENGTH_PAIR("Connection: Close \r\n"));

  // add "Content-Type" header
  switch (_contentType) {
    case ContentType::UNSET:
    case ContentType::JSON:
      output->appendText(TRI_CHAR_LENGTH_PAIR(
          "Content-Type: application/json; charset=utf-8\r\n"));
      break;
    case ContentType::VPACK:
      output->appendText(
          TRI_CHAR_LENGTH_PAIR("Content-Type: application/x-velocypack\r\n"));
      break;
    case ContentType::TEXT:
      output->appendText(
          TRI_CHAR_LENGTH_PAIR("Content-Type: text/plain; charset=utf-8\r\n"));
      break;
    case ContentType::HTML:
      output->appendText(
          TRI_CHAR_LENGTH_PAIR("Content-Type: text/html; charset=utf-8\r\n"));
      break;
    case ContentType::DUMP:
      output->appendText(TRI_CHAR_LENGTH_PAIR(
          "Content-Type: application/x-arango-dump; charset=utf-8\r\n"));
      break;
    case ContentType::CUSTOM: {
      // intentionally don't print anything here
      // the header should have been in _headers already, and should have been
      // handled above
    }
  }

  for (auto const& it : _cookies) {
    output->appendText(TRI_CHAR_LENGTH_PAIR("Set-Cookie: "));
    output->appendText(it);
    output->appendText("\r\n", 2);
  }

  if (seenTransferEncodingHeader && transferEncoding == "chunked") {
    output->appendText(
        TRI_CHAR_LENGTH_PAIR("Transfer-Encoding: chunked\r\n\r\n"));
  } else {
    if (seenTransferEncodingHeader) {
      output->appendText(TRI_CHAR_LENGTH_PAIR("Transfer-Encoding: "));
      output->appendText(transferEncoding);
      output->appendText("\r\n", 2);
    }

    output->appendText(TRI_CHAR_LENGTH_PAIR("Content-Length: "));

    if (!_generateBody) {
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
      output->appendInteger(_body->length());
    }

    output->appendText("\r\n\r\n", 4);
  }
  // end of header, body to follow
}

void HttpResponse::addPayload(VPackSlice const& slice, velocypack::Options const* options,
                              bool resolveExternals) {
  if (_contentType == rest::ContentType::JSON &&
      _contentTypeRequested == rest::ContentType::VPACK) {
    // content type was set by a handler to Json but the client wants VPACK
    // as we have a slice at had we are able to reply with VPACK
    _contentType = rest::ContentType::VPACK;
  }

  addPayloadInternal(slice, slice.byteSize(), options, resolveExternals);
}

void HttpResponse::addPayload(VPackBuffer<uint8_t>&& buffer,
                              velocypack::Options const* options, bool resolveExternals) {
  if (_contentType == rest::ContentType::JSON &&
      _contentTypeRequested == rest::ContentType::VPACK) {
    // content type was set by a handler to Json but the client wants VPACK
    // as we have a slice at had we are able to reply with VPACK
    _contentType = rest::ContentType::VPACK;
  }

  if (buffer.size() > 0) {
    addPayloadInternal(VPackSlice(buffer.data()), buffer.length(), options, resolveExternals);
  }
}

void HttpResponse::addRawPayload(VPackStringRef payload) {
  _body->appendText(payload.data(), payload.length());
}

void HttpResponse::addPayloadInternal(VPackSlice output, size_t inputLength,
                                      VPackOptions const* options, bool resolveExternals) {
  if (!options) {
    options = &velocypack::Options::Defaults;
  }

  switch (_contentType) {
    case rest::ContentType::VPACK: {
      // will contain sanitized data
      VPackBuffer<uint8_t> tmpBuffer;
      if (resolveExternals) {
        bool resolveExt = VelocyPackHelper::hasNonClientTypes(output, true, true);
        if (resolveExt) {                  // resolve
          tmpBuffer.reserve(inputLength);  // reserve space already
          VPackBuilder builder(tmpBuffer, options);
          VelocyPackHelper::sanitizeNonClientTypes(output, VPackSlice::noneSlice(),
                                                   builder, options, true, true);
          output = VPackSlice(tmpBuffer.data());
        }
      }

      VPackValueLength length = output.byteSize();
      if (_generateBody) {
        _body->appendText(output.startAs<const char>(), length);
      } else {
        headResponse(length);
      }
      break;
    }
    default: {
      setContentType(rest::ContentType::JSON);
      if (_generateBody) {
        arangodb::basics::VelocyPackDumper dumper(_body.get(), options);
        dumper.dumpValue(output);
      } else {
        // TODO can we optimize this?
        // Just dump some where else to find real length
        StringBuffer tmp(false);

        // convert object to string
        VPackStringBufferAdapter buffer(tmp.stringBuffer());

        // usual dumping -  but not to the response body
        VPackDumper dumper(&buffer, options);
        dumper.dump(output);

        headResponse(tmp.length());
      }
    }
  }
}
