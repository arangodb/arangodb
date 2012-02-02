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

#ifndef TRIAGENS_DURHAM_VOC_BASE_CURSOR
#define TRIAGENS_DURHAM_VOC_BASE_CURSOR

#include <stdlib.h>

#include <BasicsC/common.h>
#include <BasicsC/strings.h>

#include "VocBase/vocbase.h"
#include "VocBase/query.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief result set types
/// RESULTSET_TYPE_SINGLE means the result set will contain one element per 
/// result row, RESULTSET_TYPE_MULTI means the result set will contain multiple 
/// (0..n) elements per result row
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  RESULTSET_TYPE_UNDEFINED = 0,

  RESULTSET_TYPE_SINGLE,
  RESULTSET_TYPE_MULTI
} 
TRI_resultset_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for result set data
////////////////////////////////////////////////////////////////////////////////

typedef char TRI_resultset_data_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief result set element (base type)
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_resultset_element_s {
  void *_data;
  TRI_resultset_type_e _type;
  void (*free) (struct TRI_resultset_element_s*);
  bool (*toJavaScript) (struct TRI_resultset_element_s*, TRI_rc_result_t*, void*);
}
TRI_resultset_element_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief single result set element
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_resultset_element_single_s {
  TRI_doc_mptr_t *_document;
} 
TRI_resultset_element_single_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief multiple result set element
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_resultset_element_multi_s {
  TRI_voc_size_t _num;
  TRI_doc_mptr_t **_documents;
} 
TRI_resultset_element_multi_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief result set type
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_resultset_s {
  TRI_voc_size_t _length;
  TRI_voc_size_t _position;
  TRI_resultset_type_e _type;
  char *_alias;
  TRI_resultset_data_t *_data;
  TRI_resultset_data_t *_current;
  size_t _storageUsed;
  size_t _storageAllocated;

  void (*free) (struct TRI_resultset_s*);
  TRI_resultset_data_t *(*next)(struct TRI_resultset_s*);
//  bool (*toJavaScript) (struct TRI_resultset_s*, void*);
} 
TRI_resultset_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief result set row type
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_resultset_row_s {
  TRI_vector_pointer_t _data;
  bool (*toJavaScript) (struct TRI_resultset_row_s*, void*);
}
TRI_resultset_row_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor type
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_cursor_s {
  TRI_rc_context_t* _context;
  TRI_qry_select_t* _select;

  TRI_vector_pointer_t _resultsets;
  TRI_voc_size_t _length;
  TRI_voc_size_t _position;
  TRI_resultset_row_t *_currentData;

  void (*free) (struct TRI_cursor_s*);
  TRI_resultset_row_t* (*next)(struct TRI_cursor_s*);
  bool (*hasNext)(const struct TRI_cursor_s*);
  void (*addResultset)(struct TRI_cursor_s*, const TRI_resultset_t*);
}
TRI_cursor_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief add a single document to a single result set
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddDocumentSingleResultset(TRI_resultset_t *, const TRI_doc_mptr_t *);

////////////////////////////////////////////////////////////////////////////////
/// @brief add multiple documents (0..n) to a multiple result set
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddDocumentsMultiResultset(TRI_resultset_t *, const TRI_voc_size_t, 
                                    const TRI_doc_mptr_t **);


////////////////////////////////////////////////////////////////////////////////
/// @brief create a new result set
////////////////////////////////////////////////////////////////////////////////

TRI_resultset_t *TRI_CreateResultset(const TRI_resultset_type_e, const char*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new cursor
////////////////////////////////////////////////////////////////////////////////

TRI_cursor_t *TRI_CreateCursor(TRI_rc_context_t *, TRI_qry_select_t *);


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

