////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "HttpRequest.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/debugging.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/Validator.h>

using namespace arangodb;
using namespace arangodb::basics;

namespace {
std::string urlDecode(char const* begin, char const* end) {
  std::string out;
  out.reserve(static_cast<size_t>(end - begin));
  for (char const* i = begin; i != end; ++i) {
    std::string::value_type c = (*i);
    if (c == '%') {
      if (i + 2 < end) {
        int h = StringUtils::hex2int(i[1], 256) << 4;
        h += StringUtils::hex2int(i[2], 256);
        if (h >= 256) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER, "invalid encoding value in request URL");
        }
        out.push_back(static_cast<char>(h & 0xFF));
        i += 2;
      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "invalid encoding value in request URL");
      }
    } else if (c == '+') {
      out.push_back(' ');
    } else {
      out.push_back(c);
    }
  }

  return out;
}
}  // namespace

HttpRequest::HttpRequest(ConnectionInfo const& connectionInfo, uint64_t mid,
                         bool allowMethodOverride)
    : GeneralRequest(connectionInfo, mid),
      _allowMethodOverride(allowMethodOverride),
      _validatedPayload(false) {
  _contentType = ContentType::UNSET;
  _contentTypeResponse = ContentType::JSON;
  TRI_ASSERT(_memoryUsage == 0);
  _memoryUsage += sizeof(HttpRequest);
}

HttpRequest::~HttpRequest() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto expected = sizeof(HttpRequest) + _fullUrl.size() + _requestPath.size() +
                  _databaseName.size() + _user.size() + _prefix.size() +
                  _contentTypeResponsePlain.size() + _payload.size();
  for (auto const& it : _suffixes) {
    expected += it.size();
  }
  for (auto const& it : _headers) {
    expected += it.first.size() + it.second.size();
  }
  for (auto const& it : _cookies) {
    expected += it.first.size() + it.second.size();
  }
  for (auto const& it : _values) {
    expected += it.first.size() + it.second.size();
  }
  for (auto const& it : _arrayValues) {
    expected += it.first.size();
    for (auto const& it2 : it.second) {
      expected += it2.size();
    }
  }
  if (_vpackBuilder) {
    expected += _vpackBuilder->bufferRef().size();
  }

  TRI_ASSERT(_memoryUsage == expected)
      << "expected memory usage: " << expected << ", actual: " << _memoryUsage
      << ", diff: " << (_memoryUsage - expected);
#endif
}

void HttpRequest::appendBody(char const* data, size_t size) {
  _payload.append(data, size);
  _memoryUsage += size;
}

void HttpRequest::appendNullTerminator() {
  _payload.push_back('\0');
  _payload.resetTo(_payload.size() - 1);
  // DO NOT increase memory usage here
}

void HttpRequest::clearBody() noexcept {
  auto old = _payload.size();
  _payload.clear();
  TRI_ASSERT(_memoryUsage >= old);
  _memoryUsage -= old;
}

