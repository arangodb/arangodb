////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/debugging.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;

HttpRequest::HttpRequest(ConnectionInfo const& connectionInfo,
                         uint64_t mid, bool allowMethodOverride)
    : GeneralRequest(connectionInfo, mid),
      _allowMethodOverride(allowMethodOverride),
      _validatedPayload(false) {
  _contentType = ContentType::UNSET;
  _contentTypeResponse = ContentType::JSON;
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
            if (q[0] == '/' && q[1] == '_' && q[2] == 'd' && q[3] == 'b' && q[4] == '/') {
              // request contains database name
              q += 5;
              pathBegin = q;

              // read until end of database name
              while (*q != '\0') {
                if (*q == '/' || *q == '?' || *q == ' ' || *q == '\n' || *q == '\r') {
                  break;
                }
                ++q;
              }

              _databaseName = std::string(pathBegin, q - pathBegin);

              pathBegin = q;
            }
          }

          // no space, question mark or end-of-line
          if (f == valueEnd) {
            *pathEnd = '\0';
            paramEnd = paramBegin = pathEnd;

            // set full url = complete path
            _fullUrl = std::string(pathBegin, pathEnd - pathBegin);
          }

          // no question mark
          else if (*f == ' ' || *f == '\n') {
            *pathEnd = '\0';

            paramEnd = paramBegin = f;

            // set full url = complete path
            _fullUrl = std::string(pathBegin, pathEnd - pathBegin);
          }

          // found a question mark
          else {
            paramBegin = g + 1;
            paramEnd = f + 1;

            *g++ = '?';

            while (paramEnd < valueEnd && *paramEnd != ' ' && *paramEnd != '\n') {
              *g++ = *paramEnd++;
            }

            *g = '\0';
            paramEnd = g;

            // set full url = complete path + query parameters
            _fullUrl = std::string(pathBegin, paramEnd - pathBegin);

            // now that the full url was saved, we can insert the null bytes
            *pathEnd = '\0';
          }

          if (pathBegin < pathEnd) {
            _requestPath = std::string(pathBegin, pathEnd - pathBegin);
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
          setHeader(keyBegin, keyEnd - keyBegin, valueBegin, valueEnd - valueBegin);
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
          setHeader(keyBegin, keyEnd - keyBegin);
        }
      }
    }

    lineNum++;
  }
}

namespace {
  std::string url_decode(const char* begin, const char* end) {
    std::string out;
    out.reserve(static_cast<size_t>(end - begin));
    for (const char* i = begin; i != end; ++i) {
      std::string::value_type c = (*i);
      if (c == '%') {
        if (i + 2 < end) {
          int h = StringUtils::hex2int(i[1], 0) << 4;
          h += StringUtils::hex2int(i[2], 0);
          out.push_back(static_cast<char>(h & 0x7F));
          i += 2;
        }
      } else if (c == '+') {
        out.push_back(' ');
      } else {
        out.push_back(c);
      }
    }
    return out;
  }
}

void HttpRequest::parseUrl(const char* path, size_t length) {
  std::string tmp;
  tmp.reserve(length);
  // get rid of '//'
  for (size_t i = 0; i < length; ++i) {
    tmp.push_back(path[i]);
    if (path[i] == '/') {
      while (i + 1 < length && path[i+1] == '/') {
        ++i;
      }
    }
  }

  const char* start = tmp.data();
  const char* end = start + tmp.size();
  // look for database name in URL
  if (end - start >= 5) {
    char const* q = start;

    // check if the prefix is "_db"
    if (q[0] == '/' && q[1] == '_' && q[2] == 'd' && q[3] == 'b' && q[4] == '/') {
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
      _databaseName.assign(start, q - start);
      _fullUrl.assign(q, end - q);

      start = q;
    } else {
      _fullUrl.assign(start, end - start);
    }
  } else {
    _fullUrl.assign(start, end - start);
  }
  TRI_ASSERT(!_fullUrl.empty());

  char const* q = start;
  while (q != end && *q != '?') {
    ++q;
  }

  if (q == end || *q == '?') {
    _requestPath.assign(start, q - start);
  }
  if (q == end) {
    return;
  }

  bool keyPhase = true;
  const char* keyBegin = ++q;
  const char* keyEnd = keyBegin;
  const char* valueBegin = nullptr;

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

    if (q + 1 == end || *(q + 1) == '&') {
      ++q; // skip ahead

      std::string val = ::url_decode(valueBegin, q);
      if (keyEnd - keyBegin > 2 && *(keyEnd - 2) == '[' && *(keyEnd - 1) == ']') {
        // found parameter xxx[]
        _arrayValues[::url_decode(keyBegin, keyEnd - 2)]
        .emplace_back(std::move(val));
      } else {
        _values[::url_decode(keyBegin, keyEnd)] = std::move(val);
      }
      keyPhase = true;
      keyBegin = q + 1;
      continue;
    }
    ++q;
  }
}

