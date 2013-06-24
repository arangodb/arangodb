////////////////////////////////////////////////////////////////////////////////
/// @brief datafiles
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_VOC_BASE_HEADERS_H
#define TRIAGENS_VOC_BASE_HEADERS_H 1

#include "BasicsC/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_doc_mptr_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief headers
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_headers_s {
  // request a new header
  struct TRI_doc_mptr_s* (*request) (struct TRI_headers_s*, size_t);

  // release/free an existing header, putting it back onto the freelist
  void (*release) (struct TRI_headers_s*, struct TRI_doc_mptr_s*);

  // move an existing header to the end of the linked list
  void (*moveBack) (struct TRI_headers_s*, struct TRI_doc_mptr_s*, struct TRI_doc_mptr_s*);

  // move an existing header to another position in the linked list
  void (*move) (struct TRI_headers_s*, struct TRI_doc_mptr_s*, struct TRI_doc_mptr_s*);

  // unlink an existing header from the linked list, without freeing it
  void (*unlink) (struct TRI_headers_s*, struct TRI_doc_mptr_s*);

  // relink an existing header into the linked list, at its original position
  void (*relink) (struct TRI_headers_s*, struct TRI_doc_mptr_s*, struct TRI_doc_mptr_s*);

  // return the header at the head of list, without popping it from the list
  struct TRI_doc_mptr_s* (*front) (struct TRI_headers_s const*);
  
  // return the headers at the end of list, without popping it from the list
  struct TRI_doc_mptr_s* (*back) (struct TRI_headers_s const*);

  // return the number of headers currently present in the linked list
  size_t (*count) (struct TRI_headers_s const*);
  
  // return the total size of headers currently present in the linked list
  int64_t (*size) (struct TRI_headers_s const*);

#ifdef TRI_ENABLE_MAINTAINER_MODE 
  void (*dump) (struct TRI_headers_s const*);
#endif
}
TRI_headers_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new simple headers structures
////////////////////////////////////////////////////////////////////////////////

TRI_headers_t* TRI_CreateSimpleHeaders (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a simple headers structures, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySimpleHeaders (TRI_headers_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a simple headers structures, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSimpleHeaders (TRI_headers_t*);

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
