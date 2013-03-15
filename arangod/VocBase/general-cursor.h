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

#ifndef TRIAGENS_VOC_BASE_GENERAL_CURSOR_H
#define TRIAGENS_VOC_BASE_GENERAL_CURSOR_H 1

#include "BasicsC/vector.h"
#include "BasicsC/logging.h"

#include "VocBase/vocbase.h"
#include "VocBase/shadow-data.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                cursor result sets
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for cursor result set length and position
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_general_cursor_length_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for cursor result row data type
////////////////////////////////////////////////////////////////////////////////

typedef void* TRI_general_cursor_row_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor result set data type
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_general_cursor_result_s {
  void* _data;
  bool _freed;

  void (*freeData)(struct TRI_general_cursor_result_s*);
  TRI_general_cursor_row_t (*getAt)(struct TRI_general_cursor_result_s*, const TRI_general_cursor_length_t);
  TRI_general_cursor_length_t (*getLength)(struct TRI_general_cursor_result_s*);
}
TRI_general_cursor_result_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor result set
////////////////////////////////////////////////////////////////////////////////

TRI_general_cursor_result_t* TRI_CreateCursorResult (void*,
  void (*freeData)(TRI_general_cursor_result_t*),
  TRI_general_cursor_row_t (*getAt)(TRI_general_cursor_result_t*, const TRI_general_cursor_length_t),
  TRI_general_cursor_length_t (*getLength)(TRI_general_cursor_result_t*));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a cursor result set but do not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCursorResult (TRI_general_cursor_result_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a cursor result set
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCursorResult (TRI_general_cursor_result_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           cursors
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief result cursor
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_general_cursor_s {
  TRI_general_cursor_result_t* _result;
  TRI_general_cursor_length_t _length;
  TRI_general_cursor_length_t _currentRow;
  bool _hasCount;
  uint32_t _batchSize;

  TRI_mutex_t _lock;
  bool _deleted;

  void (*free)(struct TRI_general_cursor_s*);
  TRI_general_cursor_row_t (*next)(struct TRI_general_cursor_s* const);
  bool (*hasNext)(const struct TRI_general_cursor_s* const);
  bool (*hasCount)(const struct TRI_general_cursor_s* const);
  TRI_general_cursor_length_t (*getBatchSize)(const struct TRI_general_cursor_s* const);
}
TRI_general_cursor_t;

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

void TRI_FreeGeneralCursor (TRI_general_cursor_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor
////////////////////////////////////////////////////////////////////////////////

TRI_general_cursor_t* TRI_CreateGeneralCursor (TRI_general_cursor_result_t*,
                                               const bool,
                                               const TRI_general_cursor_length_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief exclusively lock a cursor
////////////////////////////////////////////////////////////////////////////////

void TRI_LockGeneralCursor (TRI_general_cursor_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock a general cursor
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockGeneralCursor (TRI_general_cursor_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a cursor based on its data pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShadowGeneralCursor (void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create shadow data store for cursors
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_store_t* TRI_CreateShadowsGeneralCursor (void);

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
