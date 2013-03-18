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

#include "headers.h"

#include "VocBase/primary-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief minium number of headers per block
////////////////////////////////////////////////////////////////////////////////

#define BLOCK_SIZE_UNIT (128)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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
/// @brief get the size (number of entries) for a block, based on a function
///
/// this adaptively increases the number of entries per block until a certain
/// threshold. the benefit of this is that small collections (with few
/// documents) only use little memory whereas bigger collections allocate new
/// blocks in bigger chunks.
/// the lowest value for the number of entries in a block is BLOCK_SIZE_UNIT,
/// the highest value is BLOCK_SIZE_UNIT << 8.
////////////////////////////////////////////////////////////////////////////////

static size_t GetBlockSize (const size_t blockNumber) {
  if (blockNumber < 8) {
    return (size_t) (BLOCK_SIZE_UNIT << blockNumber);
  }

  return (size_t) (BLOCK_SIZE_UNIT << 8);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears an header
////////////////////////////////////////////////////////////////////////////////

static void ClearSimpleHeaders (TRI_doc_mptr_t* header, size_t headerSize) {
  TRI_ASSERT_DEBUG(header);

  memset(header, 0, headerSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief requests a header
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t* RequestSimpleHeaders (TRI_headers_t* h) {
  simple_headers_t* headers = (simple_headers_t*) h;
  char const* header;
  TRI_doc_mptr_t* result;

  if (headers->_freelist == NULL) {
    char* begin;
    char* ptr;
    size_t blockSize;

    blockSize = GetBlockSize(headers->_blocks._length);
    TRI_ASSERT_DEBUG(blockSize > 0);

    begin = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, blockSize * headers->_headerSize, true);

    // out of memory
    if (begin == NULL) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }

    ptr = begin + headers->_headerSize * (blockSize - 1);

    header = NULL;

    for (;  begin <= ptr;  ptr -= headers->_headerSize) {
      ((TRI_doc_mptr_t*) ptr)->_data = header;
      header = ptr;
    }

    TRI_ASSERT_DEBUG(headers != NULL);
    headers->_freelist = (TRI_doc_mptr_t*) header;

    TRI_PushBackVectorPointer(&headers->_blocks, begin);
  }
  
  TRI_ASSERT_DEBUG(headers->_freelist != NULL);

  result = CONST_CAST(headers->_freelist);
  headers->_freelist = result->_data;
  result->_data = NULL;

  return result;
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
  simple_headers_t* headers = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(simple_headers_t), false);

  if (headers == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  headers->base.request = RequestSimpleHeaders;
  headers->base.release = ReleaseSimpleHeaders;

  headers->_freelist = NULL;
  headers->_headerSize = headerSize;

  TRI_InitVectorPointer(&headers->_blocks, TRI_UNKNOWN_MEM_ZONE);

  return &headers->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a simple headers structures, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySimpleHeaders (TRI_headers_t* h) {
  simple_headers_t* headers = (simple_headers_t*) h;
  size_t i;

  for (i = 0;  i < headers->_blocks._length;  ++i) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, headers->_blocks._buffer[i]);
  }

  TRI_DestroyVectorPointer(&headers->_blocks);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a simple headers structures, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSimpleHeaders (TRI_headers_t* headers) {
  TRI_DestroySimpleHeaders(headers);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, headers);
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
    size_t blockSize = GetBlockSize(i);
    TRI_doc_mptr_t* begin = h->_blocks._buffer[i];
    TRI_doc_mptr_t* end = begin + blockSize;

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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
