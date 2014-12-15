////////////////////////////////////////////////////////////////////////////////
/// @brief general cursors
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

#ifndef ARANGODB_VOC_BASE_GENERAL__CURSOR_H
#define ARANGODB_VOC_BASE_GENERAL__CURSOR_H 1

#include "Basics/Common.h"

#include "Basics/associative.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

#include <v8.h>

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_json_t;
struct TRI_shadow_store_s;
struct TRI_vocbase_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

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
  TRI_general_cursor_row_t (*getAt)(struct TRI_general_cursor_result_s const*, const TRI_general_cursor_length_t);
  TRI_general_cursor_length_t (*getLength)(struct TRI_general_cursor_result_s const*);
}
TRI_general_cursor_result_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor result set
////////////////////////////////////////////////////////////////////////////////

TRI_general_cursor_result_t* TRI_CreateCursorResult (void*,
  void (*freeData)(TRI_general_cursor_result_t*),
  TRI_general_cursor_row_t (*getAt)(TRI_general_cursor_result_t const*, const TRI_general_cursor_length_t),
  TRI_general_cursor_length_t (*getLength)(TRI_general_cursor_result_t const*));


////////////////////////////////////////////////////////////////////////////////
/// @brief create a result set
////////////////////////////////////////////////////////////////////////////////

TRI_general_cursor_result_t* TRI_CreateResultGeneralCursor (TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a result set
////////////////////////////////////////////////////////////////////////////////

TRI_general_cursor_result_t* TRI_CreateResultGeneralCursor (v8::Isolate* isolate,
                                                            v8::Handle<v8::Array> const);

// -----------------------------------------------------------------------------
// --SECTION--                                                      cursor store
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor store
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_general_cursor_store_s {
  TRI_spin_t                 _lock;
  TRI_associative_pointer_t  _ids; // ids
}
TRI_general_cursor_store_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                cursor result sets
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a cursor result set but do not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCursorResult (TRI_general_cursor_result_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a cursor result set
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCursorResult (TRI_general_cursor_result_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                           cursors
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief result cursor
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_general_cursor_s {
  struct TRI_vocbase_s*        _vocbase;
  TRI_general_cursor_store_t*  _store;

  TRI_general_cursor_result_t* _result;
  TRI_general_cursor_length_t  _length;
  TRI_general_cursor_length_t  _currentRow;
  uint32_t                     _batchSize;

  TRI_spin_t                   _lock;
  TRI_voc_tick_t               _id;

  struct {
    uint32_t                   _refCount;
    bool                       _isDeleted;
  }                            _usage;
  double                       _ttl;
  double                       _expires;

  struct TRI_json_t*           _extra;
  bool                         _hasCount;

  void (*free)(struct TRI_general_cursor_s*);
  TRI_general_cursor_row_t (*next)(struct TRI_general_cursor_s* const);
  bool (*hasNext)(const struct TRI_general_cursor_s* const);
  bool (*hasCount)(const struct TRI_general_cursor_s* const);
  TRI_general_cursor_length_t (*getBatchSize)(const struct TRI_general_cursor_s* const);
  struct TRI_json_t* (*getExtra)(const struct TRI_general_cursor_s* const);
}
TRI_general_cursor_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a cursor
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeGeneralCursor (TRI_general_cursor_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor
////////////////////////////////////////////////////////////////////////////////

TRI_general_cursor_t* TRI_CreateGeneralCursor (struct TRI_vocbase_s*,
                                               TRI_general_cursor_result_t*,
                                               const bool,
                                               const TRI_general_cursor_length_t,
                                               double,
                                               struct TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief exclusively lock a cursor
////////////////////////////////////////////////////////////////////////////////

void TRI_LockGeneralCursor (TRI_general_cursor_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock a general cursor
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockGeneralCursor (TRI_general_cursor_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief reference-count a general cursor
////////////////////////////////////////////////////////////////////////////////

TRI_general_cursor_t* TRI_UseGeneralCursor (TRI_general_cursor_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief de-reference-count a general cursor
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseGeneralCursor (TRI_general_cursor_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief mark as cursor as deleted
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropGeneralCursor (TRI_general_cursor_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cursor id
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_IdGeneralCursor (TRI_general_cursor_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cursor length
////////////////////////////////////////////////////////////////////////////////

size_t TRI_CountGeneralCursor (TRI_general_cursor_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief persist the cursor by setting a timeout
////////////////////////////////////////////////////////////////////////////////

void TRI_PersistGeneralCursor (struct TRI_vocbase_s*,
                               TRI_general_cursor_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a cursor by its id
////////////////////////////////////////////////////////////////////////////////

TRI_general_cursor_t* TRI_FindGeneralCursor (struct TRI_vocbase_s*,
                                             TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief mark as cursor as deleted
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveGeneralCursor (struct TRI_vocbase_s*,
                              TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief create shadow data store for cursors
////////////////////////////////////////////////////////////////////////////////

TRI_general_cursor_store_t* TRI_CreateStoreGeneralCursor (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief free the data store for cursors
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeStoreGeneralCursor (TRI_general_cursor_store_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief enumerate all cursors and remove them if
/// - their refcount is 0 and they are transient
/// - their refcount is 0 and they are expired
/// - the force flag is set
////////////////////////////////////////////////////////////////////////////////

void TRI_CleanupGeneralCursor (TRI_general_cursor_store_t*,
                               bool);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