void HttpRequest::setHeaderV2(std::string&& key, std::string&& value) {
  StringUtils::tolowerInPlace(key); // always lowercase key

  if (key == StaticStrings::ContentLength) {
    size_t len = NumberUtils::atoi_zero<uint64_t>(value.c_str(), value.c_str() + value.size());
    if (_payload.capacity() < len) {
      // lets not reserve more than 64MB at once
      uint64_t maxReserve = std::min<uint64_t>(2 << 26, len);
      _payload.reserve(maxReserve);
    }
    // do not store this header
    return;
  }

  if (key == StaticStrings::Accept) {
    _contentTypeResponse = rest::stringToContentType(value, /*default*/ContentType::JSON);
    return;
  } else if ((_contentType == ContentType::UNSET) &&
             (key == StaticStrings::ContentTypeHeader)) {
    auto res = rest::stringToContentType(value, /*default*/ContentType::UNSET);
    // simon: the "@arangodb/requests" module by default the "text/plain" content-types for JSON
    // in most tests. As soon as someone fixes all the tests we can enable these again.
    if (res == ContentType::JSON || res == ContentType::VPACK || res == ContentType::DUMP) {
      _contentType = res;
      return;
    }
  } else if (key == StaticStrings::AcceptEncoding) {
    // This can be much more elaborated as the can specify weights on encodings
    // However, for now just toggle on deflate if deflate is requested
    if (StaticStrings::EncodingDeflate == value) {
      // FXIME: cannot use substring search, Java driver chokes on deflated response
      //if (value.find(StaticStrings::EncodingDeflate) != std::string::npos) {
      _acceptEncoding = EncodingType::DEFLATE;
    }
  }

  if (key == "cookie") {
    parseCookies(value.c_str(), value.size());
    return;
  }

  if (_allowMethodOverride && key.size() >= 13 && key[0] == 'x' && key[1] == '-') {
    // handle x-... headers

    // override HTTP method?
    if (key == "x-http-method" ||
        key == "x-method-override" ||
        key == "x-http-method-override") {
      StringUtils::tolowerInPlace(value);
      _type = findRequestType(value.c_str(), value.size());
      // don't insert this header!!
      return;
    }
  }

  _headers[std::move(key)] = std::move(value);
}

void HttpRequest::setArrayValue(char const* key, size_t length, char const* value) {
  TRI_ASSERT(key != nullptr);
  TRI_ASSERT(value != nullptr);
  _arrayValues[std::string(key, length)].emplace_back(value);
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
        setArrayValue(keyBegin, key - keyBegin - 2, valueBegin);
      } else {
        _values[std::string(keyBegin, key - keyBegin)] =
            std::string(valueBegin, value - valueBegin);
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
      setArrayValue(keyBegin, key - keyBegin - 2, valueBegin);
    } else {
      _values[std::string(keyBegin, key - keyBegin)] =
          std::string(valueBegin, value - valueBegin);
    }
  }
}