void HttpRequest::parseHeader(char* start, size_t length) {
  char* end = start + length;

  // current line number
  int lineNum = 0;

  // begin and end of current line
  char* lineBegin = nullptr;

  // split request
  while (start < end) {
    lineBegin = start;

    // .............................................................................
    // FIRST LINE
    // .............................................................................

    // check for request type (GET/POST in line 0), path and parameters
    if (lineNum == 0) {
      // split line at space
      char* e = lineBegin;

      for (; e < end && *e != ' ' && *e != '\n'; ++e) {
        *e = ::StringUtils::tolower(*e);
      }

      // store key and value
      char* keyBegin = lineBegin;
      char* keyEnd = e;

      char* valueBegin = nullptr;
      char* valueEnd = nullptr;

      // we found a space (*end is '\0')
      if (*e == ' ') {
        // extract the value
        keyEnd = e;

        // trim value from the start
        while (e < end && *e == ' ') {
          ++e;
        }

        *keyEnd = '\0';

        // there is no value at all
        if (e == end) {
          valueBegin = valueEnd = keyEnd;
          start = end;
        }

        // there is only a NL
        else if (*e == '\n') {
          valueBegin = valueEnd = keyEnd;
          start = e + 1;
        }

        // find space
        else {
          valueBegin = e;

          while (e < end && *e != '\n' && *e != ' ') {
            ++e;
          }

          if (e == end) {
            valueEnd = e;
            start = end;
          } else {
            if (*e == '\n') {
              valueEnd = e;
              start = e + 1;

              // skip \r
              if (valueBegin < valueEnd && valueEnd[-1] == '\r') {
                --valueEnd;
              }
            } else {
              valueEnd = e;

              // HTTP protocol version is expected next

              // go on until eol
              while (e < end && *e != '\n') {
                ++e;
              }

              if (e == end) {
                start = end;
              } else {
                start = e + 1;
              }
            }

            *valueEnd = '\0';
          }
        }

        // check the key
        _type = findRequestType(keyBegin, keyEnd - keyBegin);

        // extract the path and decode the url and parameters
        if (_type != RequestType::ILLEGAL) {
          char* pathBegin = valueBegin;
          char* pathEnd = nullptr;

          char* paramBegin = nullptr;
          char* paramEnd = nullptr;

          // find a question mark or space
          char* f = pathBegin;

          // get ride of "//"
          char* g = f;

          // do NOT url-decode the path, we need to distinguish between
          // "/document/a/b" and "/document/a%2fb"

          while (f < valueEnd && *f != '?' && *f != ' ' && *f != '\n') {
            *g++ = *f;

            if (*f == '/') {
              while (f < valueEnd && *f == '/') {
                ++f;
              }
            } else {
              ++f;
            }
          }

          pathEnd = g;

          // look for database name in URL
          if (pathEnd - pathBegin >= 5) {
            char* q = pathBegin;

            // check if the prefix is "_db"
            if (q[0] == '/' && q[1] == '_' && q[2] == 'd' && q[3] == 'b' &&
                q[4] == '/') {
              // request contains database name
              q += 5;
              pathBegin = q;

              // read until end of database name
              while (*q != '\0') {
                if (*q == '/' || *q == '?' || *q == ' ' || *q == '\n' ||
                    *q == '\r') {
                  break;
                }
                ++q;
              }

              setDatabaseName(::urlDecode(pathBegin, q));
              if (_databaseName != normalizeUtf8ToNFC(_databaseName)) {
                THROW_ARANGO_EXCEPTION_MESSAGE(
                    TRI_ERROR_ARANGO_ILLEGAL_NAME,
                    "database name is not properly UTF-8 NFC-normalized");
              }

              pathBegin = q;
            }
          }

          // no space, question mark or end-of-line
          if (f == valueEnd) {
            *pathEnd = '\0';
            paramEnd = paramBegin = pathEnd;

            // set full url = complete path
            setFullUrl(std::string(pathBegin, pathEnd - pathBegin));
          }

          // no question mark
          else if (*f == ' ' || *f == '\n') {
            *pathEnd = '\0';

            paramEnd = paramBegin = f;

            // set full url = complete path
            setFullUrl(std::string(pathBegin, pathEnd - pathBegin));
          }

          // found a question mark
          else {
            paramBegin = g + 1;
            paramEnd = f + 1;

            *g++ = '?';

            while (paramEnd < valueEnd && *paramEnd != ' ' &&
                   *paramEnd != '\n') {
              *g++ = *paramEnd++;
            }

            *g = '\0';
            paramEnd = g;

            // set full url = complete path + query parameters
            setFullUrl(std::string(pathBegin, paramEnd - pathBegin));

            // now that the full url was saved, we can insert the null bytes
            *pathEnd = '\0';
          }

          if (pathBegin < pathEnd) {
            setRequestPath(std::string(pathBegin, pathEnd - pathBegin));
          }

          if (paramBegin < paramEnd) {
            setValues(paramBegin, paramEnd);
          }
        }
      }

      // no space, either eol or newline
      else {
        if (e < end) {
          *keyEnd = '\0';
          start = e + 1;
        } else {
          start = end;
        }

        // check the key
        _type = findRequestType(keyBegin, keyEnd - keyBegin);
      }
    }

    // .............................................................................
    // OTHER LINES
    // .............................................................................

    else {
      // split line at colon
      char* e = lineBegin;

      for (; e < end && *e != ':' && *e != '\n'; ++e) {
        *e = StringUtils::tolower(*e);
      }

      // store key and value
      char* keyBegin = lineBegin;
      char* keyEnd = e;

      // we found a colon (*end is '\0')
      if (*e == ':') {
        char* valueBegin = nullptr;
        char* valueEnd = nullptr;

        // extract the value
        keyEnd = e++;

        while (e < end && *e == ' ') {
          ++e;
        }

        while (keyBegin < keyEnd && keyEnd[-1] == ' ') {
          --keyEnd;
        }

        *keyEnd = '\0';

        // there is no value at all
        if (e == end) {
          valueBegin = valueEnd = keyEnd;
          start = end;
        } else if (*e == '\n') {
          valueBegin = valueEnd = keyEnd;
          start = e + 1;
        }

        // find \n
        else {
          valueBegin = e;

          while (e < end && *e != '\n') {
            ++e;
          }

          if (e == end) {
            valueEnd = e;
            start = end;
          } else {
            valueEnd = e;
            start = e + 1;
          }

          // skip \r
          if (valueBegin < valueEnd && valueEnd[-1] == '\r') {
            --valueEnd;
          }

          // skip trailing spaces
          while (valueBegin < valueEnd && valueEnd[-1] == ' ') {
            --valueEnd;
          }

          *valueEnd = '\0';
        }

        if (keyBegin < keyEnd) {
          setHeader(std::string(keyBegin, keyEnd - keyBegin),
                    std::string(valueBegin, valueEnd - valueBegin));
        }
      }

      // we found no colon, either eol or newline. Take the whole line as key
      else {
        if (e < end) {
          *keyEnd = '\0';
          start = e + 1;
        } else {
          start = end;
        }

        // skip \r
        if (keyBegin < keyEnd && keyEnd[-1] == '\r') {
          --keyEnd;
        }

        // use empty value
        if (keyBegin < keyEnd) {
          setHeader(std::string(keyBegin, keyEnd - keyBegin),
                    std::string("", 0));
        }
      }
    }

    lineNum++;
  }
}

