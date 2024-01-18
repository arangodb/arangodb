////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

AstResources::AstResources(ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor),
      _stringsLength(0),
      _shortStringStorage(_resourceMonitor, 1024),
      _childNodes(0) {}

AstResources::~AstResources() {
  clear();
  size_t memoryUsage = (_strings.capacity() * memoryUsageForStringBlock()) +
                       (_childNodes * memoryUsageForChildNode());
  _resourceMonitor.decreaseMemoryUsage(memoryUsage);
}

void AstResources::reserveChildNodes(AstNode* node, size_t n) {
  // the following increaseMemoryUsage call can throw. if it does, then
  // we don't have to roll back any state
  _resourceMonitor.increaseMemoryUsage(n * memoryUsageForChildNode());
  // now that we successfully reserved memory in the query, we can safely
  // bump up the number of reserved childNodes.
  _childNodes += n;

  TRI_ASSERT(node != nullptr);
  // now reserve the actual space in the AstNode. this can still fail
  // with OOM, but even if it does, there is no need to roll back the
  // two modifications above. the number of child nodes and the tracked
  // memory will be cleared in this class' dtor nicely.
  node->reserve(n);
}

// frees all data
void AstResources::clear() noexcept {
  size_t memoryUsage = (_nodes.numUsed() * sizeof(AstNode)) + _stringsLength +
                       (_childNodes * memoryUsageForChildNode());
  _nodes.clear();
  clearStrings();
  _shortStringStorage.clear();
  _childNodes = 0;
  _resourceMonitor.decreaseMemoryUsage(memoryUsage);
}

// frees most data (keeps a bit of memory around to avoid later re-allocations)
void AstResources::clearMost() noexcept {
  size_t memoryUsage = (_nodes.numUsed() * sizeof(AstNode)) + _stringsLength +
                       (_childNodes * memoryUsageForChildNode());
  _nodes.clearMost();
  clearStrings();
  _shortStringStorage.clearMost();
  _childNodes = 0;
  _resourceMonitor.decreaseMemoryUsage(memoryUsage);
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
  // ensure extra capacity for at least one more node in the allocator.
  // note that this may throw, but then no state is modified here.
  _nodes.ensureCapacity();

  // now we can unconditionally increase the memory usage for the
  // one more node. if this throws, no harm is done.
  _resourceMonitor.increaseMemoryUsage(sizeof(AstNode));

  // _nodes.allocate() will not throw if we are only creating a single
  // node wihout subnodes, which is what we do here.
  return _nodes.allocate(type);
}

// create and register an AstNode
AstNode* AstResources::registerNode(Ast* ast, velocypack::Slice slice) {
  // ensure extra capacity for at least one more node in the allocator.
  // note that this may throw, but then no state is modified here.
  _nodes.ensureCapacity();

  // now we can unconditionally increase the memory usage for the
  // one more node. if this throws, no harm is done.
  _resourceMonitor.increaseMemoryUsage(sizeof(AstNode));

  // _nodes.allocate() will not throw if we are only creating a single
  // node wihout subnodes. however, if we create a node with subnodes,
  // then the call to allocate() will recursively create the child
  // nodes. this may run out of memory. however, in allocate() we
  // unconditionally increase the memory pointer for every node, so
  // even if the allocate() call throws, we can use _nodes.numUsed()
  // to determine the actual number of nodes that were created and we
  // can use that number of decreasing memory usage safely.
  return _nodes.allocate(ast, slice);
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
