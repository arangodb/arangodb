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
#include "Aql/ResourceUsage.h"
#include "Basics/Exceptions.h"
#include "Basics/tri-strings.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {
/// @brief empty string singleton
static char const* EmptyString = "";
}

QueryResources::QueryResources(ResourceMonitor* resourceMonitor)
    : _resourceMonitor(resourceMonitor),
      _stringsLength(0),
      _shortStringStorage(_resourceMonitor, 1024) {}

QueryResources::~QueryResources() {
  // free strings
  for (auto& it : _strings) {
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, it);
  }
    
  _resourceMonitor->decreaseMemoryUsage(_strings.capacity() * sizeof(char*) + _stringsLength);

  // free nodes
  for (auto& it : _nodes) {
    delete it;
  }
  
  _resourceMonitor->decreaseMemoryUsage(_nodes.size() * sizeof(AstNode) + _nodes.capacity() * sizeof(AstNode*));
}
  
/// @brief add a node to the list of nodes
void QueryResources::addNode(AstNode* node) { 
  size_t capacity;

  if (_nodes.empty()) {
    // reserve some initial space for nodes 
    capacity = 16;
  } else {
    capacity = (std::max)(_nodes.size() + 8, _nodes.capacity());
  }

  TRI_ASSERT(capacity > _nodes.size());
  TRI_ASSERT(capacity >= _nodes.capacity());

  // reserve space
  _resourceMonitor->increaseMemoryUsage((capacity - _nodes.capacity()) * sizeof(AstNode*));
  _nodes.reserve(capacity);

  _resourceMonitor->increaseMemoryUsage(sizeof(AstNode));
  // will not fail
  _nodes.emplace_back(node); 
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

  if (length < ShortStringStorage::MaxStringLength) {
    return _shortStringStorage.registerString(p, length);
  }

  char* copy = TRI_DuplicateString(TRI_UNKNOWN_MEM_ZONE, p, length);
  return registerLongString(copy, length);
}

/// @brief register a potentially UTF-8-escaped string
/// the string is freed when the query is destroyed
char* QueryResources::registerEscapedString(char const* p, size_t length,
                                            size_t& outLength) {
  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (length == 0) {
    // optimization for the empty string
    outLength = 0;
    return const_cast<char*>(EmptyString);
  }

  char* copy = TRI_UnescapeUtf8String(TRI_UNKNOWN_MEM_ZONE, p, length, &outLength, false);
  return registerLongString(copy, outLength);
}

char* QueryResources::registerLongString(char* copy, size_t length) {  
  if (copy == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  try {
    size_t capacity;
    
    if (_strings.empty()) {
      // reserve some initial space for string storage 
      capacity = 8;
    } else {
      capacity = (std::max)(_strings.size() + 8, _strings.capacity());
    }

    TRI_ASSERT(capacity > _strings.size());
    TRI_ASSERT(capacity >= _strings.capacity());
    
    // reserve space
    _resourceMonitor->increaseMemoryUsage((capacity - _strings.size()) * sizeof(char*));
    _strings.reserve(capacity);
    
    _resourceMonitor->increaseMemoryUsage(length);
    // will not fail 
    _strings.emplace_back(copy);
    _stringsLength += length;

    return copy;
  } catch (...) {
    // prevent memleak
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, copy);
    throw;
  }
}
