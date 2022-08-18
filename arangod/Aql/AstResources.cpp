////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "AstResources.h"
#include "Aql/AstNode.h"
#include "Basics/Exceptions.h"
#include "Basics/ResourceUsage.h"
#include "Basics/ScopeGuard.h"
#include "Basics/tri-strings.h"

#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
// empty string singleton
char const* emptyString = "";
}  // namespace

AstResources::AstResources(arangodb::ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor),
      _stringsLength(0),
      _shortStringStorage(_resourceMonitor, 1024) {}

AstResources::~AstResources() {
  clear();
  size_t memoryUsage = _strings.capacity() * memoryUsageForStringBlock();
  _resourceMonitor.decreaseMemoryUsage(memoryUsage);
}

// frees all data
void AstResources::clear() noexcept {
  size_t memoryUsage = (_nodes.numUsed() * sizeof(AstNode)) + _stringsLength;
  _nodes.clear();
  clearStrings();
  _resourceMonitor.decreaseMemoryUsage(memoryUsage);

  _shortStringStorage.clear();
}

// frees most data (keeps a bit of memory around to avoid later re-allocations)
void AstResources::clearMost() noexcept {
  size_t memoryUsage = (_nodes.numUsed() * sizeof(AstNode)) + _stringsLength;
  _nodes.clearMost();
  clearStrings();
  _resourceMonitor.decreaseMemoryUsage(memoryUsage);

  _shortStringStorage.clearMost();
}

// clears dynamic string data. resets _strings and _stringsLength,
// but does not update _resourceMonitor!
void AstResources::clearStrings() noexcept {
  for (auto& it : _strings) {
    TRI_FreeString(it);
  }
  _strings.clear();
  _stringsLength = 0;
}

template<typename T>
size_t AstResources::newCapacity(T const& container,
                                 size_t initialCapacity) const noexcept {
  if (container.empty()) {
    // reserve some initial space for vector
    return initialCapacity;
  }

  size_t capacity = container.size() + 1;
  if (capacity > container.capacity()) {
    capacity = container.size() * 2;
  }
  TRI_ASSERT(container.size() + 1 <= capacity);
  return capacity;
}

// create and register an AstNode
AstNode* AstResources::registerNode(AstNodeType type) {
  // may throw
  ResourceUsageScope scope(_resourceMonitor, sizeof(AstNode));

  AstNode* node = _nodes.allocate(type);

  // now we are responsible for tracking the memory usage
  scope.steal();
  return node;
}

// create and register an AstNode
AstNode* AstResources::registerNode(Ast* ast,
                                    arangodb::velocypack::Slice slice) {
  // may throw
  ResourceUsageScope scope(_resourceMonitor, sizeof(AstNode));

  AstNode* node = _nodes.allocate(ast, slice);

  // now we are responsible for tracking the memory usage
  scope.steal();
  return node;
}

// register a string
/// the string is freed when the query is destroyed
char* AstResources::registerString(char const* p, size_t length) {
  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (length == 0) {
    // optimization for the empty string
    return const_cast<char*>(::emptyString);
  }

  if (length < ShortStringStorage::maxStringLength) {
    return _shortStringStorage.registerString(p, length);
  }

  char* copy = TRI_DuplicateString(p, length);
  return registerLongString(copy, length);
}

// register a potentially UTF-8-escaped string
/// the string is freed when the query is destroyed
char* AstResources::registerEscapedString(char const* p, size_t length,
                                          size_t& outLength) {
  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (length == 0) {
    // optimization for the empty string
    outLength = 0;
    return const_cast<char*>(::emptyString);
  }

  if (length < ShortStringStorage::maxStringLength) {
    return _shortStringStorage.unescape(p, length, &outLength);
  }

  char* copy = TRI_UnescapeUtf8String(p, length, &outLength, false);
  return registerLongString(copy, outLength);
}

// registers a long string and takes over the ownership for it
char* AstResources::registerLongString(char* copy, size_t length) {
  if (copy == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  auto guard = scopeGuard([copy]() noexcept {
    // in case something goes wrong, we must free the string we got to prevent
    // memleaks
    TRI_FreeString(copy);
  });

  size_t capacity = newCapacity(_strings, kMinCapacityForLongStrings);

  // reserve space
  if (capacity > _strings.capacity()) {
    // not enough capacity...
    ResourceUsageScope scope(
        _resourceMonitor,
        (capacity - _strings.capacity()) * memoryUsageForStringBlock() +
            length);

    _strings.reserve(capacity);

    // we are now responsible for tracking the memory usage
    scope.steal();
  } else {
    // got enough capacity for the new string
    _resourceMonitor.increaseMemoryUsage(length);
  }

  // will not fail
  TRI_ASSERT(_strings.capacity() > _strings.size());
  _strings.push_back(copy);
  _stringsLength += length;

  // safely took over the ownership fo the string, cancel the deletion now
  guard.cancel();

  return copy;
}
