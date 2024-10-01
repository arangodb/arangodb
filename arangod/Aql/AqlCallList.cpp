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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "AqlCallList.h"

#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <string_view>

using namespace arangodb;
using namespace arangodb::aql;

AqlCallList::AqlCallList(AqlCall const& call) : _specificCall{call} {
  // We can never create a new CallList with existing skipCounter
  TRI_ASSERT(call.getSkipCount() == 0);
  TRI_ASSERT(_specificCall.has_value());
}

AqlCallList::AqlCallList(AqlCall const& specificCall,
                         AqlCall const& defaultCall)
    : _specificCall{specificCall}, _defaultCall{defaultCall} {
  // We can never create a new CallList with existing skipCounter
  TRI_ASSERT(specificCall.getSkipCount() == 0);
  TRI_ASSERT(defaultCall.getSkipCount() == 0);
  TRI_ASSERT(_specificCall.has_value());
}

[[nodiscard]] auto AqlCallList::popNextCall() -> AqlCall {
  TRI_ASSERT(hasMoreCalls());
  if (_specificCall.has_value()) {
    // We only implemented for a single given call.
    auto res = std::move(_specificCall.value());
    _specificCall.reset();
    return res;
  }
  TRI_ASSERT(_defaultCall.has_value());
  return _defaultCall.value();
}

[[nodiscard]] auto AqlCallList::peekNextCall() const noexcept
    -> AqlCall const& {
  TRI_ASSERT(hasMoreCalls());
  if (_specificCall.has_value()) {
    // We only implemented for a single given call.
    return _specificCall.value();
  }
  TRI_ASSERT(_defaultCall.has_value());
  return _defaultCall.value();
}

[[nodiscard]] auto AqlCallList::hasMoreCalls() const noexcept -> bool {
  return _specificCall.has_value() || _defaultCall.has_value();
}

[[nodiscard]] auto AqlCallList::hasDefaultCalls() const noexcept -> bool {
  return _defaultCall.has_value();
}

[[nodiscard]] auto AqlCallList::modifyNextCall() -> AqlCall& {
  TRI_ASSERT(hasMoreCalls());
  if (!_specificCall.has_value()) {
    TRI_ASSERT(_defaultCall.has_value());
    // We need to emplace a copy of  defaultCall into the specific calls
    // This can then be modified and eventually be consumed
    _specificCall.emplace(_defaultCall.value());
  }
  TRI_ASSERT(_specificCall.has_value());
  return _specificCall.value();
}

void AqlCallList::createEquivalentFetchAllRowsCall() {
  if (_specificCall.has_value()) {
    _specificCall.emplace();
  }
  if (_defaultCall.has_value()) {
    _defaultCall = AqlCall{};
  }
}

