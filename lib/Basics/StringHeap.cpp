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

using namespace arangodb;

/// @brief create a StringHeap instance
StringHeap::StringHeap(size_t blockSize)
    : _blocks(), _blockSize(blockSize), _current(nullptr), _end(nullptr) {
  TRI_ASSERT(blockSize >= 64);
}

StringHeap::~StringHeap() {
  for (auto& it : _blocks) {
    delete[] it;
  }
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

void StringHeap::clear() {
  _current = nullptr;
  _end = nullptr;

  for (auto& it : _blocks) {
    delete[] it;
  }
  _blocks.clear();
}

void StringHeap::merge(StringHeap&& heap) {
  _blocks.reserve(_blocks.size() + heap._blocks.size());
  _blocks.insert(_blocks.end(), heap._blocks.begin(), heap._blocks.end());
  heap._blocks.clear();
  heap._current = nullptr;
}

/// @brief allocate a new block of memory
void StringHeap::allocateBlock() {
  char* buffer = new char[_blockSize];

  try {
    _blocks.emplace_back(buffer);
    _current = buffer;
    _end = _current + _blockSize;
  } catch (...) {
    delete[] buffer;
    throw;
  }
}
