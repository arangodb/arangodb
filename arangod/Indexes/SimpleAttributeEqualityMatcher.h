////////////////////////////////////////////////////////////////////////////////
/// @brief simple index attribute matcher
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

#ifndef ARANGODB_INDEXES_SIMPLE_ATTRIBUTE_EQUALITY_MATCHER_H
#define ARANGODB_INDEXES_SIMPLE_ATTRIBUTE_EQUALITY_MATCHER_H 1

#include "Basics/Common.h"
#include "Basics/AttributeNameParser.h"

namespace triagens {
  namespace aql { 
    class Ast;
    struct AstNode;
    struct Variable; 
  }

  namespace arango {
    class Index;

// -----------------------------------------------------------------------------
// --SECTION--                              class SimpleAttributeEqualityMatcher
// -----------------------------------------------------------------------------

    class SimpleAttributeEqualityMatcher {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        SimpleAttributeEqualityMatcher (std::vector<std::vector<triagens::basics::AttributeName>> const&);
         
// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief match a single of the attributes
/// this is used for the primary index and the edge index
////////////////////////////////////////////////////////////////////////////////
        
        bool matchOne (triagens::arango::Index const*,
                       triagens::aql::AstNode const*,
                       triagens::aql::Variable const*,
                       size_t,
                       size_t&,
                       double&);

////////////////////////////////////////////////////////////////////////////////
/// @brief match all of the attributes, in any order
/// this is used for the hash index
////////////////////////////////////////////////////////////////////////////////
        
        bool matchAll (triagens::arango::Index const*,
                       triagens::aql::AstNode const*,
                       triagens::aql::Variable const*,
                       size_t,
                       size_t&,
                       double&);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the condition parts that the index is responsible for
/// this is used for the primary index and the edge index
/// requires that a previous matchOne() returned true
/// the caller must not free the returned AstNode*, as it belongs to the ast
////////////////////////////////////////////////////////////////////////////////
        
        triagens::aql::AstNode* getOne (triagens::aql::Ast*,
                                        triagens::arango::Index const*,
                                        triagens::aql::AstNode const*,
                                        triagens::aql::Variable const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the condition parts that the index is responsible for
/// this is used for the hash index
/// requires that a previous matchAll() returned true
/// the caller must not free the returned AstNode*, as it belongs to the ast
////////////////////////////////////////////////////////////////////////////////
        
        triagens::aql::AstNode* getAll (triagens::aql::Ast*,
                                        triagens::arango::Index const*,
                                        triagens::aql::AstNode const*,
                                        triagens::aql::Variable const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief specialize the condition for the index
/// this is used for the primary index and the edge index
/// requires that a previous matchOne() returned true
////////////////////////////////////////////////////////////////////////////////
        
        triagens::aql::AstNode* specializeOne (triagens::arango::Index const*,
                                               triagens::aql::AstNode*,
                                               triagens::aql::Variable const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief specialize the condition for the index
/// this is used for the hash index
/// requires that a previous matchAll() returned true
////////////////////////////////////////////////////////////////////////////////
        
        triagens::aql::AstNode* specializeAll (triagens::arango::Index const*,
                                               triagens::aql::AstNode*,
                                               triagens::aql::Variable const*);

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the costs of using this index
/// costs returned are scaled from 0.0 to 1.0, with 0.0 being the lowest cost
////////////////////////////////////////////////////////////////////////////////

        void calculateIndexCosts (triagens::arango::Index const*,
                                  size_t,
                                  size_t&,
                                  double&) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the access fits
////////////////////////////////////////////////////////////////////////////////

        bool accessFitsIndex (triagens::arango::Index const*,
                              triagens::aql::AstNode const*,
                              triagens::aql::AstNode const*,
                              triagens::aql::AstNode const*,
                              triagens::aql::Variable const*);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
      
      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief array of attributes used for comparisons
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::vector<triagens::basics::AttributeName>> _attributes;

////////////////////////////////////////////////////////////////////////////////
/// @brief an internal map to mark which condition parts were useful and 
/// covered by the index
////////////////////////////////////////////////////////////////////////////////
          
        std::unordered_set<size_t> _found;

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
