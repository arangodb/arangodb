////////////////////////////////////////////////////////////////////////////////
/// @brief general cursors
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

#include "BasicsC/logging.h"

#include "VocBase/general-cursor.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                cursor result sets
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor result set
////////////////////////////////////////////////////////////////////////////////

TRI_general_cursor_result_t* TRI_CreateCursorResult (void* data,
  void (*freeData)(TRI_general_cursor_result_t*),
  TRI_general_cursor_row_t (*getAt)(TRI_general_cursor_result_t*, const TRI_general_cursor_length_t),
  TRI_general_cursor_length_t (*getLength)(TRI_general_cursor_result_t*)) {

  TRI_general_cursor_result_t* result;

  if (data == NULL) {
    return NULL;
  }

  result = (TRI_general_cursor_result_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_general_cursor_result_t), false);
  if (result == NULL) {
    return NULL;
  }

  result->_data = data;
  result->_freed = false;

  result->freeData = freeData;
  result->getAt = getAt;
  result->getLength = getLength;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a cursor result set but do not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCursorResult (TRI_general_cursor_result_t* const result) {
  assert(result);

  if (! result->_freed) {
    result->freeData(result);
    result->_freed = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a cursor result set
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCursorResult (TRI_general_cursor_result_t* const result) {
  if (result) {
    TRI_DestroyCursorResult(result);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           cursors
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next element
////////////////////////////////////////////////////////////////////////////////

static inline TRI_general_cursor_row_t NextGeneralCursor (TRI_general_cursor_t* const cursor) {
  if (cursor->_currentRow < cursor->_length) {
    return cursor->_result->getAt(cursor->_result, cursor->_currentRow++);
  }

  if (! cursor->_result->_freed) {
    cursor->_result->_freed = true;
    cursor->_result->freeData(cursor->_result);
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
////////////////////////////////////////////////////////////////////////////////

static inline bool HasNextGeneralCursor (const TRI_general_cursor_t* const cursor) {
  return cursor->_currentRow < cursor->_length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns if the count flag is set for the cursor
////////////////////////////////////////////////////////////////////////////////

static inline bool HasCountGeneralCursor (const TRI_general_cursor_t* const cursor) {
  return cursor->_hasCount;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the maximum number of results per transfer
////////////////////////////////////////////////////////////////////////////////

static inline TRI_general_cursor_length_t GetBatchSizeGeneralCursor (const TRI_general_cursor_t* const cursor) {
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

void TRI_FreeGeneralCursor (TRI_general_cursor_t* cursor) {
  if (cursor->_deleted) {
    return;
  }

  cursor->_deleted = true;

  TRI_FreeCursorResult(cursor->_result);

  TRI_DestroyMutex(&cursor->_lock);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, cursor);

  LOG_TRACE("destroyed general cursor");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor
////////////////////////////////////////////////////////////////////////////////

TRI_general_cursor_t* TRI_CreateGeneralCursor (TRI_general_cursor_result_t* result,
                                               const bool doCount,
                                               const TRI_general_cursor_length_t batchSize) {
  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_general_cursor_t), false);
  if (cursor == NULL) {
    return NULL;
  }

  cursor->_result = result;
  cursor->_currentRow = 0;
  cursor->_length = result->getLength(result);

  cursor->_hasCount = doCount;
  cursor->_batchSize = batchSize;
  cursor->_deleted = false;

  cursor->next = NextGeneralCursor;
  cursor->hasNext = HasNextGeneralCursor;
  cursor->hasCount = HasCountGeneralCursor;
  cursor->getBatchSize = GetBatchSizeGeneralCursor;
  cursor->free = TRI_FreeGeneralCursor;

  TRI_InitMutex(&cursor->_lock);

  LOG_TRACE("created general cursor");

  return cursor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exclusively lock a general cursor
////////////////////////////////////////////////////////////////////////////////

void TRI_LockGeneralCursor (TRI_general_cursor_t* const cursor) {
  TRI_LockMutex(&cursor->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock a general cursor
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockGeneralCursor (TRI_general_cursor_t* const cursor) {
  assert(cursor);

  TRI_UnlockMutex(&cursor->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a cursor based on its shadow data pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShadowGeneralCursor (void* data) {
  TRI_general_cursor_t* cursor = (TRI_general_cursor_t*) data;

  TRI_FreeGeneralCursor(cursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create shadow data store for cursors
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_store_t* TRI_CreateShadowsGeneralCursor (void) {
  return TRI_CreateShadowStore(&TRI_FreeShadowGeneralCursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
