////////////////////////////////////////////////////////////////////////////////
/// @brief datafiles
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2010, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "headers.h"

#include <VocBase/document-collection.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief number of headers per block
////////////////////////////////////////////////////////////////////////////////

#define NUMBER_HEADERS_PER_BLOCK (1000000)

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief headers
////////////////////////////////////////////////////////////////////////////////

typedef struct simple_headers_s {
  TRI_headers_t base;

  TRI_doc_mptr_t const* _freelist;
  TRI_vector_pointer_t _blocks;
}
simple_headers_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clears an header
////////////////////////////////////////////////////////////////////////////////

static void ClearSimpleHeaders (TRI_doc_mptr_t* header) {
  memset(header, 0, sizeof(TRI_doc_mptr_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief requests a header
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t* RequestSimpleHeaders (TRI_headers_t* h) {
  simple_headers_t* headers = (simple_headers_t*) h;
  TRI_doc_mptr_t const* header;
  union { TRI_doc_mptr_t const* c; TRI_doc_mptr_t* h; } c;

  if (headers->_freelist == NULL) {
    TRI_doc_mptr_t* begin;
    TRI_doc_mptr_t* ptr;

    begin = TRI_Allocate(NUMBER_HEADERS_PER_BLOCK * sizeof(TRI_doc_mptr_t));
    ptr = begin + (NUMBER_HEADERS_PER_BLOCK - 1);

    header = NULL;

    for (;  begin <= ptr;  --ptr) {
      ClearSimpleHeaders(ptr);
      ptr->_data = header;
      header = ptr;
    }

    headers->_freelist = header;

    TRI_PushBackVectorPointer(&headers->_blocks, begin);
  }

  c.c = headers->_freelist;
  headers->_freelist = c.c->_data;

  c.h->_data = NULL;
  return c.h;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief verifies if header is still valid, possible returning a new one
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t* VerifySimpleHeaders (TRI_headers_t* h, TRI_doc_mptr_t* header) {
  return header;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a header, putting it back onto the freelist
////////////////////////////////////////////////////////////////////////////////

static void ReleaseSimpleHeaders (TRI_headers_t* h, TRI_doc_mptr_t* header) {
  simple_headers_t* headers = (simple_headers_t*) h;

  ClearSimpleHeaders(header);

  header->_data = headers->_freelist;
  headers->_freelist = header;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new simple headers structures
////////////////////////////////////////////////////////////////////////////////

TRI_headers_t* TRI_CreateSimpleHeaders () {
  simple_headers_t* headers = TRI_Allocate(sizeof(simple_headers_t));

  headers->base.request = RequestSimpleHeaders;
  headers->base.verify = VerifySimpleHeaders;
  headers->base.release = ReleaseSimpleHeaders;

  headers->_freelist = NULL;

  TRI_InitVectorPointer(&headers->_blocks);

  return &headers->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a simple headers structures, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySimpleHeaders (TRI_headers_t* h) {
  simple_headers_t* headers = (simple_headers_t*) h;

  size_t n = TRI_SizeVectorPointer(&headers->_blocks);
  size_t i;

  for (i = 0;  i < n;  ++i) {
    TRI_Free(TRI_AtVectorPointer(&headers->_blocks, i));
  }

  TRI_DestroyVectorPointer(&headers->_blocks);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a simple headers structures, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSimpleHeaders (TRI_headers_t* headers) {
  TRI_DestroySimpleHeaders(headers);
  TRI_Free(headers);
}

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over all headers
////////////////////////////////////////////////////////////////////////////////

void TRI_IterateSimpleHeaders (TRI_headers_t* headers,
                               void (*iterator)(TRI_doc_mptr_t const*, void*),
                               void* data) {
  simple_headers_t* h = (simple_headers_t*) headers;
  size_t n;
  size_t i;

  n = TRI_SizeVectorPointer(&h->_blocks);

  for (i = 0;  i < n;   ++i) {
    TRI_doc_mptr_t* begin = TRI_AtVectorPointer(&h->_blocks, i);
    TRI_doc_mptr_t* end = begin + NUMBER_HEADERS_PER_BLOCK;

    for (;  begin < end;  ++begin) {
      iterator(begin, data);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
