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

#ifndef LIB_REST_HTTP_RESPONSE_H
#define LIB_REST_HTTP_RESPONSE_H 1

#include "Basics/Common.h"

#include "Basics/Dictionary.h"
#include "Basics/StringBuffer.h"

#include "Rest/ArangoResponse.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief http response
///
/// A http request handler is called to handle a http request. It returns its
/// answer as http response.
////////////////////////////////////////////////////////////////////////////////

class HttpResponse: public ArangoResponse {

	HttpResponse() = delete;
  	HttpResponse(HttpResponse const&) = delete;
  	HttpResponse& operator=(HttpResponse const&) = delete;

  	public:
	//   //////////////////////////////////////////////////////////////////////////////
	//   /// @brief http response codes
	//   //////////////////////////////////////////////////////////////////////////////

	// enum ResponseCode {
	//   CONTINUE = 100,
	//   SWITCHING_PROTOCOLS = 101,
	//   PROCESSING = 102,

	//   OK = 200,
	//   CREATED = 201,
	//   ACCEPTED = 202,
	//   PARTIAL = 203,
	//   NO_CONTENT = 204,
	//   RESET_CONTENT = 205,
	//   PARTIAL_CONTENT = 206,

	//   MOVED_PERMANENTLY = 301,
	//   FOUND = 302,
	//   SEE_OTHER = 303,
	//   NOT_MODIFIED = 304,
	//   TEMPORARY_REDIRECT = 307,
	//   PERMANENT_REDIRECT = 308,

	//   BAD = 400,
	//   UNAUTHORIZED = 401,
	//   PAYMENT_REQUIRED = 402,
	//   FORBIDDEN = 403,
	//   NOT_FOUND = 404,
	//   METHOD_NOT_ALLOWED = 405,
	//   NOT_ACCEPTABLE = 406,
	//   REQUEST_TIMEOUT = 408,
	//   CONFLICT = 409,
	//   GONE = 410,
	//   LENGTH_REQUIRED = 411,
	//   PRECONDITION_FAILED = 412,
	//   REQUEST_ENTITY_TOO_LARGE = 413,
	//   REQUEST_URI_TOO_LONG = 414,
	//   UNSUPPORTED_MEDIA_TYPE = 415,
	//   REQUESTED_RANGE_NOT_SATISFIABLE = 416,
	//   EXPECTATION_FAILED = 417,
	//   I_AM_A_TEAPOT = 418,
	//   UNPROCESSABLE_ENTITY = 422,
	//   LOCKED = 423,
	//   PRECONDITION_REQUIRED = 428,
	//   TOO_MANY_REQUESTS = 429,
	//   REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
	//   UNAVAILABLE_FOR_LEGAL_REASONS = 451,

	//   SERVER_ERROR = 500,
	//   NOT_IMPLEMENTED = 501,
	//   BAD_GATEWAY = 502,
	//   SERVICE_UNAVAILABLE = 503,
	//   HTTP_VERSION_NOT_SUPPORTED = 505,
	//   BANDWIDTH_LIMIT_EXCEEDED = 509,
	//   NOT_EXTENDED = 510
	// };

	public:

	  HttpResponse(ResponseCode, uint32_t);

	  //////////////////////////////////////////////////////////////////////////////
	  /// @brief deletes a http response
	  ///
	  /// The destructor will free the string buffers used. After the http response
	  /// is deleted, the string buffers returned by body() become invalid.
	  //////////////////////////////////////////////////////////////////////////////

	  ~HttpResponse();	
	public:
	  //////////////////////////////////////////////////////////////////////////////
  	  /// @brief writes the header
  	  ///
  	  /// You should call writeHeader only after the body has been created.
  	  //////////////////////////////////////////////////////////////////////////////

  	  void writeHeader(basics::StringBuffer*);

  	  //////////////////////////////////////////////////////////////////////////////
  	  /// @brief http/vstream response string
  	  ///
  	  /// Converts the response code to a string suitable for delivering to a http/vstream
  	  /// client.
  	  //////////////////////////////////////////////////////////////////////////////

  	  // std::string responseString(ResponseCode);

	  //////////////////////////////////////////////////////////////////////////////
	  /// @brief returns the body
	  ///
	  /// Returns a reference to the body. This reference is only valid as long as
	  /// http response exists. You can add data to the body by appending
	  /// information to the string buffer. Note that adding data to the body
	  /// invalidates any previously returned header. You must call header
	  /// again.
	  //////////////////////////////////////////////////////////////////////////////

	  basics::StringBuffer& body();

	protected:
	  //////////////////////////////////////////////////////////////////////////////
  	  /// @brief body
  	  //////////////////////////////////////////////////////////////////////////////

  	  basics::StringBuffer _body; 	
};
}
}	

#endif