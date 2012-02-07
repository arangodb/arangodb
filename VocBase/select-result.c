////////////////////////////////////////////////////////////////////////////////
/// @brief selects and select result sets
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

#include "VocBase/select-result.h"
#include "VocBase/query.h"


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Growth factor for select memory allocation
////////////////////////////////////////////////////////////////////////////////

#define TRI_SELECT_RESULT_GROWTH_FACTOR 1.5

////////////////////////////////////////////////////////////////////////////////
/// @brief Free memory allocated for dataparts
////////////////////////////////////////////////////////////////////////////////

static void FreeDataPart(TRI_select_datapart_t* datapart) {
  if (datapart->_alias != NULL) {
    TRI_Free(datapart->_alias);
  }
  TRI_Free(datapart);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new select datapart instance
////////////////////////////////////////////////////////////////////////////////

TRI_select_datapart_t* TRI_CreateDataPart(const char* alias, 
                                          const TRI_doc_collection_t* collection,
                                          const TRI_select_data_e type) {
  TRI_select_datapart_t* datapart;

  datapart = (TRI_select_datapart_t*) TRI_Allocate(sizeof(TRI_select_datapart_t));
  if (datapart == NULL) {
    return NULL;
  }

  datapart->_alias = TRI_DuplicateString(alias);
  datapart->_collection = (TRI_doc_collection_t*) collection;
  datapart->_type = type;
  datapart->free = FreeDataPart;

  return datapart;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free memory allocated for select result
////////////////////////////////////////////////////////////////////////////////

static void FreeSelectResult (TRI_select_result_t* _select) {
  TRI_select_datapart_t* datapart;
  size_t i;

  if (_select->_index._start != NULL) {
    TRI_Free(_select->_index._start);
  }

  if (_select->_documents._start != NULL) {
    TRI_Free(_select->_documents._start);
  }

  for (i = 0; i < _select->_dataParts->_length; i++) {
    datapart = (TRI_select_datapart_t*) _select->_dataParts->_buffer[i];
    datapart->free(datapart);
  }

  TRI_DestroyVectorPointer(_select->_dataParts);
  TRI_Free(_select->_dataParts);

  TRI_Free(_select);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Initialise a select result
////////////////////////////////////////////////////////////////////////////////

static void InitSelectResult (TRI_select_result_t* _select, 
                              TRI_vector_pointer_t* dataparts) {
  _select->_index._numAllocated = 0;
  _select->_index._numUsed = 0;
  _select->_index._start = 0;

  _select->_documents._bytesAllocated = 0;
  _select->_documents._bytesUsed = 0;
  _select->_documents._start = 0;
  _select->_documents._current = 0;
  
  _select->_dataParts = dataparts;

  _select->_numRows = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Increase storage size for select result index
////////////////////////////////////////////////////////////////////////////////

static bool IncreaseIndexStorageSelectResult (TRI_select_result_t* _select, 
                                              const size_t numNeeded) {
  void *start;
  size_t newSize;
 
  newSize = (size_t) _select->_index._numAllocated + numNeeded;
  if (newSize < 
    (size_t) (_select->_index._numAllocated * TRI_SELECT_RESULT_GROWTH_FACTOR)) {
    newSize = (size_t) (_select->_index._numAllocated * TRI_SELECT_RESULT_GROWTH_FACTOR);
  }

  start = realloc(_select->_index._start, newSize * sizeof(void *));
  if (start == 0) {
    return false;
  }
  _select->_index._numAllocated = newSize;
  
  _select->_index._start = start;
  _select->_index._current = (uint8_t *) start + 
                             (_select->_index._numUsed * sizeof(void *));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Increase storage size for select documents data
////////////////////////////////////////////////////////////////////////////////

static bool IncreaseDocumentsStorageSelectResult(TRI_select_result_t* _select, 
                                                 size_t bytesNeeded) {
  size_t newSize;
  void* start;

  newSize = (size_t) _select->_documents._bytesAllocated + bytesNeeded;
  if (newSize < (size_t) (_select->_documents._bytesAllocated * 1.5)) {
    newSize = (size_t) (_select->_documents._bytesAllocated * 1.5);
  }

  start = realloc(_select->_documents._start, newSize);
  if (start == 0) {
    return false;
  }

  if (start != _select->_documents._start && _select->_documents._start != 0) {
    // FIXME: adjust entries in index
    assert(false);
  }
  _select->_documents._bytesAllocated = newSize;

  _select->_documents._start = start;
  _select->_documents._current = (uint8_t *) start + _select->_documents._bytesUsed;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the required storage size for a document result
////////////////////////////////////////////////////////////////////////////////

static size_t GetDocumentSizeSelectResult (const TRI_select_result_t* _select, 
                                           const TRI_vector_pointer_t* documents) {
  TRI_vector_pointer_t* part;
  size_t i, total;

  total = 0;
  for (i = 0; i < documents->_length; i++) {
    total += sizeof(TRI_select_size_t);
    part = (TRI_vector_pointer_t*) documents->_buffer[i];
    total += sizeof(TRI_doc_mptr_t*) * part->_length;
  }

  return total;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Add a document to the result set
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddDocumentSelectResult (TRI_select_result_t* _select, 
                                  TRI_vector_pointer_t* documents) {
  TRI_select_size_t* numPtr;
  TRI_doc_mptr_t** dataPtr;
  void* startPtr;
  TRI_vector_pointer_t* part;
  size_t numNeeded; 
  size_t bytesNeeded;
  size_t i, j;

  numNeeded = documents->_length;
  if (_select->_index._numUsed + documents->_length > _select->_index._numAllocated) {
    if (!IncreaseIndexStorageSelectResult(_select, numNeeded)) {
      return false;
    } 
  }

  bytesNeeded = GetDocumentSizeSelectResult(_select, documents);
  if (_select->_documents._bytesUsed + bytesNeeded > _select->_documents._bytesAllocated) {
    if (!IncreaseDocumentsStorageSelectResult(_select, bytesNeeded)) {
      return false;
    } 
  }

  // store pointer to document
  startPtr = _select->_index._current;
  dataPtr = (TRI_doc_mptr_t**) startPtr; 
  *dataPtr++ = startPtr;
  _select->_index._current = (void*) dataPtr;
  _select->_index._numUsed++;

  // store document data
  startPtr = _select->_documents._current;
  numPtr = (TRI_select_size_t*) startPtr;
  for (i = 0; i < documents->_length; i++) {
    part = (TRI_vector_pointer_t*) documents->_buffer[i];
    *numPtr++ = part->_length;
    dataPtr = (TRI_doc_mptr_t**) numPtr;
    for (j = 0; j < part->_length; j++) {
      *dataPtr++ = (TRI_doc_mptr_t*) part->_buffer[j];
    }
    numPtr = (TRI_select_size_t*) dataPtr;
  }

  _select->_documents._bytesUsed += bytesNeeded;
  _select->_documents._current = numPtr;

  _select->_numRows++;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new select result
////////////////////////////////////////////////////////////////////////////////

TRI_select_result_t* TRI_CreateSelectResult (TRI_vector_pointer_t *dataparts) {
  TRI_select_result_t* result;

  result = (TRI_select_result_t*) TRI_Allocate(sizeof(TRI_select_result_t));
  if (!result) {
    return NULL;
  }
  InitSelectResult(result, dataparts);

  result->free = FreeSelectResult;

  return result;
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


