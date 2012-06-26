////////////////////////////////////////////////////////////////////////////////
/// @brief protobuf http request
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

#include "HttpRequestProtobuf.h"

#include "BasicsC/strings.h"
#include "Basics/StringUtils.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                   local constants
// -----------------------------------------------------------------------------

static char const* EMPTY_STR = "";

// -----------------------------------------------------------------------------
// --SECTION--                                         class HttpRequestProtobuf
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

HttpRequestProtobuf::HttpRequestProtobuf (PB_ArangoBatchMessage const& message)
  : HttpRequest(),
    _valid(false),
    _request(0) {
  if (message.type() == PB_BLOB_REQUEST) {
    _valid = true;
    _request = &message.blobrequest();

    switch (_request->requesttype()) {
      case PB_REQUEST_TYPE_DELETE:
        _type = HTTP_REQUEST_DELETE;
        break;

      case PB_REQUEST_TYPE_GET:
        _type = HTTP_REQUEST_GET;
        break;

      case PB_REQUEST_TYPE_HEAD:
        _type = HTTP_REQUEST_HEAD;
        break;

      case PB_REQUEST_TYPE_POST:
        _type = HTTP_REQUEST_POST;
        break;

      case PB_REQUEST_TYPE_PUT:
        _type = HTTP_REQUEST_PUT;
        break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

HttpRequestProtobuf::~HttpRequestProtobuf () {
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

char const* HttpRequestProtobuf::requestPath () const {
  return _request->url().c_str();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpRequestProtobuf::write (TRI_string_buffer_t* buffer) const {
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

  TRI_AppendStringStringBuffer(buffer, _request->url().c_str());

  // generate the request parameters
  bool first = true;

  for (::google::protobuf::RepeatedPtrField< PB_ArangoKeyValue >::const_iterator i = _request->values().begin();
       i != _request->values().end();
       ++i) {
    PB_ArangoKeyValue const& kv = *i;

    if (first) {
      TRI_AppendCharStringBuffer(buffer, '?');
      first = false;
    }
    else {
      TRI_AppendCharStringBuffer(buffer, '&');
    }

    TRI_AppendUrlEncodedStringStringBuffer(buffer, kv.key().c_str());
    TRI_AppendCharStringBuffer(buffer, '=');
    TRI_AppendUrlEncodedStringStringBuffer(buffer, kv.value().c_str());
  }

  TRI_AppendString2StringBuffer(buffer, " HTTP/1.1\r\n", 11);

  // generate the header fields
  for (::google::protobuf::RepeatedPtrField< PB_ArangoKeyValue >::const_iterator i = _request->headers().begin();
       i != _request->headers().end();
       ++i) {
    PB_ArangoKeyValue const& kv = *i;

    TRI_AppendStringStringBuffer(buffer, kv.key().c_str());
    TRI_AppendString2StringBuffer(buffer, ": ", 2);
    TRI_AppendStringStringBuffer(buffer, kv.value().c_str());
    TRI_AppendString2StringBuffer(buffer, "\r\n", 2);
  }

  TRI_AppendString2StringBuffer(buffer, "content-length: ", 16);
  TRI_AppendUInt64StringBuffer(buffer, _request->content().size());
  TRI_AppendString2StringBuffer(buffer, "\r\n\r\n", 4);
  TRI_AppendString2StringBuffer(buffer, _request->content().c_str(), _request->content().size());
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

size_t HttpRequestProtobuf::contentLength () const {
  return _request->content().size();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequestProtobuf::header (char const* key) const {
  map<string, string> result;

  ::google::protobuf::RepeatedPtrField< PB_ArangoKeyValue >::const_iterator i = _request->headers().begin();

  for (;  i != _request->headers().end();  ++i) {
    PB_ArangoKeyValue const& kv = *i;

    if (TRI_EqualString(key, kv.key().c_str())) {
      return kv.value().c_str();
    }
  }

  return EMPTY_STR;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequestProtobuf::header (char const* key, bool& found) const {
  map<string, string> result;

  ::google::protobuf::RepeatedPtrField< PB_ArangoKeyValue >::const_iterator i = _request->headers().begin();

  for (;  i != _request->headers().end();  ++i) {
    PB_ArangoKeyValue const& kv = *i;

    if (TRI_EqualString(key, kv.key().c_str())) {
      found = true;
      return kv.value().c_str();
    }
  }

  found = false;
  return EMPTY_STR;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

map<string, string> HttpRequestProtobuf::headers () const {
  map<string, string> result;

  ::google::protobuf::RepeatedPtrField< PB_ArangoKeyValue >::const_iterator i = _request->headers().begin();

  for (;  i != _request->headers().end();  ++i) {
    PB_ArangoKeyValue const& kv = *i;

    result[kv.key()] = kv.value();
  }

  result["content-length"] = StringUtils::itoa(_request->content().size());

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequestProtobuf::value (char const* key) const {
  map<string, string> result;

  ::google::protobuf::RepeatedPtrField< PB_ArangoKeyValue >::const_iterator i = _request->values().begin();

  for (;  i != _request->values().end();  ++i) {
    PB_ArangoKeyValue const& kv = *i;

    if (TRI_EqualString(key, kv.key().c_str())) {
      return kv.value().c_str();
    }
  }

  return EMPTY_STR;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequestProtobuf::value (char const* key, bool& found) const {
  map<string, string> result;

  ::google::protobuf::RepeatedPtrField< PB_ArangoKeyValue >::const_iterator i = _request->values().begin();

  for (;  i != _request->values().end();  ++i) {
    PB_ArangoKeyValue const& kv = *i;

    if (TRI_EqualString(key, kv.key().c_str())) {
      found = true;
      return kv.value().c_str();
    }
  }

  found = false;
  return EMPTY_STR;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

map<string, string> HttpRequestProtobuf::values () const {
  map<string, string> result;

  ::google::protobuf::RepeatedPtrField< PB_ArangoKeyValue >::const_iterator i = _request->values().begin();

  for (;  i != _request->values().end();  ++i) {
    PB_ArangoKeyValue const& kv = *i;

    result[kv.key()] = kv.value();
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

char const* HttpRequestProtobuf::body () const {
  return _request->content().c_str();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

size_t HttpRequestProtobuf::bodySize () const {
  return _request->content().size();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

int HttpRequestProtobuf::setBody (char const* newBody, size_t length) {
  return TRI_ERROR_NOT_IMPLEMENTED;
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
/// @brief true if message is valid
////////////////////////////////////////////////////////////////////////////////

bool HttpRequestProtobuf::isValid () const {
  return _valid;
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
