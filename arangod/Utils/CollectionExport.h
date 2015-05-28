////////////////////////////////////////////////////////////////////////////////
/// @brief collection export result container
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

#ifndef ARANGODB_ARANGO_COLLECTION_EXPORT_H
#define ARANGODB_ARANGO_COLLECTION_EXPORT_H 1

#include "Basics/Common.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/voc-types.h"

struct TRI_document_collection_t;
struct TRI_vocbase_s;

namespace triagens {
  namespace arango {

    class CollectionGuard;
    class DocumentDitch;

// -----------------------------------------------------------------------------
// --SECTION--                                            class CollectionExport
// -----------------------------------------------------------------------------

    class CollectionExport {

      friend class ExportCursor;

      public:

        struct Restrictions {
          enum Type {
            RESTRICTION_NONE,
            RESTRICTION_INCLUDE,
            RESTRICTION_EXCLUDE
          };

          Restrictions () 
            : fields(),
              type(RESTRICTION_NONE) {
          }

          std::unordered_set<std::string> fields;
          Type type;
        };

      public:

        CollectionExport (CollectionExport const&) = delete;
        CollectionExport& operator= (CollectionExport const&) = delete;

        CollectionExport (TRI_vocbase_s*, 
                          std::string const&,
                          Restrictions const&);

        ~CollectionExport ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

        void run (uint64_t, size_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

        triagens::arango::CollectionGuard*           _guard;
        struct TRI_document_collection_t*            _document;
        triagens::arango::DocumentDitch*             _ditch;
        std::string const                            _name;
        triagens::arango::CollectionNameResolver     _resolver;
        Restrictions                                 _restrictions;
        std::vector<void const*>*                    _documents;
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
