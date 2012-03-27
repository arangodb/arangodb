////////////////////////////////////////////////////////////////////////////////
/// @brief Collection locking in queries
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

#ifndef TRIAGENS_DURHAM_VOC_BASE_QUERY_LOCKS_H 
#define TRIAGENS_DURHAM_VOC_BASE_QUERY_LOCKS_H 1 

#include <BasicsC/logging.h>
#include <BasicsC/vector.h>

#include "VocBase/query-base.h"
#include "VocBase/query-cursor.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Collection lock
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_instance_lock_s {
  char*              _collectionName;
  TRI_vocbase_col_t* _collection;
}
TRI_query_instance_lock_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief free the locks for the query
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeLocksQueryInstance (TRI_vocbase_t* const, TRI_vector_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock all collections we already marked as being used
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCollectionsQueryInstance (TRI_vocbase_t* const, TRI_vector_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief set up the collection locks for the query
///
/// This will populate a vector with the collections to be read-locked for the
/// duration of the query. The order of collections in the vector is
/// deterministic (simple lexicographical order). The collections will be locked
/// in this exact same order. When the collections are read-unlocked at the end 
/// of the query, the collections will be read-unlocked in the reverse order.
/// This should avoid deadlocks when locking.
///
/// Each collection will only be put into the vector once, even if it is 
/// referenced using different aliases. 
///
/// This function will acquire a global read lock on the list of collections
/// and open them one by one. It will mark all collections are being read so
/// no other concurrent requests may delete/unload them.
///
/// The read usage markers must be removed when the query is over, otherwise
/// the collections are non-unloadable and non-deletable.
////////////////////////////////////////////////////////////////////////////////

bool TRI_LockCollectionsQueryInstance (TRI_vocbase_t* const,
                                       TRI_query_instance_t* const,
                                       TRI_vector_pointer_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks all collections of a query
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadLockCollectionsQueryInstance (TRI_query_instance_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief read-unlocks all collections of a query
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockCollectionsQueryInstance (TRI_query_instance_t* const); 

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a gc marker for all collections
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddCollectionsBarrierQueryInstance (TRI_query_instance_t* const,
                                             TRI_query_cursor_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief hand over locks from query instance to result cursor
///
/// This function is called when there is a select result with at least one row.
/// The query instance will be freed immediately after executing the select,
/// but the result set cursor might still be in use. The underlying collections
/// are still needed and are still read-locked. When the cursor usage is over,
/// the cursor is responsible for freeing the locks held.
////////////////////////////////////////////////////////////////////////////////

void TRI_HandoverLocksQueryInstance (TRI_query_instance_t* const,
                                     TRI_query_cursor_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a vector for holding collection locks
////////////////////////////////////////////////////////////////////////////////
  
TRI_vector_pointer_t* TRI_InitLocksQueryInstance (void);

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