void HttpRequest::parseUrl(char const* path, size_t length) {
  std::string tmp;
  tmp.reserve(length);
  // get rid of '//'
  for (size_t i = 0; i < length; ++i) {
    tmp.push_back(path[i]);
    if (path[i] == '/') {
      while (i + 1 < length && path[i + 1] == '/') {
        ++i;
      }
    }
  }

  char const* start = tmp.data();
  char const* end = start + tmp.size();
  // look for database name in URL
  if (end - start >= 5) {
    char const* q = start;

    // check if the prefix is "_db"
    if (q[0] == '/' && q[1] == '_' && q[2] == 'd' && q[3] == 'b' &&
        q[4] == '/') {
      // request contains database name
      q += 5;
      start = q;

      // read until end of database name
      while (q < end) {
        if (*q == '/' || *q == '?' || *q == ' ' || *q == '\n' || *q == '\r') {
          break;
        }
        ++q;
      }

      TRI_ASSERT(q >= start);
      setDatabaseName(::urlDecode(start, q));
      if (_databaseName != normalizeUtf8ToNFC(_databaseName)) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_ARANGO_ILLEGAL_NAME,
            "database name is not properly UTF-8 NFC-normalized");
      }
      setFullUrl(std::string(q, end - q));
      start = q;
    } else {
      setFullUrl(std::string(start, end - start));
    }
  } else {
    setFullUrl(std::string(start, end - start));
  }

  TRI_ASSERT(!_fullUrl.empty());

  char const* q = start;
  while (q != end && *q != '?') {
    ++q;
  }

  if (q == end || *q == '?') {
    setRequestPath(std::string(start, q - start));
  }
  if (q == end) {
    return;
  }

  bool keyPhase = true;
  char const* keyBegin = ++q;
  char const* keyEnd = keyBegin;
  char const* valueBegin = nullptr;

  while (q != end) {
    if (keyPhase) {
      keyEnd = q;
      if (*q == '=') {
        keyPhase = false;
        valueBegin = q + 1;
      }
      ++q;
      continue;
    }

    bool isAmpersand = (*q == '&');

    if (q + 1 == end || *(q + 1) == '&' || isAmpersand) {
      if (!isAmpersand) {
        ++q;  // skip ahead
      }

      std::string val = ::urlDecode(valueBegin, q);
      if (keyEnd - keyBegin > 2 && *(keyEnd - 2) == '[' &&
          *(keyEnd - 1) == ']') {
        // found parameter xxx[]
        setArrayValue(::urlDecode(keyBegin, keyEnd - 2), std::move(val));
      } else {
        setValue(::urlDecode(keyBegin, keyEnd), std::move(val));
      }
      keyPhase = true;
      keyBegin = q + 1;

      if (!isAmpersand) {
        continue;
      }
    }
    ++q;
  }
}

