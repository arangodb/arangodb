////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, index access
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_AHUACATL_AHUACATL_INDEX_H
#define TRIAGENS_AHUACATL_AHUACATL_INDEX_H 1

#include "BasicsC/common.h"
#include "BasicsC/vector.h"

#include "VocBase/index.h"

#ifdef __cplusplus
extern "C" {
#endif

struct TRI_aql_context_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_index_s {
  TRI_index_t*          _idx;
  TRI_vector_pointer_t* _fieldAccesses;
}
TRI_aql_index_t;

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
/// @brief free an index structure
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeIndexAql (TRI_aql_index_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief determine which index to use for a specific for loop
////////////////////////////////////////////////////////////////////////////////

TRI_aql_index_t* TRI_DetermineIndexAql (struct TRI_aql_context_s* const,
                                        const TRI_vector_pointer_t* const,
                                        const char* const,
                                        const TRI_vector_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
