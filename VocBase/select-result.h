////////////////////////////////////////////////////////////////////////////////
/// @brief select result
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

#ifndef TRIAGENS_DURHAM_VOC_BASE_SELECT_RESULT_H
#define TRIAGENS_DURHAM_VOC_BASE_SELECT_RESULT_H 1

#include <BasicsC/common.h>
#include <BasicsC/vector.h>
#include <BasicsC/strings.h>

#include "VocBase/document-collection.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief result part type (single document or multiple documents)
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  RESULT_PART_SINGLE = 0,
  RESULT_PART_MULTI  = 1
}
TRI_select_data_e;


////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for number of rows in a select result
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_select_size_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief select input type
////////////////////////////////////////////////////////////////////////////////
 
typedef struct TRI_select_feeder_s { 
  TRI_doc_collection_t* _collection;

  void (*init) (struct TRI_select_feeder_s*);
  void (*rewind) (struct TRI_select_feeder_s*);
  TRI_doc_mptr_t* (*current) (struct TRI_select_feeder_s*);
  TRI_doc_mptr_t* (*next) (struct TRI_select_feeder_s*);
}
TRI_select_feeder_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief select data parts type
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_select_datapart_s {
  char* _alias;
  TRI_doc_collection_t* _collection;
  TRI_select_data_e _type;

  void (*free) (struct TRI_select_datapart_s*);
}
TRI_select_datapart_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new select datapart instance
////////////////////////////////////////////////////////////////////////////////

TRI_select_datapart_t* TRI_CreateDataPart(const char*, 
                                          const TRI_doc_collection_t*,
                                          const TRI_select_data_e);


////////////////////////////////////////////////////////////////////////////////
/// @brief document index within a select result
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_select_result_index_s {
  TRI_select_size_t _numAllocated;
  TRI_select_size_t _numUsed;
  void *_start;
  void *_current;
}
TRI_select_result_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document storage within a select result
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_select_result_documents_s {
  size_t _bytesAllocated;
  size_t _bytesUsed;
  void *_start;
  void *_current;
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

  void (*free) (struct TRI_select_result_s*);
}
TRI_select_result_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Add a document to the result set
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddDocumentSelectResult (TRI_select_result_t*, TRI_vector_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new select result
////////////////////////////////////////////////////////////////////////////////

TRI_select_result_t* TRI_CreateSelectResult (TRI_vector_pointer_t*);

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

