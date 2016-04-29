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

#include "MasterPointers.h"
#include "Logger/Logger.h"
#include "VocBase/MasterPointer.h"

using namespace arangodb;

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
    return static_cast<size_t>(BLOCK_SIZE_UNIT << blockNumber);
  }

  // use a block size of 32768
  // this will use 32768 * sizeof(TRI_doc_mptr_t) bytes, i.e. 1.5 MB
  return static_cast<size_t>(BLOCK_SIZE_UNIT << 8);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the headers
////////////////////////////////////////////////////////////////////////////////

MasterPointers::MasterPointers()
    : _freelist(nullptr),
      _nrAllocated(0),
      _blocks() {
  _blocks.reserve(16);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the headers
////////////////////////////////////////////////////////////////////////////////

MasterPointers::~MasterPointers() {
  for (auto& it : _blocks) {
    delete[] it;
  }
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief returns the memory usage
////////////////////////////////////////////////////////////////////////////////

uint64_t MasterPointers::memory() const {
  return _nrAllocated * sizeof(TRI_doc_mptr_t); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief requests a new header
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* MasterPointers::request() {
  TRI_doc_mptr_t* header;

  if (_freelist == nullptr) {
    size_t blockSize = GetBlockSize(_blocks.size());
    TRI_ASSERT(blockSize > 0);

    TRI_doc_mptr_t* begin;
    try {
      begin = new TRI_doc_mptr_t[blockSize];
    } catch (std::exception&) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    TRI_doc_mptr_t* ptr = begin + (blockSize - 1);

    header = nullptr;

    for (; begin <= ptr; ptr--) {
      ptr->setVPack(static_cast<void*>(header)); // ONLY IN HEADERS
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

  TRI_doc_mptr_t* result = _freelist;
  TRI_ASSERT(result != nullptr);

  _freelist = const_cast<TRI_doc_mptr_t*>(static_cast<TRI_doc_mptr_t const*>(result->dataptr()));
  result->setVPack(nullptr); 

  _nrAllocated++;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a header, putting it back onto the freelist
////////////////////////////////////////////////////////////////////////////////

void MasterPointers::release(TRI_doc_mptr_t* header) {
  if (header == nullptr) {
    return;
  }

  header->clear();
  TRI_ASSERT(_nrAllocated > 0);
  _nrAllocated--;

  header->setVPack(static_cast<void*>(_freelist)); 
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
  }
}

