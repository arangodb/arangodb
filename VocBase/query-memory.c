////////////////////////////////////////////////////////////////////////////////
/// @brief query memory helper functions
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

#include "VocBase/query-memory.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new node for the AST and register it
////////////////////////////////////////////////////////////////////////////////

TRI_query_node_t* TRI_CreateNodeQuery (TRI_vector_pointer_t* memory,
                                       const TRI_query_node_type_e type) {
  // allocate memory
  TRI_query_node_t* node = (TRI_query_node_t *) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_query_node_t), false);
  if (!node) {
    return NULL; 
  }
  
  // keep track of memory location
  TRI_RegisterNodeQuery(memory, node);
  
  // set node type
  node->_type               = type;

  // set initial pointers to nil
  node->_value._stringValue = NULL;
  node->_lhs                = NULL;
  node->_rhs                = NULL;
  node->_next               = NULL;

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Register a string pointer in memory
////////////////////////////////////////////////////////////////////////////////

void TRI_RegisterStringQuery (TRI_vector_pointer_t* memory, 
                              const char* const string) {
  TRI_PushBackVectorPointer(memory, (void*) string);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Register a node pointer in memory
////////////////////////////////////////////////////////////////////////////////

void TRI_RegisterNodeQuery (TRI_vector_pointer_t* memory, 
                            const TRI_query_node_t* const node) {
  TRI_PushBackVectorPointer(memory, (void*) node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a node 
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeNodeQuery (TRI_query_node_t* node) {
  if (node) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, node);
    node = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a node vector
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeNodeVectorQuery (TRI_vector_pointer_t* const memory) {
  void* nodePtr;
  size_t i;

  i = memory->_length;
  // free all nodes in vector, starting at the end 
  // (prevents copying the remaining elements in vector)
  while (i > 0) {
    i--;
    nodePtr = TRI_RemoveVectorPointer(memory, i);
    TRI_FreeNodeQuery(nodePtr);
    if (i == 0) {
      break;
    }
  }
  TRI_DestroyVectorPointer(memory);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a string vector
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeStringVectorQuery (TRI_vector_pointer_t* const memory) {
  char* stringPtr;
  size_t i;

  i = memory->_length;
  // free all strings in vector, starting at the end 
  // (prevents copying the remaining elements in vector)
  while (i > 0) {
    i--;
    stringPtr = TRI_RemoveVectorPointer(memory, i);
    if (stringPtr) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, stringPtr);
      stringPtr = NULL;
    }
    if (i == 0) {
      break;
    }
  }
  TRI_DestroyVectorPointer(memory);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
