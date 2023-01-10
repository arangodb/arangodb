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

#include "AqlCallList.h"

#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Containers/Enumerate.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <string_view>

using namespace arangodb;
using namespace arangodb::aql;

AqlCallList::AqlCallList(AqlCall const& call) : _specificCalls{call} {
  // We can never create a new CallList with existing skipCounter
  TRI_ASSERT(call.getSkipCount() == 0);
}

AqlCallList::AqlCallList(AqlCall const& specificCall,
                         AqlCall const& defaultCall)
    : _specificCalls{specificCall}, _defaultCall{defaultCall} {
  // We can never create a new CallList with existing skipCounter
  TRI_ASSERT(specificCall.getSkipCount() == 0);
  TRI_ASSERT(defaultCall.getSkipCount() == 0);
}

[[nodiscard]] auto AqlCallList::popNextCall() -> AqlCall {
  TRI_ASSERT(hasMoreCalls());
  if (!_specificCalls.empty()) {
    // We only implemented for a single given call.
    TRI_ASSERT(_specificCalls.size() == 1);
    auto res = std::move(_specificCalls.back());
    _specificCalls.pop_back();
    return res;
  }
  TRI_ASSERT(_defaultCall.has_value());
  return _defaultCall.value();
}

[[nodiscard]] auto AqlCallList::peekNextCall() const -> AqlCall const& {
  TRI_ASSERT(hasMoreCalls());
  if (!_specificCalls.empty()) {
    // We only implemented for a single given call.
    TRI_ASSERT(_specificCalls.size() == 1);
    return _specificCalls.back();
  }
  TRI_ASSERT(_defaultCall.has_value());
  return _defaultCall.value();
}

[[nodiscard]] auto AqlCallList::hasMoreCalls() const noexcept -> bool {
  return !_specificCalls.empty() || _defaultCall.has_value();
}

[[nodiscard]] auto AqlCallList::hasDefaultCalls() const noexcept -> bool {
  return _defaultCall.has_value();
}

[[nodiscard]] auto AqlCallList::modifyNextCall() -> AqlCall& {
  TRI_ASSERT(hasMoreCalls());
  if (_specificCalls.empty()) {
    TRI_ASSERT(_defaultCall.has_value());
    // We need to emplace a copy of  defaultCall into the specific calls
    // This can then be modified and eventually be consumed
    _specificCalls.emplace_back(_defaultCall.value());
  }
  return _specificCalls.back();
}

void AqlCallList::createEquivalentFetchAllRowsCall() {
  std::replace_if(
      _specificCalls.begin(), _specificCalls.end(),
      [](auto const&) -> bool { return true; }, AqlCall{});
  if (_defaultCall.has_value()) {
    _defaultCall = AqlCall{};
  }
}

