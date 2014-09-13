////////////////////////////////////////////////////////////////////////////////
/// @brief http response
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpResponse.h"

#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/StringUtils.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief http response string
////////////////////////////////////////////////////////////////////////////////

string HttpResponse::responseString (HttpResponseCode code) {
  switch (code) {
    //  Informational 1xx
    case CONTINUE:                        return "100 Continue";
    case SWITCHING_PROTOCOLS:             return "101 Switching Protocols";
    case PROCESSING:                      return "102 Processing";

    //  Success 2xx
    case OK:                              return "200 OK";
    case CREATED:                         return "201 Created";
    case ACCEPTED:                        return "202 Accepted";
    case PARTIAL:                         return "203 Partial Information";
    case NO_CONTENT:                      return "204 No Content";
    case RESET_CONTENT:                   return "205 Reset Content";
    case PARTIAL_CONTENT:                 return "206 Partial Content";

    //  Redirection 3xx
    case MOVED_PERMANENTLY:               return "301 Moved";
    case FOUND:                           return "302 Found";
    case SEE_OTHER:                       return "303 See Other";
    case NOT_MODIFIED:                    return "304 Not Modified";
    case TEMPORARY_REDIRECT:              return "307 Temporary Redirect";
    case PERMANENT_REDIRECT:              return "308 Permanent Redirect";

    //  Error 4xx, 5xx
    case BAD:                             return "400 Bad Request";
    case UNAUTHORIZED:                    return "401 Unauthorized";
    case PAYMENT_REQUIRED:                return "402 Payment Required";
    case FORBIDDEN:                       return "403 Forbidden";
    case NOT_FOUND:                       return "404 Not Found";
    case METHOD_NOT_ALLOWED:              return "405 Method Not Supported";
    case NOT_ACCEPTABLE:                  return "406 Not Acceptable";
    case REQUEST_TIMEOUT:                 return "408 Request Timeout";
    case CONFLICT:                        return "409 Conflict";
    case GONE:                            return "410 Gone";
    case LENGTH_REQUIRED:                 return "411 Length Required";
    case PRECONDITION_FAILED:             return "412 Precondition Failed";
    case REQUEST_ENTITY_TOO_LARGE:        return "413 Request Entity Too Large";
    case REQUEST_URI_TOO_LONG:            return "414 Request-URI Too Long";
    case UNSUPPORTED_MEDIA_TYPE:          return "415 Unsupported Media Type";
    case REQUESTED_RANGE_NOT_SATISFIABLE: return "416 Requested Range Not Satisfiable";
    case EXPECTATION_FAILED:              return "417 Expectation Failed";
    case I_AM_A_TEAPOT:                   return "418 I'm a teapot";
    case UNPROCESSABLE_ENTITY:            return "422 Unprocessable Entity";
    case PRECONDITION_REQUIRED:           return "428 Precondition Required";
    case TOO_MANY_REQUESTS:               return "429 Too Many Requests";
    case REQUEST_HEADER_FIELDS_TOO_LARGE: return "431 Request Header Fields Too Large";

    case SERVER_ERROR:                    return "500 Internal Error";
    case NOT_IMPLEMENTED:                 return "501 Not Implemented";
    case BAD_GATEWAY:                     return "502 Bad Gateway";
    case SERVICE_UNAVAILABLE:             return "503 Service Temporarily Unavailable";
    case HTTP_VERSION_NOT_SUPPORTED:      return "505 HTTP Version Not Supported";
    case BANDWIDTH_LIMIT_EXCEEDED:        return "509 Bandwidth Limit Exceeded";
    case NOT_EXTENDED:                    return "510 Not Extended";

    // default
    default:
      LOG_WARNING("unknown HTTP response code %d returned", (int) code);
      return StringUtils::itoa((int) code) + " (unknown HttpResponseCode)";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get http response code from string
////////////////////////////////////////////////////////////////////////////////

HttpResponse::HttpResponseCode HttpResponse::responseCode (const string& str) {
  int number = ::atoi(str.c_str());

  switch (number) {
    case 100: return CONTINUE;
    case 101: return SWITCHING_PROTOCOLS;
    case 102: return PROCESSING;

    case 200: return OK;
    case 201: return CREATED;
    case 202: return ACCEPTED;
    case 203: return PARTIAL;
    case 204: return NO_CONTENT;
    case 205: return RESET_CONTENT;
    case 206: return PARTIAL_CONTENT;

    case 301: return MOVED_PERMANENTLY;
    case 302: return FOUND;
    case 303: return SEE_OTHER;
    case 304: return NOT_MODIFIED;
    case 307: return TEMPORARY_REDIRECT;
    case 308: return PERMANENT_REDIRECT;

    case 400: return BAD;
    case 401: return UNAUTHORIZED;
    case 402: return PAYMENT_REQUIRED;
    case 403: return FORBIDDEN;
    case 404: return NOT_FOUND;
    case 405: return METHOD_NOT_ALLOWED;
    case 406: return NOT_ACCEPTABLE;
    case 408: return REQUEST_TIMEOUT;
    case 409: return CONFLICT;
    case 410: return GONE;
    case 411: return LENGTH_REQUIRED;
    case 412: return PRECONDITION_FAILED;
    case 413: return REQUEST_ENTITY_TOO_LARGE;
    case 414: return REQUEST_URI_TOO_LONG;
    case 415: return UNSUPPORTED_MEDIA_TYPE;
    case 416: return REQUESTED_RANGE_NOT_SATISFIABLE;
    case 417: return EXPECTATION_FAILED;
    case 418: return I_AM_A_TEAPOT;
    case 422: return UNPROCESSABLE_ENTITY;
    case 428: return PRECONDITION_REQUIRED;
    case 429: return TOO_MANY_REQUESTS;
    case 431: return REQUEST_HEADER_FIELDS_TOO_LARGE;

    case 500: return SERVER_ERROR;
    case 501: return NOT_IMPLEMENTED;
    case 502: return BAD_GATEWAY;
    case 503: return SERVICE_UNAVAILABLE;
    case 505: return HTTP_VERSION_NOT_SUPPORTED;
    case 509: return BANDWIDTH_LIMIT_EXCEEDED;
    case 510: return NOT_EXTENDED;

    default:  return NOT_IMPLEMENTED;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the batch error count header
////////////////////////////////////////////////////////////////////////////////

const string& HttpResponse::getBatchErrorHeader () {
  static const string header = "X-Arango-Errors";

  return header;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new http response
////////////////////////////////////////////////////////////////////////////////

HttpResponse::HttpResponse (HttpResponseCode code,
                            uint32_t apiCompatibility)
  : _code(code),
    _apiCompatibility(apiCompatibility),
    _isHeadResponse(false),
    _headers(6),
    _body(TRI_UNKNOWN_MEM_ZONE),
    _bodySize(0),
    _freeables() {

  _headers.insert("server", 6, "ArangoDB");
  _headers.insert("connection", 10, "Keep-Alive");
  _headers.insert("content-type", 12, "text/plain; charset=utf-8");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a http response
////////////////////////////////////////////////////////////////////////////////

HttpResponse::~HttpResponse () {
  for (vector<char const*>::iterator i = _freeables.begin();  i != _freeables.end();  ++i) {
    delete[] (*i);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the response code
////////////////////////////////////////////////////////////////////////////////

HttpResponse::HttpResponseCode HttpResponse::responseCode () const {
  return _code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the response code
////////////////////////////////////////////////////////////////////////////////

void HttpResponse::setResponseCode (HttpResponseCode code) {
  _code = code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the content length
////////////////////////////////////////////////////////////////////////////////

size_t HttpResponse::contentLength () {
  if (_isHeadResponse) {
    return _bodySize;
  }
  else {
    Dictionary<char const*>::KeyValue const* kv = _headers.lookup("content-length", 14);

    if (kv == 0) {
      return 0;
    }

    return StringUtils::uint32(kv->_value);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the content type
////////////////////////////////////////////////////////////////////////////////

void HttpResponse::setContentType (string const& contentType) {
  setHeader("content-type", 12, contentType);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

string HttpResponse::header (string const& key) const {
  string k = StringUtils::tolower(key);
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(k.c_str());

  if (kv == 0) {
    return "";
  }
  else {
    return kv->_value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

string HttpResponse::header (const char* key, const size_t keyLength) const {
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(key, keyLength);

  if (kv == 0) {
    return "";
  }
  else {
    return kv->_value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

string HttpResponse::header (string const& key, bool& found) const {
  string k = StringUtils::tolower(key);
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(k.c_str());

  if (kv == 0) {
    found = false;
    return "";
  }
  else {
    found = true;
    return kv->_value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

string HttpResponse::header (const char* key, const size_t keyLength, bool& found) const {
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(key, keyLength);

  if (kv == 0) {
    found = false;
    return "";
  }
  else {
    found = true;
    return kv->_value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all header fields
////////////////////////////////////////////////////////////////////////////////

map<string, string> HttpResponse::headers () const {
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
/// @brief sets a header field
////////////////////////////////////////////////////////////////////////////////

void HttpResponse::setHeader (const char* key, const size_t keyLength, string const& value) {
  if (value.empty()) {
    _headers.erase(key);
  }
  else {
    char const* v = StringUtils::duplicate(value);

    if (v != 0) {
      _headers.insert(key, keyLength, v);

      _freeables.push_back(v);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a header field
////////////////////////////////////////////////////////////////////////////////

void HttpResponse::setHeader (const char* key, const size_t keyLength, const char* value) {
  if (*value == '\0') {
    _headers.erase(key);
  }
  else {
    _headers.insert(key, keyLength, value);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a header field
////////////////////////////////////////////////////////////////////////////////

void HttpResponse::setHeader (string const& key, string const& value) {
  string lk = StringUtils::tolower(key);

  if (value.empty()) {
    _headers.erase(lk.c_str());
  }
  else {
    StringUtils::trimInPlace(lk);

    char const* k = StringUtils::duplicate(lk);
    char const* v = StringUtils::duplicate(value);

    _headers.insert(k, lk.size(), v);

    _freeables.push_back(k);
    _freeables.push_back(v);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets many header fields
////////////////////////////////////////////////////////////////////////////////

void HttpResponse::setHeaders (string const& headers, bool includeLine0) {

  // make a copy we can change, this buffer will be deleted in the destructor
  char* headerBuffer = new char[headers.size() + 1];
  memcpy(headerBuffer, headers.c_str(), headers.size() + 1);

  _freeables.push_back(headerBuffer);

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

            _code = static_cast<HttpResponseCode>(::atoi(start2));
          }
          else {
            _code = NOT_IMPLEMENTED;
          }
        }
        else {
          _code = NOT_IMPLEMENTED;
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

          _headers.insert(start, end3 - start, start2);
        }
        else {
          _headers.insert(start, end3 - start, end3);
        }
          }
    }

    start = end1 + 1;

    for (end1 = start; end1 < end && *end1 != '\n';  ++end1) {
    }

    lineNum++;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a cookie
////////////////////////////////////////////////////////////////////////////////

void HttpResponse::setCookie (string const& name, string const& value,
        int lifeTimeSeconds, string const& path, string const& domain,
        bool secure, bool httpOnly) {

  triagens::basics::StringBuffer* buffer = new triagens::basics::StringBuffer(TRI_UNKNOWN_MEM_ZONE);

  string tmp = StringUtils::trim(name);
  buffer->appendText(tmp.c_str(), tmp.length());
  buffer->appendChar('=');

  tmp = StringUtils::urlEncode(value);
  buffer->appendText(tmp.c_str(), tmp.length());

  if (lifeTimeSeconds != 0) {
    time_t rawtime;

    time(&rawtime);
    if (lifeTimeSeconds > 0) {
      rawtime += lifeTimeSeconds;
    }
    else {
      rawtime = 1;
    }

    if (rawtime > 0) {
      struct tm * timeinfo;
      char buffer2[80];

      timeinfo = gmtime(&rawtime);
      strftime(buffer2, 80, "%a, %d-%b-%Y %H:%M:%S %Z", timeinfo);
      buffer->appendText("; expires=");
      buffer->appendText(buffer2);
    }
  }

  if (path != "") {
    buffer->appendText("; path=");
    buffer->appendText(path);
  }

  if (domain != "") {
    buffer->appendText("; domain=");
    buffer->appendText(domain);
  }


  if (secure) {
    buffer->appendText("; secure");
  }

  if (httpOnly) {
    buffer->appendText("; HttpOnly");
  }

  char const* l = StringUtils::duplicate(buffer->c_str());
  delete buffer;
  _cookies.push_back(l);

  _freeables.push_back(l);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief swaps data
////////////////////////////////////////////////////////////////////////////////

HttpResponse* HttpResponse::swap () {
  HttpResponse* response = new HttpResponse(_code, _apiCompatibility);

  response->_headers.swap(&_headers);
  response->_body.swap(&_body);
  response->_freeables.swap(_freeables);

  bool isHeadResponse = response->_isHeadResponse;
  response->_isHeadResponse = _isHeadResponse;
  _isHeadResponse = isHeadResponse;

  size_t bodySize = response->_bodySize;
  response->_bodySize = _bodySize;
  _bodySize = bodySize;

  return response;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes the header
////////////////////////////////////////////////////////////////////////////////

void HttpResponse::writeHeader (StringBuffer* output) {
  bool const capitalizeHeaders = (_apiCompatibility >= 20100);

  output->appendText("HTTP/1.1 ");
  output->appendText(responseString(_code));
  output->appendText("\r\n");

  basics::Dictionary<char const*>::KeyValue const* begin;
  basics::Dictionary<char const*>::KeyValue const* end;

  bool seenTransferEncoding = false;
  string transferEncoding;

  for (_headers.range(begin, end);  begin < end;  ++begin) {
    char const* key = begin->_key;

    if (key == 0) {
      continue;
    }

    const size_t keyLength = strlen(key);

    // ignore content-length
    if (keyLength == 14 && *key == 'c' && memcmp(key, "content-length", keyLength) == 0) {
      continue;
    }

    if (keyLength == 17 && *key == 't' && memcmp(key, "transfer-encoding", keyLength) == 0) {
      seenTransferEncoding = true;
      transferEncoding = begin->_value;
      continue;
    }

    if (capitalizeHeaders) {
      char const* p = key;
      char const* end = key + keyLength;
      int capState = 1;
      while (p < end) {
        if (capState == 1) {
          // upper case
          output->appendChar(::toupper(*p));
          capState = 0;
        }
        else if (capState == 0) {
          // normal case
          output->appendChar(::tolower(*p));
          if (*p == '-') {
            capState = 1;
          }
          else if (*p == ':') {
            capState = 2;
          }
        }
        else {
          // output as is
          output->appendChar(*p);
        }
        ++p;
      }
    }
    else {
      output->appendText(key, keyLength);
    }
    output->appendText(": ", 2);
    output->appendText(begin->_value);
    output->appendText("\r\n", 2);
  }

  for (vector<char const*>::iterator iter = _cookies.begin();
          iter != _cookies.end(); ++iter) {
    if (capitalizeHeaders) {
      output->appendText("Set-Cookie: ", 12);
    }
    else {
      output->appendText("set-cookie: ", 12);
    }
    output->appendText(*iter);
    output->appendText("\r\n", 2);
  }

  if (seenTransferEncoding && transferEncoding == "chunked") {
    if (capitalizeHeaders) {
      output->appendText("Transfer-Encoding: chunked\r\n\r\n", 30);
    }
    else {
      output->appendText("transfer-encoding: chunked\r\n\r\n", 30);
    }
  }
  else {
    if (seenTransferEncoding) {
      if (capitalizeHeaders) {
        output->appendText("Transfer-Encoding: ", 19);
      }
      else {
        output->appendText("transfer-encoding: ", 19);
      }
      output->appendText(transferEncoding);
      output->appendText("\r\n", 2);
    }

    if (capitalizeHeaders) {
      output->appendText("Content-Length: ", 16);
    }
    else {
      output->appendText("content-length: ", 16);
    }

    if (_isHeadResponse) {
      // From http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.13
      //
      // 14.13 Content-Length
      //
      // The Content-Length entity-header field indicates the size of the entity-body,
      // in decimal number of OCTETs, sent to the recipient or, in the case of the HEAD method,
      // the size of the entity-body that would have been sent had the request been a GET.
      output->appendInteger(_bodySize);
    }
    else {
      output->appendInteger(_body.length());
    }

    output->appendText("\r\n\r\n", 4);
  }
  // end of header, body to follow
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the size of the body
////////////////////////////////////////////////////////////////////////////////

size_t HttpResponse::bodySize () const {
  if (_isHeadResponse) {
    return _bodySize;
  }
  else {
    return _body.length();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the body
////////////////////////////////////////////////////////////////////////////////

StringBuffer& HttpResponse::body () {
  return _body;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates a head response
////////////////////////////////////////////////////////////////////////////////

void HttpResponse::headResponse (size_t size) {
  _body.clear();
  _isHeadResponse = true;
  _bodySize = size;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deflates the response body
/// the body must already be set. deflate is then run on the existing body
////////////////////////////////////////////////////////////////////////////////

int HttpResponse::deflate (size_t bufferSize) {
  int res = _body.deflate(bufferSize);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  setHeader("content-encoding", strlen("content-encoding"), "deflate");
  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
