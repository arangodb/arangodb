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

#include "HttpRequest.h"
#include "Basics/NumberUtils.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;

HttpRequest::HttpRequest(ConnectionInfo const& connectionInfo, char const* header,
                         size_t length, bool allowMethodOverride)
    : GeneralRequest(connectionInfo),
      _contentLength(0),
      _header(nullptr),
      _allowMethodOverride(allowMethodOverride),
      _vpackBuilder(nullptr) {
  if (0 < length) {
    _contentType = ContentType::JSON;
    _contentTypeResponse = ContentType::JSON;
    _header = std::unique_ptr<char[]>(new char[length + 1]);
    memcpy(_header.get(), header, length);

    (_header.get())[length] = 0;
    parseHeader(length);
  }
}

HttpRequest::HttpRequest(ContentType contentType, char const* body, int64_t contentLength,
                         std::unordered_map<std::string, std::string> const& headers)
    : GeneralRequest(ConnectionInfo()),
      _contentLength(contentLength),
      _header(nullptr),
      _body(body, contentLength),
      _allowMethodOverride(false),
      _vpackBuilder(nullptr) {
  _contentType = contentType;
  _contentTypeResponse = contentType;
  GeneralRequest::_headers = headers;
}

void HttpRequest::parseHeader(size_t length) {
  char* start = _header.get();
  char* end = start + length;
  size_t const versionLength = strlen("http/1.x");

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
        *e = ::tolower(*e);
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
              // trim value
              while (e < end && *e == ' ') {
                ++e;
              }

              if ((size_t)(end - e) > versionLength) {
                if ((e[0] == 'h' || e[0] == 'H') && (e[1] == 't' || e[1] == 'T') &&
                    (e[2] == 't' || e[2] == 'T') && (e[3] == 'p' || e[3] == 'P') &&
                    e[4] == '/' && e[5] == '1' && e[6] == '.') {
                  if (e[7] == '1') {
                    _version = ProtocolVersion::HTTP_1_1;
                  } else if (e[7] == '0') {
                    _version = ProtocolVersion::HTTP_1_0;
                  } else {
                    _version = ProtocolVersion::UNKNOWN;
                  }

                  e += versionLength;
                }
              }

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

          // do NOT url-decode the path, we need to distingush between
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
            setFullUrl(pathBegin, pathEnd);
          }

          // no question mark
          else if (*f == ' ' || *f == '\n') {
            *pathEnd = '\0';

            paramEnd = paramBegin = f;

            // set full url = complete path
            setFullUrl(pathBegin, pathEnd);
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
            setFullUrl(pathBegin, paramEnd);

            // now that the full url was saved, we can insert the null bytes
            *pathEnd = '\0';
          }

          if (pathBegin < pathEnd) {
            setRequestPath(pathBegin, pathEnd);
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
        *e = ::tolower(*e);
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

void HttpRequest::setArrayValue(std::string const&& key, std::string const&& value) {
  _arrayValues[key].emplace_back(value);
}

void HttpRequest::setArrayValue(char* key, size_t length, char const* value) {
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
    _contentLength = NumberUtils::atoi_zero<int64_t>(value, value + valueLength);
    // do not store this header
    return;
  }

  if (keyLength == StaticStrings::Accept.size() &&
      valueLength == StaticStrings::MimeTypeVPack.size() &&
      memcmp(key, StaticStrings::Accept.c_str(), keyLength) == 0 &&
      memcmp(value, StaticStrings::MimeTypeVPack.c_str(), valueLength) == 0) {
    _contentTypeResponse = ContentType::VPACK;
  } else if (keyLength == StaticStrings::ContentTypeHeader.size() &&
             valueLength == StaticStrings::MimeTypeVPack.size() &&
             memcmp(key, StaticStrings::ContentTypeHeader.c_str(), keyLength) == 0 &&
             memcmp(value, StaticStrings::MimeTypeVPack.c_str(), valueLength) == 0) {
    _contentType = ContentType::VPACK;
    // don't insert this header!!
    return;
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
      StringUtils::tolowerInPlace(&overriddenType);

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

std::string const& HttpRequest::body() const { return _body; }

void HttpRequest::setBody(char const* body, size_t length) {
  TRI_ASSERT(body != nullptr);
  _body.reserve(length + 1);
  _body.append(body, length);
  // make sure the string is null-terminated
  _body[length] = '\0';
}

VPackSlice HttpRequest::payload(VPackOptions const* options) {
  TRI_ASSERT(options != nullptr);

  if (_contentType == ContentType::JSON) {
    if (!_body.empty()) {
      if (!_vpackBuilder) {
        VPackParser parser(options);
        parser.parse(_body);
        _vpackBuilder = parser.steal();
      }
      return VPackSlice(_vpackBuilder->slice());
    }
    return VPackSlice::noneSlice();  // no body
  } else if (_contentType == ContentType::VPACK) {
    VPackOptions validationOptions = *options;  // intentional copy
    validationOptions.validateUtf8Strings = true;
    VPackValidator validator(&validationOptions);
    validator.validate(_body.c_str(), _body.length());
    return VPackSlice(_body.c_str());
  }
  return VPackSlice::noneSlice();
}

HttpRequest* HttpRequest::createHttpRequest(
    ContentType contentType, char const* body, int64_t contentLength,
    std::unordered_map<std::string, std::string> const& headers) {
  return new HttpRequest(contentType, body, contentLength, headers);
}
