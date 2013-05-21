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

#include "BasicsC/logging.h"
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
  TRI_headers_t          base;

  TRI_doc_mptr_t const*  _freelist;    // free headers

  TRI_doc_mptr_t*        _begin;       // start pointer to list of allocated headers
  TRI_doc_mptr_t*        _end;         // end pointer to list of allocated headers
  size_t                 _nrAllocated; // number of allocated headers
  size_t                 _nrLinked;    // number of linked headers

  TRI_vector_pointer_t   _blocks;
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

static void ClearHeader (TRI_headers_t* h,
                         TRI_doc_mptr_t* header) { 
  simple_headers_t* headers = (simple_headers_t*) h;

  TRI_ASSERT_MAINTAINER(header != NULL);

  memset(header, 0, sizeof(TRI_doc_mptr_t));

  TRI_ASSERT_MAINTAINER(headers->_nrAllocated > 0);
  headers->_nrAllocated--;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves an existing header to the end of the list
////////////////////////////////////////////////////////////////////////////////

static void MoveBackHeader (TRI_headers_t* h, 
                            TRI_doc_mptr_t* header) {
  simple_headers_t* headers = (simple_headers_t*) h;

  if (header == NULL) {
    return;
  }

  TRI_ASSERT_MAINTAINER(headers->_nrAllocated > 0);
  TRI_ASSERT_MAINTAINER(headers->_nrLinked > 0);
  
  // we have at least one element in the list
  TRI_ASSERT_MAINTAINER(headers->_begin != NULL);
  TRI_ASSERT_MAINTAINER(headers->_end != NULL);
  TRI_ASSERT_MAINTAINER(header->_prev != header);
  TRI_ASSERT_MAINTAINER(header->_next != header);

  if (headers->_end == header) {
    // header is already at the end
    TRI_ASSERT_MAINTAINER(header->_next == NULL);
    return;
  }

  TRI_ASSERT_MAINTAINER(headers->_begin != headers->_end);

  // unlink the element
  if (header->_prev != NULL) {
    header->_prev->_next = header->_next;
  }
  if (header->_next != NULL) {
    header->_next->_prev = header->_prev;
  }
  
  if (headers->_begin == header) {
    TRI_ASSERT_MAINTAINER(header->_next != NULL);
    headers->_begin = header->_next;
  }

  header->_prev   = headers->_end;
  header->_next   = NULL;
  headers->_end   = header;
  header->_prev->_next = header;
  
  TRI_ASSERT_MAINTAINER(headers->_begin != NULL);
  TRI_ASSERT_MAINTAINER(headers->_end != NULL);
  TRI_ASSERT_MAINTAINER(header->_prev != header);
  TRI_ASSERT_MAINTAINER(header->_next != header);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlinks a header from the list, without freeing it
////////////////////////////////////////////////////////////////////////////////

static void UnlinkHeader (TRI_headers_t* h,
                          TRI_doc_mptr_t* header) {
  simple_headers_t* headers = (simple_headers_t*) h;

  TRI_ASSERT_MAINTAINER(header != NULL); 
  TRI_ASSERT_MAINTAINER(header->_prev != header);
  TRI_ASSERT_MAINTAINER(header->_next != header);

  // unlink the header
  if (header->_prev != NULL) {
    header->_prev->_next = header->_next;
  }

  if (header->_next != NULL) {
    header->_next->_prev = header->_prev;
  }
  
  // adjust begin & end pointers
  if (headers->_begin == header) {
    headers->_begin = header->_next;
  }

  if (headers->_end == header) {
    headers->_end = header->_prev;
  }

  TRI_ASSERT_MAINTAINER(headers->_begin != header);
  TRI_ASSERT_MAINTAINER(headers->_end != header);
  
  TRI_ASSERT_MAINTAINER(headers->_nrLinked > 0);
  headers->_nrLinked--;

  if (headers->_nrLinked == 0) {
    TRI_ASSERT_MAINTAINER(headers->_begin == NULL);
    TRI_ASSERT_MAINTAINER(headers->_end == NULL);
  }
  else {
    TRI_ASSERT_MAINTAINER(headers->_begin != NULL);
    TRI_ASSERT_MAINTAINER(headers->_end != NULL);
  }
  
  TRI_ASSERT_MAINTAINER(header->_prev != header);
  TRI_ASSERT_MAINTAINER(header->_next != header);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves a header around in the list, using its previous position
/// (specified in "old")
////////////////////////////////////////////////////////////////////////////////

static void MoveHeader (TRI_headers_t* h, 
                        TRI_doc_mptr_t* header,
                        TRI_doc_mptr_t* old) {
  simple_headers_t* headers = (simple_headers_t*) h;

  if (header == NULL) {
    return;
  }

  TRI_ASSERT_MAINTAINER(headers->_nrAllocated > 0);
  TRI_ASSERT_MAINTAINER(header->_prev != header);
  TRI_ASSERT_MAINTAINER(header->_next != header);

  // adjust list start and end pointers
  if (old->_prev == NULL) {
    headers->_begin = header;
  }
  else if (headers->_begin == header) {
    if (header->_next != NULL) {
      headers->_begin = header->_next;
    }
  }

  if (old->_next == NULL) {
    headers->_end = header;
  }
  else if (headers->_end == header) {
    if (header->_prev != NULL) {
      headers->_end = header->_prev;
    }
  }

  if (header->_prev != NULL) {
    if (header->_prev == old->_next) {
      header->_prev->_next = NULL;
    }
    else {
      header->_prev->_next = header->_next;
    }
  }

  if (header->_next != NULL) {
    if (header->_next == old->_prev) {
      header->_next->_prev = NULL;
    }
    else {
      header->_next->_prev = header->_prev;
    }
  }

  if (old->_prev != NULL) {
    old->_prev->_next = header;
    header->_prev = old->_prev;
  }
  else {
    header->_prev = NULL;
  }

  if (old->_next != NULL) {
    old->_next->_prev = header;
    header->_next = old->_next;
  }
  else {
    header->_next = NULL;
  }

  TRI_ASSERT_MAINTAINER(headers->_begin != NULL);
  TRI_ASSERT_MAINTAINER(headers->_end != NULL);
  TRI_ASSERT_MAINTAINER(header->_prev != header);
  TRI_ASSERT_MAINTAINER(header->_next != header);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves a header back into the list, using its previous position
/// (specified in "old")
////////////////////////////////////////////////////////////////////////////////

static void RelinkHeader (TRI_headers_t* h, 
                          TRI_doc_mptr_t* header,
                          TRI_doc_mptr_t* old) {
  simple_headers_t* headers = (simple_headers_t*) h;

  if (header == NULL) {
    return;
  }

  TRI_ASSERT_MAINTAINER(headers->_begin != header);
  TRI_ASSERT_MAINTAINER(headers->_end != header);
 
  MoveHeader(h, header, old);
  headers->_nrLinked++;

  TRI_ASSERT_MAINTAINER(header->_prev != header);
  TRI_ASSERT_MAINTAINER(header->_next != header);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief requests a new header
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t* RequestHeader (TRI_headers_t* h) {
  simple_headers_t* headers = (simple_headers_t*) h;
  char const* header;
  TRI_doc_mptr_t* result;
  
  if (headers->_freelist == NULL) {
    char* begin;
    char* ptr;
    size_t blockSize;

    blockSize = GetBlockSize(headers->_blocks._length);
    TRI_ASSERT_MAINTAINER(blockSize > 0);

    // initialise the memory with 0's
    begin = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, blockSize * sizeof(TRI_doc_mptr_t), true);

    // out of memory
    if (begin == NULL) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }

    ptr = begin + sizeof(TRI_doc_mptr_t) * (blockSize - 1);

    header = NULL;

    for (;  begin <= ptr;  ptr -= sizeof(TRI_doc_mptr_t)) {
      ((TRI_doc_mptr_t*) ptr)->_data = header;
      header = ptr;
    }

    TRI_ASSERT_MAINTAINER(headers != NULL);
    headers->_freelist = (TRI_doc_mptr_t*) header;

    TRI_PushBackVectorPointer(&headers->_blocks, begin);
  }
  
  TRI_ASSERT_MAINTAINER(headers->_freelist != NULL);

  result = CONST_CAST(headers->_freelist);
  TRI_ASSERT_MAINTAINER(result != NULL);

  headers->_freelist = result->_data;
  result->_data = NULL;

  // put new header at the end of the list
  if (headers->_begin == NULL) {
    // list of headers is empty
    TRI_ASSERT_MAINTAINER(headers->_nrLinked == 0);

    headers->_begin = result;
    headers->_end   = result;

    result->_prev   = NULL;
    result->_next   = NULL;
  }
  else {
    // list is not empty
    TRI_ASSERT_MAINTAINER(headers->_nrLinked > 0);
    TRI_ASSERT_MAINTAINER(headers->_nrAllocated > 0);
    TRI_ASSERT_MAINTAINER(headers->_begin != NULL);
    TRI_ASSERT_MAINTAINER(headers->_end != NULL);

    headers->_end->_next = result;
    result->_prev        = headers->_end;
    result->_next        = NULL;
    headers->_end        = result;
  }

  headers->_nrAllocated++;
  headers->_nrLinked++;
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a header, putting it back onto the freelist
////////////////////////////////////////////////////////////////////////////////

static void ReleaseHeader (TRI_headers_t* h, 
                           TRI_doc_mptr_t* header) {
  simple_headers_t* headers = (simple_headers_t*) h;
  
  if (header == NULL) {
    return;
  }

  UnlinkHeader(h, header);
  ClearHeader(h, header);

  header->_data = headers->_freelist;
  headers->_freelist = header;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the element at the head of the list
///
/// note: the element returned might be NULL
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t* FrontHeaders (TRI_headers_t const* h) {
  simple_headers_t* headers = (simple_headers_t*) h;

  return headers->_begin;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the element at the tail of the list
///
/// note: the element returned might be NULL
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t* BackHeaders (TRI_headers_t const* h) {
  simple_headers_t* headers = (simple_headers_t*) h;

  return headers->_end;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of linked headers
////////////////////////////////////////////////////////////////////////////////

static size_t CountHeaders (TRI_headers_t const* h) {
  simple_headers_t* headers = (simple_headers_t*) h;

  return headers->_nrLinked;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump all headers
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE

static void DumpHeaders (TRI_headers_t const* h) {
  simple_headers_t* headers = (simple_headers_t*) h;
  TRI_doc_mptr_t* next = headers->_begin;
  size_t i = 0;

  printf("number of allocated headers: %lu\n", (unsigned long) headers->_nrAllocated);
  printf("number of linked headers: %lu\n", (unsigned long) headers->_nrLinked);
  printf("begin ptr: %p\n", headers->_begin);
  printf("end ptr: %p\n", headers->_end);

  while (next != NULL) {
    printf("- header #%lu: ptr: %p, prev: %p, next: %p, key: %s\n", 
              (unsigned long) i, 
              next, 
              next->_prev, 
              next->_next, 
              next->_key);
    i++;

    if (next->_next == NULL) {
      TRI_ASSERT_MAINTAINER(next == headers->_end);
    }

    next = next->_next;
  }

  TRI_ASSERT_MAINTAINER(i == headers->_nrLinked);
}

#endif

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

TRI_headers_t* TRI_CreateSimpleHeaders () {
  simple_headers_t* headers = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(simple_headers_t), false);

  if (headers == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  headers->_freelist    = NULL;

  headers->_begin       = NULL;
  headers->_end         = NULL;
  headers->_nrAllocated = 0;
  headers->_nrLinked    = 0;

  headers->base.request  = RequestHeader;
  headers->base.release  = ReleaseHeader;
  headers->base.moveBack = MoveBackHeader;
  headers->base.move     = MoveHeader;
  headers->base.relink   = RelinkHeader;
  headers->base.unlink   = UnlinkHeader;
  headers->base.front    = FrontHeaders;
  headers->base.back     = BackHeaders;
  headers->base.count    = CountHeaders;
#ifdef TRI_ENABLE_MAINTAINER_MODE  
  headers->base.dump     = DumpHeaders;
#endif

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

  headers->_nrAllocated = 0;
  headers->_nrLinked    = 0;

  TRI_DestroyVectorPointer(&headers->_blocks);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a simple headers structures, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSimpleHeaders (TRI_headers_t* headers) {
  TRI_DestroySimpleHeaders(headers);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
