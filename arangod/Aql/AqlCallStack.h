////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQL_CALLSTACK_H
#define ARANGOD_AQL_AQL_CALLSTACK_H 1

#include "Aql/AqlCall.h"
#include "Cluster/ResultT.h"

#include <stack>

namespace arangodb {
namespace velocypack {
class Slice;
}
namespace aql {

class AqlCallStack {
 public:
  // Initial
  explicit AqlCallStack(AqlCall call, bool compatibilityMode3_6 = false);
  // Used in subquery
  AqlCallStack(AqlCallStack const& other, AqlCall call);
  // Used to pass between blocks
  AqlCallStack(AqlCallStack const& other);

  AqlCallStack& operator=(AqlCallStack const& other) = default;

  static auto fromVelocyPack(velocypack::Slice) -> ResultT<AqlCallStack>;

  auto toString() const -> std::string;

  // Quick test is this CallStack is of local relevance, or it is sufficient to pass it through
  bool isRelevant() const;

  // Get the top most Call element (this must be relevant).
  // This is popped of the stack and caller can take responsibility for it
  AqlCall popCall();

  // Peek at the top most Call element (this must be relevant).
  // The responsibility will stay at the stack
  AqlCall const& peek() const;

  // Put another call on top of the stack.
  void pushCall(AqlCall&& call);

  // Pops one subquery level.
  // if this isRelevent it pops the top-most call from the stack.
  // if this is not revelent it reduces the depth by 1.
  // Can be savely called on every subquery Start.
  void pop();

  // Increase the subquery by one, not placing another call on the stack
  // This is used to bypass all executors until we reach the next subquery start.
  void increaseSubqueryDepth();

  // TODO: Remove me again, only used to fake DONE
  [[deprecated]] auto empty() const noexcept -> bool {
    return _operations.empty() && _depth == 0;
  }

  auto subqueryLevel() const noexcept -> size_t {
    return _operations.size() + _depth;
  }

  void toVelocyPack(velocypack::Builder& builder) const;

  auto is36Compatible() const noexcept -> bool { return _compatibilityMode3_6; }

  /**
   * @brief Create an equivalent call stack that does a full-produce
   *        of all Subquery levels. This is required for blocks
   *        that are not allowed to be bpassed.
   *        The top-most call remains unmodified, as the Executor might
   *        require some soft limit on it.
   *
   * @return AqlCallStack a stack of equivalent size, that does not skip
   *         on any lower subquery.
   */
  auto createEquivalentFetchAllShadowRowsStack() const -> AqlCallStack;

  /**
   * @brief Check if we are in a subquery that is in-fact required to
   *        be skipped. This is relevant for executors that have created
   *        an equivalentFetchAllShadowRows stack, in order to decide if
   *        the need to produce output or if they are skipped.
   *
   * @return true
   * @return false
   */
  auto needToSkipSubquery() const noexcept -> bool;

  /**
   * @brief This is only valid if needToSkipSubquery is true.
   *        It will resolve to the heighest subquery level
   *        (outermost) that needs to be skipped.
   *
   *
   * @return size_t Depth of the subquery that asks to be skipped.
   */
  auto shadowRowDepthToSkip() const -> size_t;

  /**
   * @brief Get a reference to the call at the given shadowRowDepth
   *
   * @param depth ShadowRow depth we need to work on
   * @return AqlCall& reference to the call, can be modified.
   */
  auto modifyCallAtDepth(size_t depth) -> AqlCall&;

 private:
  explicit AqlCallStack(std::vector<AqlCall>&& operations);

 private:
  // The list of operations, stacked by depth (e.g. bottom element is from main
  // query) NOTE: This is only mutable on 3.6 compatibility mode. We need to
  // inject an additional call in any const operation here just to pretend we
  // are not empty. Can be removed after 3.7.
  mutable std::vector<AqlCall> _operations;

  // The depth of subqueries that have not issued calls into operations,
  // as they have been skipped.
  // In most cases this will be zero.
  // However if we skip a subquery that has a nested subquery this depth will be 1 in the nested subquery.
  size_t _depth{0};

  // This flag will be set if and only if
  // we are called with the 3.6 and earlier API
  // As we only support upgrades between 3.6.* -> 3.7.*
  // and not 3.6.* -> 3.8.* we can savely remove
  // this flag and all it's side effects on the
  // version after 3.7.
  bool _compatibilityMode3_6{false};
};

}  // namespace aql
}  // namespace arangodb

#endif
