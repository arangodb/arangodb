////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlCallList.h"
#include "Basics/ResultT.h"

#include <vector>

namespace arangodb {
namespace velocypack {
class Slice;
}
namespace aql {

/**
 * @brief This is a stack of AqlCallLists.
 *        While the AqlCallList defines the call(s) for a single
 *        Executor only, the AqlCallStack is used to transport information
 *        for outer subqueries.
 *        At the very beginning of the query this Stack has exactly one
 *        CallList entry, which defines what should be done on the outermost
 * query. If we now enter a spliced subquery, there will be another CallList
 *        added on top of this stack. If we leave the spliced subquery, the
 * topmost CallList will be removed.
 *
 *        Using this stack, we can transport all necessary calls from the outer
 *        subqueries to the Executors that need to produce data for them.
 */
class AqlCallStack {
 public:
  // Initial
  explicit AqlCallStack(AqlCallList call);
  // Used in subquery
  AqlCallStack(AqlCallStack const& other, AqlCallList call);
  // Used to pass between blocks
  AqlCallStack(AqlCallStack const& other) = default;
  AqlCallStack(AqlCallStack&& other) noexcept = default;

  AqlCallStack& operator=(AqlCallStack const& other) = default;
  AqlCallStack& operator=(AqlCallStack&& other) noexcept = default;

  static auto fromVelocyPack(velocypack::Slice) -> ResultT<AqlCallStack>;

  auto toString() const -> std::string;

  // Get the top most Call element.
  // This is popped of the stack and caller can take responsibility for it
  AqlCallList popCall();

  // Peek at the topmost Call element (this must be relevant).
  // The responsibility for the peek-ed call will stay with the stack
  AqlCall const& peek() const;

  // Put another call on top of the stack.
  void pushCall(AqlCallList&& call);

  // Put another call on top of the stack.
  void pushCall(AqlCallList const& call);

  auto empty() const noexcept -> bool { return _operations.empty(); }

  auto subqueryLevel() const noexcept -> size_t { return _operations.size(); }

  void toVelocyPack(velocypack::Builder& builder) const;

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
   *        be counted, either as skipped or as produced.
   *        This is relevant for executors that have created
   *        an equivalentFetchAllShadowRows stack, in order to decide if
   *        the need to produce output or if they are skipped.
   *
   * @return true
   * @return false
   */
  auto needToCountSubquery() const noexcept -> bool;

  auto needToSkipSubquery() const noexcept -> bool;
  /**
   * @brief This is only valid if needToCountSubquery is true.
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
  auto modifyCallListAtDepth(size_t depth) -> AqlCallList&;

  /**
   * @brief Get a const reference to the call at the given shadowRowDepth
   *
   * @param depth ShadowRow depth we need to work on
   * @return AqlCall& reference to the call, can be modified.
   */
  auto getCallAtDepth(size_t depth) const -> AqlCall const&;

  /**
   * @brief Get a reference to the top most call.
   *        This is modifiable, but caller will not take
   *        responsibility.
   *
   * @return AqlCall& reference to the call, can be modified.
   */
  auto modifyTopCall() -> AqlCall&;

  /**
   * @brief Test if this stack has at least 1 valid call
   *        on every subquery depth. If not, we canot continue with this stack
   *        but need to return.
   *
   * @return true We have all valid calls
   * @return false We ahve at least on depth without a valid call
   */
  auto hasAllValidCalls() const noexcept -> bool;

  /**
   * @brief Tests if this requires at most an identical
   *        amount of data as the other stack does.
   *
   *        Or in other words: Starting with the other stack,
   *        is there a combination of didSkip/didProduce
   *        calls to reach this state. (empty combination allowed)
   *        This also checks if the depths of the stacks are identical
   *
   * @return true
   * @return false
   */
  auto requestLessDataThan(AqlCallStack const& other) const noexcept -> bool;

 private:
  explicit AqlCallStack(std::vector<AqlCallList>&& operations) noexcept;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto validateNoCallHasSkippedRows() -> void;
#endif

 private:
  // The list of operations, stacked by depth (e.g. bottom element is from
  // main query)
  std::vector<AqlCallList> _operations;
};

}  // namespace aql
}  // namespace arangodb
