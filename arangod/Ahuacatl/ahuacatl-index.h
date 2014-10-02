////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, index access
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AHUACATL_AHUACATL__INDEX_H
#define ARANGODB_AHUACATL_AHUACATL__INDEX_H 1

#include "Basics/Common.h"
#include "Basics/vector.h"

#include "VocBase/index.h"

struct TRI_aql_context_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

typedef struct TRI_aql_index_s {
  TRI_index_t*          _idx;
  TRI_vector_pointer_t* _fieldAccesses;
}
TRI_aql_index_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

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

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
