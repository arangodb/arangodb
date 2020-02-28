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

#include "Aql/AqlItemBlockManager.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ResultT.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include <map>

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

auto AqlExecuteResult::fromVelocyPack(velocypack::Slice const slice, AqlItemBlockManager& itemBlockManager) -> ResultT<AqlExecuteResult> {
  if (ADB_UNLIKELY(!slice.isObject())) {
    using namespace std::string_literals;
    return Result(TRI_ERROR_TYPE_ERROR,
        "When deserializating AqlExecuteResult: Expected object, got "s + slice.typeName());
  }

  auto expectedPropertiesFound = std::map<std::string_view, bool>{};
  expectedPropertiesFound.emplace(StaticStrings::AqlRemoteState, false);
  expectedPropertiesFound.emplace(StaticStrings::AqlRemoteSkipped, false);
  expectedPropertiesFound.emplace(StaticStrings::AqlRemoteBlock, false);

  auto state = ExecutionState::HASMORE;
  auto skipped = std::size_t{};
  auto block = SharedAqlItemBlockPtr{};

  auto const readState = [](velocypack::Slice slice) -> ResultT<ExecutionState> {
    if (ADB_UNLIKELY(!slice.isString())) {
      auto message = std::string{
          "When deserializating AqlExecuteResult: When reading state: "
          "Unexpected type "};
      message += slice.typeName();
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
    auto value = slice.stringView();
    if (value == StaticStrings::AqlRemoteStateDone) {
      return ExecutionState::DONE;
    }
    else if (value == StaticStrings::AqlRemoteStateHasmore) {
      return ExecutionState::HASMORE;
    }
    else {
      auto message = std::string{
          "When deserializating AqlExecuteResult: When reading state: "
          "Unexpected value '"};
      message += value;
      message += "'";
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
  };

  auto const readSkipped = [](velocypack::Slice slice) -> ResultT<std::size_t> {
    if (!slice.isInteger()) {
      auto message = std::string{
          "When deserializating AqlExecuteResult: When reading skipped: "
          "Unexpected type "};
      message += slice.typeName();
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
    try {
      return slice.getNumber<std::size_t>();
    } catch (velocypack::Exception const& ex) {
      auto message = std::string{
          "When deserializating AqlExecuteResult: When reading skipped: "};
      message += ex.what();
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
  };

  auto const readBlock = [&itemBlockManager](velocypack::Slice slice) -> ResultT<SharedAqlItemBlockPtr> {
    if (slice.isNull()) {
      return SharedAqlItemBlockPtr{nullptr};
    }
    TRI_ASSERT(slice.isObject());
    return itemBlockManager.requestAndInitBlock(slice);
  };

  for (auto const it : velocypack::ObjectIterator(slice)) {
    auto const keySlice = it.key;
    if (ADB_UNLIKELY(!keySlice.isString())) {
      return Result(TRI_ERROR_TYPE_ERROR,
          "When deserializating AqlExecuteResult: Key is not a string");
    }
    auto const key = keySlice.stringView();

    if (auto propIt = expectedPropertiesFound.find(key);
        ADB_LIKELY(propIt != expectedPropertiesFound.end())) {
      if (ADB_UNLIKELY(propIt->second)) {
        return Result(
            TRI_ERROR_TYPE_ERROR,
            "When deserializating AqlExecuteResult: Encountered duplicate key");
      }
      propIt->second = true;
    }

    if (key == StaticStrings::AqlRemoteState) {
      auto maybeState = readState(it.value);
      if (maybeState.fail()) {
        return std::move(maybeState).result();
      }
      state = maybeState.get();
    }
    else if (key == StaticStrings::AqlRemoteSkipped) {
      auto maybeSkipped = readSkipped(it.value);
      if (maybeSkipped.fail()) {
        return std::move(maybeSkipped).result();
      }
      skipped = maybeSkipped.get();
    }
    else if (key == StaticStrings::AqlRemoteBlock) {
      auto maybeBlock = readBlock(it.value);
      if (maybeBlock.fail()) {
        return std::move(maybeBlock).result();
      }
      block = maybeBlock.get();
    } else {
      LOG_TOPIC("cc6f4", WARN, Logger::AQL)
          << "When deserializating AqlExecuteResult: Encountered unexpected key " << key;
      // If you run into this assertion during rolling upgrades after adding a
      // new attribute, remove it in the older version.
      TRI_ASSERT(false);
    }
  }

  for (auto const& it : expectedPropertiesFound) {
    if (ADB_UNLIKELY(!it.second)) {
      auto message = std::string{"When deserializating AqlExecuteResult: missing key "};
      message += it.first;
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
  }

  return AqlExecuteResult{state, skipped, std::move(block)};
}

auto AqlExecuteResult::asTuple() const noexcept
    -> std::tuple<ExecutionState, std::size_t, SharedAqlItemBlockPtr> {
  return {state(), skipped(), block()};
}
