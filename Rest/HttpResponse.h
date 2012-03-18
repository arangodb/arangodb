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

#ifndef TRIAGENS_FYN_REST_HTTP_RESPONSE_H
#define TRIAGENS_FYN_REST_HTTP_RESPONSE_H 1

#include <Basics/Common.h>

#include <Basics/Dictionary.h>
#include <Basics/StringBuffer.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief 500: Method (GET/PUT/POST/DELETE) not allowed.
/// 
/// Will be raised when the request method is not allowed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_REST_ERROR_METHOD_NOT_ALLOWED                                  (500)

////////////////////////////////////////////////////////////////////////////////
/// @brief 501: Bad parameter.
/// 
/// Will be raised when the a bad does not fulfill the requirements.
////////////////////////////////////////////////////////////////////////////////

#define TRI_REST_ERROR_BAD_PARAMETER                                       (501)

////////////////////////////////////////////////////////////////////////////////
/// @brief 502: Invalid JSON object.
/// 
/// Will be raised when a string representation an JSON object is corrupt.
////////////////////////////////////////////////////////////////////////////////

#define TRI_REST_ERROR_CORRUPTED_JSON                                      (502)

////////////////////////////////////////////////////////////////////////////////
/// @brief 503: Tailing URL parts.
/// 
/// Will be raised when the URL contains superfluous suffices.
////////////////////////////////////////////////////////////////////////////////

#define TRI_REST_ERROR_SUPERFLUOUS_SUFFICES                                (503)

////////////////////////////////////////////////////////////////////////////////
/// @brief 503: Not implemented.
/// 
/// Will be raised when the URL exists, but has not yet been implemented.
////////////////////////////////////////////////////////////////////////////////

#define TRI_REST_ERROR_NOT_IMPLEMENTED                                     (504)

namespace triagens {
  namespace rest {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief http response
    ///
    /// A http request handler is called to handle a http request. It returns its
    /// answer as http response.
    ////////////////////////////////////////////////////////////////////////////////

    class  HttpResponse {
      private:
        HttpResponse (HttpResponse const&);
        HttpResponse& operator= (HttpResponse const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief http response codes
        ////////////////////////////////////////////////////////////////////////////////

        enum HttpResponseCode {
          OK                   = 200,
          CREATED              = 201,
          ACCEPTED             = 202,
          PARTIAL              = 203,
          NO_CONTENT           = 204,

          MOVED_PERMANENTLY    = 301,
          FOUND                = 302,
          SEE_OTHER            = 303,
          NOT_MODIFIED         = 304,
          TEMPORARY_REDIRECT   = 307,

          BAD                  = 400,
          UNAUTHORIZED         = 401,
          PAYMENT              = 402,
          FORBIDDEN            = 403,
          NOT_FOUND            = 404,
          METHOD_NOT_ALLOWED   = 405,
          CONFLICT             = 409,
          PRECONDITION_FAILED  = 412,
          UNPROCESSABLE_ENTITY = 422,

          SERVER_ERROR         = 500,
          NOT_IMPLEMENTED      = 501,
          BAD_GATEWAY          = 502,
          SERVICE_UNAVAILABLE  = 503
        };

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief http response string
        ///
        /// Converts the response code to a string suitable for delivering to a http
        /// client.
        ////////////////////////////////////////////////////////////////////////////////

        static string responseString (HttpResponseCode);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief get http response code from string
        ///
        /// Converts the response code string to the internal code
        ////////////////////////////////////////////////////////////////////////////////

        static HttpResponseCode responseCode (string const& str);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new http response
        ////////////////////////////////////////////////////////////////////////////////

        HttpResponse ();

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new http response
        ////////////////////////////////////////////////////////////////////////////////

        explicit
        HttpResponse (string const& lines);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new http response
        ////////////////////////////////////////////////////////////////////////////////

        explicit
        HttpResponse (HttpResponseCode);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief deletes a http response
        ///
        /// The descrutor will free the string buffers used. After the http response
        /// is deleted, the string buffers returned by body() become invalid.
        ////////////////////////////////////////////////////////////////////////////////

        virtual ~HttpResponse ();

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the response code
        ////////////////////////////////////////////////////////////////////////////////

        HttpResponseCode responseCode () {
          return code;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the content length
        ////////////////////////////////////////////////////////////////////////////////

        size_t contentLength ();

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets the content type
        ///
        /// Sets the content type of the information of the body.
        ////////////////////////////////////////////////////////////////////////////////

        void setContentType (string const& contentType);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns a header field
        ///
        /// Returns the value of a header field with given name. If no header field
        /// with the given name was specified by the client, the empty string is
        /// returned.
        ////////////////////////////////////////////////////////////////////////////////

        string header (string const& field) const;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns a header field
        ///
        /// Returns the value of a header field with given name. If no header field
        /// with the given name was specified by the client, the empty string is
        /// returned. found is try if the client specified the header field.
        ////////////////////////////////////////////////////////////////////////////////

        string header (string const& field, bool& found) const;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns all header fields
        ///
        /// Returns all header fields
        ////////////////////////////////////////////////////////////////////////////////

        map<string, string> headers () const;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets a header field
        ///
        /// The key is automatically converted to lower case.
        ////////////////////////////////////////////////////////////////////////////////

        void setHeader (string const& key, string const& value);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets many header fields
        ///
        /// The key is automatically converted to lower case.
        ////////////////////////////////////////////////////////////////////////////////

        void setHeaders (string const& headers, bool includeLine0);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief swaps data
        ////////////////////////////////////////////////////////////////////////////////

        HttpResponse* swap ();

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief writes the header
        ///
        /// You should call writeHeader only after the body has been created.
        ////////////////////////////////////////////////////////////////////////////////

        void writeHeader (basics::StringBuffer*);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the body
        ///
        /// Returns a reference to the body. This reference is only valid as long as
        /// http response exists. You can add data to the body by appending
        /// information to the string buffer. Note that adding data to the body
        /// invalidates any previously returned header. You must call header
        /// again.
        ////////////////////////////////////////////////////////////////////////////////

        basics::StringBuffer& body ();

      private:
        HttpResponseCode code;

        basics::Dictionary<char const*> headerFields;
        basics::StringBuffer bodyValue;

        vector<char const*> freeables;
    };
  }
}

#endif
