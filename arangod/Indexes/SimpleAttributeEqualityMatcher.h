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
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Variable.h"
#include "Indexes/Index.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                              class SimpleAttributeEqualityMatcher
// -----------------------------------------------------------------------------

    class SimpleAttributeEqualityMatcher {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        SimpleAttributeEqualityMatcher (std::vector<std::vector<std::string>> const& attributes)
          : _attributes(attributes) {
        }
         
// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief match a single of the attributes
////////////////////////////////////////////////////////////////////////////////
        
        inline bool matchOne (triagens::arango::Index const* index,
                              triagens::aql::AstNode const* node,
                              triagens::aql::Variable const* reference,
                              double& estimatedCost) const {
          std::unordered_set<size_t> found;

          for (size_t i = 0; i < node->numMembers(); ++i) {
            auto op = node->getMember(i);
            if (op->type == triagens::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
              TRI_ASSERT(op->numMembers() == 2);
              if (accessFitsIndex(index, op->getMember(0), op->getMember(1), reference, found) ||
                  accessFitsIndex(index, op->getMember(1), op->getMember(0), reference, found)) {
                return true;
              }
            }
            else if (op->type == triagens::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
              TRI_ASSERT(op->numMembers() == 2);
              if (accessFitsIndex(index, op->getMember(0), op->getMember(1), reference, found)) {
                return true;
              }
            }
          }

          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief match all of the attributes, in any order
////////////////////////////////////////////////////////////////////////////////
        
        inline bool matchAll (triagens::arango::Index const* index,
                              triagens::aql::AstNode const* node,
                              triagens::aql::Variable const* reference,
                              double& estimatedCost) const {
          std::unordered_set<size_t> found;

          for (size_t i = 0; i < node->numMembers(); ++i) {
            auto op = node->getMember(i);
            if (op->type == triagens::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
              TRI_ASSERT(op->numMembers() == 2);
              if (accessFitsIndex(index, op->getMember(0), op->getMember(1), reference, found) ||
                  accessFitsIndex(index, op->getMember(1), op->getMember(0), reference, found)) {
               break;
              }
            }
            else if (op->type == triagens::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
              TRI_ASSERT(op->numMembers() == 2);
              if (accessFitsIndex(index, op->getMember(0), op->getMember(1), reference, found)) {
                break;
              }
            }
          }

          return (found.size() == _attributes.size());
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the access fits
////////////////////////////////////////////////////////////////////////////////

        bool accessFitsIndex (triagens::arango::Index const* index,
                              triagens::aql::AstNode const* access,
                              triagens::aql::AstNode const* other,
                              triagens::aql::Variable const* reference,
                              std::unordered_set<size_t>& found) const {
          if (index->type() == triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX || 
              index->type() == triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
            auto pathBasedIndex = static_cast<triagens::arango::PathBasedIndex const*>(index);
            bool const sparse = pathBasedIndex->sparse();

            if (sparse) {
              if (! other->isConstant()) {
                return false;
              }
              if (other->isNullValue()) {
                return false;
              }
            }
          }

          // test if the reference variable is contained on both side of the expression
          std::unordered_set<aql::Variable const*> variables;
          triagens::aql::Ast::getReferencedVariables(other, variables);
          if (variables.find(reference) != variables.end()) {
            // yes. then we cannot use an index here
            return false;
          }

          if (access->type != triagens::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
            return false;
          }
        
          std::vector<std::string> fieldNames;
          while (access->type == triagens::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
            fieldNames.emplace_back(std::string(access->getStringValue(), access->getStringLength()));
            access = access->getMember(0);
          }

          if (access->type != triagens::aql::NODE_TYPE_REFERENCE) {
            return false;
          }

          if (access->getData() != reference) {
            // this access is not referencing this collection
            return false;
          }
    
          for (size_t i = 0; i < _attributes.size(); ++i) {
            if (_attributes[i].size() != fieldNames.size()) {
              // attribute path length differs
              continue;
            }

            bool match = true;
            for (size_t j = 0; j < _attributes[i].size(); ++j) {
              if (_attributes[i][j] != fieldNames[j]) {
                match = false;
                break;
              }
            }

            if (match) {
              // mark ith attribute as being covered
              found.emplace(i);
              return true;
            }
          }

          return false;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
      
      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief array of attributes used for comparisons
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::vector<std::string>> _attributes;

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
