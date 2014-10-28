////////////////////////////////////////////////////////////////////////////////
/// @brief path handler
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2008-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_HTTP_SERVER_PATH_HANDLER_H
#define ARANGODB_HTTP_SERVER_PATH_HANDLER_H 1

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

          Options (std::string const& path)
            : path(path), contentType("text/html"), allowSymbolicLink(false), cacheMaxAge(0) {
          }

          Options (std::string const& path, std::string const& contentType)
            : path(path), contentType(contentType), allowSymbolicLink(false), cacheMaxAge(0) {
          }

          std::string path;
          std::string contentType;
          bool allowSymbolicLink;
          std::string defaultFile;
          int64_t cacheMaxAge;
        };

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief factory methods
////////////////////////////////////////////////////////////////////////////////

        static HttpHandler* create (HttpRequest* request, void* data) {
          Options* options = static_cast<Options*>(data);

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

        status_t execute ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void handleError (basics::TriagensError const&);

      private:
        std::string path;
        std::string contentType;
        bool allowSymbolicLink;
        std::string defaultFile;
        int64_t cacheMaxAge;
        std::string maxAgeHeader;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