auto AqlCallList::fromVelocyPack(VPackSlice slice) -> ResultT<AqlCallList> {
  if (ADB_UNLIKELY(!slice.isObject())) {
    using namespace std::string_literals;
    return Result(TRI_ERROR_TYPE_ERROR,
                  "When deserializating AqlCallList: Expected object, got "s +
                      slice.typeName());
  }

  // we only have 2 different keys to check. using an std::map requires
  // dynamic memory allocation and would be wasteful. instead, use a simple
  // std::array here to get rid of any allocations
  auto expectedPropertiesFound =
      std::array<std::pair<std::string_view, bool>, 2>{
          {{StaticStrings::AqlCallListSpecific, false},
           {StaticStrings::AqlCallListDefault, false}}};

  auto const readSpecific =
      [](velocypack::Slice slice) -> ResultT<std::vector<AqlCall>> {
    if (ADB_UNLIKELY(!slice.isArray())) {
      auto message = std::string{"When deserializating AqlCall: When reading " +
                                 StaticStrings::AqlCallListSpecific +
                                 ": "
                                 "Unexpected type "};
      message += slice.typeName();
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
    std::vector<AqlCall> res;
    res.reserve(slice.length());
    for (VPackSlice c : VPackArrayIterator(slice)) {
      auto maybeAqlCall = AqlCall::fromVelocyPack(c);
      if (ADB_UNLIKELY(maybeAqlCall.fail())) {
        auto message = std::string{"When deserializing AqlCallList: entry "};
        message += std::to_string(res.size());
        message += ": ";
        message += maybeAqlCall.errorMessage();
        return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
      }
      res.emplace_back(std::move(maybeAqlCall.get()));
    }
    return res;
  };

  auto const readDefault =
      [](velocypack::Slice slice) -> ResultT<std::optional<AqlCall>> {
    if (slice.isNull()) {
      return {std::nullopt};
    }
    if (ADB_UNLIKELY(!slice.isObject())) {
      auto message =
          std::string{"When deserializating AqlCallList: When reading " +
                      StaticStrings::AqlCallListDefault +
                      ": "
                      "Unexpected type "};
      message += slice.typeName();
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
    auto maybeAqlCall = AqlCall::fromVelocyPack(slice);
    if (ADB_UNLIKELY(maybeAqlCall.fail())) {
      auto message = std::string{"When deserializing AqlCallList: default "};
      message += maybeAqlCall.errorMessage();
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
    return {std::move(maybeAqlCall.get())};
  };

  AqlCallList result{AqlCall{}};

  for (auto const it : velocypack::ObjectIterator(slice)) {
    auto key = it.key.stringView();

    if (auto propIt = std::find_if(
            expectedPropertiesFound.begin(), expectedPropertiesFound.end(),
            [&key](auto const& epf) { return epf.first == key; });
        ADB_LIKELY(propIt != expectedPropertiesFound.end())) {
      propIt->second = true;
    }

    if (key == StaticStrings::AqlCallListSpecific) {
      auto maybeCalls = readSpecific(it.value);
      if (maybeCalls.fail()) {
        return std::move(maybeCalls).result();
      }
      result._specificCalls = maybeCalls.get();
    } else if (key == StaticStrings::AqlCallListDefault) {
      auto maybeCall = readDefault(it.value);
      if (maybeCall.fail()) {
        return std::move(maybeCall).result();
      }
      result._defaultCall = maybeCall.get();
    } else {
      LOG_TOPIC("c30c1", WARN, Logger::AQL)
          << "When deserializating AqlCallList: Encountered unexpected key "
          << key;
      // If you run into this assertion during rolling upgrades after adding a
      // new attribute, remove it in the older version.
      TRI_ASSERT(false);
    }
  }

  for (auto const& it : expectedPropertiesFound) {
    if (ADB_UNLIKELY(!it.second)) {
      auto message = std::string{"When deserializating AqlCall: missing key "};
      message += it.first;
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
  }

  return result;
}

auto AqlCallList::toVelocyPack(VPackBuilder& builder) const -> void {
  // We need to have something that is serializable
  TRI_ASSERT(hasMoreCalls());
  builder.openObject();
  builder.add(VPackValue(StaticStrings::AqlCallListSpecific));

  {
    builder.openArray();
    for (auto const& call : _specificCalls) {
      call.toVelocyPack(builder);
    }
    builder.close();
  }
  builder.add(VPackValue(StaticStrings::AqlCallListDefault));
  if (_defaultCall.has_value()) {
    _defaultCall.value().toVelocyPack(builder);
  } else {
    builder.add(VPackSlice::nullSlice());
  }

  builder.close();
}

auto AqlCallList::toString() const -> std::string {
  auto stream = std::stringstream{};
  stream << *this;
  return stream.str();
}

auto AqlCallList::requestLessDataThan(AqlCallList const& other) const noexcept
    -> bool {
  if (other._defaultCall.has_value()) {
    // Let is straightly check the default call.
    // We do not know if we have already filled one specific call
    if (!_defaultCall.has_value()) {
      // We cannot lose the default call
      return false;
    }

    if (!(_defaultCall.value() == other._defaultCall.value())) {
      return false;
    }

    for (auto const& call : _specificCalls) {
      if (!call.requestLessDataThan(other._defaultCall.value())) {
        return false;
      }
    }
    return true;
  }
  if (_defaultCall.has_value()) {
    // We cannot reach a state with default call, if the original does not have
    // one
    return false;
  }
  // NOTE: For simplicity we only implemented this for the single specific call
  // used up to now.
  TRI_ASSERT(_specificCalls.size() <= 1);
  TRI_ASSERT(other._specificCalls.size() <= 1);
  if (!_specificCalls.empty()) {
    if (other._specificCalls.empty()) {
      // Cannot have generated a specific call
      return false;
    }
    return _specificCalls[0].requestLessDataThan(other._specificCalls[0]);
  }
  return true;
}

bool arangodb::aql::operator==(AqlCallList const& left,
                               AqlCallList const& right) {
  if (left._specificCalls.size() != right._specificCalls.size()) {
    return false;
  }
  // Sorry call does not implement operator!=
  if (!(left._defaultCall == right._defaultCall)) {
    return false;
  }
  for (auto const& [index, call] : enumerate(left._specificCalls)) {
    if (!(call == right._specificCalls[index])) {
      return false;
    }
  }
  return true;
}

auto arangodb::aql::operator<<(std::ostream& out, AqlCallList const& list)
    -> std::ostream& {
  out << "{specific: [ ";
  for (auto const& [index, call] : enumerate(list._specificCalls)) {
    if (index > 0) {
      out << ", ";
    }
    out << call;
  }
  out << " ]";
  if (list._defaultCall.has_value()) {
    out << ", default: " << list._defaultCall.value();
  }
  out << "}";
  return out;
}
