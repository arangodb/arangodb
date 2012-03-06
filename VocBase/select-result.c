////////////////////////////////////////////////////////////////////////////////
/// @brief SELECT result data structures and functionality 
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
#include "VocBase/query-base.h"


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Initial size for result index (in number of rows)
///
/// The index buffer will get an initial pre-allocation of this amount of rows
/// to save multiple calls to malloc() for the first few additions of rows. 
/// re-allocation is postponed until we have collected a few rows already. 
/// This will save realloc overhead for smaller result sets.
////////////////////////////////////////////////////////////////////////////////

#define INDEX_INIT_SIZE       32

////////////////////////////////////////////////////////////////////////////////
/// @brief Initial size for result data storage (in number of bytes)
///
/// The result data buffer will get an initial pre-allocation of this amount of 
/// bytes to save multiple calls to malloc() for the first few additions of 
/// results. re-allocation is postponed until we have collected a few rows 
/// already. This will save realloc overhead for smaller result sets.
////////////////////////////////////////////////////////////////////////////////

#define RESULT_INIT_SIZE      128

////////////////////////////////////////////////////////////////////////////////
/// @brief Growth factor for index memory allocation
///
/// For each re-allocation, the memory size will be increased by at least this
/// factor.
////////////////////////////////////////////////////////////////////////////////

#define INDEX_GROWTH_FACTOR   1.5

////////////////////////////////////////////////////////////////////////////////
/// @brief Growth factor for result data memory allocation
///
/// For each re-allocation, the memory size will be increased by at least this
/// factor.
////////////////////////////////////////////////////////////////////////////////

#define RESULT_GROWTH_FACTOR  1.5

////////////////////////////////////////////////////////////////////////////////
/// @brief Free memory allocated for dataparts
////////////////////////////////////////////////////////////////////////////////

