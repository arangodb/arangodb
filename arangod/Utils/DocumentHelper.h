////////////////////////////////////////////////////////////////////////////////
/// @brief document utility functions
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_UTILS_DOCUMENT_HELPER_H
#define ARANGODB_UTILS_DOCUMENT_HELPER_H 1

#include "Basics/Common.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/voc-types.h"

struct TRI_json_t;

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                              class DocumentHelper
// -----------------------------------------------------------------------------

    class DocumentHelper {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      private:

        DocumentHelper () = delete;

        ~DocumentHelper () = delete;

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a document id from a string and a string
////////////////////////////////////////////////////////////////////////////////

        static std::string assembleDocumentId (std::string const&,
                                               std::string const& key);

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a document id from a string and a char* key
////////////////////////////////////////////////////////////////////////////////

        static std::string assembleDocumentId (std::string const&,
                                               const TRI_voc_key_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection id and document key from an id
////////////////////////////////////////////////////////////////////////////////

        static bool parseDocumentId (triagens::arango::CollectionNameResolver const&,
                                     char const*,
                                     TRI_voc_cid_t&,
                                     char**);

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the "_key" attribute from a JSON object
////////////////////////////////////////////////////////////////////////////////

        static int getKey (struct TRI_json_t const*,
                           TRI_voc_key_t*);

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
