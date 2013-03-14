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

struct TRI_shaped_json_s;
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
  struct TRI_doc_mptr_s* (*request) (struct TRI_headers_s*);
  struct TRI_doc_mptr_s* (*verify) (struct TRI_headers_s*, struct TRI_doc_mptr_s*);
  void (*release) (struct TRI_headers_s*, struct TRI_doc_mptr_s*);
}
TRI_headers_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new simple headers structures
////////////////////////////////////////////////////////////////////////////////

TRI_headers_t* TRI_CreateSimpleHeaders (size_t headerSize);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a simple headers structures, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySimpleHeaders (TRI_headers_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a simple headers structures, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSimpleHeaders (TRI_headers_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over all headers
////////////////////////////////////////////////////////////////////////////////

void TRI_IterateSimpleHeaders (TRI_headers_t*,
                               void (*iterator)(struct TRI_doc_mptr_s const*, void*),
                               void* data);

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
