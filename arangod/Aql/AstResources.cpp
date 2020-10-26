////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/ResourceUsage.h"
#include "Basics/Exceptions.h"
#include "Basics/tri-strings.h"
#include "Basics/ScopeGuard.h"

#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
/// @brief empty string singleton
char const* emptyString = "";
}  // namespace

AstResources::AstResources(ResourceMonitor* resourceMonitor)
    : _resourceMonitor(resourceMonitor),
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _stringsLength(0),
#endif
      _shortStringStorage(_resourceMonitor, 1024) {
}

AstResources::~AstResources() {
  // free strings
  for (auto& it : _strings) {
    TRI_FreeString(it);
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // we are in the destructor here already. decreasing the memory usage counters
  // will only provide a benefit (in terms of assertions) if we are in
  // maintainer mode, so we can save all these operations in non-maintainer mode
  _resourceMonitor->decreaseMemoryUsage(_strings.capacity() * sizeof(char*) + _stringsLength);
  _resourceMonitor->decreaseMemoryUsage(_nodes.numUsed() * sizeof(AstNode));
#endif
}

/// @brief create and register an AstNode
AstNode* AstResources::registerNode(AstNodeType type) {
  // may throw
  _resourceMonitor->increaseMemoryUsage(sizeof(AstNode));

  try {
    return _nodes.allocate(type);
  } catch (...) {
    _resourceMonitor->decreaseMemoryUsage(sizeof(AstNode));
    throw;
  }
}

/// @brief create and register an AstNode
AstNode* AstResources::registerNode(Ast* ast, arangodb::velocypack::Slice slice) {
  // may throw
  _resourceMonitor->increaseMemoryUsage(sizeof(AstNode));

  try {
    return _nodes.allocate(ast, slice);
  } catch (...) {
    _resourceMonitor->decreaseMemoryUsage(sizeof(AstNode));
    throw;
  }
}

/// @brief register a string
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

/// @brief register a potentially UTF-8-escaped string
/// the string is freed when the query is destroyed
char* AstResources::registerEscapedString(char const* p, size_t length, size_t& outLength) {
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

  size_t capacity;

  if (_strings.empty()) {
    // reserve some initial space for string storage
    capacity = 8;
  } else {
    capacity = _strings.size() + 1;
    if (capacity > _strings.capacity()) {
      capacity *= 2;
    }
  }

  TRI_ASSERT(capacity > _strings.size());

  // reserve space
  if (capacity > _strings.capacity()) {
    // not enough capacity...
    _resourceMonitor->increaseMemoryUsage(
        ((capacity - _strings.capacity()) * sizeof(char*)) + length);
    try {
      _strings.reserve(capacity);
    } catch (...) {
      // revert change in memory increase
      _resourceMonitor->decreaseMemoryUsage(
          ((capacity - _strings.capacity()) * sizeof(char*)) + length);
      throw;
    }
  } else {
    // got enough capacity for the new string
    _resourceMonitor->increaseMemoryUsage(length);
  }

  // will not fail
  _strings.push_back(copy);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _stringsLength += length;
#endif

  // safely took over the ownership fo the string, cancel the deletion now
  guard.cancel();

  return copy;
}
