////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, general AST walking
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Ahuacatl/ahuacatl-tree-walker.h"

// -----------------------------------------------------------------------------
// --SECTION--                                            modifiying tree walker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ModifyNode (TRI_aql_modify_tree_walker_t* const walker,
                                   TRI_aql_node_t* node) {
  TRI_aql_node_t* result = NULL;

  assert(walker);

  while (node) {
    size_t i;
    size_t n = node->_members._length;

    // process members first
    for (i = 0; i < n; ++i) {
      TRI_aql_node_t* member = (TRI_aql_node_t*) node->_members._buffer[i];

      if (!member) {
        continue;
      }

      node->_members._buffer[i] = ModifyNode(walker, member);
    }
    
    // the visit function might set it to NULL
    node = walker->visitFunc(walker->_data, node);
    if (!result) {
      result = node;
    }

    if (node) {
      node = node->_next;
    }
  }
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @} 
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a tree walker
////////////////////////////////////////////////////////////////////////////////

TRI_aql_modify_tree_walker_t* TRI_CreateModifyTreeWalkerAql (void* data, 
                                                             TRI_aql_node_modify_visit_f f) {
  TRI_aql_modify_tree_walker_t* walker;

  assert(f);

  walker = (TRI_aql_modify_tree_walker_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_modify_tree_walker_t), false);
  if (!walker) {
    return NULL;
  }

  walker->_data = data;
  walker->visitFunc = f;

  return walker;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a tree walker
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeModifyTreeWalkerAql (TRI_aql_modify_tree_walker_t* const walker) {
  assert(walker);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, walker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the tree walk
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_ModifyWalkTreeAql (TRI_aql_modify_tree_walker_t* const walker, 
                                       TRI_aql_node_t* node) {
  assert(walker);

  return ModifyNode(walker, node);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 const tree walker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

static void ProcessNode (TRI_aql_const_tree_walker_t* const walker,
                         const TRI_aql_node_t* const data) {
  TRI_aql_node_t* node = (TRI_aql_node_t*) data;
  assert(walker);

  if (walker->preRecurseFunc) {
    walker->preRecurseFunc(walker->_data);
  }

  while (node) {
    size_t i;
    size_t n = node->_members._length;

    if (walker->preVisitFunc) {
      walker->preVisitFunc(walker->_data, node);
    }

    // process members first
    for (i = 0; i < n; ++i) {
      TRI_aql_node_t* member = (TRI_aql_node_t*) node->_members._buffer[i];

      if (!member) {
        continue;
      }

      ProcessNode(walker, member);
    }
   
    if (walker->postVisitFunc) { 
      walker->postVisitFunc(walker->_data, node);
    }

    if (node) {
      node = node->_next;
    }
  }
  
  if (walker->postRecurseFunc) {
    walker->postRecurseFunc(walker->_data);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a tree walker
////////////////////////////////////////////////////////////////////////////////

TRI_aql_const_tree_walker_t* TRI_CreateConstTreeWalkerAql (void* data, 
                                                           TRI_aql_tree_const_visit_f preVisit,
                                                           TRI_aql_tree_const_visit_f postVisit,
                                                           TRI_aql_tree_recurse_f preRecurse,
                                                           TRI_aql_tree_recurse_f postRecurse) {
  TRI_aql_const_tree_walker_t* walker;

  walker = (TRI_aql_const_tree_walker_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_const_tree_walker_t), false);
  if (!walker) {
    return NULL;
  }

  walker->_data = data;
  walker->preVisitFunc = preVisit;
  walker->postVisitFunc = postVisit;
  walker->preRecurseFunc = preRecurse;
  walker->postRecurseFunc = postRecurse;

  return walker;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a tree walker
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeConstTreeWalkerAql (TRI_aql_const_tree_walker_t* const walker) {
  assert(walker);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, walker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the tree walk
////////////////////////////////////////////////////////////////////////////////

void TRI_ConstWalkTreeAql (TRI_aql_const_tree_walker_t* const walker, 
                           const TRI_aql_node_t* const node) {
  assert(walker);

  ProcessNode(walker, node);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
