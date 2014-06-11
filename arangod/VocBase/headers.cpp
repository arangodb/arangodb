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
#include "VocBase/document-collection.h"

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
  int64_t                _totalSize;   // total size of markers for linked headers

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

static inline size_t GetBlockSize (size_t blockNumber) {
  static size_t const BLOCK_SIZE_UNIT = 128;

  if (blockNumber < 8) {
    // use a small block size in the beginning to save memory
    return (size_t) (BLOCK_SIZE_UNIT << blockNumber);
  }

  // use a block size of 32768
  // this will use 32768 * sizeof(TRI_doc_mptr_t) bytes, i.e. 1.5 MB
  return (size_t) (BLOCK_SIZE_UNIT << 8);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears a header
////////////////////////////////////////////////////////////////////////////////

static void ClearHeader (TRI_headers_t* h,
                         TRI_doc_mptr_t* header) { 
  simple_headers_t* headers = (simple_headers_t*) h;

  TRI_ASSERT(header != nullptr);

  header->clear();

  TRI_ASSERT(headers->_nrAllocated > 0);
  headers->_nrAllocated--;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves an existing header to the end of the list
/// this is called when there is an update operation on a document
////////////////////////////////////////////////////////////////////////////////

static void MoveBackHeader (TRI_headers_t* h, 
                            TRI_doc_mptr_t* header, 
                            TRI_doc_mptr_t* old) {
  simple_headers_t* headers = (simple_headers_t*) h;

  if (header == nullptr) {
    return;
  }

  TRI_ASSERT(headers->_nrAllocated > 0);
  TRI_ASSERT(headers->_nrLinked > 0);
  TRI_ASSERT(headers->_totalSize > 0);
  
  // we have at least one element in the list
  TRI_ASSERT(headers->_begin != nullptr);
  TRI_ASSERT(headers->_end != nullptr);
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);
  
  TRI_ASSERT(old != nullptr);
  TRI_ASSERT(old->getDataPtr() != nullptr);  // ONLY IN HEADERS
 
  int64_t newSize = (int64_t) (((TRI_df_marker_t*) header->getDataPtr())->_size);  // ONLY IN HEADERS
  int64_t oldSize = (int64_t) (((TRI_df_marker_t*) old->getDataPtr())->_size);  // ONLY IN HEADERS

  // we must adjust the size of the collection
  headers->_totalSize += TRI_DF_ALIGN_BLOCK(newSize);
  headers->_totalSize -= TRI_DF_ALIGN_BLOCK(oldSize);
  
  if (headers->_end == header) {
    // header is already at the end
    TRI_ASSERT(header->_next == nullptr);
    return;
  }

  TRI_ASSERT(headers->_begin != headers->_end);

  // unlink the element
  if (header->_prev != nullptr) {
    header->_prev->_next = header->_next;
  }
  if (header->_next != nullptr) {
    header->_next->_prev = header->_prev;
  }
  
  if (headers->_begin == header) {
    TRI_ASSERT(header->_next != nullptr);
    headers->_begin = header->_next;
  }

  header->_prev   = headers->_end;
  header->_next   = nullptr;
  headers->_end   = header;
  header->_prev->_next = header;
  
  TRI_ASSERT(headers->_begin != nullptr);
  TRI_ASSERT(headers->_end != nullptr);
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);

  TRI_ASSERT(headers->_totalSize > 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlinks a header from the list, without freeing it
////////////////////////////////////////////////////////////////////////////////

static void UnlinkHeader (TRI_headers_t* h,
                          TRI_doc_mptr_t* header) {
  simple_headers_t* headers = (simple_headers_t*) h;
  int64_t size;

  TRI_ASSERT(header != nullptr); 
  TRI_ASSERT(header->getDataPtr() != nullptr); // ONLY IN HEADERS
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);
  
  size = (int64_t) ((TRI_df_marker_t*) header->getDataPtr())->_size; // ONLY IN HEADERS
  TRI_ASSERT(size > 0);

  // unlink the header
  if (header->_prev != nullptr) {
    header->_prev->_next = header->_next;
  }

  if (header->_next != nullptr) {
    header->_next->_prev = header->_prev;
  }
  
  // adjust begin & end pointers
  if (headers->_begin == header) {
    headers->_begin = header->_next;
  }

  if (headers->_end == header) {
    headers->_end = header->_prev;
  }

  TRI_ASSERT(headers->_begin != header);
  TRI_ASSERT(headers->_end != header);
  
  TRI_ASSERT(headers->_nrLinked > 0);
  headers->_nrLinked--;
  headers->_totalSize -= TRI_DF_ALIGN_BLOCK(size);

  if (headers->_nrLinked == 0) {
    TRI_ASSERT(headers->_begin == nullptr);
    TRI_ASSERT(headers->_end == nullptr);
    TRI_ASSERT(headers->_totalSize == 0);
  }
  else {
    TRI_ASSERT(headers->_begin != nullptr);
    TRI_ASSERT(headers->_end != nullptr);
    TRI_ASSERT(headers->_totalSize > 0);
  }
  
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves a header around in the list, using its previous position
/// (specified in "old")
////////////////////////////////////////////////////////////////////////////////

static void MoveHeader (TRI_headers_t* h, 
                        TRI_doc_mptr_t* header,
                        TRI_doc_mptr_t* old) {
  simple_headers_t* headers = (simple_headers_t*) h;

  if (header == nullptr) {
    return;
  }

  TRI_ASSERT(headers->_nrAllocated > 0);
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);
  TRI_ASSERT(header->getDataPtr() != nullptr); // ONLY IN HEADERS
  TRI_ASSERT(((TRI_df_marker_t*) header->getDataPtr())->_size > 0); // ONLY IN HEADERS
  TRI_ASSERT(old != nullptr);
  TRI_ASSERT(old->getDataPtr() != nullptr); // ONLY IN HEADERS
  
  int64_t newSize = (int64_t) (((TRI_df_marker_t*) header->getDataPtr())->_size); // ONLY IN HEADERS
  int64_t oldSize = (int64_t) (((TRI_df_marker_t*) old->getDataPtr())->_size); // ONLY IN HEADERS

  headers->_totalSize -= TRI_DF_ALIGN_BLOCK(newSize);
  headers->_totalSize += TRI_DF_ALIGN_BLOCK(oldSize);

  // adjust list start and end pointers
  if (old->_prev == nullptr) {
    headers->_begin = header;
  }
  else if (headers->_begin == header) {
    if (old->_prev != nullptr) {
      headers->_begin = old->_prev;
    }
  }

  if (old->_next == nullptr) {
    headers->_end = header;
  }
  else if (headers->_end == header) {
    if (old->_next != nullptr) {
      headers->_end = old->_next;
    }
  }


  if (header->_prev != nullptr) {
    header->_prev->_next = header->_next;
  }
  if (header->_next != nullptr) {
    header->_next->_prev = header->_prev;
  }

  if (old->_prev != nullptr) {
    old->_prev->_next = header;
  }
  if (old->_next != nullptr) {
    old->_next->_prev = header;
  }

  header->_prev = old->_prev;
  header->_next = old->_next;

  TRI_ASSERT(headers->_begin != nullptr);
  TRI_ASSERT(headers->_end != nullptr);
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves a header back into the list, using its previous position
/// (specified in "old")
////////////////////////////////////////////////////////////////////////////////

static void RelinkHeader (TRI_headers_t* h, 
                          TRI_doc_mptr_t* header,
                          TRI_doc_mptr_t* old) {
  simple_headers_t* headers = (simple_headers_t*) h;

  if (header == nullptr) {
    return;
  }

  TRI_ASSERT(header->getDataPtr() != nullptr); // ONLY IN HEADERS

  int64_t size = (int64_t) ((TRI_df_marker_t*) header->getDataPtr())->_size; // ONLY IN HEADERS
  TRI_ASSERT(size > 0);

  TRI_ASSERT(headers->_begin != header);
  TRI_ASSERT(headers->_end != header);
 
  MoveHeader(h, header, old);
  headers->_nrLinked++;
  headers->_totalSize += TRI_DF_ALIGN_BLOCK(size);
  TRI_ASSERT(headers->_totalSize > 0);

  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief requests a new header
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t* RequestHeader (TRI_headers_t* h,
                                      size_t size) {
  simple_headers_t* headers = (simple_headers_t*) h;
  TRI_doc_mptr_t* header;

  TRI_ASSERT(size > 0);
  
  if (headers->_freelist == nullptr) {
    size_t blockSize = GetBlockSize(headers->_blocks._length);
    TRI_ASSERT(blockSize > 0);

    TRI_doc_mptr_t* begin;
    try {
      begin = new TRI_doc_mptr_t[blockSize];
    }
    catch (std::exception& e) {
      begin = nullptr;
    }

    // out of memory
    if (begin == nullptr) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
    
    TRI_doc_mptr_t* ptr = begin + (blockSize - 1);

    header = nullptr;

    for (;  begin <= ptr;  ptr--) {
      ptr->setDataPtr(header); // ONLY IN HEADERS
      header = ptr;
    }

    TRI_ASSERT(headers != nullptr);
    headers->_freelist = header;

    TRI_PushBackVectorPointer(&headers->_blocks, begin);
  }
  
  TRI_ASSERT(headers->_freelist != nullptr);

  TRI_doc_mptr_t* result = const_cast<TRI_doc_mptr_t*>(headers->_freelist); 
  TRI_ASSERT(result != nullptr);

  headers->_freelist = static_cast<TRI_doc_mptr_t const*>(result->getDataPtr()); // ONLY IN HEADERS
  result->setDataPtr(nullptr); // ONLY IN HEADERS

  // put new header at the end of the list
  if (headers->_begin == nullptr) {
    // list of headers is empty
    TRI_ASSERT(headers->_nrLinked == 0);
    TRI_ASSERT(headers->_totalSize == 0);

    headers->_begin = result;
    headers->_end   = result;

    result->_prev   = nullptr;
    result->_next   = nullptr;
  }
  else {
    // list is not empty
    TRI_ASSERT(headers->_nrLinked > 0);
    TRI_ASSERT(headers->_totalSize > 0);
    TRI_ASSERT(headers->_nrAllocated > 0);
    TRI_ASSERT(headers->_begin != nullptr);
    TRI_ASSERT(headers->_end != nullptr);

    headers->_end->_next = result;
    result->_prev        = headers->_end;
    result->_next        = nullptr;
    headers->_end        = result;
  }

  headers->_nrAllocated++;
  headers->_nrLinked++;
  headers->_totalSize += (int64_t) TRI_DF_ALIGN_BLOCK(size);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a header, putting it back onto the freelist
////////////////////////////////////////////////////////////////////////////////

static void ReleaseHeader (TRI_headers_t* h, 
                           TRI_doc_mptr_t* header,
                           bool unlinkHeader) {
  simple_headers_t* headers = (simple_headers_t*) h;
  
  if (header == nullptr) {
    return;
  }

  if (unlinkHeader) {
    UnlinkHeader(h, header);
  }

  ClearHeader(h, header);

  header->setDataPtr(headers->_freelist); // ONLY IN HEADERS
  headers->_freelist = header;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the element at the head of the list
///
/// note: the element returned might be nullptr
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t* FrontHeaders (TRI_headers_t const* h) {
  simple_headers_t* headers = (simple_headers_t*) h;

  return headers->_begin;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the element at the tail of the list
///
/// note: the element returned might be nullptr
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
/// @brief return the total size of linked headers
////////////////////////////////////////////////////////////////////////////////

static int64_t TotalSizeHeaders (TRI_headers_t const* h) {
  simple_headers_t* headers = (simple_headers_t*) h;

  return (int64_t) headers->_totalSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adjust the total size of the markers handed out
/// this is called by the collector
////////////////////////////////////////////////////////////////////////////////

static void AdjustTotalSizeHeaders (TRI_headers_t const* h,
                                    int64_t oldSize,
                                    int64_t newSize) {
  simple_headers_t* headers = (simple_headers_t*) h;

  // oldSize = size of marker in WAL
  // newSize = size of marker in datafile 

  TRI_ASSERT(oldSize >= newSize);
  headers->_totalSize -= (oldSize - newSize);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new simple headers structures
////////////////////////////////////////////////////////////////////////////////

TRI_headers_t* TRI_CreateSimpleHeaders () {
  simple_headers_t* headers = static_cast<simple_headers_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(simple_headers_t), false));

  if (headers == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  headers->_freelist            = nullptr;
  headers->_begin               = nullptr;
  headers->_end                 = nullptr;
  headers->_nrAllocated         = 0;
  headers->_nrLinked            = 0;
  headers->_totalSize           = 0;

  headers->base.request         = RequestHeader;
  headers->base.release         = ReleaseHeader;
  headers->base.moveBack        = MoveBackHeader;
  headers->base.move            = MoveHeader;
  headers->base.relink          = RelinkHeader;
  headers->base.unlink          = UnlinkHeader;
  headers->base.front           = FrontHeaders;
  headers->base.back            = BackHeaders;
  headers->base.count           = CountHeaders;
  headers->base.size            = TotalSizeHeaders;
  headers->base.adjustTotalSize = AdjustTotalSizeHeaders;

  TRI_InitVectorPointer2(&headers->_blocks, TRI_UNKNOWN_MEM_ZONE, 8);

  return &headers->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a simple headers structures, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySimpleHeaders (TRI_headers_t* h) {
  simple_headers_t* headers = (simple_headers_t*) h;

  for (size_t i = 0;  i < headers->_blocks._length;  ++i) {
    delete[] static_cast<TRI_doc_mptr_t*>(headers->_blocks._buffer[i]);
  }

  headers->_nrAllocated = 0;
  headers->_nrLinked    = 0;
  headers->_totalSize   = 0;

  TRI_DestroyVectorPointer(&headers->_blocks);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a simple headers structures, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSimpleHeaders (TRI_headers_t* headers) {
  TRI_DestroySimpleHeaders(headers);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, headers);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
