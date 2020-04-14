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

#ifndef ARANGOD_AQL_AQL_CALL_LIST_H
#define ARANGOD_AQL_AQL_CALL_LIST_H 1

#include "Aql/AqlCall.h"

#include <optional>
#include <vector>

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::aql {

/**
 * @brief This is a list of AqlCalls that should be used on a single Executor.
 *        On the current Subquery run the first call in this list counts.
 *        As soon as the current Subquery is done (by finding the ShadowRow)
 *        The first call counts as completed and the executor can request the
 * next call. This new call is what counts for the subquery that is about to
 * start. There may be situations where a next call is not available. in this
 * case the executor cannot start with the next subquery run, but needs to
 * return and wait for more information given by the client.
 */
class AqlCallList {
 public:
  friend bool operator==(AqlCallList const& left, AqlCallList const& right);

  friend auto operator<<(std::ostream& out, const arangodb::aql::AqlCallList& list)
      -> std::ostream&;

  /**
   * @brief Construct a new Aql Call List object
   *        This constructor will only allow a single call.
   *        As soon as this call is completed, there will be no next call.
   *        Hance the executor needs to return.
   * @param call The call relevant for this subquery run.
   */
  explicit AqlCallList(AqlCall const& call);

  /**
   * @brief Construct a new Aql Call List object
   *        This constructor will allow for a specific call, which is used for the ongoing subquery run.
   *        And a default call, which should be used for every following subquery.
   *        Simply speaking this constructor creates an endless list of
   *        [specificCall, defaultCall, defaultCall, defaultCall, defaultCall,...]
   *        calls. Whenever a subquery run is done the executor will have a new
   *        call and can continue with the next run
   * @param specificCall The call of the current subquery run. This call can be different from a defaultCall,
   *                     but it must be producable by the defaultCall using only didSkip() and didProduce().
   *                     Simply put: the specificCall is a defaultCall that has already applied some productions
   *                     of aql rows.
   * @param defaultCall The call to be used whenever a new SubqueryRun is started.
   */
  AqlCallList(AqlCall const& specificCall, AqlCall const& defaultCall);

  /**
   * @brief Take the next call from the call list and take ownership
   *        of it. The list will be modified.
   *        This can only be called if hasMoreCalls() == true
   *
   * @return AqlCall The next call
   */
  [[nodiscard]] auto popNextCall() -> AqlCall;

  /**
   * @brief Peek the next call from the call list.
   *        Neither the list nor the call will not be modified.
   *        This can only be called if hasMoreCalls() == true
   *
   * @return AqlCall The next call
   */
  [[nodiscard]] auto peekNextCall() const -> AqlCall const&;

  /**
   * @brief Test if there are more calls available in the list.
   *        After construction this is true.
   *        After a call to popNextCall() this might turn into false.
   *
   * @return true
   * @return false
   */
  [[nodiscard]] auto hasMoreCalls() const noexcept -> bool;

  [[nodiscard]] auto hasDefaultCalls() const noexcept -> bool;

  /**
   * @brief Get a reference to the next call.
   *        This is modifiable, but caller will not take
   *        responsibility. This modifies the list.
   *        NOTE: In case of the defaultCall variant, the default
   *        will not be altered, only the call that you will get
   *        with pop|peekNextCall().
   *
   * @return AqlCall& reference to the call, can be modified.
   */
  [[nodiscard]] auto modifyNextCall() -> AqlCall&;

  void createEquivalentFetchAllRowsCall();

  static auto fromVelocyPack(velocypack::Slice) -> ResultT<AqlCallList>;
  auto toVelocyPack(velocypack::Builder&) const -> void;
  auto toString() const -> std::string;

  /**
   * @brief Tests if this requires at most an identical
   *        amount of data as the other list does.
   *
   *        Or in other words: Starting with the other stack,
   *        is there a combination of didSkip/didProduce
   *        calls to reach this state. (empty combination allowed)
   *        This also checks if the depths of the stacks are identical
   *
   * @return true
   * @return false
   */
  auto requestLessDataThan(AqlCallList const& other) const noexcept -> bool;

 private:
  /**
   * @brief A list of specific calls for subqueries.
   *        Right now we have only implemented variants where there is
   *        at most one call in this list. But the code is actually ready for
   *        any number of calls here.
   */
  std::vector<AqlCall> _specificCalls{};
  std::optional<AqlCall> _defaultCall{std::nullopt};
};

auto operator==(AqlCallList const& left, AqlCallList const& right) -> bool;

auto operator<<(std::ostream& out, const arangodb::aql::AqlCallList& list) -> std::ostream&;

}  // namespace arangodb::aql

#endif
