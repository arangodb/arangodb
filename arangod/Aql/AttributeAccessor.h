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
#include "Utils/AqlTransaction.h"

struct TRI_document_collection_t;

namespace triagens {
  namespace aql {
    struct Variable;

////////////////////////////////////////////////////////////////////////////////
/// @brief AttributeAccessor
////////////////////////////////////////////////////////////////////////////////

    class AttributeAccessor {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////
      
        AttributeAccessor (char const*, 
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
                      std::vector<TRI_document_collection_t const*>&,
                      std::vector<AqlValue>&,
                      size_t,
                      std::vector<Variable*> const&,
                      std::vector<RegisterId> const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the attribute name
////////////////////////////////////////////////////////////////////////////////

        char const* _name;

////////////////////////////////////////////////////////////////////////////////
/// @brief the accessed variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _variable;

////////////////////////////////////////////////////////////////////////////////
/// @brief buffer for temporary strings
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::StringBuffer _buffer;

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