void HttpRequest::setHeader(std::string key, std::string value) {
  StringUtils::tolowerInPlace(key);  // always lowercase key

  if (key == StaticStrings::ContentLength) {
    size_t len = NumberUtils::atoi_zero<uint64_t>(value.c_str(),
                                                  value.c_str() + value.size());
    if (_payload.capacity() < len) {
      // lets not reserve more than 64MB at once
      uint64_t maxReserve = std::min<uint64_t>(2 << 26, len);
      _payload.reserve(maxReserve);
    }
    // do not store this header
    return;
  }

  if (key == StaticStrings::Accept) {
    _contentTypeResponse =
        rest::stringToContentType(value, /*default*/ ContentType::JSON);
    if (value.find(',') != std::string::npos) {
      setStringValue(_contentTypeResponsePlain, std::move(value));
    } else {
      setStringValue(_contentTypeResponsePlain, std::string());
    }
    return;
  } else if (_contentType == ContentType::UNSET &&
             key == StaticStrings::ContentTypeHeader) {
    auto res = rest::stringToContentType(value, /*default*/ ContentType::UNSET);
    // simon: the "@arangodb/requests" module by default the "text/plain"
    // content-types for JSON in most tests. As soon as someone fixes all the
    // tests we can enable these again.
    if (res == ContentType::JSON || res == ContentType::VPACK ||
        res == ContentType::DUMP) {
      _contentType = res;
      return;
    }
  } else if (key == StaticStrings::AcceptEncoding) {
    // This can be much more elaborated as the can specify weights on encodings
    // However, for now just toggle on deflate if deflate is requested
    if (StaticStrings::EncodingDeflate == value) {
      // FXIME: cannot use substring search, Java driver chokes on deflated
      // response
      _acceptEncoding = EncodingType::DEFLATE;
    } else if (StaticStrings::EncodingGzip == value) {
      _acceptEncoding = EncodingType::GZIP;
    }
  } else if (key == "cookie") {
    parseCookies(value.c_str(), value.size());
    return;
  }

  if (_allowMethodOverride && key.starts_with("x-")) {
    // handle x-... headers

    // override HTTP method?
    if (key == "x-http-method" || key == "x-method-override" ||
        key == "x-http-method-override") {
      StringUtils::tolowerInPlace(value);
      _type = findRequestType(value.c_str(), value.size());
      // don't insert this header!!
      return;
    }
  }

  auto memoryUsage = key.size() + value.size();
  auto it = _headers.try_emplace(std::move(key), std::move(value));
  if (!it.second) {
    auto old = it.first->first.size() + it.first->second.size();
    // cppcheck-suppress accessMoved
    _headers[std::move(key)] = std::move(value);
    _memoryUsage -= old;
  }
  _memoryUsage += memoryUsage;
}

