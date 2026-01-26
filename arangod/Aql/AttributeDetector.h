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

#include "Aql/AttributeNamePath.h"
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
}

namespace aql {
struct AstNode;
class ExecutionNode;
class ExecutionPlan;
struct Variable;

/// @brief AttributeDetector analyzes AQL execution plans to determine which
/// attributes are accessed from collections. This information is used for
/// Attribute-Based Access Control (ABAC).
class AttributeDetector final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
 public:
  struct AttributeAccessInfo {
    bool requiresAll{false};
    std::vector<std::vector<std::string>> attributes;

    template<class Inspector>
    friend auto inspect(Inspector& f, AttributeAccessInfo& x) {
      return f.object(x).fields(f.field("requiresAll", x.requiresAll),
                                f.field("attributes", x.attributes));
    }
  };

  struct CollectionAccess {
    std::string collectionName;
    containers::FlatHashSet<AttributeNamePath> readAttributes;
    bool requiresAllAttributesRead{false};
    bool requiresAllAttributesWrite{false};
    Variable const* outVariable{nullptr};

    template<class Inspector>
    friend auto inspect(Inspector& f, CollectionAccess& x) {
      std::vector<std::vector<std::string>> readAttrs;
      readAttrs.reserve(x.readAttributes.size());
      for (auto const& path : x.readAttributes) {
        readAttrs.push_back(path.get());
      }

      AttributeAccessInfo read{x.requiresAllAttributesRead,
                               std::move(readAttrs)};
      AttributeAccessInfo write{x.requiresAllAttributesWrite, {}};
      return f.object(x).fields(f.field("collection", x.collectionName),
                                f.field("read", read), f.field("write", write));
    }
  };

  explicit AttributeDetector(ExecutionPlan* plan);
  ~AttributeDetector() = default;

  void detect();
  void toVelocyPack(velocypack::Builder& builder) const;

  std::vector<CollectionAccess> const& getCollectionAccesses() const {
    return _collectionAccesses;
  }

  bool before(ExecutionNode* node) override final;
  void after(ExecutionNode* node) override final;
  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final;

 private:
  ExecutionPlan* _plan;
  containers::FlatHashMap<std::string, std::unique_ptr<CollectionAccess>>
      _collectionAccessMap;
  std::vector<CollectionAccess> _collectionAccesses;
};

}  // namespace aql
}  // namespace arangodb
