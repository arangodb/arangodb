////////////////////////////////////////////////////////////////////////////////
/// @brief datafiles
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "headers.h"

#include <VocBase/document-collection.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
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

  size_t _headerSize;
}
simple_headers_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clears an header
////////////////////////////////////////////////////////////////////////////////

static void ClearSimpleHeaders (TRI_doc_mptr_t* header, size_t headerSize) {
  memset(header, 0, headerSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief requests a header
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t* RequestSimpleHeaders (TRI_headers_t* h) {
  simple_headers_t* headers = (simple_headers_t*) h;
  char const* header;
  union { TRI_doc_mptr_t const* c; TRI_doc_mptr_t* h; } c;

  if (headers->_freelist == NULL) {
    char* begin;
    char* ptr;

    begin = TRI_Allocate(NUMBER_HEADERS_PER_BLOCK * headers->_headerSize);
    ptr = begin + headers->_headerSize * (NUMBER_HEADERS_PER_BLOCK - 1);

    header = NULL;

    for (;  begin <= ptr;  ptr -= headers->_headerSize) {
      ClearSimpleHeaders((TRI_doc_mptr_t*) ptr, headers->_headerSize);
      ((TRI_doc_mptr_t*) ptr)->_data = header;
      header = ptr;
    }

    headers->_freelist = (TRI_doc_mptr_t*) header;

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

  ClearSimpleHeaders(header, headers->_headerSize);

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
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new simple headers structures
////////////////////////////////////////////////////////////////////////////////

TRI_headers_t* TRI_CreateSimpleHeaders (size_t headerSize) {
  simple_headers_t* headers = TRI_Allocate(sizeof(simple_headers_t));

  headers->base.request = RequestSimpleHeaders;
  headers->base.verify = VerifySimpleHeaders;
  headers->base.release = ReleaseSimpleHeaders;

  headers->_freelist = NULL;
  headers->_headerSize = headerSize;

  TRI_InitVectorPointer(&headers->_blocks);

  return &headers->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a simple headers structures, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySimpleHeaders (TRI_headers_t* h) {
  simple_headers_t* headers = (simple_headers_t*) h;
  size_t i;

  for (i = 0;  i < headers->_blocks._length;  ++i) {
    TRI_Free(headers->_blocks._buffer[i]);
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
  size_t i;

  for (i = 0;  i < h->_blocks._length;   ++i) {
    TRI_doc_mptr_t* begin = h->_blocks._buffer[i];
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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
