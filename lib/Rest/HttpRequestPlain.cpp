////////////////////////////////////////////////////////////////////////////////
/// @brief plain http request
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

#include "HttpRequestPlain.h"

#include "BasicsC/conversions.h"
#include "BasicsC/strings.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                   local constants
// -----------------------------------------------------------------------------

static char const* EMPTY_STR = "";

// -----------------------------------------------------------------------------
// --SECTION--                                            class HttpRequestPlain
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief http request constructor
////////////////////////////////////////////////////////////////////////////////

HttpRequestPlain::HttpRequestPlain (char const* header, size_t length)
  : HttpRequest(),
    _requestPath(EMPTY_STR),
    _headers(5),
    _values(10),
    _contentLength(0),
    _body(0),
    _bodySize(0),
    _freeables() {

  // copy request - we will destroy/rearrange the content to compute the
  // headers and values in-place

  char* request = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, header, length);
  _freeables.push_back(request);

  parseHeader(request, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief http request constructor
////////////////////////////////////////////////////////////////////////////////

HttpRequestPlain::HttpRequestPlain ()
  : HttpRequest(),
    _requestPath(EMPTY_STR),
    _headers(1),
    _values(1),
    _contentLength(0),
    _body(0),
    _bodySize(0),
    _freeables() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

HttpRequestPlain::~HttpRequestPlain () {
  for (vector<char*>::iterator i = _freeables.begin();  i != _freeables.end();  ++i) {
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, (*i));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               HttpRequest methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequestPlain::requestPath () const {
  return _requestPath;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpRequestPlain::write (TRI_string_buffer_t* buffer) const {
  switch (_type) {
    case HTTP_REQUEST_GET:
      TRI_AppendString2StringBuffer(buffer, "GET ", 4);
      break;

    case HTTP_REQUEST_POST:
      TRI_AppendString2StringBuffer(buffer, "POST ", 5);
      break;

    case HTTP_REQUEST_PUT:
      TRI_AppendString2StringBuffer(buffer, "PUT ", 4);
      break;

    case HTTP_REQUEST_DELETE:
      TRI_AppendString2StringBuffer(buffer, "DELETE ", 7);
      break;

    case HTTP_REQUEST_HEAD:
      TRI_AppendString2StringBuffer(buffer, "HEAD ", 5);
      break;

    default:
      TRI_AppendString2StringBuffer(buffer, "UNKNOWN ", 8);
      break;
  }

  // do NOT url-encode the path, we need to distingush between
  // "/document/a/b" and "/document/a%2fb"

  TRI_AppendStringStringBuffer(buffer, _requestPath);

  // generate the request parameters
  basics::Dictionary<char const*>::KeyValue const* begin;
  basics::Dictionary<char const*>::KeyValue const* end;

  bool first = true;

  for (_values.range(begin, end);  begin < end;  ++begin) {
    char const* key = begin->_key;

    if (key == 0) {
      continue;
    }

    if (first) {
      TRI_AppendCharStringBuffer(buffer, '?');
      first = false;
    }
    else {
      TRI_AppendCharStringBuffer(buffer, '&');
    }

    char const* value = begin->_value;

    TRI_AppendUrlEncodedStringStringBuffer(buffer, key);
    TRI_AppendCharStringBuffer(buffer, '=');
    TRI_AppendUrlEncodedStringStringBuffer(buffer, value);
  }

  TRI_AppendString2StringBuffer(buffer, " HTTP/1.1\r\n", 11);

  // generate the header fields
  for (_headers.range(begin, end);  begin < end;  ++begin) {
    char const* key = begin->_key;

    if (key == 0) {
      continue;
    }

    if (strcmp(key, "content-length") == 0) {
      continue;
    }

    char const* value = begin->_value;

    TRI_AppendStringStringBuffer(buffer, key);
    TRI_AppendString2StringBuffer(buffer, ": ", 2);
    TRI_AppendStringStringBuffer(buffer, value);
    TRI_AppendString2StringBuffer(buffer, "\r\n", 2);
  }

  TRI_AppendString2StringBuffer(buffer, "content-length: ", 16);
  TRI_AppendUInt64StringBuffer(buffer, _contentLength);
  TRI_AppendString2StringBuffer(buffer, "\r\n\r\n", 4);

  if (_body != 0 && 0 < _bodySize) {
    TRI_AppendString2StringBuffer(buffer, _body, _bodySize);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

size_t HttpRequestPlain::contentLength () const {
  return _contentLength;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequestPlain::header (char const* key) const {
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(key);

  if (kv == 0) {
    return EMPTY_STR;
  }
  else {
    return kv->_value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequestPlain::header (char const* key, bool& found) const {
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(key);

  if (kv == 0) {
    found = false;
    return EMPTY_STR;
  }
  else {
    found = true;
    return kv->_value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

map<string, string> HttpRequestPlain::headers () const {
  basics::Dictionary<char const*>::KeyValue const* begin;
  basics::Dictionary<char const*>::KeyValue const* end;

  map<string, string> result;

  for (_headers.range(begin, end);  begin < end;  ++begin) {
    char const* key = begin->_key;

    if (key == 0) {
      continue;
    }

    result[key] = begin->_value;
  }

  result["content-length"] = StringUtils::itoa(_contentLength);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequestPlain::value (char const* key) const {
  Dictionary<char const*>::KeyValue const* kv = _values.lookup(key);

  if (kv == 0) {
    return EMPTY_STR;
  }
  else {
    return kv->_value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequestPlain::value (char const* key, bool& found) const {
  Dictionary<char const*>::KeyValue const* kv = _values.lookup(key);

  if (kv == 0) {
    found = false;
    return EMPTY_STR;
  }
  else {
    found = true;
    return kv->_value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

map<string, string> HttpRequestPlain::values () const {
  basics::Dictionary<char const*>::KeyValue const* begin;
  basics::Dictionary<char const*>::KeyValue const* end;

  map<string, string> result;

  for (_values.range(begin, end);  begin < end;  ++begin) {
    char const* key = begin->_key;

    if (key == 0) {
      continue;
    }

    result[key] = begin->_value;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequestPlain::body () const {
  return _body == 0 ? EMPTY_STR : _body;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

size_t HttpRequestPlain::bodySize () const {
  return _bodySize;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

int HttpRequestPlain::setBody (char const* newBody, size_t length) {
  _body = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, newBody, length);
  _contentLength = _bodySize = length;

  _freeables.push_back(_body);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the header type
////////////////////////////////////////////////////////////////////////////////

HttpRequest::HttpRequestType HttpRequestPlain::getRequestType (const char* ptr, const size_t length) {
  switch (length) {
    case 3:
      if (ptr[0] == 'g' && ptr[1] == 'e' && ptr[2] == 't') {
        return HTTP_REQUEST_GET;
      }
      if (ptr[0] == 'p' && ptr[1] == 'u' && ptr[2] == 't') { 
        return HTTP_REQUEST_PUT;
      }
      break;

    case 4:
      if (ptr[0] == 'p' && ptr[1] == 'o' && ptr[2] == 's' && ptr[3] == 't') { 
        return HTTP_REQUEST_POST;
      }
      if (ptr[0] == 'h' && ptr[1] == 'e' && ptr[2] == 'a' && ptr[3] == 'd') { 
        return HTTP_REQUEST_HEAD;
      }
      break;

    case 6:
      if (ptr[0] == 'd' && ptr[1] == 'e' && ptr[2] == 'l' && ptr[3] == 'e' && ptr[4] == 't' && ptr[5] == 'e') { 
        return HTTP_REQUEST_DELETE;
      }
      break;
  }

  return HTTP_REQUEST_ILLEGAL;
}       

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the http header
////////////////////////////////////////////////////////////////////////////////

void HttpRequestPlain::parseHeader (char* ptr, size_t length) {
  char* start = ptr;
  char* end = start + length;

  // current line number
  int lineNum = 0;

  // begin and end of current line
  char* lineBegin = 0;

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

      for (;  e < end && *e != ' ' && *e != '\n';  ++e) {
        *e = ::tolower(*e);
      }

      // store key and value
      char* keyBegin = lineBegin;
      char* keyEnd = e;

      char* valueBegin = 0;
      char* valueEnd = 0;

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
          }
          else {
            if (*e == '\n') {
              valueEnd = e;
              start = e + 1;

              // skip \r
              if (valueBegin < valueEnd && valueEnd[-1] == '\r') {
                --valueEnd;
              }
            }
            else {
              valueEnd = e;

              // HTTP protocol version is expected next
              // trim value
              while (e < end && *e == ' ') {
                ++e;
              }

              if (end - e > strlen("http/1.x")) {
                if ((e[0] == 'h' || e[0] == 'H') &&
                    (e[1] == 't' || e[1] == 'T') &&
                    (e[2] == 't' || e[2] == 'T') &&
                    (e[3] == 'p' || e[3] == 'P') &&
                     e[4] == '/' &&
                     e[5] == '1' &&
                     e[6] == '.') {
                  if (e[7] == '1') {
                    _version = HTTP_1_1;
                  }
                  else {
                    _version = HTTP_1_0;
                  }

                  e += strlen("http/1.x");
                }
              }

              // go on until eol
              while (e < end && *e != '\n') {
                ++e;
              }

              if (e == end) {
                start = end;
              }
              else {
                start = e + 1;
              }
            }

            *valueEnd = '\0';
          }
        }

        // check the key
        _type = getRequestType(keyBegin, keyEnd - keyBegin);

        // extract the path and decode the url and parameters
        if (_type != HTTP_REQUEST_ILLEGAL) {
          char* pathBegin = valueBegin;
          char* pathEnd = 0;

          char* paramBegin = 0;
          char* paramEnd = 0;

          // find a question mark or space
          char* f = pathBegin;

          // do NOT url-decode the path, we need to distingush between
          // "/document/a/b" and "/document/a%2fb"

          while (f < valueEnd && *f != '?' && *f != ' ' && *f != '\n') {
            ++f;
          }

          // no space, question mark or end-of-line
          if (f == valueEnd) {
            pathEnd = f;
            paramEnd = paramBegin = pathEnd;
          }

          // no question mark
          else if (*f == ' ' || *f == '\n') {
            pathEnd = f;
            *pathEnd = '\0';

            paramEnd = paramBegin = pathEnd;
          }

          // found a question mark
          else {
            pathEnd = f;
            *pathEnd = '\0';

            paramBegin = f + 1;
            paramEnd = paramBegin;

            while (paramEnd < valueEnd && *paramEnd != ' ' && *paramEnd != '\n') {
              ++paramEnd;
            }

            *paramEnd = '\0';
          }

          if (pathBegin < pathEnd) {
            setRequestPath(pathBegin);
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
        }
        else {
          start = end;
        }

        // skip \r
        if (keyBegin < valueEnd && keyEnd[-1] == '\r') {
          --keyEnd;
          *keyEnd = '\0';
        }

        // check the key
        _type = getRequestType(keyBegin, keyEnd - keyBegin);
      }
    }

    // .............................................................................
    // OTHER LINES
    // .............................................................................

    else {

      // split line at colon
      char* e = lineBegin;

      for (;  e < end && *e != ':' && *e != '\n';  ++e) {
        *e = ::tolower(*e);
      }

      // store key and value
      char* keyBegin = lineBegin;
      char* keyEnd = e;

      char* valueBegin = 0;
      char* valueEnd = 0;

      // we found a colon (*end is '\0')
      if (*e == ':') {

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
        }
        else if (*e == '\n') {
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
          }
          else {
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
          setHeader(keyBegin, keyEnd - keyBegin, valueBegin);
        }
      }

      // we found no colon, either eol or newline. Take the whole line as key
      else {
        if (e < end) {
          *keyEnd = '\0';
          start = e + 1;
        }
        else {
          start = end;
        }

        // skip \r
        if (keyBegin < keyEnd && keyEnd[-1] == '\r') {
          --keyEnd;
        }

        // use empty value
        if (keyBegin < keyEnd) {
          setHeader(keyBegin, keyEnd - keyBegin, keyEnd);
        }
      }
    }

    lineNum++;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the path of the request
////////////////////////////////////////////////////////////////////////////////

void HttpRequestPlain::setRequestPath (char const* path) {
  _requestPath = path;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the header values
////////////////////////////////////////////////////////////////////////////////

void HttpRequestPlain::setValues (char* buffer, char* end) {
  char* keyBegin = 0;
  char* key = 0;

  char* valueBegin = 0;
  char* value = 0;

  enum { KEY, VALUE } phase = KEY;
  enum { NORMAL, HEX1, HEX2 } reader = NORMAL;

  int hex = 0;

  char const AMB     = '&';
  char const EQUAL   = '=';
  char const PERCENT = '%';
  char const PLUS    = '+';

  for (keyBegin = key = buffer; buffer < end; buffer++) {
    char next = *buffer;

    if (phase == KEY && next == EQUAL) {
      phase = VALUE;

      valueBegin = value = buffer + 1;

      continue;
    }
    else if (next == AMB) {
      phase = KEY;

      *key = '\0';

      // check for missing value phase
      if (valueBegin == 0) {
        valueBegin = value = key;
      }
      else {
        *value = '\0';
      }

      _values.insert(keyBegin, key - keyBegin, valueBegin);

      keyBegin = key = buffer + 1;
      valueBegin = value = 0;

      continue;
    }
    else if (next == PERCENT) {
      reader = HEX1;
      continue;
    }
    else if (reader == HEX1) {
      int h1 = StringUtils::hex2int(next, -1);

      if (h1 == -1) {
        reader = NORMAL;
        --buffer;
        continue;
      }

      hex = h1 * 16;
      reader = HEX2;
      continue;
    }
    else if (reader == HEX2) {
      int h1 = StringUtils::hex2int(next, -1);

      if (h1 == -1) {
        --buffer;
      }
      else {
        hex += h1;
      }

      reader = NORMAL;
      next = static_cast<char>(hex);
    }
    else if (next == PLUS) {
      next = ' ';
    }

    if (phase == KEY) {
      *key++ = next;
    }
    else {
      *value++ = next;
    }
  }

  if (keyBegin != key) {
    *key = '\0';

    // check for missing value phase
    if (valueBegin == 0) {
      valueBegin = value = key;
    }
    else {
      *value = '\0';
    }

    _values.insert(keyBegin, key - keyBegin, valueBegin);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set a header field
////////////////////////////////////////////////////////////////////////////////

void HttpRequestPlain::setHeader (char const* key, size_t keyLength, char const* value) {
  if (keyLength == 14 && TRI_EqualString2(key, "content-length", keyLength)) {
    _contentLength = TRI_UInt64String(value);
  }
  else {
    _headers.insert(key, keyLength, value);
  }
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
