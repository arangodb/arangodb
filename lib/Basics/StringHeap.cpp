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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "StringHeap.h"

#include "Basics/Exceptions.h"
#include "Basics/ResourceUsage.h"
#include "Basics/debugging.h"

using namespace arangodb;

/// @brief create a StringHeap instance
StringHeap::StringHeap(ResourceMonitor& resourceMonitor, size_t blockSize)
    : _resourceMonitor(resourceMonitor),
      _blockSize(blockSize), 
      _current(nullptr), 
      _end(nullptr) {
  TRI_ASSERT(blockSize >= 64);
}

StringHeap::~StringHeap() {
  clear();
}

/// @brief register a string
arangodb::velocypack::StringRef StringHeap::registerString(char const* ptr, size_t length) {
  if (_current == nullptr || (_current + length + 1 > _end)) {
    allocateBlock();
  }

  TRI_ASSERT(!_blocks.empty());
  TRI_ASSERT(_current != nullptr);
  TRI_ASSERT(_end != nullptr);
  TRI_ASSERT(_current + length + 1 <= _end);

  char* position = _current;
  memcpy(static_cast<void*>(position), ptr, length);
  // null-terminate the string
  _current[length] = '\0';
  _current += length + 1;

  return arangodb::velocypack::StringRef(position, length);
}

void StringHeap::clear() noexcept {
  _current = nullptr;
  _end = nullptr;

  for (auto& it : _blocks) {
    delete[] it;
  }

  _resourceMonitor.decreaseMemoryUsage(_blocks.size() * (sizeof(char*) + _blockSize));
  _blocks.clear();
}

/// @brief allocate a new block of memory
void StringHeap::allocateBlock() {
  size_t capacity;
  if (_blocks.empty()) {
    // reserve some initial space
    capacity = 8;
  } else {
    capacity = _blocks.size() + 1;
    // allocate with power of 2 growth
    if (capacity > _blocks.capacity()) {
      capacity *= 2;
    }
  }

  TRI_ASSERT(capacity > _blocks.size());

  // reserve space
  if (capacity > _blocks.capacity()) {
    // if this fails, we don't have to rollback anything
    _blocks.reserve(capacity);
  }

  // may throw
  ResourceUsageScope scope(_resourceMonitor, sizeof(char*) + _blockSize);

  // if this fails, we don't have to rollback anything but the scope 
  // (which will do so automatically)
  char* buffer = new char[_blockSize];

  // the emplace_back will work, because we added enough capacity before
  TRI_ASSERT(_blocks.capacity() >= _blocks.size() + 1);
  _blocks.emplace_back(buffer);

  _current = buffer;
  _end = _current + _blockSize;

  // StringHeap now responsible for tracking the memory usage
  scope.steal();
}
