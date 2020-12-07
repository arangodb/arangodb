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

#include "QueryResources.h"
#include "Aql/AstNode.h"
#include "Basics/Exceptions.h"
#include "Basics/ResourceUsage.h"
#include "Basics/tri-strings.h"
#include "Basics/ScopeGuard.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {
/// @brief empty string singleton
static char const* EmptyString = "";
}  // namespace

QueryResources::QueryResources(arangodb::ResourceMonitor* resourceMonitor)
    : _resourceMonitor(resourceMonitor),
      _stringsLength(0),
      _shortStringStorage(_resourceMonitor, 1024) {
}

QueryResources::~QueryResources() {
  // free strings
  for (auto& it : _strings) {
    TRI_FreeString(it);
  }

  // free nodes
  for (auto& it : _nodes) {
    delete it;
  }

  size_t memoryUsage = (_strings.capacity() * sizeof(char*) + _stringsLength) +
                       (_nodes.capacity() * (sizeof(AstNode*) + sizeof(AstNode)));
  _resourceMonitor->decreaseMemoryUsage(memoryUsage);
}

/// @brief add a node to the list of nodes
void QueryResources::addNode(AstNode* node) {
  auto guard = scopeGuard([node]() {
    // in case something goes wrong, we must free the node we got to prevent
    // memleaks
    delete node;
  });

  size_t capacity;

  if (_nodes.empty()) {
    // reserve some initial space for nodes
    capacity = 64;
  } else {
    capacity = _nodes.size() + 1;
    if (capacity > _nodes.capacity()) {
      capacity *= 2;
    }
  }


  // reserve space for pointers
  if (capacity > _nodes.capacity()) {
    ResourceUsageScope guard(_resourceMonitor, (capacity - _nodes.capacity()) * (sizeof(AstNode*) + sizeof(AstNode)));

    _nodes.reserve(capacity);

    // now we are responsible for tracking the memory usage
    guard.steal();
  }

  // will not fail
  TRI_ASSERT(_nodes.capacity() > _nodes.size());
  _nodes.push_back(node);

  // safely took over the ownership for the node, cancel the deletion now
  guard.cancel();
}

/// @brief register a string
/// the string is freed when the query is destroyed
char* QueryResources::registerString(char const* p, size_t length) {
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
char* QueryResources::registerEscapedString(char const* p, size_t length, size_t& outLength) {
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
char* QueryResources::registerLongString(char* copy, size_t length) {
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
    ResourceUsageScope guard(_resourceMonitor, ((capacity - _strings.capacity()) * sizeof(char*)) + length);
      
    _strings.reserve(capacity);

    // we are now responsible for tracking the memory usage
    guard.steal();
  } else {
    // got enough capacity for the new string
    _resourceMonitor->increaseMemoryUsage(length);
  }

  // will not fail
  TRI_ASSERT(_strings.capacity() > _strings.size());
  _strings.push_back(copy);
  _stringsLength += length;

  // safely took over the ownership fo the string, cancel the deletion now
  guard.cancel();

  return copy;
}
