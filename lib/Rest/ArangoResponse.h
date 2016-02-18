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
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_REST_ARANGO_RESPONSE_H
#define LIB_REST_ARANGO_RESPONSE_H 1

#include "Basics/Common.h"

#include "Basics/Dictionary.h"
#include "Basics/StringBuffer.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace rest {

class ArangoResponse {
  ArangoResponse() = delete;
  ArangoResponse(ArangoResponse const&) = delete;
  ArangoResponse& operator=(ArangoResponse const&) = delete;

  public:
  	enum ResponseCode {CONTINUE = 100,
	  SWITCHING_PROTOCOLS = 101,
	  PROCESSING = 102,

	  OK = 200,
	  CREATED = 201,
	  ACCEPTED = 202,
	  PARTIAL = 203,
	  NO_CONTENT = 204,
	  RESET_CONTENT = 205,
	  PARTIAL_CONTENT = 206,

	  MOVED_PERMANENTLY = 301,
	  FOUND = 302,
	  SEE_OTHER = 303,
	  NOT_MODIFIED = 304,
	  TEMPORARY_REDIRECT = 307,
	  PERMANENT_REDIRECT = 308,

	  BAD = 400,
	  UNAUTHORIZED = 401,
	  PAYMENT_REQUIRED = 402,
	  FORBIDDEN = 403,
	  NOT_FOUND = 404,
	  METHOD_NOT_ALLOWED = 405,
	  NOT_ACCEPTABLE = 406,
	  REQUEST_TIMEOUT = 408,
	  CONFLICT = 409,
	  GONE = 410,
	  LENGTH_REQUIRED = 411,
	  PRECONDITION_FAILED = 412,
	  REQUEST_ENTITY_TOO_LARGE = 413,
	  REQUEST_URI_TOO_LONG = 414,
	  UNSUPPORTED_MEDIA_TYPE = 415,
	  REQUESTED_RANGE_NOT_SATISFIABLE = 416,
	  EXPECTATION_FAILED = 417,
	  I_AM_A_TEAPOT = 418,
	  UNPROCESSABLE_ENTITY = 422,
	  LOCKED = 423,
	  PRECONDITION_REQUIRED = 428,
	  TOO_MANY_REQUESTS = 429,
	  REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
	  UNAVAILABLE_FOR_LEGAL_REASONS = 451,

	  SERVER_ERROR = 500,
	  NOT_IMPLEMENTED = 501,
	  BAD_GATEWAY = 502,
	  SERVICE_UNAVAILABLE = 503,
	  HTTP_VERSION_NOT_SUPPORTED = 505,
	  BANDWIDTH_LIMIT_EXCEEDED = 509,
	  NOT_EXTENDED = 510
	};

  public:

  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief whether or not the response is a HTTP/VSTREAM HEAD response
  	//////////////////////////////////////////////////////////////////////////////

  	bool isHeadResponse() const { return _isHeadResponse; }

  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief whether or not the response is a HTTP/VSTREAM STATUS response
  	//////////////////////////////////////////////////////////////////////////////

  	bool isStatusResponse() const { return _isStatusResponse; }

  	//////////////////////////////////////////////////////////////////////////////
	/// @brief get http/vstream response code from string
	///
	/// Converts the response code string to the internal code
	//////////////////////////////////////////////////////////////////////////////

  	ResponseCode responseCode(std::string const& str);


	//////////////////////////////////////////////////////////////////////////////
	/// @brief get http/vstream response code from integer error code
	//////////////////////////////////////////////////////////////////////////////

	static ResponseCode responseCode(int);

	//////////////////////////////////////////////////////////////////////////////
	/// @brief returns the response code
	//////////////////////////////////////////////////////////////////////////////

	ResponseCode responseCode() const;

	//////////////////////////////////////////////////////////////////////////////
  	/// @brief http/vstream response string
  	///
  	/// Converts the response code to a string suitable for delivering to a http/vstream
  	/// client.
  	//////////////////////////////////////////////////////////////////////////////

  	std::string responseString(ResponseCode);

	//////////////////////////////////////////////////////////////////////////////
	/// @brief sets the response code
	//////////////////////////////////////////////////////////////////////////////

	void setResponseCode(ResponseCode);

	//////////////////////////////////////////////////////////////////////////////
  	/// @brief returns the content length
  	//////////////////////////////////////////////////////////////////////////////

  	size_t contentLength();

  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief returns the content length
  	//////////////////////////////////////////////////////////////////////////////

  	size_t contentLength(std::string);

    //////////////////////////////////////////////////////////////////////////////
	/// @brief sets the content type
  	///
  	/// Sets the content type of the information of the body.
  	//////////////////////////////////////////////////////////////////////////////

  	void setContentType(std::string const& contentType);

  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief checks if chunked encoding is set
  	//////////////////////////////////////////////////////////////////////////////

  	bool isChunked() const;
  	
  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief returns a header field
  	///
  	/// Returns the value of a header field with given name. If no header field
  	/// with the given name was specified by the client, the empty string is
  	/// returned.
  	//////////////////////////////////////////////////////////////////////////////

  	std::string header(std::string const& field) const;

  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief returns a header field
  	///
  	/// Returns the value of a header field with given name. If no header field
  	/// with the given name was specified by the client, the empty string is
  	/// returned.
 	/// The header field name must already be trimmed and lower-cased
  	//////////////////////////////////////////////////////////////////////////////

