////////////////////////////////////////////////////////////////////////////////
/// @brief http request
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

#include "HttpRequest.h"

#include "Logger/Logger.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"

using namespace triagens::basics;
using namespace triagens::rest;

static char const* EMPTY_STR = "";

// -----------------------------------------------------------------------------
// --SECTION--                                                class ArangoServer
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

HttpRequest::HttpRequest (string const& header)
  : type(HTTP_REQUEST_ILLEGAL),
    _requestPath(EMPTY_STR),
    _suffix(),
    _headers(5),
    _values(10),
    bodyValue(EMPTY_STR),
    _bodySize(0),
    _connectionInfo(),
    freeables() {

  // copy request
  char* request = StringUtils::duplicate(header);
  freeables.push_back(request);

  parseHeader(request, header.size());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief http request constructor
////////////////////////////////////////////////////////////////////////////////

HttpRequest::HttpRequest (char const* header, size_t length)
  : type(HTTP_REQUEST_ILLEGAL),
    _requestPath(EMPTY_STR),
    _suffix(),
    _headers(5),
    _values(10),
    bodyValue(EMPTY_STR),
    _bodySize(0),
    _connectionInfo(),
    freeables() {

  // copy request
  char* request = StringUtils::duplicate(header, length);
  freeables.push_back(request);

  parseHeader(request, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief http request constructor
////////////////////////////////////////////////////////////////////////////////

HttpRequest::HttpRequest ()
  : type(HTTP_REQUEST_ILLEGAL),
    _requestPath(EMPTY_STR),
    _suffix(),
    _headers(5),
    _values(5),
    bodyValue(EMPTY_STR),
    _bodySize(0),
    _connectionInfo(),
    freeables() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

HttpRequest::~HttpRequest () {
  for (vector<char const*>::iterator i = freeables.begin();  i != freeables.end();  ++i) {
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, (*i));
  }
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
/// @brief returns the http request type
////////////////////////////////////////////////////////////////////////////////

HttpRequest::HttpRequestType HttpRequest::requestType () const {
  return type;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the http request type
////////////////////////////////////////////////////////////////////////////////

void HttpRequest::setRequestType (HttpRequestType newType) {
  type = newType;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the path of the request
///
/// The path consists of the URL without the host and without any parameters.
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequest::requestPath () const {
  return _requestPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the path of the request
////////////////////////////////////////////////////////////////////////////////

void setRequestPath (char const* path) {
  if (_requestPath != EMPTY_STR) {
    freeables.push_back(_requestPath);
  }

  _requestPath = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the content length
////////////////////////////////////////////////////////////////////////////////

size_t HttpRequest::contentLength () const {
  char const* cl = header("content-length");

  while (*cl == ' ' || *cl == '\t') {
    ++cl;
  }

  if (*cl) {
    return TRI_UInt64String(cl);
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server IP
////////////////////////////////////////////////////////////////////////////////

ConnectionInfo const& HttpRequest::connectionInfo () const {
  return _connectionInfo;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the server IP
////////////////////////////////////////////////////////////////////////////////

void HttpRequest::setConnectionInfo (ConnectionInfo const& info) {
  _connectionInfo = info;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes representation to string buffer
////////////////////////////////////////////////////////////////////////////////

void HttpRequest::write (StringBuffer* buffer) const {
  switch (type) {
    case HTTP_REQUEST_GET:
      buffer->appendText("GET ");
      break;

    case HTTP_REQUEST_POST:
      buffer->appendText("POST ");
      break;

    case HTTP_REQUEST_PUT:
      buffer->appendText("PUT ");
      break;

    case HTTP_REQUEST_DELETE:
      buffer->appendText("DELETE ");
      break;

    case HTTP_REQUEST_HEAD:
      buffer->appendText("HEAD ");
      break;

    default:
      buffer->appendText("UNKNOWN ");
      break;
  }

  // do not URL decode the path, we need to distingush between
  // "/document/a/b" and "/document/a%2fb"
  buffer->appendText(_requestPath);

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
      buffer->appendChar('?');
      first = false;
    }
    else {
      buffer->appendChar('&');
    }

    char const* value = begin->_value;

    buffer->appendText(StringUtils::urlEncode(key));
    buffer->appendChar('=');
    buffer->appendText(StringUtils::urlEncode(value));
  }

  buffer->appendText(" HTTP/1.1\r\n");

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

    buffer->appendText(key);
    buffer->appendText(": ");
    buffer->appendText(value);
    buffer->appendText("\r\n");
  }

  buffer->appendText("content-length: ");
  buffer->appendInteger(bodyValue.size());
  buffer->appendText("\r\n\r\n");

  buffer->appendText(bodyValue.c_str(), bodyValue.size());
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             public header methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequest::header (char const* key) const {
  char const* k = TRI_LowerAsciiString(key);
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(k);
  TRI_FreeString(TRI_CORE_MEM_ZONE, k);

  if (kv == 0) {
    return EMPTY_STR;
  }
  else {
    return kv->_value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequest::header (char const* key, bool& found) const {
  char const* k = TRI_LowerAsciiString(key);
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(k);
  TRI_FreeString(TRI_CORE_MEM_ZONE, k);

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
/// @brief returns all header fields
////////////////////////////////////////////////////////////////////////////////

map<string, string> HttpRequest::headers () const {
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

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set a header field
////////////////////////////////////////////////////////////////////////////////

void HttpRequest::setHeader (string const& key, string const& value) {
  char const* k = StringUtils::duplicate(StringUtils::tolower(key));
  char const* v = StringUtils::duplicate(value);

  _headers.insert(k, key.size(), v);

  freeables.push_back(k);
  freeables.push_back(v);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             public suffix methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all suffix parts
////////////////////////////////////////////////////////////////////////////////

vector<string> const& HttpRequest::suffix () const {
  return _suffix;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a suffix part
////////////////////////////////////////////////////////////////////////////////

void addSuffix (string const& part) {
  _suffix.push_back(part);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              public value methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the value of a key
////////////////////////////////////////////////////////////////////////////////

string HttpRequest::value (string const& key) const {
  Dictionary<char const*>::KeyValue const* kv = _values.lookup(key.c_str());

  if (kv == 0) {
    return EMPTY_STR;
  }
  else {
    return kv->_value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the value of a key
////////////////////////////////////////////////////////////////////////////////

string HttpRequest::value (string const& key, bool& found) const {
  Dictionary<char const*>::KeyValue const* kv = _values.lookup(key.c_str());

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
/// @brief returns all values
////////////////////////////////////////////////////////////////////////////////

map<string, string> HttpRequest::values () const {
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
/// @brief sets the key/values from an url encoded string
////////////////////////////////////////////////////////////////////////////////

void HttpRequest::setValues (string const& params) {
  char* copy = StringUtils::duplicate(params);

  freeables.push_back(copy);

  setValues(copy, copy + params.size());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a key/value pair
////////////////////////////////////////////////////////////////////////////////

void HttpRequest::setValue (string const& key, string const& value) {
  char const* k = StringUtils::duplicate(key);
  char const* v = StringUtils::duplicate(value);

  _values.insert(k, key.size(), v);

  freeables.push_back(k);
  freeables.push_back(v);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               public body methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the body
////////////////////////////////////////////////////////////////////////////////

char const* body () const {
  return _body;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the body size
////////////////////////////////////////////////////////////////////////////////

size_t bodySize () const {
  return _bodySize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the body
///
/// In a POST request the body contains additional key/value pairs. The
/// function parses the body and adds the corresponding pairs.
////////////////////////////////////////////////////////////////////////////////

void setBody (char const* newBody) {
  size_t newLength = strlen(newBody);

  setBody(newBody, newLength);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the body
////////////////////////////////////////////////////////////////////////////////

void setBody (char const* newBody, size_t length) {
  _body = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, newBody, length);
  _bodySize = length;

  freeables.push_back(_body);
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
/// @brief parses the http header
////////////////////////////////////////////////////////////////////////////////

void HttpRequest::parseHeader (char* ptr, size_t length) {
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

        while (e < end && *e == ' ') {
          ++e;
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
        if (strcmp(keyBegin, "post") == 0) {
          type = HTTP_REQUEST_POST;
        }
        else if (strcmp(keyBegin, "put") == 0) {
          type = HTTP_REQUEST_PUT;
        }
        else if (strcmp(keyBegin, "delete") == 0) {
          type = HTTP_REQUEST_DELETE;
        }
        else if (strcmp(keyBegin, "get") == 0) {
          type = HTTP_REQUEST_GET;
        }
        else if (strcmp(keyBegin, "head") == 0) {
          type = HTTP_REQUEST_HEAD;
        }

        // extract the path and decode the url and parameters
        if (type != HTTP_REQUEST_ILLEGAL) {
          char* pathBegin = valueBegin;
          char* pathEnd = 0;

          char* paramBegin = 0;
          char* paramEnd = 0;

          // find a question mark or space
          char* f = pathBegin;

          // do not URL decode the path, we need to distingush between
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
        }

        // check the key
        if (strcmp(keyBegin, "post") == 0) {
          type = HTTP_REQUEST_POST;
        }
        else if (strcmp(keyBegin, "put") == 0) {
          type = HTTP_REQUEST_PUT;
        }
        else if (strcmp(keyBegin, "delete") == 0) {
          type = HTTP_REQUEST_DELETE;
        }
        else if (strcmp(keyBegin, "get") == 0) {
          type = HTTP_REQUEST_GET;
        }
        else if (strcmp(keyBegin, "head") == 0) {
          type = HTTP_REQUEST_HEAD;
        }
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
          _headers.insert(keyBegin, keyEnd - keyBegin, valueBegin);
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
          _headers.insert(keyBegin, keyEnd - keyBegin, keyEnd);
        }
      }
    }

    lineNum++;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the header values
////////////////////////////////////////////////////////////////////////////////

void HttpRequest::setValues (char* buffer, char* end) {
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
