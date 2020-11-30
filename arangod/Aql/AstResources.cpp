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

#include "AstResources.h"
#include "Aql/AstNode.h"
#include "Basics/Exceptions.h"
#include "Basics/ResourceUsage.h"
#include "Basics/ScopeGuard.h"
#include "Basics/tri-strings.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {
/// @brief empty string singleton
static char const* EmptyString = "";
}  // namespace

AstResources::AstResources(arangodb::ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor),
      _stringsLength(0),
      _shortStringStorage(_resourceMonitor, 1024) {
}

AstResources::~AstResources() {
  // free strings
  for (auto& it : _strings) {
    TRI_FreeString(it);
  }

  // free nodes
  for (auto& it : _nodes) {
    delete it;
  }
  
  size_t memoryUsage = 
      (_nodes.capacity() * memoryUsageForNode()) + 
      (_strings.capacity() * memoryUsageForStringBlock()) +
      _stringsLength;
  _resourceMonitor.decreaseMemoryUsage(memoryUsage);
}

template <typename T>
size_t AstResources::newCapacity(T const& container, size_t initialCapacity) const noexcept {
  if (container.empty()) {
    // reserve some initial space for vector
    return initialCapacity;
  }

  size_t capacity = container.size() + 1;
  if (capacity > container.capacity()) {
    capacity *= 2;
  }
  return capacity;
}

/// @brief add a node to the list of nodes
void AstResources::addNode(AstNode* node) {
  auto guard = scopeGuard([node]() {
    // in case something goes wrong, we must free the node we got to prevent
    // memleaks
    delete node;
  });

  size_t capacity = newCapacity(_nodes, 64);

  // reserve space for pointers
  if (capacity > _nodes.capacity()) {
    ResourceUsageScope scope(_resourceMonitor, (capacity - _nodes.capacity()) * memoryUsageForNode());

    _nodes.reserve(capacity);

    // now we are responsible for tracking the memory usage
    scope.steal();
  }

  // will not fail
  TRI_ASSERT(_nodes.capacity() > _nodes.size());
  _nodes.push_back(node);

  // safely took over the ownership for the node, cancel the deletion now
  guard.cancel();
}

/// @brief register a string
/// the string is freed when the query is destroyed
char* AstResources::registerString(char const* p, size_t length) {
  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (length == 0) {
    // optimization for the empty string
    return const_cast<char*>(EmptyString);
  }

  if (length < ShortStringStorage::maxStringLength) {
    return _shortStringStorage.registerString(p, length);
  }

  char* copy = TRI_DuplicateString(p, length);
  return registerLongString(copy, length);
}

/// @brief register a potentially UTF-8-escaped string
/// the string is freed when the query is destroyed
char* AstResources::registerEscapedString(char const* p, size_t length, size_t& outLength) {
  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (length == 0) {
    // optimization for the empty string
    outLength = 0;
    return const_cast<char*>(EmptyString);
  }

  if (length < ShortStringStorage::maxStringLength) {
    return _shortStringStorage.unescape(p, length, &outLength);
  }

  char* copy = TRI_UnescapeUtf8String(p, length, &outLength, false);
  return registerLongString(copy, outLength);
}

/// @brief registers a long string and takes over the ownership for it
char* AstResources::registerLongString(char* copy, size_t length) {
  if (copy == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  auto guard = scopeGuard([copy]() {
    // in case something goes wrong, we must free the string we got to prevent
    // memleaks
    TRI_FreeString(copy);
  });

  size_t capacity = newCapacity(_strings, 8);

  // reserve space
  if (capacity > _strings.capacity()) {
    // not enough capacity...
    ResourceUsageScope scope(_resourceMonitor, (capacity - _strings.capacity()) * memoryUsageForStringBlock() + length);
      
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

constexpr size_t AstResources::memoryUsageForStringBlock() const noexcept {
  return sizeof(char*);
}

constexpr size_t AstResources::memoryUsageForNode() const noexcept {
  return sizeof(AstNode*) + sizeof(AstNode);
}
