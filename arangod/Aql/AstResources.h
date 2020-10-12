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

#ifndef ARANGOD_AQL_AST_RESOURCES_H
#define ARANGOD_AQL_AST_RESOURCES_H 1

#include <string>
#include <vector>

#include "Aql/AstNode.h"
#include "Aql/ShortStringStorage.h"
#include "Basics/Common.h"
#include "Basics/FixedSizeAllocator.h"

namespace arangodb {
namespace velocypack {
class Slice;
}

namespace aql {

class Ast;
struct AstNode;
struct ResourceMonitor;

class AstResources {
 public:
  AstResources() = delete;
  AstResources(AstResources const&) = delete;
  AstResources& operator=(AstResources const&) = delete;

  explicit AstResources(ResourceMonitor*);
  ~AstResources();

  /// @brief create and register an AstNode
  AstNode* registerNode(AstNodeType type);
  
  /// @brief create and register an AstNode
  AstNode* registerNode(Ast*, arangodb::velocypack::Slice slice);

  /// @brief register a string
  /// the string is freed when the query is destroyed
  char* registerString(char const* p, size_t length);

  /// @brief register a string
  /// the string is freed when the query is destroyed
  char* registerString(std::string const& p) {
    return registerString(p.data(), p.size());
  }

  /// @brief register a potentially UTF-8-escaped string
  /// the string is freed when the query is destroyed
  char* registerEscapedString(char const* p, size_t length, size_t& outLength);

 private:
  /// @brief registers a long string and takes over the ownership for it
  char* registerLongString(char* copy, size_t length);

 private:
  ResourceMonitor* _resourceMonitor;

  /// @brief all nodes created in the AST - will be used for freeing them later
  FixedSizeAllocator<AstNode> _nodes;

  /// @brief strings created in the query - used for easy memory deallocation
  std::vector<char*> _strings;

  /// @brief cumulated length of strings in _strings
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  size_t _stringsLength;
#endif

  /// @brief short string storage. uses less memory allocations for short
  /// strings
  ShortStringStorage _shortStringStorage;
};

}  // namespace aql
}  // namespace arangodb

#endif
