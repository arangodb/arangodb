////////////////////////////////////////////////////////////////////////////////
/// @brief AQL, specialized attribute accessor for expressions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_ATTRIBUTE_ACCESSOR_H
#define ARANGODB_AQL_ATTRIBUTE_ACCESSOR_H 1

#include "Basics/Common.h"
#include "Aql/AqlValue.h"
#include "Aql/types.h"
#include "Basics/StringBuffer.h"
#include "ShapedJson/shaped-json.h"
#include "Utils/AqlTransaction.h"

struct TRI_document_collection_t;

namespace triagens {
  namespace aql {

    class AqlItemBlock;
    struct Variable;

////////////////////////////////////////////////////////////////////////////////
/// @brief AttributeAccessor
////////////////////////////////////////////////////////////////////////////////

    class AttributeAccessor {

        enum AttributeType {
          ATTRIBUTE_TYPE_KEY,
          ATTRIBUTE_TYPE_REV,
          ATTRIBUTE_TYPE_ID,
          ATTRIBUTE_TYPE_FROM,
          ATTRIBUTE_TYPE_TO,
          ATTRIBUTE_TYPE_REGULAR
        };

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////
      
        AttributeAccessor (std::vector<char const*> const&,
                           Variable const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~AttributeAccessor ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the accessor
////////////////////////////////////////////////////////////////////////////////

        AqlValue get (triagens::arango::AqlTransaction* trx,
                      AqlItemBlock const*,
                      size_t,
                      std::vector<Variable*> const&,
                      std::vector<RegisterId> const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _rev attribute from a ShapedJson marker
////////////////////////////////////////////////////////////////////////////////

        AqlValue extractRev (AqlValue const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _id attribute from a ShapedJson marker
////////////////////////////////////////////////////////////////////////////////

        AqlValue extractId (AqlValue const&,
                            triagens::arango::AqlTransaction*,
                            struct TRI_document_collection_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _from attribute from a ShapedJson marker
////////////////////////////////////////////////////////////////////////////////

        AqlValue extractFrom (AqlValue const&,
                              triagens::arango::AqlTransaction*);

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _to attribute from a ShapedJson marker
////////////////////////////////////////////////////////////////////////////////

        AqlValue extractTo (AqlValue const&,
                            triagens::arango::AqlTransaction*);

////////////////////////////////////////////////////////////////////////////////
/// @brief extract any other attribute from a ShapedJson marker
////////////////////////////////////////////////////////////////////////////////
        
        AqlValue extractRegular (AqlValue const&,
                                 triagens::arango::AqlTransaction*,
                                 struct TRI_document_collection_t const*);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the attribute names vector (e.g. [ "a", "b", "c" ] for a.b.c)
////////////////////////////////////////////////////////////////////////////////

        std::vector<char const*> const _attributeParts;

////////////////////////////////////////////////////////////////////////////////
/// @brief full attribute name (e.g. "a.b.c")
////////////////////////////////////////////////////////////////////////////////
        
        std::string _combinedName;

////////////////////////////////////////////////////////////////////////////////
/// @brief the accessed variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _variable;

////////////////////////////////////////////////////////////////////////////////
/// @brief buffer for temporary strings
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::StringBuffer _buffer;

////////////////////////////////////////////////////////////////////////////////
/// @brief shaper
////////////////////////////////////////////////////////////////////////////////

        TRI_shaper_t* _shaper;

////////////////////////////////////////////////////////////////////////////////
/// @brief attribute path id cache for shapes
////////////////////////////////////////////////////////////////////////////////

        TRI_shape_pid_t _pid;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection name lookup cache
////////////////////////////////////////////////////////////////////////////////

        struct {
          std::string value;
          TRI_voc_cid_t cid;
        }
        _nameCache;

////////////////////////////////////////////////////////////////////////////////
/// @brief attribute type (to save repeated strcmp calls)
////////////////////////////////////////////////////////////////////////////////

        AttributeType _attributeType;

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
