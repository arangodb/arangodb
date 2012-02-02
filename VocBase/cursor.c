////////////////////////////////////////////////////////////////////////////////
/// @brief cursors and result sets
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

#include "cursor.h"

////////////////////////////////////////////////////////////////////////////////
/// @def TRI_RESULTSET_INIT_SIZE
///
/// initial data size (in bytes) in each result set
////////////////////////////////////////////////////////////////////////////////

#define TRI_RESULTSET_INIT_SIZE 256

////////////////////////////////////////////////////////////////////////////////
/// @def TRI_RESULTSET_GROWTH_FACTOR
///
/// percentual growth factor for data in result sets
/// if a new allocation has to be made, the result set will grow by at least 
/// this factor. The intention of this is to reduce the number of malloc calls 
////////////////////////////////////////////////////////////////////////////////

#define TRI_RESULTSET_GROWTH_FACTOR 0.1


// -----------------------------------------------------------------------------
// --SECTION--                                                       result sets
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare next element in a single result set
////////////////////////////////////////////////////////////////////////////////

TRI_resultset_data_t *NextSingleResultset(TRI_resultset_t *resultset) {
  TRI_resultset_data_t *current;

  assert(resultset->_type == RESULTSET_TYPE_SINGLE);
  
  if (resultset->_position == 0) {
    resultset->_current = resultset->_data;
  }

  current = resultset->_current;
  resultset->_current += sizeof(TRI_doc_mptr_t *);

  ++resultset->_position;

  assert(resultset->_position < resultset->_length);

  return current;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare next element in a multi result set
////////////////////////////////////////////////////////////////////////////////

TRI_resultset_data_t *NextMultiResultset(TRI_resultset_t *resultset) {
  TRI_resultset_data_t *current;
  TRI_resultset_element_multi_t *data;
  TRI_voc_size_t n;
  TRI_doc_mptr_t **documents;

  assert(resultset->_type == RESULTSET_TYPE_MULTI);
  
  if (resultset->_position == 0) {
    resultset->_current = resultset->_data;
  }
  current = resultset->_current;

  data = (TRI_resultset_element_multi_t *) resultset->_current;
  n = data->_num;
  documents = data->_documents;
  resultset->_current += sizeof(TRI_voc_size_t) + n * sizeof(TRI_doc_mptr_t *);

  ++resultset->_position;

  assert(resultset->_position < resultset->_length);
  
  return current;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free common data structures in the result set
////////////////////////////////////////////////////////////////////////////////

static void FreeResultset(TRI_resultset_t *resultset) {
  TRI_Free(resultset->_alias);

  TRI_Free(resultset->_data);
  
  // free result set itself
  TRI_Free(resultset);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free special data structures for single result sets
////////////////////////////////////////////////////////////////////////////////

static void FreeSingleResultset(TRI_resultset_t *resultset) {
  FreeResultset(resultset);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free special data structures for multi result sets
////////////////////////////////////////////////////////////////////////////////

static void FreeMultiResultset(TRI_resultset_t *resultset) {
  FreeResultset(resultset);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the data storage size of a result size
/// 
/// the data size will grow by at least TRI_RESULTSET_GROWTH_FACTOR 
////////////////////////////////////////////////////////////////////////////////

static bool IncreaseStorageResultset(TRI_resultset_t *resultset, size_t size) {
  TRI_resultset_data_t *data;
  size_t newSize = resultset->_storageAllocated + size;

  if (resultset->_data == 0) {
    // if result set is broken after a failed realloc, we must exit early
    return false;
  }

  // increase by at least TRI_RESULTSET_GROWTH_FACTOR
  if (newSize < (size_t) (resultset->_storageAllocated * TRI_RESULTSET_GROWTH_FACTOR)) {
    newSize = (size_t) resultset->_storageAllocated * TRI_RESULTSET_GROWTH_FACTOR;
  }

  // TODO: wrap realloc
  data = (TRI_resultset_data_t *) realloc(resultset->_data, newSize);
  if (data == 0) {
    return false;
  }

  resultset->_data = data;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a single document to a single result set
////////////////////////////////////////////////////////////////////////////////
  
bool TRI_AddDocumentSingleResultset(TRI_resultset_t *resultset, const TRI_doc_mptr_t *document) {
  size_t storageNeeded = sizeof(TRI_doc_mptr_t *);

  assert(resultset->_type == RESULTSET_TYPE_SINGLE);

  if (resultset->_storageUsed + storageNeeded < resultset->_storageAllocated) {
    if (IncreaseStorageResultset(resultset, storageNeeded)) {
      // could not get any more memory => error
      return false;
    }
  }

  memcpy(resultset->_data + resultset->_storageUsed, document, sizeof(TRI_doc_mptr_t *));
  resultset->_storageUsed += storageNeeded;
 
  ++resultset->_length;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add multiple documents to a multiple result set
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddDocumentsMultiResultset(TRI_resultset_t *resultset, const TRI_voc_size_t n, 
                                    const TRI_doc_mptr_t **documents) {
  size_t storageNeeded = sizeof(TRI_voc_size_t) + n * sizeof(TRI_doc_mptr_t *);
  
  assert(resultset->_type == RESULTSET_TYPE_MULTI);
   
  if (resultset->_storageUsed + storageNeeded < resultset->_storageAllocated) {
    if (IncreaseStorageResultset(resultset, storageNeeded)) {
      // could not get any more memory => error
      return false;
    }
  }
  
  memcpy(resultset->_data + resultset->_storageUsed, &n, sizeof(TRI_voc_size_t));
  resultset->_storageUsed += sizeof(TRI_voc_size_t);

  memcpy(resultset->_data + resultset->_storageUsed, documents, n);
  resultset->_storageUsed += n * sizeof(TRI_doc_mptr_t *);
  
  ++resultset->_length;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new result set
////////////////////////////////////////////////////////////////////////////////

TRI_resultset_t *TRI_CreateResultset(const TRI_resultset_type_e type, const char *alias) {
  TRI_resultset_t *resultset = (TRI_resultset_t *) TRI_Allocate(sizeof(TRI_resultset_t));

  if (resultset == 0) {
    return 0;
  }

  resultset->_alias = TRI_DuplicateString(alias);  
  if (resultset->_alias == 0) {
    TRI_Free(resultset);
    return 0;
  }

  // init storage
  resultset->_data = (TRI_resultset_data_t *) 
    TRI_Allocate(TRI_RESULTSET_INIT_SIZE * sizeof(TRI_resultset_data_t *));

  if (resultset->_data == 0) {
    TRI_Free(resultset->_alias);
    TRI_Free(resultset);
    return 0;
  }

  resultset->_storageAllocated = TRI_RESULTSET_INIT_SIZE;
  resultset->_storageUsed = 0;

  resultset->_type = type;
  resultset->_length = 0;
  resultset->_position = 0;

  // setup function pointers
  if (type == RESULTSET_TYPE_SINGLE) { 
    resultset->next = NextSingleResultset;
    resultset->free = FreeSingleResultset;
  } 
  else if (type == RESULTSET_TYPE_MULTI) {
    resultset->next = NextMultiResultset;
    resultset->free = FreeMultiResultset;
  }
  else {
    assert(false);
  }

  return resultset;
}


// -----------------------------------------------------------------------------
// --SECTION--                                                           cursors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief free a cursor
////////////////////////////////////////////////////////////////////////////////

static void FreeCursor(TRI_cursor_t *cursor) {
  size_t i = cursor->_resultsets._length;

  // free individual result sets (backwards, to save memmove calls in vector implementation
  if (i > 0) {
    while (true) {
      i--;
      FreeResultset((TRI_resultset_t *) cursor->_resultsets._buffer[i]);
      if (i == 0) {
        break;
      }
    }
  }

  // free results vector
  TRI_DestroyVectorPointer(&cursor->_currentData->_data);
  TRI_Free(cursor->_currentData);
  
  // free result sets vector
  TRI_DestroyVectorPointer(&cursor->_resultsets);

  // free cursor itself
  TRI_Free(cursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if the cursor has more elements
////////////////////////////////////////////////////////////////////////////////

static inline bool HasNextCursor(const TRI_cursor_t *cursor) {
  return (cursor->_position < cursor->_length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns next element for the cursor
////////////////////////////////////////////////////////////////////////////////

TRI_resultset_row_t* NextCursor(TRI_cursor_t *cursor) {
  size_t i;
  TRI_resultset_data_t *data;
  TRI_resultset_t *resultset;

  if (!HasNextCursor(cursor)) {
    return 0;
  }

  // iterate over all result sets in cursor
  for (i = 0; i < cursor->_resultsets._length; i++) {
    resultset = (TRI_resultset_t *) cursor->_resultsets._buffer[i];

    // set data pointer for result set
    data = resultset->next(resultset);
    cursor->_currentData->_data._buffer[i] = data;
  }

  ++cursor->_position;
  return cursor->_currentData;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a result set to a cursor
////////////////////////////////////////////////////////////////////////////////

static void AddResultsetCursor(TRI_cursor_t *cursor, const TRI_resultset_t *resultset) {
  TRI_PushBackVectorPointer(&cursor->_resultsets, (TRI_resultset_t *) resultset);

  // start with 0 pointer
  TRI_PushBackVectorPointer(&cursor->_currentData->_data, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new cursor
////////////////////////////////////////////////////////////////////////////////

TRI_cursor_t *TRI_CreateCursor(TRI_rc_context_t *context, TRI_qry_select_t *select) {
  TRI_cursor_t *cursor = (TRI_cursor_t *) TRI_Allocate(sizeof(TRI_cursor_t));

  if (cursor == 0) {
    return 0;
  }
  
  cursor->_currentData = (TRI_resultset_row_t *) TRI_Allocate(sizeof(TRI_resultset_row_t));
  if (cursor->_currentData == 0) {
    TRI_Free(cursor);
    return 0;
  }
  
  // init row result sets vector
  TRI_InitVectorPointer(&cursor->_currentData->_data);

  // init result sets vector
  TRI_InitVectorPointer(&cursor->_resultsets);

  // store context
  cursor->_context = context;
  cursor->_select = select;

  // setup function pointers
  cursor->next = NextCursor;
  cursor->hasNext = HasNextCursor;
  cursor->free = FreeCursor;
  cursor->addResultset = AddResultsetCursor;
  
  // init positional data
  cursor->_length = 0;
  cursor->_position = 0;

  return cursor;
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

