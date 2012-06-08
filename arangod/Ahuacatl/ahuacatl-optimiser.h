////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, optimiser
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

#ifndef TRIAGENS_DURHAM_AHUACATL_OPTIMISER_H
#define TRIAGENS_DURHAM_AHUACATL_OPTIMISER_H 1

#include <BasicsC/common.h>
#include <BasicsC/associative.h>
#include <BasicsC/hashes.h>
#include <BasicsC/json-utilities.h>
#include <BasicsC/logging.h>
#include <BasicsC/strings.h>
#include <BasicsC/string-buffer.h>
#include <BasicsC/vector.h>

#include "Ahuacatl/ahuacatl-access-optimiser.h"
#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-codegen.h"
#include "Ahuacatl/ahuacatl-log.h"
#include "Ahuacatl/ahuacatl-scope.h"
#include "Ahuacatl/ahuacatl-tree-walker.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief scope type used during optimisation
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_optimiser_scope_s {
  char* _variableName;
  TRI_aql_node_t* _node;
  TRI_vector_pointer_t* _ranges;
  TRI_aql_scope_e _type;
}
TRI_aql_optimiser_scope_t;

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
/// @brief optimise the AST
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_OptimiseAql (TRI_aql_context_t* const, 
                                 TRI_aql_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
