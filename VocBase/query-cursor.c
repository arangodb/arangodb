////////////////////////////////////////////////////////////////////////////////
/// @brief query cursors
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

#include "VocBase/query-cursor.h"
#include "VocBase/query-context.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the gc markers for all collections of a query
////////////////////////////////////////////////////////////////////////////////

static void RemoveCollectionsQueryCursor (TRI_query_cursor_t* cursor) {
  size_t i;

  for (i = 0; i < cursor->_containers._length; ++i) {
    TRI_barrier_t* ce = cursor->_containers._buffer[i];
    assert(ce);    
    TRI_FreeBarrier(ce);
  }

  cursor->_containers._length = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the data of a cursor 
///
/// This will release any locks on the underlying collection data and might be 
/// called before the actual cursor is disposed.
////////////////////////////////////////////////////////////////////////////////

static void FreeData (TRI_query_cursor_t* const cursor) {
  if (cursor->_deleted) {
    return;
  }

  cursor->_deleted = true;

  assert(cursor->_functionCode);
  TRI_Free(cursor->_functionCode);

  // free result set
  if (cursor->_result._selectResult) {
    cursor->_result._selectResult->free(cursor->_result._selectResult);
  }

  // free select
  RemoveCollectionsQueryCursor(cursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next element 
////////////////////////////////////////////////////////////////////////////////

static TRI_rc_result_t* NextQueryCursor (TRI_query_cursor_t* const cursor) {
  if (cursor->_currentRow < cursor->_length) {
    cursor->_result._dataPtr = 
      cursor->_result._selectResult->getAt(cursor->_result._selectResult, cursor->_currentRow++);

    return &cursor->_result;
  }

  FreeData(cursor);

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted 
////////////////////////////////////////////////////////////////////////////////

static bool HasNextQueryCursor (const TRI_query_cursor_t* const cursor) {
  return cursor->_currentRow < cursor->_length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns if the count flag is set for the cursor
////////////////////////////////////////////////////////////////////////////////

static bool HasCountQueryCursor (const TRI_query_cursor_t* const cursor) {
  return cursor->_hasCount;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the maximum number of results per transfer
////////////////////////////////////////////////////////////////////////////////

static uint32_t GetBatchSizeQueryCursor (const TRI_query_cursor_t* const cursor) {
  return cursor->_batchSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a cursor 
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryCursor (TRI_query_cursor_t* cursor) {
  FreeData(cursor);
  
  TRI_DestroyMutex(&cursor->_lock);
  TRI_DestroyVectorPointer(&cursor->_containers);

  TRI_Free(cursor);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor
////////////////////////////////////////////////////////////////////////////////

TRI_query_cursor_t* TRI_CreateQueryCursor (TRI_query_instance_t* const instance,
                                           const TRI_select_result_t* const selectResult,
                                           const bool doCount,
                                           const uint32_t batchSize) {
  TRI_query_cursor_t* cursor;

  assert(instance);
  
  cursor = TRI_Allocate(sizeof(TRI_query_cursor_t));
  if (!cursor) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }

  assert(instance->_query._select._functionCode);
  cursor->_functionCode = TRI_DuplicateString(instance->_query._select._functionCode);
  if (!cursor->_functionCode) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
    TRI_Free(cursor);
    return NULL;
  }

  cursor->_hasCount = doCount;
  cursor->_batchSize = batchSize;
  cursor->_deleted = false;
  cursor->_vocbase = instance->_template->_vocbase;
   
  cursor->_result._selectResult = (TRI_select_result_t*) selectResult;
  cursor->_result._dataPtr = NULL;
  
  cursor->next = NextQueryCursor;
  cursor->hasNext = HasNextQueryCursor;
  cursor->hasCount = HasCountQueryCursor;
  cursor->getBatchSize = GetBatchSizeQueryCursor;
  cursor->free = TRI_FreeQueryCursor;
  
  TRI_InitMutex(&cursor->_lock);
  TRI_InitVectorPointer(&cursor->_containers);

  return cursor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exclusively lock a query cursor
////////////////////////////////////////////////////////////////////////////////

void TRI_LockQueryCursor (TRI_query_cursor_t* const cursor) {
  TRI_LockMutex(&cursor->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock a query cursor
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockQueryCursor (TRI_query_cursor_t* const cursor) {
  assert(cursor);

  TRI_UnlockMutex(&cursor->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a cursor based on its shadow data pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShadowQueryCursor (void* data) {
  TRI_query_cursor_t* cursor = (TRI_query_cursor_t*) data; 

  TRI_FreeQueryCursor(cursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create shadow data store for cursors 
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_store_t* TRI_CreateShadowsQueryCursor (void) {
  return TRI_CreateShadowStore(&TRI_FreeShadowQueryCursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