void HttpRequest::setValues(char* buffer, char* end) {
  char* keyBegin = nullptr;
  char* key = nullptr;

  char* valueBegin = nullptr;
  char* value = nullptr;

  enum { KEY, VALUE } phase = KEY;
  enum { NORMAL, HEX1, HEX2 } reader = NORMAL;

  int hex = 0;

  char const AMB = '&';
  char const EQUAL = '=';
  char const PERCENT = '%';
  char const PLUS = '+';

  for (keyBegin = key = buffer; buffer < end; buffer++) {
    char next = *buffer;

    if (phase == KEY && next == EQUAL) {
      phase = VALUE;

      valueBegin = value = buffer + 1;

      continue;
    } else if (next == AMB) {
      phase = KEY;

      *key = '\0';

      // check for missing value phase
      if (valueBegin == nullptr) {
        valueBegin = value = key;
      } else {
        *value = '\0';
      }

      if (key - keyBegin > 2 && (*(key - 2)) == '[' && (*(key - 1)) == ']') {
        // found parameter xxx[]
        *(key - 2) = '\0';
        setArrayValue(std::string(keyBegin, key - keyBegin - 2),
                      std::string(valueBegin, value - valueBegin));
      } else {
        setValue(std::string(keyBegin, key - keyBegin),
                 std::string(valueBegin, value - valueBegin));
      }

      keyBegin = key = buffer + 1;
      valueBegin = value = nullptr;

      continue;
    } else if (next == PERCENT) {
      reader = HEX1;
      continue;
    } else if (reader == HEX1) {
      int h1 = StringUtils::hex2int(next, -1);

      if (h1 == -1) {
        reader = NORMAL;
        --buffer;
        continue;
      }

      hex = h1 * 16;
      reader = HEX2;
      continue;
    } else if (reader == HEX2) {
      int h1 = StringUtils::hex2int(next, -1);

      if (h1 == -1) {
        --buffer;
      } else {
        hex += h1;
      }

      reader = NORMAL;
      next = static_cast<char>(hex);
    } else if (next == PLUS) {
      next = ' ';
    }

    if (phase == KEY) {
      *key++ = next;
    } else {
      *value++ = next;
    }
  }

  if (keyBegin != key) {
    *key = '\0';

    // check for missing value phase
    if (valueBegin == nullptr) {
      valueBegin = value = key;
    } else {
      *value = '\0';
    }

    if (key - keyBegin > 2 && (*(key - 2)) == '[' && (*(key - 1)) == ']') {
      // found parameter xxx[]
      *(key - 2) = '\0';
      setArrayValue(std::string(keyBegin, key - keyBegin - 2),
                    std::string(valueBegin, value - valueBegin));
    } else {
      setValue(std::string(keyBegin, key - keyBegin),
               std::string(valueBegin, value - valueBegin));
    }
  }
}

void HttpRequest::setCookie(std::string key, std::string value) {
  auto memoryUsage = key.size() + value.size();
  auto it = _cookies.try_emplace(std::move(key), std::move(value));
  if (!it.second) {
    auto old = it.first->first.size() + it.first->second.size();
    // cppcheck-suppress accessMoved
    _cookies[std::move(key)] = std::move(value);
    _memoryUsage -= old;
  }
  _memoryUsage += memoryUsage;
}

