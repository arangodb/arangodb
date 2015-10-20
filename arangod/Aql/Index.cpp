////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, Index
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
/// @author Michael Hackstein
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Index.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Aql/Variable.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                                  methods of Index
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the index internals
////////////////////////////////////////////////////////////////////////////////
      
triagens::arango::Index* Index::getInternals () const {
  if (internals == nullptr) {
    TRI_PrintBacktrace();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "accessing undefined index internals");
  }
  return internals; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the index internals
////////////////////////////////////////////////////////////////////////////////
      
void Index::setInternals (triagens::arango::Index* idx) {
  TRI_ASSERT(internals == nullptr);
  internals = idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether or not the index supports the filter condition
/// and calculate the filter costs and number of items
////////////////////////////////////////////////////////////////////////////////
      
bool Index::supportsFilterCondition (triagens::aql::AstNode const* node,
                                     triagens::aql::Variable const* reference,
                                     size_t itemsInIndex,
                                     size_t& estimatedItems,
                                     double& estimatedCost) const {
  return getInternals()->supportsFilterCondition(node, reference, itemsInIndex, estimatedItems, estimatedCost);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether or not the index supports the sort condition
/// and calculate the sort costs
////////////////////////////////////////////////////////////////////////////////
      
bool Index::supportsSortCondition (triagens::aql::SortCondition const* sortCondition,
                                   triagens::aql::Variable const* reference,
                                   size_t itemsInIndex,
                                   double& estimatedCost) const {
  return getInternals()->supportsSortCondition(sortCondition, reference, itemsInIndex, estimatedCost);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an iterator for the index
////////////////////////////////////////////////////////////////////////////////
      
triagens::arango::IndexIterator* Index::getIterator (arango::IndexIteratorContext* context, 
                                                     triagens::aql::Ast* ast,
                                                     triagens::aql::AstNode const* condition,
                                                     triagens::aql::Variable const* reference,
                                                     bool reverse) const {
  return getInternals()->iteratorForCondition(context, ast, condition, reference, reverse);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief specialize the condition for the index
/// this will remove all nodes from the condition that the index cannot
/// handle
////////////////////////////////////////////////////////////////////////////////

triagens::aql::AstNode* Index::specializeCondition (triagens::aql::AstNode* node,
                                                    triagens::aql::Variable const* reference) const {
  return getInternals()->specializeCondition(node, reference);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the index to an output stream
////////////////////////////////////////////////////////////////////////////////
     
std::ostream& operator<< (std::ostream& stream,
                          triagens::aql::Index const* index) {
  stream << index->getInternals()->context();
  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the index to an output stream
////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<< (std::ostream& stream,
                          triagens::aql::Index const& index) {
  stream << index.getInternals()->context();
  return stream;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

