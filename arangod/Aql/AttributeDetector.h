////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/WalkerWorker.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"
#include "VocBase/AccessMode.h"

#include <string>
#include <string_view>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
class Ast;
struct AstNode;
class ExecutionPlan;
struct Variable;

class AttributeDetector final
    : public WalkerWorker<AstNode, WalkerUniqueness::NonUnique> {
 public:
  struct AttributeAccess {
    std::string name;
    bool read{false};
    bool write{false};
  };

  struct AttributeAccessInfo {
    bool requiresAll{false};
    std::vector<std::string> attributes;

    template<class Inspector>
    friend auto inspect(Inspector& f, AttributeAccessInfo& x) {
      return f.object(x).fields(
          f.field("requiresAll", x.requiresAll),
          f.field("attributes", x.attributes));
    }
  };

  struct CollectionAccess {
    std::string collectionName;
    containers::FlatHashSet<std::string> readAttributes;
    containers::FlatHashSet<std::string> writeAttributes;
    bool requiresAllAttributesRead{false};
    bool requiresAllAttributesWrite{false};

    template<class Inspector>
    friend auto inspect(Inspector& f, CollectionAccess& x) {
      AttributeAccessInfo read{x.requiresAllAttributesRead,
                               std::vector<std::string>(x.readAttributes.begin(),
                                                        x.readAttributes.end())};
      AttributeAccessInfo write{x.requiresAllAttributesWrite,
                                std::vector<std::string>(x.writeAttributes.begin(),
                                                         x.writeAttributes.end())};
      return f.object(x).fields(
          f.field("collection", x.collectionName),
          f.field("read", read),
          f.field("write", write));
    }
  };

  explicit AttributeDetector(Ast* ast);
  ~AttributeDetector() = default;

  void detect();
  void toVelocyPack(velocypack::Builder& builder) const;

  std::vector<CollectionAccess> const& getCollectionAccesses() const {
    return _collectionAccesses;
  }

  bool requiresWildcardAccess() const;

  bool before(AstNode* node) override final;
  void after(AstNode* node) override final;
  bool enterSubquery(AstNode*, AstNode*) override final;

 private:
  void trackAttributeAccess(Variable const* var, std::string_view attribute,
                            bool isWrite);
  void markRequiresAllAttributes(Variable const* var, bool isWrite);
  std::string resolveVariableToCollection(Variable const* var) const;
  void processAttributeAccess(AstNode const* node, bool isWrite);
  void processModificationNode(AstNode const* node);
  bool isFullDocumentAccess(AstNode const* node) const;
  void extractAttributesFromObject(AstNode const* node,
                                    std::vector<std::string>& attributes);
  void processViewAccess(AstNode const* node, Variable const* var);
  void processGraphTraversal(AstNode const* node);

  Ast* _ast;
  containers::FlatHashMap<Variable const*, std::string>
      _variableToCollection;
  containers::FlatHashMap<std::string, CollectionAccess> _collectionAccessMap;
  std::vector<CollectionAccess> _collectionAccesses;
  bool _inWriteContext{false};
  Variable const* _modificationVariable{nullptr};
};

}  // namespace aql
}  // namespace arangodb

