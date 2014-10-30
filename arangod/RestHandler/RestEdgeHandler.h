////////////////////////////////////////////////////////////////////////////////
/// @brief edge request handler
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
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_HANDLER_REST_EDGE_HANDLER_H
#define ARANGODB_REST_HANDLER_REST_EDGE_HANDLER_H 1

#include "Basics/Common.h"

#include "RestHandler/RestDocumentHandler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                   RestEdgeHandler
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

////////////////////////////////////////////////////////////////////////////////
/// @brief collection request handler
////////////////////////////////////////////////////////////////////////////////

    class RestEdgeHandler : public RestDocumentHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestEdgeHandler (rest::HttpRequest*);

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief get collection type
////////////////////////////////////////////////////////////////////////////////

        virtual TRI_col_type_e getCollectionType () const {
          return TRI_COL_TYPE_EDGE;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

    private:

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an edge
////////////////////////////////////////////////////////////////////////////////

      bool createDocument ();

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document (an edge), coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

      bool createDocumentCoordinator (std::string const& collname,
                                      bool waitForSync,
                                      TRI_json_t* json,
                                      char const* from,
                                      char const* to);
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
