////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Aql/AstNode.h"
#include "Aql/ShortStringStorage.h"
#include "Basics/Common.h"
#include "Basics/FixedSizeAllocator.h"

namespace arangodb {
struct ResourceMonitor;

namespace velocypack {
class Slice;
}

namespace aql {
class Ast;
struct AstNode;

class AstResources {
 public:
  AstResources() = delete;
  AstResources(AstResources const&) = delete;
  AstResources& operator=(AstResources const&) = delete;

  explicit AstResources(ResourceMonitor&);
  ~AstResources();

  // track number of child nodes of AstNodes. note: can throw!
  void reserveChildNodes(AstNode* node, size_t n);

  // frees all data
  void clear() noexcept;

  // frees most data (keeps a bit of memory around to avoid later
  // re-allocations)
  void clearMost() noexcept;

  // create and register an AstNode. note: can throw!
  AstNode* registerNode(AstNodeType type);

  // create and register an AstNode. note: can throw!
  AstNode* registerNode(Ast*, velocypack::Slice slice);

  // register a string
  // the string is freed when the query is destroyed.
  // note: can throw!
  char* registerString(char const* p, size_t length);

  // register a string
  // the string is freed when the query is destroyed.
  // note: can throw!
  char* registerString(std::string_view value) {
    return registerString(value.data(), value.size());
  }

  // register a potentially UTF-8-escaped string
  // the string is freed when the query is destroyed
  char* registerEscapedString(char const* p, size_t length, size_t& outLength);

  // return the memory usage for a block of strings
  constexpr static size_t memoryUsageForStringBlock() { return sizeof(char*); }

  // return the memory usage for an AstNode child node pointer
  constexpr static size_t memoryUsageForChildNode() { return sizeof(AstNode*); }

  // return the minimum capacity for long strings container
  constexpr static size_t kMinCapacityForLongStrings = 8;

 private:
  // clears dynamic string data. resets _strings and _stringsLength,
  // but does not update _resourceMonitor!
  void clearStrings() noexcept;

  // calculate the new capacity for the container
  template<typename T>
  size_t newCapacity(T const& container, size_t initialCapacity) const noexcept;

  // registers a long string and takes over the ownership for it
  char* registerLongString(char* copy, size_t length);

  // resource monitor used for tracking allocations/deallocations
  ResourceMonitor& _resourceMonitor;

  // all nodes created in the AST - will be used for freeing them later
  FixedSizeAllocator<AstNode> _nodes;

  // strings created in the query - used for easy memory deallocation
  std::vector<char*> _strings;

  // cumulated length of strings in _strings
  size_t _stringsLength;

  // short string storage. uses less memory allocations for short
  /// strings
  ShortStringStorage _shortStringStorage;

  // number of child node pointers (for which memory was allocated)
  size_t _childNodes;
};

}  // namespace aql
}  // namespace arangodb
