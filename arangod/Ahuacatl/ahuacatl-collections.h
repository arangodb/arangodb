////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, collections
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

#ifndef ARANGODB_AHUACATL_AHUACATL__COLLECTIONS_H
#define ARANGODB_AHUACATL_AHUACATL__COLLECTIONS_H 1

#include "Basics/Common.h"
#include "Basics/vector.h"

#include "VocBase/vocbase.h"

#include "Ahuacatl/ahuacatl-index.h"
#include "Ahuacatl/ahuacatl-scope.h"

struct TRI_aql_context_s;
struct TRI_vector_pointer_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                           defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief max number of collections usable in a query
////////////////////////////////////////////////////////////////////////////////

#define AQL_MAX_COLLECTIONS  32

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief a collection container
///
/// one container is used for each collection referenced in a query
/// if a collection is used multiple times in the query, there is still only
/// one container for it
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_collection_s {
  TRI_vocbase_col_t*           _collection; // this might be nullptr !
  char*                        _name;
  struct TRI_vector_pointer_s* _availableIndexes;
}
TRI_aql_collection_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection hint container
///
/// a collection hint container is used to attach access information to
/// collections used in a query
/// if a collection is used multiple times in the query, each instance gets its
/// own hint container
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_aql_collection_hint_s {
  TRI_vector_pointer_t* _ranges;
  TRI_aql_index_t*      _index;
  TRI_aql_collection_t* _collection;
  TRI_aql_limit_t       _limit;
  char*                 _variableName;
}
TRI_aql_collection_hint_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the JSON representation of a collection hint
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_GetJsonCollectionHintAql (TRI_aql_collection_hint_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection hint
////////////////////////////////////////////////////////////////////////////////

TRI_aql_collection_hint_t* TRI_CreateCollectionHintAql (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a collection hint
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollectionHintAql (TRI_aql_collection_hint_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollectionAql (TRI_aql_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a collection in the internal vector
////////////////////////////////////////////////////////////////////////////////

TRI_aql_collection_t* TRI_GetCollectionAql (const struct TRI_aql_context_s*,
                                            const char*);

////////////////////////////////////////////////////////////////////////////////
/// @brief init all collections
////////////////////////////////////////////////////////////////////////////////

bool TRI_SetupCollectionsAql (struct TRI_aql_context_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection name to the list of collections used
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddCollectionAql (struct TRI_aql_context_s*,
                           const char*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get available indexes of a collection
////////////////////////////////////////////////////////////////////////////////

struct TRI_vector_pointer_s* TRI_GetIndexesCollectionAql (struct TRI_aql_context_s*,
                                                          TRI_aql_collection_t*);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