static void FreeDataPart (TRI_select_datapart_t* datapart) {
  if (datapart->_alias) {
    TRI_Free(datapart->_alias);
  }

  TRI_Free(datapart);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new select datapart instance
////////////////////////////////////////////////////////////////////////////////

TRI_select_datapart_t* TRI_CreateDataPart(const char* alias, 
                                          const TRI_doc_collection_t* collection,
                                          const TRI_select_part_e type,
                                          const size_t extraDataSize) {
  TRI_select_datapart_t* datapart;
  
  if (extraDataSize) {
    assert(!collection);
  }
  if (collection) {
    assert(extraDataSize == 0);
  }

  datapart = (TRI_select_datapart_t*) TRI_Allocate(sizeof(TRI_select_datapart_t));
  if (!datapart) {
    return NULL;
  }

  datapart->_alias         = TRI_DuplicateString(alias);
  datapart->_collection    = (TRI_doc_collection_t*) collection;
  datapart->_type          = type;
  datapart->_extraDataSize = extraDataSize;

  datapart->free           = FreeDataPart;

  return datapart;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get document pointer for a specific row
////////////////////////////////////////////////////////////////////////////////

static TRI_sr_documents_t* GetSelectResult (const TRI_select_result_t* result, 
                                       const TRI_select_size_t rowNum) {

  TRI_sr_index_t* docPtr = (TRI_sr_index_t*) result->_index._start;
  return (TRI_sr_documents_t*) *(docPtr + rowNum);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get pointer to document index start
////////////////////////////////////////////////////////////////////////////////

static TRI_sr_index_t* FirstSelectResult (const TRI_select_result_t* result) {
  return result->_index._start;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get pointer to document index end
////////////////////////////////////////////////////////////////////////////////

static TRI_sr_index_t* LastSelectResult (const TRI_select_result_t* result) {
  TRI_sr_index_t* docPtr = (TRI_sr_index_t*) result->_index._start;
  return (TRI_sr_index_t*) (docPtr + result->_index._numUsed);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free memory allocated for select result
////////////////////////////////////////////////////////////////////////////////

static void FreeSelectResult (TRI_select_result_t* result) {
  TRI_select_datapart_t* datapart;
  size_t i;

  if (result->_index._start) {
    TRI_Free(result->_index._start);
  }

  if (result->_documents._start) {
    TRI_Free(result->_documents._start);
  }

  for (i = 0; i < result->_dataParts->_length; i++) {
    datapart = (TRI_select_datapart_t*) result->_dataParts->_buffer[i];
    datapart->free(datapart);
  }

  TRI_DestroyVectorPointer(result->_dataParts);
  TRI_Free(result->_dataParts);

  TRI_Free(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Initialise a select result
///
/// This will also pre-allocate initial memory for the result set index and data
////////////////////////////////////////////////////////////////////////////////

static void InitSelectResult (TRI_select_result_t* result, 
                              TRI_vector_pointer_t* dataparts) {

  assert(INDEX_INIT_SIZE > 0);
  assert(INDEX_GROWTH_FACTOR > 1.0);
  assert(RESULT_INIT_SIZE > 0);
  assert(RESULT_GROWTH_FACTOR > 1.0);

  result->_index._numAllocated       = 0;
  result->_index._numUsed            = 0;
  result->_index._start              = TRI_Allocate(INDEX_INIT_SIZE * 
                                                    sizeof(TRI_sr_index_t));
  result->_index._current            = result->_index._start;
  if (result->_index._start) {
    result->_index._numAllocated     = INDEX_INIT_SIZE;
  } 

  result->_documents._bytesAllocated = 0;
  result->_documents._bytesUsed      = 0;
  result->_documents._start          = TRI_Allocate(RESULT_INIT_SIZE);
  result->_documents._current        = result->_documents._start;
  if (result->_documents._start) {
    result->_documents._bytesAllocated = RESULT_INIT_SIZE;
  } 
  
  result->_dataParts = dataparts;

  result->_numRows = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Increase storage size for select result index
////////////////////////////////////////////////////////////////////////////////

static bool IncreaseIndexStorageSelectResult (TRI_select_result_t* result, 
                                              const size_t numNeeded) {
  TRI_sr_index_t* start;
  size_t newSize;
 
  // Extend by at least INDEX_GROWTH_FACTOR to save the cost of at least some
  // reallocations
  newSize = (size_t) result->_index._numAllocated + numNeeded;
  if (newSize < (size_t) (result->_index._numAllocated * INDEX_GROWTH_FACTOR)) {
    newSize = (size_t) (result->_index._numAllocated * INDEX_GROWTH_FACTOR);
  }

  assert(newSize > result->_index._numAllocated);

  start = TRI_Reallocate(result->_index._start, newSize * sizeof(TRI_sr_index_t));
  if (!start) {
    return false;
  }
  result->_index._numAllocated = newSize; // number of entries allocated

  // Index start pointer might have been moved by realloc, so save the new 
  // position and calculate the end position
  result->_index._start = start;
  result->_index._current = ((TRI_sr_index_t*) start) + result->_index._numUsed;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Increase storage size for select documents data
////////////////////////////////////////////////////////////////////////////////

static bool IncreaseDocumentsStorageSelectResult (TRI_select_result_t* result, 
                                                  const size_t bytesNeeded) {
  TRI_sr_documents_t* start;
  TRI_sr_index_t* indexStart;
  TRI_sr_index_t* indexEnd;
  TRI_sr_index_t value;
  size_t diff;
  size_t newSize;

  // Extend by at least RESULT_GROWTH_FACTOR to save the cost of at least some
  // reallocations
  newSize = (size_t) result->_documents._bytesAllocated + bytesNeeded;
  if (newSize < (size_t) (result->_documents._bytesAllocated * RESULT_GROWTH_FACTOR)) {
    newSize = (size_t) (result->_documents._bytesAllocated * RESULT_GROWTH_FACTOR);
  }

  assert(newSize > result->_documents._bytesAllocated);

  start = (TRI_sr_documents_t*) TRI_Reallocate(result->_documents._start, newSize);
  if (!start) {
    return false;
  }

  // calc movement
  diff = start - (TRI_sr_documents_t*) result->_documents._start;

  // realloc might move the data. if it does, we need to adjust the index as well
  if ((start != result->_documents._start) && (result->_documents._start != 0)) {
    // data was moved, now adjust entries in index
    indexStart = (TRI_sr_index_t*) result->_index._start;
    indexEnd = (TRI_sr_index_t*) result->_index._current;

    while (indexStart < indexEnd) {
      value = *indexStart;
      *indexStart++ = (TRI_sr_index_t) (((TRI_sr_index_t*) value) + diff);
    }
  }
  result->_documents._bytesAllocated = newSize;
  result->_documents._start = start;
  result->_documents._current = result->_documents._current + diff;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the required storage size for a row result of a join - DEPRECATED
///
/// A result row of a join might contain data from multiple collections. Results
/// might also be single documents or multiple documents, depending on the join
/// type. 
/// This function will calculate the total required size to store all documents 
/// of all collections of the row result.
////////////////////////////////////////////////////////////////////////////////

static size_t GetJoinDocumentSizeX (const TRI_select_join_t* join) {
  TRI_join_part_t* part;
  size_t i, n, total;

  total = 0;

  for (i = 0; i < join->_parts._length; i++) {
    part = (TRI_join_part_t*) join->_parts._buffer[i];
    if (part->_type == JOIN_TYPE_LIST) {
      n = part->_listDocuments._length;

      // adjust for extra data
      total += part->_extraData._size * n;
    }
    else {
      n = 1;
    }

    // number of documents
    total += sizeof(TRI_select_size_t);
    // document pointers
    total += (size_t) (sizeof(TRI_sr_documents_t) * n);

    // adjust for extra data
    if (part->_extraData._size) {
      total += sizeof(TRI_select_size_t);
      total += part->_extraData._size;
    }
  }

  return total;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the required storage size for a row result of a join
///
/// A result row of a join might contain data from multiple collections. Results
/// might also be single documents or multiple documents, depending on the join
/// type. 
/// This function will calculate the total required size to store all documents 
/// of all collections of the row result.
////////////////////////////////////////////////////////////////////////////////

static size_t GetJoinDocumentSize (const query_instance_t* inst) {
  size_t i, total;
  TRI_query_instance_t* instance = (TRI_query_instance_t*) inst;

  total = 0;

  for (i = 0; i < instance->_join._length; i++) {
    TRI_join_part_t* part;
    size_t n;

    part = (TRI_join_part_t*) instance->_join._buffer[i];
    if (part->_type == JOIN_TYPE_LIST) {
      n = part->_listDocuments._length;

      // adjust for extra data
      total += part->_extraData._size * n;
    }
    else {
      n = 1;
    }

    // number of documents
    total += sizeof(TRI_select_size_t);
    // document pointers
    total += (size_t) (sizeof(TRI_sr_documents_t) * n);

    // adjust for extra data
    if (part->_extraData._size) {
      total += sizeof(TRI_select_size_t);
      total += part->_extraData._size;
    }
  }

  return total;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Add documents from a join to the result set - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddJoinSelectResultX (TRI_select_result_t* result, TRI_select_join_t* join) {
  TRI_sr_index_t* indexPtr;
  TRI_sr_documents_t* docPtr;
  TRI_select_size_t* numPtr;
  TRI_join_part_t* part;
  TRI_doc_mptr_t* document;
  size_t numNeeded; 
  size_t bytesNeeded;
  size_t i, j;

  // need space for one pointer 
  numNeeded = 1;

  if (result->_index._numUsed + numNeeded > result->_index._numAllocated) {
    if (!IncreaseIndexStorageSelectResult(result, numNeeded)) {
      return false;
    } 
  }

  bytesNeeded = GetJoinDocumentSizeX(join);
  if (result->_documents._bytesUsed + bytesNeeded > result->_documents._bytesAllocated) {
    if (!IncreaseDocumentsStorageSelectResult(result, bytesNeeded)) {
      return false;
    } 
  }

  // store pointer to document in index
  docPtr = result->_documents._current;
  indexPtr = (TRI_sr_index_t*) result->_index._current;
  *indexPtr++ = (TRI_sr_index_t) docPtr;

  result->_index._current = (TRI_sr_index_t*) indexPtr;
  result->_index._numUsed++;

  // store document data
  numPtr = (TRI_select_size_t*) docPtr;
  for (i = 0; i < join->_parts._length; i++) {
    part = (TRI_join_part_t*) join->_parts._buffer[i];
    if (part->_type == JOIN_TYPE_LIST) {
      // multiple documents
      *numPtr++ = part->_listDocuments._length;
      docPtr = (TRI_sr_documents_t*) numPtr;
      for (j = 0; j < part->_listDocuments._length; j++) {
        document = (TRI_doc_mptr_t*) part->_listDocuments._buffer[j];

        *docPtr++ = (TRI_sr_documents_t) document->_data;
      }

      if (part->_extraData._size) {
        // copy extra data
        assert(part->_listDocuments._length == part->_extraData._listValues._length);
        numPtr = (TRI_select_size_t*) docPtr;
        *numPtr++ = part->_listDocuments._length;
        docPtr = (TRI_sr_documents_t*) numPtr;

        for (j = 0; j < part->_extraData._listValues._length; j++) {
          memcpy(docPtr, part->_extraData._listValues._buffer[j], part->_extraData._size);
          docPtr = (TRI_sr_documents_t*) ((uint8_t*) docPtr + part->_extraData._size);
        }
      }
    } 
    else {
      // single document
      *numPtr++ = 1;
      docPtr = (TRI_sr_documents_t*) numPtr;
      document = (TRI_doc_mptr_t*) part->_singleDocument;
      if (document) {
        *docPtr++ = (TRI_sr_documents_t) document->_data;
      } 
      else {
        // document is null
        *docPtr++ = 0;
      }

      if (part->_extraData._size) {
        // copy extra data
        numPtr = (TRI_select_size_t*) docPtr;
        *numPtr++ = 1;
        docPtr = (TRI_sr_documents_t*) numPtr;
        memcpy(docPtr, part->_extraData._singleValue, part->_extraData._size);
        docPtr = (TRI_sr_documents_t*) ((uint8_t*) docPtr + part->_extraData._size);
      }
    }
    numPtr = (TRI_select_size_t*) docPtr;
  }

  result->_documents._bytesUsed += bytesNeeded;
  result->_documents._current = (TRI_sr_documents_t*) numPtr;

  result->_numRows++;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Add documents from a join to the result set
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddJoinSelectResult (query_instance_t* const inst, 
                              TRI_select_result_t* result) {
  TRI_sr_index_t* indexPtr;
  TRI_sr_documents_t* docPtr;
  TRI_select_size_t* numPtr;
  TRI_join_part_t* part;
  TRI_doc_mptr_t* document;
  size_t numNeeded; 
  size_t bytesNeeded;
  size_t i, j;

  TRI_query_instance_t* instance = (TRI_query_instance_t*) inst;

  // need space for one pointer 
  numNeeded = 1;

  if (result->_index._numUsed + numNeeded > result->_index._numAllocated) {
    if (!IncreaseIndexStorageSelectResult(result, numNeeded)) {
      return false;
    } 
  }

  bytesNeeded = GetJoinDocumentSize(instance);
  if (result->_documents._bytesUsed + bytesNeeded > result->_documents._bytesAllocated) {
    if (!IncreaseDocumentsStorageSelectResult(result, bytesNeeded)) {
      return false;
    } 
  }

  // store pointer to document in index
  docPtr = result->_documents._current;
  indexPtr = (TRI_sr_index_t*) result->_index._current;
  *indexPtr++ = (TRI_sr_index_t) docPtr;

  result->_index._current = (TRI_sr_index_t*) indexPtr;
  result->_index._numUsed++;

  // store document data
  numPtr = (TRI_select_size_t*) docPtr;
  for (i = 0; i < instance->_join._length; i++) {
    part = (TRI_join_part_t*) instance->_join._buffer[i];
    if (part->_type == JOIN_TYPE_LIST) {
      // multiple documents
      *numPtr++ = part->_listDocuments._length;
      docPtr = (TRI_sr_documents_t*) numPtr;
      for (j = 0; j < part->_listDocuments._length; j++) {
        document = (TRI_doc_mptr_t*) part->_listDocuments._buffer[j];

        *docPtr++ = (TRI_sr_documents_t) document->_data;
      }

      if (part->_extraData._size) {
        // copy extra data
        assert(part->_listDocuments._length == part->_extraData._listValues._length);
        numPtr = (TRI_select_size_t*) docPtr;
        *numPtr++ = part->_listDocuments._length;
        docPtr = (TRI_sr_documents_t*) numPtr;

        for (j = 0; j < part->_extraData._listValues._length; j++) {
          memcpy(docPtr, part->_extraData._listValues._buffer[j], part->_extraData._size);
          docPtr = (TRI_sr_documents_t*) ((uint8_t*) docPtr + part->_extraData._size);
        }
      }
    } 
    else {
      // single document
      *numPtr++ = 1;
      docPtr = (TRI_sr_documents_t*) numPtr;
      document = (TRI_doc_mptr_t*) part->_singleDocument;
      if (document) {
        *docPtr++ = (TRI_sr_documents_t) document->_data;
      } 
      else {
        // document is null
        *docPtr++ = 0;
      }

      if (part->_extraData._size) {
        // copy extra data
        numPtr = (TRI_select_size_t*) docPtr;
        *numPtr++ = 1;
        docPtr = (TRI_sr_documents_t*) numPtr;
        memcpy(docPtr, part->_extraData._singleValue, part->_extraData._size);
        docPtr = (TRI_sr_documents_t*) ((uint8_t*) docPtr + part->_extraData._size);
      }
    }
    numPtr = (TRI_select_size_t*) docPtr;
  }

  result->_documents._bytesUsed += bytesNeeded;
  result->_documents._current = (TRI_sr_documents_t*) numPtr;

  result->_numRows++;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Slice a select result (apply skip/limit)
////////////////////////////////////////////////////////////////////////////////

bool TRI_SliceSelectResult (TRI_select_result_t* result, 
                            const TRI_voc_size_t skip, 
                            const TRI_voc_ssize_t limit) {
  TRI_sr_index_t* oldIndex;
  TRI_sr_index_t* newIndex;
  size_t newSize;
 
  if (result->_numRows == 0 || !result->_index._start) {
    // no need to do anything
    return true;
  }

  // allocate new space for document index
  newSize = (size_t) limit;
  if (limit == 0) {
    newSize = 1;
  }

  newIndex = (TRI_sr_index_t*) TRI_Allocate(newSize * sizeof(TRI_sr_index_t));
  if (!newIndex) {
    // error: memory allocation failed
    return false;
  }

  oldIndex = (TRI_sr_index_t*) result->_index._start;

  memcpy(newIndex, oldIndex + skip, limit * sizeof(TRI_sr_index_t));

  TRI_Free(oldIndex);

  result->_numRows             = limit;
  result->_index._start        = newIndex;
  result->_index._current      = newIndex + 1;
  result->_index._numAllocated = newSize;
  result->_index._numUsed      = (size_t) limit;

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

  result->getAt = GetSelectResult;
  result->first = FirstSelectResult;
  result->last  = LastSelectResult;
  result->free  = FreeSelectResult;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
