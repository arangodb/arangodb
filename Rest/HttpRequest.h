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

#ifndef TRIAGENS_FYN_REST_HTTP_REQUEST_H
#define TRIAGENS_FYN_REST_HTTP_REQUEST_H 1

#include <Basics/Common.h>

#include <Basics/Dictionary.h>
#include <Rest/ConnectionInfo.h>

namespace triagens {
  namespace basics {
    class StringBuffer;
  }

  namespace rest {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief http request
    ///
    /// The http server reads the request string from the client and converts it
    /// into an instance of this class. An http request object provides methods to
    /// inspect the header and parameter fields.
    ////////////////////////////////////////////////////////////////////////////////

    class  HttpRequest {
      private:
        HttpRequest (HttpRequest const&);
        HttpRequest& operator= (HttpRequest const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief http request type
        ////////////////////////////////////////////////////////////////////////////////

        enum HttpRequestType {
          HTTP_REQUEST_DELETE,
          HTTP_REQUEST_GET,
          HTTP_REQUEST_HEAD,
          HTTP_REQUEST_POST,
          HTTP_REQUEST_PUT,
          HTTP_REQUEST_ILLEGAL
        };

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief http request
        ///
        /// Constructs a http request given the header string. A client request
        /// consists of two parts: the header and the body. For a GET request the
        /// body is always empty and all information about the request is delivered
        /// in the header. For a POST or PUT request some information is also
        /// delivered in the body. However, it is necessary to parse the header
        /// information, before the body can be read.
        ////////////////////////////////////////////////////////////////////////////////

        explicit
        HttpRequest (string const& header);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief http request
        ///
        /// Constructs a http request given the header string. A client request
        /// consists of two parts: the header and the body. For a GET request the
        /// body is always empty and all information about the request is delivered
        /// in the header. For a POST or PUT request some information is also
        /// delivered in the body. However, it is necessary to parse the header
        /// information, before the body can be read.
        ////////////////////////////////////////////////////////////////////////////////

        HttpRequest (char const* header, size_t length);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief http request
        ///
        /// Constructs a http request given nothing
        ////////////////////////////////////////////////////////////////////////////////

        HttpRequest ();

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructor
        ////////////////////////////////////////////////////////////////////////////////

        virtual ~HttpRequest ();

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the http request type
        ////////////////////////////////////////////////////////////////////////////////

        HttpRequestType requestType () const {
          return type;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets the http request type
        ////////////////////////////////////////////////////////////////////////////////

        void setRequestType (HttpRequestType newType) {
          type = newType;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the path of the request
        ///
        /// The path consists of the URL without the host and without any parameters.
        ////////////////////////////////////////////////////////////////////////////////

        string const& requestPath () const {
          return requestPathValue;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets the path of the request
        ////////////////////////////////////////////////////////////////////////////////

        void setRequestPath (string const& path) {
          requestPathValue = path;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the content length
        ////////////////////////////////////////////////////////////////////////////////

        size_t contentLength () const;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the server IP
        ////////////////////////////////////////////////////////////////////////////////

        ConnectionInfo const& connectionInfo () const {
          return connectionInfoValue;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets the server IP
        ////////////////////////////////////////////////////////////////////////////////

        void setConnectionInfo (ConnectionInfo const& info) {
          connectionInfoValue = info;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief writes representation to string buffer
        ////////////////////////////////////////////////////////////////////////////////

        void write (basics::StringBuffer*) const;

      public:

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
        /// @brief set a header field
        ///
        /// Set the value of a header field with given name.
        ////////////////////////////////////////////////////////////////////////////////

        void setHeader (string const& field, string const& value);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns all suffix parts
        ////////////////////////////////////////////////////////////////////////////////

        vector<string> const& suffix () const {
          return suffixValue;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief adds a suffix part
        ////////////////////////////////////////////////////////////////////////////////

        void addSuffix (string const& part) {
          suffixValue.push_back(part);
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the value of a key
        ///
        /// Returns the value of a key. The empty string is returned if key was not
        /// specified by the client.
        ////////////////////////////////////////////////////////////////////////////////

        string value (string const& key) const;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the value of a key
        ///
        /// Returns the value of a key. The empty string is returned if key was not
        /// specified by the client. found is true if the client specified the key.
        ////////////////////////////////////////////////////////////////////////////////

        string value (string const& key, bool& found) const;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns all values
        ///
        /// Returns all key/value pairs of the request.
        ////////////////////////////////////////////////////////////////////////////////

        map<string, string> values () const;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets the key/values from an url encoded string
        ////////////////////////////////////////////////////////////////////////////////

        void setValues (string const& params);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets a key/value pair
        ////////////////////////////////////////////////////////////////////////////////

        void setValue (string const& key, string const& value);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief gets the body
        ////////////////////////////////////////////////////////////////////////////////

        string const& body () const {
          return bodyValue;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets the body
        ///
        /// In a POST request the body contains additional key/value pairs. The
        /// function parses the body and adds the corresponding pairs.
        ////////////////////////////////////////////////////////////////////////////////

        void setBody (string const& newBody) {
          bodyValue = newBody;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets the body
        ///
        /// In a POST request the body contains additional key/value pairs. The
        /// function parses the body and adds the corresponding pairs.
        ////////////////////////////////////////////////////////////////////////////////

        void setBody (char const* newBody, size_t length) {
          bodyValue = string(newBody, length);
        }

      private:
        void parseHeader (char* ptr, size_t length);
        void setValues (char* begin, char* end);

      private:
        HttpRequestType type;

        string requestPathValue;

        vector<string> suffixValue;
        basics::Dictionary<char const*> headerFields;
        basics::Dictionary<char const*> requestFields;

        string bodyValue;

        ConnectionInfo connectionInfoValue;

        vector<char const*> freeables;
    };
  }
}

#endif
