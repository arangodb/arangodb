////////////////////////////////////////////////////////////////////////////////
/// @brief index sort condition
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_SORT_CONDITION_H
#define ARANGODB_AQL_SORT_CONDITION_H 1

#include "Basics/Common.h"
#include "Aql/Variable.h"
#include "Basics/AttributeNameParser.h"

namespace triagens {
  namespace aql {
    struct AstNode;

// -----------------------------------------------------------------------------
// --SECTION--                                               class SortCondition
// -----------------------------------------------------------------------------

    class SortCondition {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        SortCondition () = delete;
        SortCondition (SortCondition const&) = delete;
        SortCondition& operator= (SortCondition const&) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the sort condition
////////////////////////////////////////////////////////////////////////////////

        explicit SortCondition (std::vector<std::pair<AstNode const*, bool>> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief create the sort condition
////////////////////////////////////////////////////////////////////////////////

        SortCondition (std::vector<std::pair<VariableId, bool>> const&, 
                       std::unordered_map<VariableId, AstNode const*> const&); 

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the sort condition
////////////////////////////////////////////////////////////////////////////////

        ~SortCondition ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the condition consists only of attribute accesses
////////////////////////////////////////////////////////////////////////////////
        
        inline bool isOnlyAttributeAccess () const {
          return _onlyAttributeAccess;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not all conditions have the same sort order
////////////////////////////////////////////////////////////////////////////////
        
        inline bool isUnidirectional () const {
          return _unidirectional;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not all sort directions are ascending
/// note that the return value of this function is only meaningful if the
/// sort is unidirectional
////////////////////////////////////////////////////////////////////////////////
        
        inline bool isAscending () const {
          TRI_ASSERT(isUnidirectional());
          return _ascending;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not all sort directions are descending
/// this is the reverse of isAscending()
////////////////////////////////////////////////////////////////////////////////
        
        inline bool isDescending () const {
          return ! isAscending();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not there are fields
////////////////////////////////////////////////////////////////////////////////

        inline bool isEmpty () const {
          return _fields.empty();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief number of attributes in condition
////////////////////////////////////////////////////////////////////////////////

        inline size_t numAttributes () const {
          return _fields.size();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of attributes in the sort condition covered
/// by the specified index fields
////////////////////////////////////////////////////////////////////////////////

        size_t isCoveredBy (Variable const*,
                            std::vector<std::vector<triagens::basics::AttributeName>> const&) const;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief sort expressions
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<AstNode const*, bool>> _expressions;

////////////////////////////////////////////////////////////////////////////////
/// @brief fields used in the sort conditions
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<Variable const*, std::vector<triagens::basics::AttributeName>>> _fields;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the sort is unidirectional
////////////////////////////////////////////////////////////////////////////////

        bool _unidirectional;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether the sort only consists of attribute accesses
////////////////////////////////////////////////////////////////////////////////

        bool _onlyAttributeAccess;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not all sorts are in ascending order. 
/// this is only meaningful if the sort is unidirectional
////////////////////////////////////////////////////////////////////////////////

        bool _ascending;
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
