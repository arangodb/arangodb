////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "headers.h"
#include "Basics/Logger.h"
#include "VocBase/document-collection.h"

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

static inline size_t GetBlockSize(size_t blockNumber) {
  static size_t const BLOCK_SIZE_UNIT = 128;

  if (blockNumber < 8) {
    // use a small block size in the beginning to save memory
    return (size_t)(BLOCK_SIZE_UNIT << blockNumber);
  }

  // use a block size of 32768
  // this will use 32768 * sizeof(TRI_doc_mptr_t) bytes, i.e. 1.5 MB
  return (size_t)(BLOCK_SIZE_UNIT << 8);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the headers
////////////////////////////////////////////////////////////////////////////////

TRI_headers_t::TRI_headers_t()
    : _freelist(nullptr),
      _begin(nullptr),
      _end(nullptr),
      _nrAllocated(0),
      _nrLinked(0),
      _totalSize(0),
      _blocks() {
  _blocks.reserve(16);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the headers
////////////////////////////////////////////////////////////////////////////////

TRI_headers_t::~TRI_headers_t() {
  for (auto& it : _blocks) {
    delete[] it;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves an existing header to the end of the list
/// this is called when there is an update operation on a document
////////////////////////////////////////////////////////////////////////////////

void TRI_headers_t::moveBack(TRI_doc_mptr_t* header, TRI_doc_mptr_t const* old) {
  if (header == nullptr) {
    return;
  }

  validate("before moveback", header, old);

  TRI_ASSERT(_nrAllocated > 0);
  TRI_ASSERT(_nrLinked > 0);
  TRI_ASSERT(_totalSize > 0);

  // we have at least one element in the list
  TRI_ASSERT(_begin != nullptr);
  TRI_ASSERT(_end != nullptr);
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);

  TRI_ASSERT(old != nullptr);
  TRI_ASSERT(old->getDataPtr() !=
             nullptr);  // ONLY IN HEADERS, PROTECTED by RUNTIME

  int64_t newSize =
      (int64_t)(((TRI_df_marker_t*)header->getDataPtr())
                    ->_size);  // ONLY IN HEADERS, PROTECTED by RUNTIME
  int64_t oldSize =
      (int64_t)(((TRI_df_marker_t*)old->getDataPtr())
                    ->_size);  // ONLY IN HEADERS, PROTECTED by RUNTIME

  // we must adjust the size of the collection
  _totalSize += (TRI_DF_ALIGN_BLOCK(newSize) - TRI_DF_ALIGN_BLOCK(oldSize));

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

  header->_prev = _end;
  header->_next = nullptr;
  _end = header;
  header->_prev->_next = header;

  TRI_ASSERT(_begin != nullptr);
  TRI_ASSERT(_end != nullptr);
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);

  TRI_ASSERT(_totalSize > 0);

  validate("after moveback", header, old);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlinks a header from the linked list, without freeing it
////////////////////////////////////////////////////////////////////////////////

void TRI_headers_t::unlink(TRI_doc_mptr_t* header) {
  validate("before unlink", header, nullptr);

  TRI_ASSERT(header != nullptr);
  TRI_ASSERT(header->getDataPtr() !=
             nullptr);  // ONLY IN HEADERS, PROTECTED by RUNTIME
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);

  int64_t size = (int64_t)((TRI_df_marker_t*)header->getDataPtr())
             ->_size;  // ONLY IN HEADERS, PROTECTED by RUNTIME
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
  } else {
    TRI_ASSERT(_begin != nullptr);
    TRI_ASSERT(_end != nullptr);
    TRI_ASSERT(_totalSize > 0);
  }

  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);

  validate("after unlink", header, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves a header around in the list, using its previous position
/// (specified in "old"), note that this is only used in revert operations
////////////////////////////////////////////////////////////////////////////////

void TRI_headers_t::move(TRI_doc_mptr_t* header, TRI_doc_mptr_t const* old) {
  if (header == nullptr) {
    return;
  }

  validate("before move", header, old);

  TRI_ASSERT(_nrAllocated > 0);
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);
  TRI_ASSERT(header->getDataPtr() !=
             nullptr);  // ONLY IN HEADERS, PROTECTED by RUNTIME
  TRI_ASSERT(((TRI_df_marker_t*)header->getDataPtr())->_size >
             0);  // ONLY IN HEADERS, PROTECTED by RUNTIME
  TRI_ASSERT(old != nullptr);
  TRI_ASSERT(old->getDataPtr() !=
             nullptr);  // ONLY IN HEADERS, PROTECTED by RUNTIME

  int64_t newSize =
      (int64_t)(((TRI_df_marker_t*)header->getDataPtr())
                    ->_size);  // ONLY IN HEADERS, PROTECTED by RUNTIME
  int64_t oldSize =
      (int64_t)(((TRI_df_marker_t*)old->getDataPtr())
                    ->_size);  // ONLY IN HEADERS, PROTECTED by RUNTIME

  // Please note the following: This operation is only used to revert an
  // update operation. The "new" document is removed again and the "old"
  // one is used once more. Therefore, the signs in the following statement
  // are actually OK:
  _totalSize -= (TRI_DF_ALIGN_BLOCK(newSize) - TRI_DF_ALIGN_BLOCK(oldSize));

  // first unlink
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

  // now header is unlinked
  
  header->_prev = old->_prev;
  header->_next = old->_next;

  if (old->_prev != nullptr) {
    old->_prev->_next = header;
  }
  if (old->_next != nullptr) {
    old->_next->_prev = header;
  }

  if (old->_prev == nullptr) {
    _begin = header;
  }
  if (old->_next == nullptr) {
    _end = header;
  }

  TRI_ASSERT(_begin != nullptr);
  TRI_ASSERT(_end != nullptr);
  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);

  validate("after move", header, old);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves a header back into the list, using its previous position
/// (specified in "old")
////////////////////////////////////////////////////////////////////////////////

void TRI_headers_t::relink(TRI_doc_mptr_t* header, TRI_doc_mptr_t const* old) {
  if (header == nullptr) {
    return;
  }

  validate("before relink", header, old);

  TRI_ASSERT(header->getDataPtr() !=
             nullptr);  // ONLY IN HEADERS, PROTECTED by RUNTIME

  int64_t size = (int64_t)((TRI_df_marker_t*)header->getDataPtr())
                     ->_size;  // ONLY IN HEADERS, PROTECTED by RUNTIME
  TRI_ASSERT(size > 0);

  TRI_ASSERT(_begin != header);
  TRI_ASSERT(_end != header);

  this->move(header, old);
  _nrLinked++;
  _totalSize += TRI_DF_ALIGN_BLOCK(size);
  TRI_ASSERT(_totalSize > 0);

  TRI_ASSERT(header->_prev != header);
  TRI_ASSERT(header->_next != header);

  validate("after relink", header, old);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief requests a new header
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* TRI_headers_t::request(size_t size) {
  TRI_doc_mptr_t* header;

  TRI_ASSERT(size > 0);

  if (_freelist == nullptr) {
    size_t blockSize = GetBlockSize(_blocks.size());
    TRI_ASSERT(blockSize > 0);

    TRI_doc_mptr_t* begin;
    try {
      begin = new TRI_doc_mptr_t[blockSize];
    } catch (std::exception&) {
      begin = nullptr;
    }

    // out of memory
    if (begin == nullptr) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    TRI_doc_mptr_t* ptr = begin + (blockSize - 1);

    header = nullptr;

    for (; begin <= ptr; ptr--) {
      ptr->setDataPtr(header);  // ONLY IN HEADERS
      header = ptr;
    }

    _freelist = header;

    try {
      _blocks.emplace_back(begin);
    } catch (...) {
      // out of memory
      delete[] begin;
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  TRI_ASSERT(_freelist != nullptr);

  TRI_doc_mptr_t* result = const_cast<TRI_doc_mptr_t*>(_freelist);
  TRI_ASSERT(result != nullptr);

  _freelist = static_cast<TRI_doc_mptr_t const*>(
      result->getDataPtr());    // ONLY IN HEADERS, PROTECTED by RUNTIME
  result->setDataPtr(nullptr);  // ONLY IN HEADERS

  // put new header at the end of the list
  if (_begin == nullptr) {
    // list of headers is empty
    TRI_ASSERT(_nrLinked == 0);
    TRI_ASSERT(_totalSize == 0);

    _begin = result;
    _end = result;

    result->_prev = nullptr;
    result->_next = nullptr;
  } else {
    // list is not empty
    TRI_ASSERT(_nrLinked > 0);
    TRI_ASSERT(_totalSize > 0);
    TRI_ASSERT(_nrAllocated > 0);
    TRI_ASSERT(_begin != nullptr);
    TRI_ASSERT(_end != nullptr);

    _end->_next = result;
    result->_prev = _end;
    result->_next = nullptr;
    _end = result;
  }

  _nrAllocated++;
  _nrLinked++;
  _totalSize += (int64_t)TRI_DF_ALIGN_BLOCK(size);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a header, putting it back onto the freelist
////////////////////////////////////////////////////////////////////////////////

void TRI_headers_t::release(TRI_doc_mptr_t* header, bool unlinkHeader) {
  if (header == nullptr) {
    return;
  }

  validate("before release", header, nullptr);

  if (unlinkHeader) {
    this->unlink(header);
  }

  header->clear();
  TRI_ASSERT(_nrAllocated > 0);
  _nrAllocated--;

  header->setDataPtr(_freelist);  // ONLY IN HEADERS
  _freelist = header;

  if (_nrAllocated == 0 && _blocks.size() >= 8) {
    // if this was the last header, we can safely reclaim some
    // memory by freeing all already-allocated blocks and wiping the freelist
    // we only do this if we had allocated 8 blocks of headers
    // this limit is arbitrary, but will ensure we only free memory if
    // it is sensible and not every time the last document is removed

    for (auto& it : _blocks) {
      delete[] it;
    }
    _blocks.clear();

    _freelist = nullptr;
    _begin = nullptr;
    _end = nullptr;
  }

  validate("after release", header, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adjust the total size of the markers handed out
/// this is called by the collector
////////////////////////////////////////////////////////////////////////////////

void TRI_headers_t::adjustTotalSize(int64_t oldSize, int64_t newSize) {
  // oldSize = size of marker in WAL
  // newSize = size of marker in datafile

  _totalSize -= (TRI_DF_ALIGN_BLOCK(oldSize) - TRI_DF_ALIGN_BLOCK(newSize));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validates the linked list
////////////////////////////////////////////////////////////////////////////////

#ifdef VALIDATE_MASTER_POINTERS
void TRI_headers_t::validate (char const* msg, 
                              TRI_doc_mptr_t const* header, 
                              TRI_doc_mptr_t const* old) {
  LOG(TRACE) << "validating master pointers for " << (void*) this << ", stage: " << msg;
  LOG(TRACE) << "begin: " << (void*) _begin << ", end: " << (void*) _end << ", nrLinked: " << _nrLinked;
  
  if (header != nullptr) {
    LOG(TRACE) << "header: " << (void*) header << ", rid: " << header->_rid << ", fid: " << header->_fid << ", prev: " << (void*) header->_prev << ", next: " << (void*) header->_next << ", data: " << (void*) header->getDataPtr() << ", key: " << TRI_EXTRACT_MARKER_KEY(header);
  }
  if (old != nullptr) {
    LOG(TRACE) << "old: " << (void*) old << ", rid: " << old->_rid << ", fid: " << old->_fid << ", prev: " << (void*) old->_prev << ", next: " << (void*) old->_next << ", data: " << (void*) old->getDataPtr() << ", key: " << TRI_EXTRACT_MARKER_KEY(old);
  }
  
  auto current = _begin;
  size_t i = 0;
  while (current != nullptr) {
    LOG(TRACE) << "- mptr #" << ++i << ", current: " << (void*) current << ", rid: " << current->_rid << ", fid: " << current->_fid << ", prev: " << (void*) current->_prev << ", next: " << (void*) current->_next << ", data: " << (void*) current->getDataPtr() << ", key: " << TRI_EXTRACT_MARKER_KEY(current);
    
    if (current->_next == nullptr) {
      // last element
      break;
    }
    TRI_ASSERT(current == current->_next->_prev);
    decltype(current) last = current;
    current = current->_next;
    TRI_ASSERT(last == current->_prev);
  }
  TRI_ASSERT(current == _end);
}
#endif

