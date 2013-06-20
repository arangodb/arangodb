////////////////////////////////////////////////////////////////////////////////
/// @brief path handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_HTTP_SERVER_PATH_HANDLER_H
#define TRIAGENS_HTTP_SERVER_PATH_HANDLER_H 1

#include "Basics/Common.h"

#include "HttpServer/HttpHandler.h"

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief path handler
////////////////////////////////////////////////////////////////////////////////

    class PathHandler : public HttpHandler {
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief path options
////////////////////////////////////////////////////////////////////////////////

        struct Options {
          Options ()
            : allowSymbolicLink(false), cacheMaxAge(0) {
          }

          Options (string const& path)
            : path(path), contentType("text/html"), allowSymbolicLink(false), cacheMaxAge(0) {
          }

          Options (string const& path, string const& contentType)
            : path(path), contentType(contentType), allowSymbolicLink(false), cacheMaxAge(0) {
          }

          string path;
          string contentType;
          bool allowSymbolicLink;
          string defaultFile;
          int64_t cacheMaxAge;
        };

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief factory methods
////////////////////////////////////////////////////////////////////////////////

        static HttpHandler* create (HttpRequest* request, void* data) {
          Options* options = (Options*) data;

          return new PathHandler(request, options);
        }

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        PathHandler (HttpRequest* request, Options const* options);

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool isDirect () {
          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        status_e execute ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void handleError (basics::TriagensError const&);
      
      private:
        string path;
        string contentType;
        bool allowSymbolicLink;
        string defaultFile;
        int64_t cacheMaxAge;
        string maxAgeHeader; 
    };
  }
}

#endif

