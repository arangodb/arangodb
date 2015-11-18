////////////////////////////////////////////////////////////////////////////////
/// @brief read-accesses internals of a document
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
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_DOCUMENT_ACCESSOR_H
#define ARANGODB_VOC_BASE_DOCUMENT_ACCESSOR_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"
#include "VocBase/document-collection.h"
#include "VocBase/shape-accessor.h"
#include "VocBase/shaped-json.h"
#include "VocBase/Shaper.h"

class TRI_doc_mptr_t;

namespace triagens {
  namespace arango {
    class CollectionNameResolver;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  DocumentAccessor
// -----------------------------------------------------------------------------

class DocumentAccessor {

  public:

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

    DocumentAccessor (DocumentAccessor const&);
    DocumentAccessor& operator= (DocumentAccessor const&);

    DocumentAccessor (triagens::arango::CollectionNameResolver const* resolver,
                      TRI_document_collection_t* document,
                      TRI_doc_mptr_t const* mptr);

    ~DocumentAccessor ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

  public:

    bool hasKey (std::string const& attribute) const;

    bool isObject () const;

    bool isArray () const;

    size_t length () const;

    DocumentAccessor& get (char const* name, size_t nameLength);

    DocumentAccessor& get (std::string const& name);
    
    DocumentAccessor& at (int64_t index);
    
    triagens::basics::Json toJson (); 

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

  private:

    void setToNull ();

    void lookupJsonAttribute (char const* name, size_t nameLength);

    void lookupDocumentAttribute (char const* name, size_t nameLength);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

  private:
    
    triagens::arango::CollectionNameResolver const* _resolver;

    TRI_document_collection_t* _document;
    
    TRI_doc_mptr_t const* _mptr;

    std::unique_ptr<TRI_json_t> _json; // the JSON that we own

    TRI_json_t* _current; // the JSON that we point to

};

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
