////////////////////////////////////////////////////////////////////////////////
/// @brief SELECT result data structures and functionalitly
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
/// @author Dr. Frank Celler
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_SELECT_RESULT_H
#define TRIAGENS_DURHAM_VOC_BASE_SELECT_RESULT_H 1

#include <BasicsC/common.h>
#include <BasicsC/vector.h>
#include <BasicsC/strings.h>

#include "VocBase/document-collection.h"
#include "VocBase/context.h"
#include "VocBase/join.h"
#include "VocBase/result.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief result part type (single document or multiple documents)
///
/// single = the result contains one document
/// multi  = the result contains n documents (e.g. used for LIST JOINs)
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  RESULT_PART_DOCUMENT_SINGLE = 0,
  RESULT_PART_DOCUMENT_MULTI  = 1,
  RESULT_PART_VALUE_SINGLE    = 2,
  RESULT_PART_VALUE_MULTI     = 3
}
TRI_select_part_e;


////////////////////////////////////////////////////////////////////////////////
/// @brief select data parts type
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_select_datapart_s {
  char* _alias;
  TRI_doc_collection_t* _collection;
  TRI_select_part_e _type;
  size_t _extraDataSize;

  void (*free) (struct TRI_select_datapart_s*);
}
TRI_select_datapart_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new select datapart instance
////////////////////////////////////////////////////////////////////////////////

TRI_select_datapart_t* TRI_CreateDataPart (const char*, 
                                           const TRI_doc_collection_t*,
                                           const TRI_select_part_e,
                                           const size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief document index within a select result
///
/// This index is a structure that allows quick and random access to all 
/// rows in the select result. It will contain one pointer for each row.
/// The index pointer points to the start of the data storage 
/// (@ref TRI_select_result_documents_t) for all documents of all collections of 
/// that row.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_select_result_index_s {
  size_t _numAllocated;
  size_t _numUsed;
  TRI_sr_index_t* _start;
  TRI_sr_index_t* _current;
}
TRI_select_result_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document storage within a select result
///
/// Contains the documents for all rows of a select result.
/// The data is organised as sequential row data. Each row contains the document
/// pointers for all collections of that row. As there might be a variable 
/// number of documents for each collection, each row entry starts with a 
/// "number of documents" entry, followed by the actual document pointers. This
/// is repeated for all collections that are used in the select. 
/// The data for the next row will follow directly.
/// The document index (@ref TRI_select_result_index_t) contains pointers to the
/// starts of the row data in this storage.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_select_result_documents_s {
  size_t _bytesAllocated;
  size_t _bytesUsed;
  TRI_sr_documents_t* _start;
  TRI_sr_documents_t* _current;
}
TRI_select_result_documents_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief select result type
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_select_result_s {
  TRI_select_size_t _numRows;
  TRI_vector_pointer_t* _dataParts;
  TRI_select_result_index_t _index;
  TRI_select_result_documents_t _documents;

  TRI_sr_documents_t* (*getAt) (const struct TRI_select_result_s*, const TRI_select_size_t);
  TRI_sr_index_t* (*first) (const struct TRI_select_result_s*);
  TRI_sr_index_t* (*last) (const struct TRI_select_result_s*);
  void (*free) (struct TRI_select_result_s*);
}
TRI_select_result_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Add documents from a join to the result set
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddJoinSelectResult (TRI_select_result_t*, TRI_select_join_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Slice a select result (apply skip/limit)
///
/// This will adjust the number of rows in the select and move elements in the
/// data index (@ref TRI_select_result_index_t).
////////////////////////////////////////////////////////////////////////////////

bool TRI_SliceSelectResult (TRI_select_result_t*,
                            const TRI_voc_size_t,
                            const TRI_voc_ssize_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new select result
////////////////////////////////////////////////////////////////////////////////

TRI_select_result_t* TRI_CreateSelectResult (TRI_vector_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief the result documents for the next result
///
/// The result documents of the next result. When a cursor returns the next set
/// of documents, it will return these documents as instance of this class.  The
/// methods @FN{shapedJson} and @FN{toJavaScript} of @CODE{TRI_qry_select_t}
/// must be used to convert these documents into the final json or JavaScript
/// result object. Note that a call to @FN{next} will free the current result.
/// You do not need to free the result returned by @FN{next}, it is destroyed
/// when the cursor itself is destroyed or when @FN{next} is called again.
///
/// The member @CODE{_primary} holds the primary document pointer for the
/// primary collection.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_rc_result_s {
  TRI_rc_context_t* _context;

  TRI_doc_mptr_t* _primary;
  TRI_select_result_t* _selectResult;
  TRI_sr_documents_t* _dataPtr;
  TRI_json_t _augmention;
}
TRI_rc_result_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Convert a raw data document pointer into a master pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_MarkerMasterPointer (void const*, TRI_doc_mptr_t*);

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

