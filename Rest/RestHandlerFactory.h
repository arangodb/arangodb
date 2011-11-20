////////////////////////////////////////////////////////////////////////////////
/// @brief handler factory
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

#ifndef TRIAGENS_REST_REST_HANDLER_FACTORY_H
#define TRIAGENS_REST_REST_HANDLER_FACTORY_H 1

#include <Rest/HttpHandlerFactory.h>

////////////////////////////////////////////////////////////////////////////////
/// @defgroup HttpServer Http-Server
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace rest {
    class RestModel;

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup HttpServer
    /// @brief handler factory
    ////////////////////////////////////////////////////////////////////////////////

    class RestHandlerFactory : public HttpHandlerFactory {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new handler factory
        ////////////////////////////////////////////////////////////////////////////////

        RestHandlerFactory (RestModel* model)
          : model(model) {
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        pair<size_t, size_t> sizeRestrictions ();

      private:
        RestModel* model;
    };
  }
}

#endif