/// @brief sets a key/value header
void HttpRequest::setHeader(char const* key, size_t keyLength,
                            char const* value, size_t valueLength) {
  TRI_ASSERT(key != nullptr);
  TRI_ASSERT(value != nullptr);

  if (keyLength == StaticStrings::ContentLength.size() &&
      memcmp(key, StaticStrings::ContentLength.c_str(), keyLength) == 0) {  // 14 = strlen("content-length")
    // do not store this header
    return;
  }

  if (keyLength == StaticStrings::Accept.size() &&
      valueLength == StaticStrings::MimeTypeVPack.size() &&
      memcmp(key, StaticStrings::Accept.c_str(), keyLength) == 0 &&
      memcmp(value, StaticStrings::MimeTypeVPack.c_str(), valueLength) == 0) {
    _contentTypeResponse = ContentType::VPACK;
  } else if (keyLength == StaticStrings::AcceptEncoding.size() &&
      valueLength == StaticStrings::EncodingDeflate.size() &&
      memcmp(key, StaticStrings::AcceptEncoding.c_str(), keyLength) == 0 &&
      memcmp(value, StaticStrings::EncodingDeflate.c_str(), valueLength) == 0) {
    // This can be much more elaborated as the can specify weights on encodings
    // However, for now just toggle on deflate if deflate is requested
    _acceptEncoding = EncodingType::DEFLATE;
  } else if ((_contentType == ContentType::UNSET) &&
             (keyLength == StaticStrings::ContentTypeHeader.size()) &&
             (memcmp(key, StaticStrings::ContentTypeHeader.c_str(), keyLength) == 0)) {
    if (valueLength == StaticStrings::MimeTypeVPack.size() &&
        memcmp(value, StaticStrings::MimeTypeVPack.c_str(), valueLength) == 0) {
      _contentType = ContentType::VPACK;
      // don't insert this header!!
      return;
    }
    if (valueLength >= StaticStrings::MimeTypeJsonNoEncoding.size() &&
        memcmp(value, StaticStrings::MimeTypeJsonNoEncoding.c_str(),
               StaticStrings::MimeTypeJsonNoEncoding.length()) == 0) {
      _contentType = ContentType::JSON;
      // don't insert this header!!
      return;
    }
  }

  if (keyLength == 6 && memcmp(key, "cookie", keyLength) == 0) {  // 6 = strlen("cookie")
    parseCookies(value, valueLength);
    return;
  }

  if (_allowMethodOverride && keyLength >= 13 && *key == 'x' && *(key + 1) == '-') {
    // handle x-... headers

    // override HTTP method?
    if ((keyLength == 13 && memcmp(key, "x-http-method", keyLength) == 0) ||
        (keyLength == 17 && memcmp(key, "x-method-override", keyLength) == 0) ||
        (keyLength == 22 && memcmp(key, "x-http-method-override", keyLength) == 0)) {
      std::string overriddenType(value, valueLength);
      StringUtils::tolowerInPlace(overriddenType);

      _type = findRequestType(overriddenType.c_str(), overriddenType.size());

      // don't insert this header!!
      return;
    }
  }

  _headers[std::string(key, keyLength)] = std::string(value, valueLength);
}

/// @brief sets a key-only header
void HttpRequest::setHeader(char const* key, size_t keyLength) {
  _headers[std::string(key, keyLength)] = StaticStrings::Empty;
}

void HttpRequest::setCookie(char* key, size_t length, char const* value) {
  _cookies[std::string(key, length)] = value;
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

      setCookie(keyBegin, key - keyBegin, valueBegin);

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
      valueBegin = value = key;
    } else {
      *value = '\0';
    }

    setCookie(keyBegin, key - keyBegin, valueBegin);
  }
}

std::string const& HttpRequest::cookieValue(std::string const& key) const {
  auto it = _cookies.find(key);

  if (it == _cookies.end()) {
    return StaticStrings::Empty;
  }

  return it->second;
}

std::string const& HttpRequest::cookieValue(std::string const& key, bool& found) const {
  auto it = _cookies.find(key);

  if (it == _cookies.end()) {
    found = false;
    return StaticStrings::Empty;
  }

  found = true;
  return it->second;
}

VPackStringRef HttpRequest::rawPayload() const {
  return VPackStringRef(reinterpret_cast<const char*>(_payload.data()), _payload.size());
};

VPackSlice HttpRequest::payload(bool strictValidation) {
  if ((_contentType == ContentType::UNSET) || (_contentType == ContentType::JSON)) {
    if (!_payload.empty()) {
      if (!_vpackBuilder) {
        TRI_ASSERT(!_validatedPayload);
        VPackOptions const* options = validationOptions(strictValidation);
        VPackParser parser(options);
        parser.parse(_payload.data(), _payload.size());
        _vpackBuilder = parser.steal();
        _validatedPayload = true;
      }
      TRI_ASSERT(_validatedPayload);
      return VPackSlice(_vpackBuilder->slice());
    }
    return VPackSlice::noneSlice();  // no body
  } else if (_contentType == ContentType::VPACK) {
    if (!_validatedPayload) {
      VPackOptions const* options = validationOptions(strictValidation);
      VPackValidator validator(options);
      _validatedPayload = validator.validate(_payload.data(), _payload.length()); // throws on error
    }
    TRI_ASSERT(_validatedPayload);
    return VPackSlice(reinterpret_cast<uint8_t const*>(_payload.data()));
  }
  return VPackSlice::noneSlice();
}