auto AqlCallList::fromVelocyPack(VPackSlice slice) -> ResultT<AqlCallList> {
  if (ADB_UNLIKELY(!slice.isObject())) {
    return Result(
        TRI_ERROR_TYPE_ERROR,
        absl::StrCat("When deserializing AqlCallList: Expected object, got ",
                     slice.typeName()));
  }

  auto expectedPropertiesFound =
      std::array<std::pair<std::string_view, bool>, 2>{
          {{StaticStrings::AqlCallListSpecific, false},
           {StaticStrings::AqlCallListDefault, false}}};

  auto const readSpecific =
      [](velocypack::Slice slice) -> ResultT<std::vector<AqlCall>> {
    if (ADB_UNLIKELY(!slice.isArray())) {
      return Result(TRI_ERROR_TYPE_ERROR,
                    absl::StrCat("When deserializing AqlCall: When reading ",
                                 StaticStrings::AqlCallListSpecific,
                                 ": Unexpected type ", slice.typeName()));
    }
    std::vector<AqlCall> res;
    res.reserve(slice.length());
    for (VPackSlice c : VPackArrayIterator(slice)) {
      auto maybeAqlCall = AqlCall::fromVelocyPack(c);
      if (ADB_UNLIKELY(maybeAqlCall.fail())) {
        return Result(
            TRI_ERROR_TYPE_ERROR,
            absl::StrCat("When deserializing AqlCallList: entry ", res.size(),
                         ": ", maybeAqlCall.errorMessage()));
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
      return Result(
          TRI_ERROR_TYPE_ERROR,
          absl::StrCat("When deserializing AqlCallList: When reading ",
                       StaticStrings::AqlCallListDefault, ": Unexpected type ",
                       slice.typeName()));
    }
    auto maybeAqlCall = AqlCall::fromVelocyPack(slice);
    if (ADB_UNLIKELY(maybeAqlCall.fail())) {
      return Result(TRI_ERROR_TYPE_ERROR,
                    absl::StrCat("When deserializing AqlCallList: default ",
                                 maybeAqlCall.errorMessage()));
    }
    return {std::move(maybeAqlCall.get())};
  };

  AqlCallList result{AqlCall{}};

  for (auto it :
       velocypack::ObjectIterator(slice, /*useSequentialIteration*/ true)) {
    auto key = it.key.stringView();

    if (auto propIt = std::find_if(
            expectedPropertiesFound.begin(), expectedPropertiesFound.end(),
            [&key](auto const& epf) { return epf.first == key; });
        ADB_LIKELY(propIt != expectedPropertiesFound.end())) {
      TRI_ASSERT(!propIt->second);
      propIt->second = true;
    }

    if (key == StaticStrings::AqlCallListSpecific) {
      result._specificCall.reset();
      auto maybeCalls = readSpecific(it.value);
      if (maybeCalls.fail()) {
        return std::move(maybeCalls).result();
      }
      if (!maybeCalls.get().empty()) {
        if (maybeCalls.get().size() > 1) {
          return Result(TRI_ERROR_TYPE_ERROR,
                        absl::StrCat("When deserializing AqlCallList: invalid "
                                     "number of specific calls: ",
                                     maybeCalls.get().size()));
        }
        result._specificCall.emplace(std::move(maybeCalls.get().front()));
      }
    } else if (key == StaticStrings::AqlCallListDefault) {
      auto maybeCall = readDefault(it.value);
      if (maybeCall.fail()) {
        return std::move(maybeCall).result();
      }
      result._defaultCall = std::move(maybeCall.get());
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
      return Result(
          TRI_ERROR_TYPE_ERROR,
          absl::StrCat("When deserializing AqlCall: missing key ", it.first));
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
    if (_specificCall.has_value()) {
      _specificCall->toVelocyPack(builder);
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

    if (_specificCall.has_value() &&
        !_specificCall->requestLessDataThan(other._defaultCall.value())) {
      return false;
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
  if (_specificCall.has_value()) {
    if (!other._specificCall.has_value()) {
      // Cannot have generated a specific call
      return false;
    }
    return _specificCall->requestLessDataThan(other._specificCall.value());
  }
  return true;
}

bool arangodb::aql::operator==(AqlCallList const& left,
                               AqlCallList const& right) {
  if (left._specificCall.has_value() != right._specificCall.has_value()) {
    return false;
  }
  // Sorry call does not implement operator!=
  if (!(left._defaultCall == right._defaultCall)) {
    return false;
  }
  if (left._specificCall.has_value()) {
    TRI_ASSERT(right._specificCall.has_value());
    if (!(left._specificCall == right._specificCall)) {
      return false;
    }
  }
  return true;
}

auto arangodb::aql::operator<<(std::ostream& out, AqlCallList const& list)
    -> std::ostream& {
  out << "{specific: [ ";
  if (list._specificCall.has_value()) {
    out << list._specificCall.value();
  }
  out << " ]";
  if (list._defaultCall.has_value()) {
    out << ", default: " << list._defaultCall.value();
  }
  out << "}";
  return out;
}
