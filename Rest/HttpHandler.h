////////////////////////////////////////////////////////////////////////////////
/// @brief abstract class for http handlers
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_REST_HTTP_HANDLER_H
#define TRIAGENS_REST_HTTP_HANDLER_H 1

#include <Rest/Handler.h>

namespace triagens {
  namespace rest {
    class HttpRequest;
    class HttpResponse;

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup HttpServer
    /// @brief abstract class for http handlers
    ////////////////////////////////////////////////////////////////////////////////

    class  HttpHandler : public Handler {
      private:
        HttpHandler (HttpHandler const&);
        HttpHandler& operator= (HttpHandler const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new handler
        ///
        /// Note that the handler owns the request and the response. It is its
        /// responsibility to destroy them both.
        ////////////////////////////////////////////////////////////////////////////////

        explicit
        HttpHandler (HttpRequest*);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructs a handler
        ////////////////////////////////////////////////////////////////////////////////

        ~HttpHandler ();

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the response
        ////////////////////////////////////////////////////////////////////////////////

        HttpResponse* getResponse ();

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief the request
        ////////////////////////////////////////////////////////////////////////////////

        HttpRequest* request;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief the response
        ////////////////////////////////////////////////////////////////////////////////

        HttpResponse* response;
    };
  }
}

#endif