void HttpRequest::parseCookies(char const* buffer, size_t length) {
  char* keyBegin = nullptr;
  char* key = nullptr;

  char* valueBegin = nullptr;
  char* value = nullptr;

  enum { KEY, VALUE } phase = KEY;
  enum { NORMAL, HEX1, HEX2 } reader = NORMAL;

  int hex = 0;

  char const AMB = ';';
  char const EQUAL = '=';
  char const PERCENT = '%';
  char const SPACE = ' ';

  char* buffer2 = (char*)buffer;
  char* end = buffer2 + length;

  for (keyBegin = key = buffer2; buffer2 < end; buffer2++) {
    char next = *buffer2;

    if (phase == KEY && next == EQUAL) {
      phase = VALUE;

      valueBegin = value = buffer2 + 1;

      continue;
    } else if (next == AMB) {
      phase = KEY;

      *key = '\0';

      // check for missing value phase
      if (valueBegin == nullptr) {
        valueBegin = value = key;
      } else {
        *value = '\0';
      }

      setCookie(std::string(keyBegin, key - keyBegin),
                std::string(valueBegin, value - valueBegin));

      // keyBegin = key = buffer2 + 1;
      while (*(keyBegin = key = buffer2 + 1) == SPACE && buffer2 < end) {
        buffer2++;
      }
      valueBegin = value = nullptr;

      continue;
    } else if (next == PERCENT) {
      reader = HEX1;
      continue;
    } else if (reader == HEX1) {
      int h1 = StringUtils::hex2int(next, -1);

      if (h1 == -1) {
        reader = NORMAL;
        --buffer2;
        continue;
      }

      hex = h1 * 16;
      reader = HEX2;
      continue;
    } else if (reader == HEX2) {
      int h1 = StringUtils::hex2int(next, -1);

      if (h1 == -1) {
        --buffer2;
      } else {
        hex += h1;
      }

      reader = NORMAL;
      next = static_cast<char>(hex);
    }

    if (phase == KEY) {
      *key++ = next;
    } else {
      *value++ = next;
    }
  }

  if (keyBegin != key) {
    *key = '\0';

    // check for missing value phase
    if (valueBegin == nullptr) {
      valueBegin = key;
    } else {
      *value = '\0';
    }

    setCookie(std::string(keyBegin, key - keyBegin),
              std::string(valueBegin, value - valueBegin));
  }
}

std::string const& HttpRequest::cookieValue(std::string const& key) const {
  auto it = _cookies.find(key);

  if (it == _cookies.end()) {
    return StaticStrings::Empty;
  }

  return it->second;
}

std::string const& HttpRequest::cookieValue(std::string const& key,
                                            bool& found) const {
  auto it = _cookies.find(key);

  if (it == _cookies.end()) {
    found = false;
    return StaticStrings::Empty;
  }

  found = true;
  return it->second;
}

std::string_view HttpRequest::rawPayload() const {
  return std::string_view(reinterpret_cast<char const*>(_payload.data()),
                          _payload.size());
}

VPackSlice HttpRequest::payload(bool strictValidation) {
  if (_contentType == ContentType::UNSET || _contentType == ContentType::JSON) {
    if (!_payload.empty()) {
      if (!_vpackBuilder) {
        TRI_ASSERT(!_validatedPayload);
        VPackOptions const* options = validationOptions(strictValidation);
        VPackParser parser(options);
        parser.parse(_payload.data(), _payload.size());
        _vpackBuilder = parser.steal();
        _validatedPayload = true;
        _memoryUsage += _vpackBuilder->bufferRef().size();
      }
      TRI_ASSERT(_validatedPayload);
      return VPackSlice(_vpackBuilder->slice());
    }
    // no body
    // fallthrough intentional
  } else if (_contentType == ContentType::VPACK) {
    if (!_payload.empty()) {
      if (!_validatedPayload) {
        VPackOptions const* options = validationOptions(strictValidation);
        VPackValidator validator(options);
        _validatedPayload = validator.validate(
            _payload.data(), _payload.length());  // throws on error
      }
      TRI_ASSERT(_validatedPayload);
      return VPackSlice(reinterpret_cast<uint8_t const*>(_payload.data()));
    }
    // no body
    // fallthrough intentional
  }

  return VPackSlice::noneSlice();
}
