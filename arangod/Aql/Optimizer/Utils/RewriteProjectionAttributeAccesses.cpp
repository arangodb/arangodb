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
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer/Utils/RewriteProjectionAttributeAccesses.h"
#include "Aql/Projections.h"

#include <string_view>
#include <span>

namespace arangodb::aql::optimizer {

namespace {
// Walks the plan and rewrites attribute accesses on `searchVariable` into
// direct register reads on `replaceVariable`. Used by both
// optimizeProjections and materializeForEnumerateNear.
class AttributeAccessReplacer final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
 public:
  AttributeAccessReplacer(ExecutionNode const* self,
                          Variable const* searchVariable,
                          std::span<std::string_view> attribute,
                          Variable const* replaceVariable, size_t index)
      : _self(self),
        _searchVariable(searchVariable),
        _attribute(attribute),
        _replaceVariable(replaceVariable),
        _index(index) {
    TRI_ASSERT(_searchVariable != nullptr);
    TRI_ASSERT(!_attribute.empty());
    TRI_ASSERT(_replaceVariable != nullptr);
  }

  bool before(ExecutionNode* en) override final {
    en->replaceAttributeAccess(_self, _searchVariable, _attribute,
                               _replaceVariable, _index);
    return false;
  }

 private:
  ExecutionNode const* _self;
  Variable const* _searchVariable;
  std::span<std::string_view> _attribute;
  Variable const* _replaceVariable;
  size_t _index;
};

}  // namespace

void rewriteProjectionAttributeAccesses(ExecutionPlan& plan,
                                        ExecutionNode* self,
                                        Variable const* searchVariable,
                                        Projections& projections,
                                        size_t index) {
  std::vector<std::string_view> path;
  for (size_t i = 0; i < projections.size(); ++i) {
    TRI_ASSERT(projections[i].variable == nullptr);
    projections[i].variable =
        plan.getAst()->variables()->createTemporaryVariable();

    path.clear();
    for (auto const& it : projections[i].path.get()) {
      path.emplace_back(it);
    }
    AttributeAccessReplacer replacer(self, searchVariable, std::span(path),
                                     projections[i].variable, index);
    plan.root()->walk(replacer);
  }
}

}  // namespace arangodb::aql::optimizer