  	std::string header(char const*, size_t) const;

  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief returns a header field
  	///
  	/// Returns the value of a header field with given name. If no header field
  	/// with the given name was specified by the client, the empty string is
  	/// returned. found is set if the client specified the header field.
  	//////////////////////////////////////////////////////////////////////////////

  	std::string header(std::string const&, bool& found) const;

  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief returns a header field
  	///
  	/// Returns the value of a header field with given name. If no header field
  	/// with the given name was specified by the client, the empty string is
  	/// returned. found is set if the client specified the header field.
  	//////////////////////////////////////////////////////////////////////////////

  	std::string header(char const*, size_t, bool& found) const;


  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief returns all header fields
  	///
  	/// Returns all header fields
  	//////////////////////////////////////////////////////////////////////////////

  	std::map<std::string, std::string> headers() const;


	//////////////////////////////////////////////////////////////////////////////
	/// @brief sets a header field
	///
	/// The key must be lowercased and trimmed already
	/// The key string must remain valid until the response is destroyed
	//////////////////////////////////////////////////////////////////////////////

	void setHeader(char const*, size_t, std::string const& value);

	//////////////////////////////////////////////////////////////////////////////
	/// @brief sets a header field
	///
	/// The key must be lowercased and trimmed already
	/// The key string must remain valid until the response is destroyed
	/// The value string must remain valid until the response is destroyed
	//////////////////////////////////////////////////////////////////////////////

	void setHeader(char const*, size_t, char const*);

	//////////////////////////////////////////////////////////////////////////////
	/// @brief sets a header field
	///
	/// The key is automatically converted to lower case and trimmed.
	//////////////////////////////////////////////////////////////////////////////

	void setHeader(std::string const& key, std::string const& value);


	//////////////////////////////////////////////////////////////////////////////
  	/// @brief sets many header fields
  	///
  	/// The key is automatically converted to lower case.
  	//////////////////////////////////////////////////////////////////////////////

  	virtual void setHeaders(std::string const&, bool){};

  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief sets a cookie
  	///
  	/// The name is automatically trimmed.
  	//////////////////////////////////////////////////////////////////////////////

  	virtual void setCookie(std::string const& , std::string const& ,
                 int, std::string const&, std::string const&, bool, bool ){};

  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief swaps data
  	//////////////////////////////////////////////////////////////////////////////

  	virtual ArangoResponse* swap();

  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief returns the size of the body
  	//////////////////////////////////////////////////////////////////////////////

  	virtual size_t bodySize() const = 0;

	
  	//////////////////////////////////////////////////////////////////////////////
  	/// @brief indicates a head response
  	///
  	/// In case of HEAD request, no body must be defined. However, the response
  	/// needs to know the size of body.
  	//////////////////////////////////////////////////////////////////////////////

  	virtual void headResponse(size_t){};  	

  	//////////////////////////////////////////////////////////////////////////////
	/// @brief deflates the response body
	///
	/// the body must already be set. deflate is then run on the existing body
	//////////////////////////////////////////////////////////////////////////////

	// virtual int deflate(size_t = 16384){};

	//////////////////////////////////////////////////////////////////////////////
  	/// @brief checks for special headers
  	//////////////////////////////////////////////////////////////////////////////

  	void checkHeader(char const* key, char const* value);

  public:
	  //////////////////////////////////////////////////////////////////////////////
	  /// @brief constructs a new http/vstream response
	  //////////////////////////////////////////////////////////////////////////////

	  ArangoResponse(ResponseCode, uint32_t);

	  //////////////////////////////////////////////////////////////////////////////
	  /// @brief deletes a http/vstream response
	  ///
	  /// The destructor will free the string buffers used. After the http response
	  /// is deleted, the string buffers returned by body() become invalid.
	  //////////////////////////////////////////////////////////////////////////////

	  ~ArangoResponse();

  protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief response code (Http/VelocyStream)
  //////////////////////////////////////////////////////////////////////////////

  ResponseCode _code;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compatibility
  //////////////////////////////////////////////////////////////////////////////

  uint32_t const _apiCompatibility;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief head response flag
  //////////////////////////////////////////////////////////////////////////////

  bool _isHeadResponse;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief status response flag
  //////////////////////////////////////////////////////////////////////////////

  bool _isStatusResponse;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief chunked flag
  //////////////////////////////////////////////////////////////////////////////

  bool _isChunked;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief headers dictionary
  //////////////////////////////////////////////////////////////////////////////

  basics::Dictionary<char const*> _headers;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief cookies
  //////////////////////////////////////////////////////////////////////////////

  std::vector<char const*> _cookies;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief body size
  //////////////////////////////////////////////////////////////////////////////

  size_t _bodySize;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief freeable list
  //////////////////////////////////////////////////////////////////////////////

  std::vector<char const*> _freeables;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief batch error count header
  //////////////////////////////////////////////////////////////////////////////

  static std::string const BatchErrorHeader;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief was docuBlock serverHideProductHeader
  ////////////////////////////////////////////////////////////////////////////////

  static bool HideProductHeader;	  

};
}
}	

#endif