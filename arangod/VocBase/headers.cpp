////////////////////////////////////////////////////////////////////////////////
/// @brief headers
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "headers.h"

#include "Basics/logging.h"
#include "VocBase/document-collection.h"

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

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors & destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the headers
////////////////////////////////////////////////////////////////////////////////

TRI_headers_t::TRI_headers_t ()
  : _freelist(nullptr),
    _begin(nullptr),
    _end(nullptr),
    _nrAllocated(0),
    _nrLinked(0),
    _totalSize(0) {

  TRI_InitVectorPointer2(&_blocks, TRI_UNKNOWN_MEM_ZONE, 16);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the headers
////////////////////////////////////////////////////////////////////////////////

TRI_headers_t::~TRI_headers_t () {
  for (size_t i = 0;  i < _blocks._length;  ++i) {
    delete[] static_cast<TRI_doc_mptr_t*>(_blocks._buffer[i]);
  }

  TRI_DestroyVectorPointer(&_blocks);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief moves an existing header to the end of the list
/// this is called when there is an update operation on a document
////////////////////////////////////////////////////////////////////////////////

void TRI_headers_t::moveBack (TRI_doc_mptr_t* header,
                              TRI_doc_mptr_t* old) {
  if (header == nullptr) {
    return;
  }

  TRI_ASSERT(_nrAllocated > 0);
  TRI_ASSERT(_nrLinked > 0);
  TRI_ASSERT(_totalSize > 0);

  // we have at least one element in the list
  TRI_ASSERT(_begin != nullptr);
  TRI_ASSERT(_end != nullptr);
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);

  TRI_ASSERT(old != nullptr);
  TRI_ASSERT(old->getDataPtr() != nullptr);  // ONLY IN HEADERS, PROTECTED by RUNTIME

  int64_t newSize = (int64_t) (((TRI_df_marker_t*) header->getDataPtr())->_size);  // ONLY IN HEADERS, PROTECTED by RUNTIME
  int64_t oldSize = (int64_t) (((TRI_df_marker_t*) old->getDataPtr())->_size);  // ONLY IN HEADERS, PROTECTED by RUNTIME

  // we must adjust the size of the collection
  _totalSize += (  TRI_DF_ALIGN_BLOCK(newSize)
                 - TRI_DF_ALIGN_BLOCK(oldSize));

  if (_end == header) {
    // header is already at the end
    TRI_ASSERT(header->_next == nullptr);
    return;
  }

  TRI_ASSERT(_begin != _end);

  // unlink the element
  if (header->_prev != nullptr) {
    header->_prev->_next = header->_next;
  }
  if (header->_next != nullptr) {
    header->_next->_prev = header->_prev;
  }

  if (_begin == header) {
    TRI_ASSERT(header->_next != nullptr);
    _begin = header->_next;
  }

  header->_prev   = _end;
  header->_next   = nullptr;
  _end            = header;
  header->_prev->_next = header;

  TRI_ASSERT(_begin != nullptr);
  TRI_ASSERT(_end != nullptr);
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);

  TRI_ASSERT(_totalSize > 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlinks a header from the linked list, without freeing it
////////////////////////////////////////////////////////////////////////////////

void TRI_headers_t::unlink (TRI_doc_mptr_t* header) {
  int64_t size;

  TRI_ASSERT(header != nullptr);
  TRI_ASSERT(header->getDataPtr() != nullptr); // ONLY IN HEADERS, PROTECTED by RUNTIME
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);

  size = (int64_t) ((TRI_df_marker_t*) header->getDataPtr())->_size; // ONLY IN HEADERS, PROTECTED by RUNTIME
  TRI_ASSERT(size > 0);

  // unlink the header
  if (header->_prev != nullptr) {
    header->_prev->_next = header->_next;
  }

  if (header->_next != nullptr) {
    header->_next->_prev = header->_prev;
  }

  // adjust begin & end pointers
  if (_begin == header) {
    _begin = header->_next;
  }

  if (_end == header) {
    _end = header->_prev;
  }

  TRI_ASSERT(_begin != header);
  TRI_ASSERT(_end != header);

  TRI_ASSERT(_nrLinked > 0);
  _nrLinked--;
  _totalSize -= TRI_DF_ALIGN_BLOCK(size);

  if (_nrLinked == 0) {
    TRI_ASSERT(_begin == nullptr);
    TRI_ASSERT(_end == nullptr);
    TRI_ASSERT(_totalSize == 0);
  }
  else {
    TRI_ASSERT(_begin != nullptr);
    TRI_ASSERT(_end != nullptr);
    TRI_ASSERT(_totalSize > 0);
  }

  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves a header around in the list, using its previous position
/// (specified in "old"), note that this is only used in revert operations
////////////////////////////////////////////////////////////////////////////////

void TRI_headers_t::move (TRI_doc_mptr_t* header,
                          TRI_doc_mptr_t* old) {
  if (header == nullptr) {
    return;
  }

  TRI_ASSERT(_nrAllocated > 0);
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);
  TRI_ASSERT(header->getDataPtr() != nullptr); // ONLY IN HEADERS, PROTECTED by RUNTIME
  TRI_ASSERT(((TRI_df_marker_t*) header->getDataPtr())->_size > 0); // ONLY IN HEADERS, PROTECTED by RUNTIME
  TRI_ASSERT(old != nullptr);
  TRI_ASSERT(old->getDataPtr() != nullptr); // ONLY IN HEADERS, PROTECTED by RUNTIME

  int64_t newSize = (int64_t) (((TRI_df_marker_t*) header->getDataPtr())->_size); // ONLY IN HEADERS, PROTECTED by RUNTIME
  int64_t oldSize = (int64_t) (((TRI_df_marker_t*) old->getDataPtr())->_size); // ONLY IN HEADERS, PROTECTED by RUNTIME

  // Please note the following: This operation is only used to revert an
  // update operation. The "new" document is removed again and the "old"
  // one is used once more. Therefore, the signs in the following statement
  // are actually OK:
  _totalSize -= (  TRI_DF_ALIGN_BLOCK(newSize)
                 - TRI_DF_ALIGN_BLOCK(oldSize));

  // adjust list start and end pointers
  if (old->_prev == nullptr) {
    _begin = header;
  }
  else if (_begin == header) {
    if (old->_prev != nullptr) {
      _begin = old->_prev;
    }
  }

  if (old->_next == nullptr) {
    _end = header;
  }
  else if (_end == header) {
    if (old->_next != nullptr) {
      _end = old->_next;
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

  TRI_ASSERT(_begin != nullptr);
  TRI_ASSERT(_end != nullptr);
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves a header back into the list, using its previous position
/// (specified in "old")
////////////////////////////////////////////////////////////////////////////////

void TRI_headers_t::relink (TRI_doc_mptr_t* header,
                            TRI_doc_mptr_t* old) {
  if (header == nullptr) {
    return;
  }

  TRI_ASSERT(header->getDataPtr() != nullptr); // ONLY IN HEADERS, PROTECTED by RUNTIME

  int64_t size = (int64_t) ((TRI_df_marker_t*) header->getDataPtr())->_size; // ONLY IN HEADERS, PROTECTED by RUNTIME
  TRI_ASSERT(size > 0);

  TRI_ASSERT(_begin != header);
  TRI_ASSERT(_end != header);

  this->move(header, old);
  _nrLinked++;
  _totalSize += TRI_DF_ALIGN_BLOCK(size);
  TRI_ASSERT(_totalSize > 0);

  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief requests a new header
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* TRI_headers_t::request (size_t size) {
  TRI_doc_mptr_t* header;

  TRI_ASSERT(size > 0);

  if (_freelist == nullptr) {
    size_t blockSize = GetBlockSize(_blocks._length);
    TRI_ASSERT(blockSize > 0);

    TRI_doc_mptr_t* begin;
    try {
      begin = new TRI_doc_mptr_t[blockSize];
    }
    catch (std::exception&) {
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

    _freelist = header;

    TRI_PushBackVectorPointer(&_blocks, begin);
  }

  TRI_ASSERT(_freelist != nullptr);

  TRI_doc_mptr_t* result = const_cast<TRI_doc_mptr_t*>(_freelist);
  TRI_ASSERT(result != nullptr);

  _freelist = static_cast<TRI_doc_mptr_t const*>(result->getDataPtr()); // ONLY IN HEADERS, PROTECTED by RUNTIME
  result->setDataPtr(nullptr); // ONLY IN HEADERS

  // put new header at the end of the list
  if (_begin == nullptr) {
    // list of headers is empty
    TRI_ASSERT(_nrLinked == 0);
    TRI_ASSERT(_totalSize == 0);

    _begin = result;
    _end   = result;

    result->_prev   = nullptr;
    result->_next   = nullptr;
  }
  else {
    // list is not empty
    TRI_ASSERT(_nrLinked > 0);
    TRI_ASSERT(_totalSize > 0);
    TRI_ASSERT(_nrAllocated > 0);
    TRI_ASSERT(_begin != nullptr);
    TRI_ASSERT(_end != nullptr);

    _end->_next   = result;
    result->_prev = _end;
    result->_next = nullptr;
    _end          = result;
  }

  _nrAllocated++;
  _nrLinked++;
  _totalSize += (int64_t) TRI_DF_ALIGN_BLOCK(size);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a header, putting it back onto the freelist
////////////////////////////////////////////////////////////////////////////////

void TRI_headers_t::release (TRI_doc_mptr_t* header,
                             bool unlinkHeader) {
  if (header == nullptr) {
    return;
  }

  if (unlinkHeader) {
    this->unlink(header);
  }

  header->clear();
  TRI_ASSERT(_nrAllocated > 0);
  _nrAllocated--;

  header->setDataPtr(_freelist); // ONLY IN HEADERS
  _freelist = header;

  if (_nrAllocated == 0 && _blocks._length >= 8) {
    // if this was the last header, we can safely reclaim some
    // memory by freeing all already-allocated blocks and wiping the freelist
    // we only do this if we had allocated 8 blocks of headers
    // this limit is arbitrary, but will ensure we only free memory if
    // it is sensible and not everytime the last document is removed

    for (size_t i = 0;  i < _blocks._length;  ++i) {
      delete[] static_cast<TRI_doc_mptr_t*>(_blocks._buffer[i]);
      _blocks._buffer[i] = nullptr;
    }

    // set length to 0
    _blocks._length = 0;
    _freelist = nullptr;
    _begin = nullptr;
    _end = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adjust the total size of the markers handed out
/// this is called by the collector
////////////////////////////////////////////////////////////////////////////////

void TRI_headers_t::adjustTotalSize (int64_t oldSize,
                                     int64_t newSize) {
  // oldSize = size of marker in WAL
  // newSize = size of marker in datafile

  _totalSize -= (  TRI_DF_ALIGN_BLOCK(oldSize) 
                 - TRI_DF_ALIGN_BLOCK(newSize));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
