////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "AqlExecuteResult.h"

#include "Basics/StaticStrings.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::aql;

auto AqlExecuteResult::state() const noexcept -> ExecutionState {
  return _state;
}

auto AqlExecuteResult::skipped() const noexcept -> std::size_t {
  return _skipped;
}

auto AqlExecuteResult::block() const noexcept -> SharedAqlItemBlockPtr const& {
  return _block;
}

void AqlExecuteResult::toVelocyPack(velocypack::Builder& builder,
                                    velocypack::Options const* const options) {
  using namespace arangodb::velocypack;
  auto const stateToValue = [](ExecutionState state) -> Value {
    switch (state) {
      case ExecutionState::DONE:
        return Value(StaticStrings::AqlRemoteStateDone);
      case ExecutionState::HASMORE:
        return Value(StaticStrings::AqlRemoteStateHasmore);
      case ExecutionState::WAITING:
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL_AQL,
            "Unexpected state WAITING, must not be serialized.");
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL, "Unhandled state");
  };

  builder.add(StaticStrings::AqlRemoteState, stateToValue(state()));
  builder.add(StaticStrings::AqlRemoteSkipped, Value(skipped()));
  if (block() != nullptr) {
    ObjectBuilder guard(&builder, StaticStrings::AqlRemoteBlock);
    block()->toVelocyPack(options, builder);
  } else {
    builder.add(StaticStrings::AqlRemoteBlock, Value(ValueType::Null));
  }
}

auto AqlExecuteResult::fromVelocyPack(velocypack::Slice) -> AqlExecuteResult {
  // TODO implement
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto AqlExecuteResult::asTuple() const noexcept
    -> std::tuple<ExecutionState, std::size_t, SharedAqlItemBlockPtr> {
  return {state(), skipped(), block()};
}
