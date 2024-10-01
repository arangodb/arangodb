////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include <velocypack/Sink.h>
#include <time.h>

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Meta/conversion.h"
#include "Rest/GeneralRequest.h"

#include <string_view>

using namespace arangodb;
using namespace arangodb::basics;

HttpResponse::HttpResponse(ResponseCode code, uint64_t mid,
                           std::unique_ptr<basics::StringBuffer> buffer,
                           rest::ResponseCompressionType rct)
    : GeneralResponse(code, mid),
      _body(std::move(buffer)),
      _bodySize(0),
      _allowCompression(rct) {
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
                             std::string const& domain, bool secure,
                             bool httpOnly) {
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
      buffer.appendText(std::string_view("; expires="));
      buffer.appendText(buffer2);
    }
  }

  if (!path.empty()) {
    buffer.appendText(std::string_view("; path="));
    buffer.appendText(path);
  }

  if (!domain.empty()) {
    buffer.appendText(std::string_view("; domain="));
    buffer.appendText(domain);
  }

  if (secure) {
    buffer.appendText(std::string_view("; secure"));
  }

  if (httpOnly) {
    buffer.appendText(std::string_view("; HttpOnly"));
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

void HttpResponse::clearBody() noexcept {
  _body->clear();
  _bodySize = 0;
}

void HttpResponse::setAllowCompression(
    rest::ResponseCompressionType rct) noexcept {
  if (_allowCompression == rest::ResponseCompressionType::kUnset) {
    _allowCompression = rct;
  }
}

rest::ResponseCompressionType HttpResponse::compressionAllowed()
    const noexcept {
  return _allowCompression;
}

void HttpResponse::writeHeader(StringBuffer* output) {
  output->appendText(std::string_view("HTTP/1.1 "));
  output->appendText(responseString(_responseCode));
  output->appendText("\r\n", 2);

  bool seenServerHeader = false;
  bool seenTransferEncodingHeader = false;
  std::string transferEncoding;

  for (auto const& it : _headers) {
    std::string const& key = it.first;
    size_t const keyLength = key.size();

    // ignore content-length
    if (key == StaticStrings::ContentLength) {
      continue;
    } else if (key == StaticStrings::Connection) {
      // this ensures we don't print two "Connection" headers
      continue;
    }

    // save transfer encoding
    if (key == StaticStrings::TransferEncoding) {
      seenTransferEncodingHeader = true;
      transferEncoding = it.second;
      continue;
    }

    if (key == StaticStrings::Server) {
      // this ensures we don't print two "Server" headers
      seenServerHeader = true;
      // go on and use the user-defined "Server" header value
    }

    // reserve enough space for header name + ": " + value + "\r\n"
    if (output->reserve(keyLength + 2 + it.second.size() + 2) !=
        TRI_ERROR_NO_ERROR) {
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
  if (!seenServerHeader) {
    output->appendText(std::string_view("Server: ArangoDB\r\n"));
  }

  // this is just used by the batch handler, close connection
  output->appendText(std::string_view("Connection: Close \r\n"));

  // add "Content-Type" header
  switch (_contentType) {
    case ContentType::UNSET:
    case ContentType::JSON:
      output->appendText(std::string_view(
          "Content-Type: application/json; charset=utf-8\r\n"));
      break;
    case ContentType::VPACK:
      output->appendText(
          std::string_view("Content-Type: application/x-velocypack\r\n"));
      break;
    case ContentType::TEXT:
      output->appendText(
          std::string_view("Content-Type: text/plain; charset=utf-8\r\n"));
      break;
    case ContentType::HTML:
      output->appendText(
          std::string_view("Content-Type: text/html; charset=utf-8\r\n"));
      break;
    case ContentType::DUMP:
      output->appendText(std::string_view(
          "Content-Type: application/x-arango-dump; charset=utf-8\r\n"));
      break;
    case ContentType::CUSTOM: {
      // intentionally don't print anything here
      // the header should have been in _headers already, and should have been
      // handled above
    }
  }

  for (auto const& it : _cookies) {
    output->appendText(std::string_view("Set-Cookie: "));
    output->appendText(it);
    output->appendText("\r\n", 2);
  }

  if (seenTransferEncodingHeader && transferEncoding == "chunked") {
    output->appendText(std::string_view("Transfer-Encoding: chunked\r\n\r\n"));
  } else {
    if (seenTransferEncodingHeader) {
      output->appendText(std::string_view("Transfer-Encoding: "));
      output->appendText(transferEncoding);
      output->appendText("\r\n", 2);
    }

    // From http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.13
    //
    // 14.13 Content-Length
    //
    // "The Content-Length entity-header field indicates the size of the
    // entity-body, in decimal number of OCTETs, sent to the recipient or, in
    // the case of the HEAD method, the size of the entity-body that would have
    // been sent had the request been a GET."
    //
    // Note that a corner case exists where the HEAD method is sent with an
    // X-Arango-Async header. This causes the server to store the result, which
    // can be later retrieved via PUT. However, the PUT response cannot possibly
    // return the initial Content-Length header, but will return 0 instead.
    output->appendText(std::string_view("Content-Length: "));
    output->appendInteger(bodySize());
    output->appendText("\r\n\r\n", 4);
  }
  // end of header, body to follow
}

void HttpResponse::addPayload(VPackSlice slice,
                              velocypack::Options const* options,
                              bool resolveExternals) {
  if (_contentType == rest::ContentType::JSON &&
      _contentTypeRequested == rest::ContentType::VPACK) {
    // content type was set by a handler to Json but the client wants VPACK
    // as we have a slice at had we are able to reply with VPACK
    _contentType = rest::ContentType::VPACK;
  }

  addPayloadInternal(slice.start(), slice.byteSize(), options,
                     resolveExternals);
}

void HttpResponse::addPayload(VPackBuffer<uint8_t>&& buffer,
                              velocypack::Options const* options,
                              bool resolveExternals) {
  if (_contentType == rest::ContentType::JSON &&
      _contentTypeRequested == rest::ContentType::VPACK) {
    // content type was set by a handler to Json but the client wants VPACK
    // as we have a slice at had we are able to reply with VPACK
    _contentType = rest::ContentType::VPACK;
  }

  if (buffer.size() > 0) {
    addPayloadInternal(buffer.data(), buffer.length(), options,
                       resolveExternals);
  }
}

void HttpResponse::addRawPayload(std::string_view payload) {
  _body->appendText(payload.data(), payload.length());
}

void HttpResponse::addPayloadInternal(uint8_t const* data, size_t length,
                                      VPackOptions const* options,
                                      bool resolveExternals) {
  TRI_ASSERT(data != nullptr);

  if (!options) {
    options = &velocypack::Options::Defaults;
  }
  TRI_ASSERT(options != nullptr);

  if (_contentType == rest::ContentType::VPACK) {
    // the input (data) may contain multiple velocypack values, written
    // one after the other
    // here, we iterate over the slices in the input data, until we have
    // reached the specified total size (length)

    // total length of our generated response
    size_t resultLength = 0;

    while (length > 0) {
      VPackSlice currentData(data);
      VPackValueLength const inputLength = currentData.byteSize();
      VPackValueLength outputLength = inputLength;

      TRI_ASSERT(length >= inputLength);

      // will contain sanitized data
      VPackBuffer<uint8_t> tmpBuffer;
      if (resolveExternals) {
        bool resolveExt = VelocyPackHelper::hasNonClientTypes(currentData);
        if (resolveExt) {                  // resolve
          tmpBuffer.reserve(inputLength);  // reserve space already
          VPackBuilder builder(tmpBuffer, options);
          VelocyPackHelper::sanitizeNonClientTypes(
              currentData, VPackSlice::noneSlice(), builder, *options);
          currentData = VPackSlice(tmpBuffer.data());
          outputLength = currentData.byteSize();
        }
      }

      if (_generateBody) {
        _body->appendText(currentData.startAs<const char>(), outputLength);
      }
      resultLength += outputLength;

      // advance to next slice (if any)
      if (length < inputLength) {
        // oops, length specification may be wrong?!
        break;
      }

      data += inputLength;
      length -= inputLength;
    }

    if (!_generateBody) {
      headResponse(resultLength);
    }
    return;
  }

  setContentType(rest::ContentType::JSON);

  /// dump options contain have the escapeUnicode attribute set to true
  /// this allows dumping of string values as plain 7-bit ASCII values.
  /// for example, the string "möter" will be dumped as "m\u00F6ter".
  /// this allows faster JSON parsing in some client implementations,
  /// which speculate on ASCII strings first and only fall back to slower
  /// multibyte strings on first actual occurrence of a multibyte character.
  VPackOptions tmpOpts = *options;
  tmpOpts.escapeUnicode = true;

  // here, the input (data) must **not** contain multiple velocypack values,
  // written one after the other
  VPackSlice current(data);
  TRI_ASSERT(current.byteSize() == length);

  if (_generateBody) {
    // convert object to JSON string
    VPackStringBufferAdapter buffer(_body->stringBuffer());

    velocypack::Dumper dumper(&buffer, &tmpOpts);
    dumper.dump(current);
  } else {
    // determine the length of the to-be-generated JSON string,
    // without actually generating it
    velocypack::StringLengthSink sink;

    // usual dumping -  but not to the response body
    velocypack::Dumper dumper(&sink, &tmpOpts);
    dumper.dump(current);

    headResponse(static_cast<size_t>(sink.length()));
  }
}

ErrorCode HttpResponse::zlibDeflate(bool onlyIfSmaller) {
  return _body->zlibDeflate(onlyIfSmaller);
}

ErrorCode HttpResponse::gzipCompress(bool onlyIfSmaller) {
  return _body->gzipCompress(onlyIfSmaller);
}

ErrorCode HttpResponse::lz4Compress(bool onlyIfSmaller) {
  return _body->lz4Compress(onlyIfSmaller);
}
