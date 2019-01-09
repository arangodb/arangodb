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

#include "ShortStringStorage.h"
#include "Aql/ResourceUsage.h"
#include "Basics/Exceptions.h"
#include "Basics/tri-strings.h"

using namespace arangodb::aql;

/// @brief create a short string storage instance
ShortStringStorage::ShortStringStorage(ResourceMonitor* resourceMonitor, size_t blockSize)
    : _resourceMonitor(resourceMonitor),
      _blockSize(blockSize),
      _current(nullptr),
      _end(nullptr) {
  TRI_ASSERT(blockSize >= 64);
}

/// @brief destroy a short string storage instance
ShortStringStorage::~ShortStringStorage() {
  for (auto& it : _blocks) {
    _resourceMonitor->decreaseMemoryUsage(_blockSize);
    delete[] it;
  }
}

/// @brief register a short string
char* ShortStringStorage::registerString(char const* p, size_t length) {
  TRI_ASSERT(length <= maxStringLength);

  if (_current == nullptr || (_current + length + 1 > _end)) {
    allocateBlock();
  }

  TRI_ASSERT(!_blocks.empty());
  TRI_ASSERT(_current != nullptr);
  TRI_ASSERT(_end != nullptr);
  TRI_ASSERT(_current + length + 1 <= _end);

  char* position = _current;
  memcpy(static_cast<void*>(position), p, length);
  // add NUL byte at the end
  _current[length] = '\0';
  _current += length + 1;

  return position;
}

/// @brief register a short string, unescaping it
char* ShortStringStorage::unescape(char const* p, size_t length, size_t* outLength) {
  TRI_ASSERT(length <= maxStringLength);

  if (_current == nullptr || (_current + length + 1 > _end)) {
    allocateBlock();
  }

  TRI_ASSERT(!_blocks.empty());
  TRI_ASSERT(_current != nullptr);
  TRI_ASSERT(_end != nullptr);
  TRI_ASSERT(_current + length + 1 <= _end);

  char* position = _current;
  *outLength = TRI_UnescapeUtf8StringInPlace(_current, p, length);
  // add NUL byte at the end
  _current[*outLength] = '\0';
  _current += *outLength + 1;

  return position;
}

/// @brief allocate a new block of memory
void ShortStringStorage::allocateBlock() {
  char* buffer = new char[_blockSize];

  try {
    _resourceMonitor->increaseMemoryUsage(_blockSize);
    try {
      _blocks.emplace_back(buffer);
    } catch (...) {
      // rollback
      _resourceMonitor->decreaseMemoryUsage(_blockSize);
      throw;
    }
    _current = buffer;
    _end = _current + _blockSize;
  } catch (...) {
    delete[] buffer;
    throw;
  }
}
