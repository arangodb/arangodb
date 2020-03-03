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

#include "AqlCallStack.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

// TODO: This class is not yet memory efficient or optimized in any way.
// it might be reimplement soon to have the above features, Focus now is on
// the API we want to use.

using namespace arangodb;
using namespace arangodb::aql;

AqlCallStack::AqlCallStack(AqlCall call, bool compatibilityMode3_6)
    : _operations{{std::move(call)}}, _compatibilityMode3_6(compatibilityMode3_6) {}

AqlCallStack::AqlCallStack(AqlCallStack const& other, AqlCall call)
    : _operations{other._operations} {
  // We can only use this constructor on relevant levels
  // Alothers need to use passThrough constructor
  TRI_ASSERT(other._depth == 0);
  _operations.emplace_back(std::move(call));
  _compatibilityMode3_6 = other._compatibilityMode3_6;
}

AqlCallStack::AqlCallStack(AqlCallStack const& other)
    : _operations{other._operations},
      _depth(other._depth),
      _compatibilityMode3_6(other._compatibilityMode3_6) {}

AqlCallStack::AqlCallStack(std::vector<AqlCall>&& operations)
    : _operations(std::move(operations)) {}

bool AqlCallStack::isRelevant() const { return _depth == 0; }

AqlCall AqlCallStack::popCall() {
  TRI_ASSERT(isRelevant());
  TRI_ASSERT(_compatibilityMode3_6 || !_operations.empty());
  if (_compatibilityMode3_6 && _operations.empty()) {
    // This is only for compatibility with 3.6
    // there we do not have the stack beeing passed-through
    // in AQL, we only have a single call.
    // We can only get into this state in the abscence of
    // LIMIT => we always do an unlimted softLimit call
    // to the upwards subquery.
    // => Simply put another fetchAll Call on the stack.
    // This code is to be removed in the next version after 3.7
    _operations.emplace_back(AqlCall{});
  }
  auto call = _operations.back();
  _operations.pop_back();
  return call;
}

AqlCall const& AqlCallStack::peek() const {
  TRI_ASSERT(isRelevant());
  TRI_ASSERT(!_operations.empty());
  return _operations.back();
}

void AqlCallStack::pushCall(AqlCall&& call) {
  // TODO is this correct on subqueries?
  TRI_ASSERT(isRelevant());
  _operations.emplace_back(std::move(call));
}

void AqlCallStack::pop() {
  if (isRelevant()) {
    // We have one element to pop
    std::ignore = popCall();
  } else {
    _depth--;
  }
}

auto AqlCallStack::increaseSubqueryDepth() -> void {
  // Avoid overflow. If you actually have a subquery nesting of size_t many subqueries
  // there is a rather high chance that your query will not perform well.
  TRI_ASSERT(_depth < std::numeric_limits<size_t>::max() - 2);
  _depth++;
  TRI_ASSERT(!isRelevant());
}

auto AqlCallStack::fromVelocyPack(velocypack::Slice const slice) -> ResultT<AqlCallStack> {
  if (ADB_UNLIKELY(!slice.isArray())) {
    using namespace std::string_literals;
    return Result(TRI_ERROR_TYPE_ERROR,
                  "When deserializing AqlCallStack: expected array, got "s +
                      slice.typeName());
  }
  if (ADB_UNLIKELY(slice.isEmptyArray())) {
    return Result(TRI_ERROR_TYPE_ERROR,
                  "When deserializing AqlCallStack: stack is empty");
  }

  auto stack = std::vector<AqlCall>{};
  auto i = std::size_t{0};
  stack.reserve(slice.length());
  for (auto const entry : VPackArrayIterator(slice)) {
    auto maybeAqlCall = AqlCall::fromVelocyPack(entry);

    if (ADB_UNLIKELY(maybeAqlCall.fail())) {
      auto message = std::string{"When deserializing AqlCallStack: entry "};
      message += std::to_string(i);
      message += ": ";
      message += std::move(maybeAqlCall).errorMessage();
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }

    stack.emplace_back(maybeAqlCall.get());

    ++i;
  }

  TRI_ASSERT(i > 0);

  return AqlCallStack{std::move(stack)};
}

void AqlCallStack::toVelocyPack(velocypack::Builder& builder) const {
  builder.openArray();
  for (auto const& call : _operations) {
    call.toVelocyPack(builder);
  }
  builder.close();
}

auto AqlCallStack::toString() const -> std::string {
  auto result = std::string{};
  result += "[";
  bool isFirst = true;
  for (auto const& op : _operations) {
    if (!isFirst) {
      result += ",";
    }
    isFirst = false;
    result += " ";
    result += op.toString();
  }
  result += " ]";
  return result;
}

auto AqlCallStack::createEquivalentFetchAllShadowRowsStack() const -> AqlCallStack {
  AqlCallStack res{*this};
  if (subqueryLevel() > 1) {
    // We only replace the subquery levels.
    // The releveant call may include a softLimit
    // which needs to be honored here.

    std::replace_if(
        res._operations.begin(), res._operations.end() - 1,
        [](auto const&) -> bool { return true; }, AqlCall{});
  }
  return res;
}

auto AqlCallStack::needToSkipSubquery() const noexcept -> bool {
  if (subqueryLevel() > 1) {
    return std::any_of(_operations.begin(), _operations.end() - 1,
                       [](AqlCall const& call) -> bool {
                         return call.needSkipMore();
                       });
  }
  return false;
}