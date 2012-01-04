////////////////////////////////////////////////////////////////////////////////
/// @brief http response
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

#include "HttpResponse.h"

#include <Basics/StringUtils.h>

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // static public methods
    // -----------------------------------------------------------------------------

    string HttpResponse::responseString (HttpResponseCode code) {
      switch (code) {

        //  Success 2xx
        case OK:                   return "200 OK";
        case CREATED:              return "201 Created";
        case ACCEPTED:             return "202 Accepted";
        case PARTIAL:              return "203 Partial Information";
        case NO_CONTENT:           return "204 No Content";

        //  Redirection 3xx
        case MOVED_PERMANENTLY:    return "301 Moved";
        case FOUND:                return "302 Found";
        case SEE_OTHER:            return "303 See Other";
        case NOT_MODIFIED:         return "304 Not Modified";
        case TEMPORARY_REDIRECT:   return "307 Temporary Redirect";

        //  Error 4xx, 5xx
        case BAD:                  return "400 Bad Request";
        case UNAUTHORIZED:         return "401 Unauthorized";
        case PAYMENT:              return "402 Payment Required";
        case FORBIDDEN:            return "403 Forbidden";
        case NOT_FOUND:            return "404 Not Found";
        case METHOD_NOT_ALLOWED:   return "405 Method";
        case CONFLICT:             return "409 Conflict";
        case UNPROCESSABLE_ENTITY: return "422 Unprocessable Entity";

        case SERVER_ERROR:         return "500 Internal Error";
        case NOT_IMPLEMENTED:      return "501 Not implemented";
        case BAD_GATEWAY:          return "502 Bad Gateway";
        case SERVICE_UNAVAILABLE:  return "503 Service Temporarily Unavailable";

        // default
        default:                   return StringUtils::itoa((int) code) + " (unknown HttpResponseCode)";
      }
    }



    HttpResponse::HttpResponseCode HttpResponse::responseCode (const string& str) {
      int number = ::atoi(str.c_str());

      switch (number) {
        case 200: return OK;
        case 201: return CREATED;
        case 202: return ACCEPTED;
        case 203: return PARTIAL;
        case 204: return NO_CONTENT;

        case 301: return MOVED_PERMANENTLY;
        case 302: return FOUND;
        case 303: return SEE_OTHER;
        case 304: return NOT_MODIFIED;
        case 307: return TEMPORARY_REDIRECT;

        case 400: return BAD;
        case 401: return UNAUTHORIZED;
        case 402: return PAYMENT;
        case 403: return FORBIDDEN;
        case 404: return NOT_FOUND;
        case 405: return METHOD_NOT_ALLOWED;
        case 409: return CONFLICT;
        case 422: return UNPROCESSABLE_ENTITY;

        case 500: return SERVER_ERROR;
        case 501: return NOT_IMPLEMENTED;
        case 502: return BAD_GATEWAY;
        case 503: return SERVICE_UNAVAILABLE;

        default:  return NOT_IMPLEMENTED;
      }
    }

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    HttpResponse::HttpResponse ()
      : code(NOT_IMPLEMENTED),
        headerFields(5),
        bodyValue(),
        freeables() {
      bodyValue.initialise();
    }



    HttpResponse::HttpResponse (string const& header)
      : code(NOT_IMPLEMENTED),
        headerFields(5),
        bodyValue(),
        freeables() {
      bodyValue.initialise();

      setHeaders(header, true);
    }



    HttpResponse::HttpResponse (HttpResponseCode code)
      : code(code),
        headerFields(5),
        bodyValue(),
        freeables() {
      bodyValue.initialise();

      char* headerBuffer = StringUtils::duplicate("server\ntriagens GmbH High-Performance HTTP Server\n"
                                                  "connection\nKeep-Alive\n"
                                                  "content-type\ntext/plain;charset=utf-8\n");
      freeables.push_back(headerBuffer);

      bool key = true;
      char* startKey = headerBuffer;
      char* startValue = 0;
      char* end = headerBuffer + strlen(headerBuffer);

      for (char* ptr = headerBuffer;  ptr < end;  ++ptr) {
        if (*ptr == '\n') {
          *ptr = '\0';

          if (key) {
            startValue = ptr + 1;
            key = false;
          }
          else {
            headerFields.insert(startKey, startValue);

            startKey = ptr + 1;
            startValue = 0;
            key = true;
          }
        }
      }
    }



    HttpResponse::~HttpResponse () {
      bodyValue.free();

      for (vector<char const*>::iterator i = freeables.begin();  i != freeables.end();  ++i) {
        delete[] (*i);
      }
    }

    // -----------------------------------------------------------------------------
    // HttpResponse methods
    // -----------------------------------------------------------------------------

    void HttpResponse::setContentType (string const& contentType) {
      setHeader("content-type", contentType);
    }



    size_t HttpResponse::contentLength () {
      char const* const* i = headerFields.lookup("content-length");

      if (i == 0) {
        return 0;
      }

      return StringUtils::uint32(*i);
    }



    string HttpResponse::header (string const& key) const {
      string k = StringUtils::tolower(key);
      char const* const* i = headerFields.lookup(k.c_str());

      if (i == 0) {
        return "";
      }
      else {
        return *i;
      }
    }



    string HttpResponse::header (string const& key, bool& found) const {
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



    map<string, string> HttpResponse::headers () const {
      basics::Dictionary<char const*>::KeyValue const* begin;
      basics::Dictionary<char const*>::KeyValue const* end;

      map<string, string> result;

      for (headerFields.range(begin, end);  begin < end;  ++begin) {
        char const* key = begin->_key;

        if (key == 0) {
          continue;
        }

        result[key] = begin->_value;
      }

      return result;
    }



    void HttpResponse::setHeader (string const& key, string const& value) {
      string lk = StringUtils::tolower(key);

      if (value.empty()) {
        headerFields.erase(lk.c_str());
      }
      else {
        StringUtils::trimInPlace(lk);

        char const* k = StringUtils::duplicate(lk);
        char const* v = StringUtils::duplicate(value);

        headerFields.insert(k, lk.size(), v);

        freeables.push_back(k);
        freeables.push_back(v);
      }
    }



    void HttpResponse::setHeaders (string const& headers, bool includeLine0) {

      // make a copy we can change, this buffer will be deleted in the destructor
      char* headerBuffer = new char[headers.size() + 1];
      memcpy(headerBuffer, headers.c_str(), headers.size() + 1);

      freeables.push_back(headerBuffer);

      // check for '\n' (we check for '\r' later)
      int lineNum = includeLine0 ? 0 : 1;

      char* start = headerBuffer;
      char* end = headerBuffer + headers.size();
      char* end1 = start;

      for (; end1 < end && *end1 != '\n';  ++end1) {
      }

      while (start < end) {
        char* end2 = end1;

        if (start < end2 && end2[-1] == '\r') {
          --end2;
        }

        // the current line is [start, end2)
        if (start < end2) {

          // now split line at the first spaces
          char* end3 = start;

          for (; end3 < end2 && *end3 != ' ' && *end3 != ':'; ++end3) {
            *end3 = ::tolower(*end3);
          }

          // the current token is [start, end3) and all in lower case
          if (lineNum == 0) {

            // the start should be HTTP/1.1 followed by blanks followed by the result code
            if (start + 8 <= end3 && memcmp(start, "http/1.1", 8) == 0) {

              char* start2 = end3;

              for (; start2 < end2 && (*start2 == ' ' || *start2 == ':');  ++start2) {
              }

              if (start2 < end2) {
                *end2 = '\0';

                code = static_cast<HttpResponseCode>(::atoi(start2));
              }
              else {
                code = NOT_IMPLEMENTED;
              }
            }
            else {
              code = NOT_IMPLEMENTED;
            }
          }

          // normal header field, key is [start, end3) and the value is [start2, end4)
          else {
            for (;  end3 < end2 && *end3 != ':';  ++end3) {
              *end3 = ::tolower(*end3);
            }

            *end3 = '\0';

            if (end3 < end2) {
              char* start2 = end3 + 1;

              for (;  start2 < end2 && *start2 == ' ';  ++start2) {
              }

              char* end4 = end2;

              for (;  start2 < end4 && *(end4 - 1) == ' ';  --end4) {
              }

              *end4 = '\0';

              headerFields.insert(start, end3 - start, start2);
            }
            else {
              headerFields.insert(start, end3 - start, end3);
            }
          }
        }

        start = end1 + 1;

        for (end1 = start; end1 < end && *end1 != '\n';  ++end1) {
        }

        lineNum++;
      }
    }



    HttpResponse* HttpResponse::swap () {
      HttpResponse* response = new HttpResponse(code);

      response->headerFields.swap(&headerFields);
      response->bodyValue.swap(&bodyValue);
      response->freeables.swap(freeables);

      return response;
    }



    void HttpResponse::writeHeader (StringBuffer* output) {
      output->appendText("HTTP/1.1 ");
      output->appendText(responseString(code));
      output->appendText("\r\n");

      basics::Dictionary<char const*>::KeyValue const* begin;
      basics::Dictionary<char const*>::KeyValue const* end;

      bool seenContentLength = false;
      bool seenTransferEncoding = false;
      string transferEncoding;

      for (headerFields.range(begin, end);  begin < end;  ++begin) {
        char const* key = begin->_key;

        if (key == 0) {
          continue;
        }

        if (strcmp(key, "content-length") == 0) {
          seenContentLength = true;
          continue;
        }

        if (strcmp(key, "transfer-encoding") == 0) {
          seenTransferEncoding = true;
          transferEncoding = begin->_value;
          continue;
        }

        char const* value = begin->_value;

        output->appendText(key);
        output->appendText(": ");
        output->appendText(value);
        output->appendText("\r\n");
      }

      if (seenTransferEncoding && transferEncoding == "chunked") {
        output->appendText("transfer-encoding: chunked\r\n");
      }
      else {
        if (seenTransferEncoding) {
          output->appendText("transfer-encoding: ");
          output->appendText(transferEncoding);
          output->appendText("\r\n");
        }

        output->appendText("content-length: ");
        output->appendInteger(static_cast<uint32_t>(bodyValue.length()));
        output->appendText("\r\n");
      }

      output->appendText("\r\n");
    }



    StringBuffer& HttpResponse::body () {
      return bodyValue;
    }
  }
}
