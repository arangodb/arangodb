////////////////////////////////////////////////////////////////////////////////
/// @brief http request
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2008-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpRequest.h"

#include <Basics/Logger.h>
#include <Basics/StringBuffer.h>
#include <Basics/StringUtils.h>

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    HttpRequest::HttpRequest (string const& header)
      : type(HTTP_REQUEST_ILLEGAL),
        requestPathValue(""),
        headerFields(5),
        requestFields(10) {

      // copy request
      char* request = StringUtils::duplicate(header);
      freeables.push_back(request);

      parseHeader(request, header.size());
    }



    HttpRequest::HttpRequest (char const* header, size_t length)
      : type(HTTP_REQUEST_ILLEGAL),
        requestPathValue(""),
        headerFields(5),
        requestFields(10) {

      // copy request
      char* request = StringUtils::duplicate(header, length);
      freeables.push_back(request);

      parseHeader(request, length);
    }



    HttpRequest::HttpRequest ()
      : type(HTTP_REQUEST_ILLEGAL),
        requestPathValue(""),
        headerFields(5),
        requestFields(5) {
    }



    HttpRequest::~HttpRequest () {
      for (vector<char const*>::iterator i = freeables.begin();  i != freeables.end();  ++i) {
        delete[] (*i);
      }
    }

    // -----------------------------------------------------------------------------
    // HttpRequest methods
    // -----------------------------------------------------------------------------

    size_t HttpRequest::contentLength () const {
      string const& contentLengthString = StringUtils::trim(header("content-length"));

      if (contentLengthString != "") {
        return StringUtils::uint32(contentLengthString);
      }

      return 0;
    }



    string HttpRequest::header (string const& key) const {
      string k = StringUtils::tolower(key);
      char const* const* i = headerFields.lookup(k.c_str());

      if (i == 0) {
        return "";
      }
      else {
        return *i;
      }
    }



    string HttpRequest::header (string const& key, bool& found) const {
      string k = StringUtils::tolower(key);
      char const* const* i = headerFields.lookup(k.c_str());

      if (i == 0) {
        found = false;
        return "";
      }
      else {
        found = true;
        return *i;
      }
    }



    map<string, string> HttpRequest::headers () const {
      basics::Dictionary<char const*>::KeyValue const* begin;
      basics::Dictionary<char const*>::KeyValue const* end;

      map<string, string> result;

      for (headerFields.range(begin, end);  begin < end;  ++begin) {
        char const* key = begin->key;

        if (key == 0) {
          continue;
        }

        result[key] = begin->value;
      }

      return result;
    }



    void HttpRequest::setHeader (string const& key, string const& value) {
      char const* k = StringUtils::duplicate(StringUtils::tolower(key));
      char const* v = StringUtils::duplicate(value);

      headerFields.insert(k, key.size(), v);

      freeables.push_back(k);
      freeables.push_back(v);
    }



    string HttpRequest::value (string const& key) const {
      char const* const* i = requestFields.lookup(key.c_str());

      if (i == 0) {
        return "";
      }
      else {
        return *i;
      }
    }



    string HttpRequest::value (string const& key, bool& found) const {
      char const* const* i = requestFields.lookup(key.c_str());

      if (i == 0) {
        found = false;
        return "";
      }
      else {
        found = true;
        return *i;
      }
    }



    map<string, string> HttpRequest::values () const {
      basics::Dictionary<char const*>::KeyValue const* begin;
      basics::Dictionary<char const*>::KeyValue const* end;

      map<string, string> result;

      for (requestFields.range(begin, end);  begin < end;  ++begin) {
        char const* key = begin->key;

        if (key == 0) {
          continue;
        }

        result[key] = begin->value;
      }

      return result;
    }



    void HttpRequest::setValues (string const& params) {
      char* copy = StringUtils::duplicate(params);

      freeables.push_back(copy);

      setValues(copy, copy + params.size());
    }



    void HttpRequest::setValue (string const& key, string const& value) {
      char const* k = StringUtils::duplicate(key);
      char const* v = StringUtils::duplicate(value);

      requestFields.insert(k, key.size(), v);

      freeables.push_back(k);
      freeables.push_back(v);
    }



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

        default:
          buffer->appendText("UNKNOWN ");
          break;
      }

      // do not URL decode the path, we need to distingush between
      // "/document/a/b" and "/document/a%2fb"
      buffer->appendText(requestPathValue);

      // generate the request parameters
      basics::Dictionary<char const*>::KeyValue const* begin;
      basics::Dictionary<char const*>::KeyValue const* end;

      bool first = true;

      for (requestFields.range(begin, end);  begin < end;  ++begin) {
        char const* key = begin->key;

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

        char const* value = begin->value;

        buffer->appendText(StringUtils::urlEncode(key));
        buffer->appendChar('=');
        buffer->appendText(StringUtils::urlEncode(value));
      }

      buffer->appendText(" HTTP/1.1\r\n");

      // generate the header fields
      for (headerFields.range(begin, end);  begin < end;  ++begin) {
        char const* key = begin->key;

        if (key == 0) {
          continue;
        }

        if (strcmp(key, "content-length") == 0) {
          continue;
        }

        char const* value = begin->value;

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

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

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

        //
        // FIRST LINE
        //

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
                requestPathValue = pathBegin;
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
          }
        }

        //
        // OTHER LINES
        //

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
              headerFields.insert(keyBegin, keyEnd - keyBegin, valueBegin);
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
              headerFields.insert(keyBegin, keyEnd - keyBegin, keyEnd);
            }
          }
        }

        lineNum++;
      }
    }

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

          requestFields.insert(keyBegin, key - keyBegin, valueBegin);

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

        requestFields.insert(keyBegin, key - keyBegin, valueBegin);
      }
    }
  }
}
