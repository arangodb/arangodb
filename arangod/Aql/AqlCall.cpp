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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "AqlCall.h"

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <array>
#include <iostream>
#include <string_view>

using namespace arangodb;
using namespace arangodb::aql;

auto AqlCall::fromVelocyPack(velocypack::Slice slice) -> ResultT<AqlCall> {
  if (ADB_UNLIKELY(!slice.isObject())) {
    return Result(
        TRI_ERROR_TYPE_ERROR,
        absl::StrCat("When deserializing AqlCall: Expected object, got ",
                     slice.typeName()));
  }

  auto expectedPropertiesFound =
      std::array<std::pair<std::string_view, bool>, 4>{
          {{StaticStrings::AqlRemoteLimit, false},
           {StaticStrings::AqlRemoteLimitType, false},
           {StaticStrings::AqlRemoteFullCount, false},
           {StaticStrings::AqlRemoteOffset, false}}};

  auto limit = AqlCall::Limit{};
  auto limitType = std::optional<AqlCall::LimitType>{};
  auto offset = decltype(AqlCall::offset){0};
  auto fullCount = false;

  auto const readLimit =
      [](velocypack::Slice slice) -> ResultT<AqlCall::Limit> {
    if (slice.isString() &&
        slice.isEqualString(StaticStrings::AqlRemoteInfinity)) {
      return AqlCall::Limit{AqlCall::Infinity{}};
    }
    if (slice.isInteger()) {
      try {
        return AqlCall::Limit{slice.getNumber<std::size_t>()};
      } catch (velocypack::Exception const& ex) {
        return Result(
            TRI_ERROR_TYPE_ERROR,
            absl::StrCat("When deserializing AqlCall: When reading limit: ",
                         ex.what()));
      }
    }
    if (slice.isString()) {
      return Result(TRI_ERROR_TYPE_ERROR,
                    absl::StrCat("When deserializing AqlCall: When reading "
                                 "limit: Unexpected value '",
                                 slice.stringView(), "'"));
    }
    return Result(TRI_ERROR_TYPE_ERROR,
                  absl::StrCat("When deserializing AqlCall: When reading "
                               "limit: Unexpected type ",
                               slice.typeName()));
  };

  auto const readLimitType = [](velocypack::Slice slice)
      -> ResultT<std::optional<AqlCall::LimitType>> {
    if (slice.isNull()) {
      return {std::nullopt};
    }
    if (ADB_UNLIKELY(!slice.isString())) {
      return Result(
          TRI_ERROR_TYPE_ERROR,
          absl::StrCat("When deserializing AqlCall: When reading limitType: "
                       "Unexpected type ",
                       slice.typeName()));
    }
    auto value = slice.stringView();
    if (value == StaticStrings::AqlRemoteLimitTypeSoft) {
      return {AqlCall::LimitType::SOFT};
    }
    if (value == StaticStrings::AqlRemoteLimitTypeHard) {
      return {AqlCall::LimitType::HARD};
    }
    return Result(
        TRI_ERROR_TYPE_ERROR,
        absl::StrCat("When deserializing AqlCall: When reading limitType: "
                     "Unexpected value '",
                     value, "'"));
  };

  auto const readFullCount = [](velocypack::Slice slice) -> ResultT<bool> {
    if (ADB_UNLIKELY(!slice.isBool())) {
      return Result(
          TRI_ERROR_TYPE_ERROR,
          absl::StrCat("When deserializing AqlCall: When reading fullCount: "
                       "Unexpected type ",
                       slice.typeName()));
    }
    return slice.getBool();
  };

  auto const readOffset =
      [](velocypack::Slice slice) -> ResultT<decltype(offset)> {
    if (!slice.isInteger()) {
      return Result(
          TRI_ERROR_TYPE_ERROR,
          absl::StrCat("When deserializing AqlCall: When reading offset: "
                       "Unexpected type ",
                       slice.typeName()));
    }
    try {
      return slice.getNumber<decltype(offset)>();
    } catch (velocypack::Exception const& ex) {
      return Result(
          TRI_ERROR_TYPE_ERROR,
          absl::StrCat("When deserializing AqlCall: When reading offset: ",
                       ex.what()));
    }
  };

  for (auto it :
       velocypack::ObjectIterator(slice, /*useSequentialIteration*/ true)) {
    auto keySlice = it.key;
    if (ADB_UNLIKELY(!keySlice.isString())) {
      return Result(TRI_ERROR_TYPE_ERROR,
                    "When deserializing AqlCall: Key is not a string");
    }
    auto key = keySlice.stringView();

    if (auto propIt = std::find_if(
            expectedPropertiesFound.begin(), expectedPropertiesFound.end(),
            [&key](auto const& epf) { return epf.first == key; });
        ADB_LIKELY(propIt != expectedPropertiesFound.end())) {
      propIt->second = true;
    }

    if (key == StaticStrings::AqlRemoteLimit) {
      auto maybeLimit = readLimit(it.value);
      if (maybeLimit.fail()) {
        return std::move(maybeLimit).result();
      }
      limit = maybeLimit.get();
    } else if (key == StaticStrings::AqlRemoteLimitType) {
      auto maybeLimitType = readLimitType(it.value);
      if (maybeLimitType.fail()) {
        return std::move(maybeLimitType).result();
      }
      limitType = maybeLimitType.get();
    } else if (key == StaticStrings::AqlRemoteFullCount) {
      auto maybeFullCount = readFullCount(it.value);
      if (maybeFullCount.fail()) {
        return std::move(maybeFullCount).result();
      }
      fullCount = maybeFullCount.get();
    } else if (key == StaticStrings::AqlRemoteOffset) {
      auto maybeOffset = readOffset(it.value);
      if (maybeOffset.fail()) {
        return std::move(maybeOffset).result();
      }
      offset = maybeOffset.get();
    } else {
      LOG_TOPIC("404b0", WARN, Logger::AQL)
          << "When deserializing AqlCall: Encountered unexpected key " << key;
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

  auto call = AqlCall{};

  if (limitType.has_value()) {
    switch (limitType.value()) {
      case LimitType::SOFT:
        call.softLimit = limit;
        break;
      case LimitType::HARD:
        call.hardLimit = limit;
        break;
    }
  } else if (ADB_UNLIKELY(!std::holds_alternative<Infinity>(limit))) {
    return Result(
        TRI_ERROR_TYPE_ERROR,
        "When deserializing AqlCall: limit set, but limitType is missing");
  }

  call.offset = offset;
  call.fullCount = fullCount;

  return call;
}

void AqlCall::resetSkipCount() noexcept { skippedRows = 0; }

void AqlCall::toVelocyPack(velocypack::Builder& builder) const {
  using namespace velocypack;

  auto limitType = std::optional<LimitType>{};
  auto limit = Limit{Infinity{}};
  if (hasHardLimit()) {
    limitType = LimitType::HARD;
    limit = hardLimit;
  } else if (hasSoftLimit()) {
    limitType = LimitType::SOFT;
    limit = softLimit;
  }

  auto const limitValue = std::visit(
      overload{
          [](Infinity) { return Value(StaticStrings::AqlRemoteInfinity); },
          [](std::size_t limit) { return Value(limit); },
      },
      limit);
  auto const limitTypeValue = std::invoke([&]() {
    if (!limitType.has_value()) {
      return Value(ValueType::Null);
    }
    switch (limitType.value()) {
      case LimitType::SOFT:
        return Value(StaticStrings::AqlRemoteLimitTypeSoft);
      case LimitType::HARD:
        return Value(StaticStrings::AqlRemoteLimitTypeHard);
    }
    // unreachable
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  });

  builder.openObject(/*unindexed*/ true);
  builder.add(StaticStrings::AqlRemoteLimit, limitValue);
  builder.add(StaticStrings::AqlRemoteLimitType, limitTypeValue);
  builder.add(StaticStrings::AqlRemoteFullCount, Value(fullCount));
  builder.add(StaticStrings::AqlRemoteOffset, Value(offset));
  builder.close();
}

auto AqlCall::toString() const -> std::string {
  auto stream = std::stringstream{};
  stream << *this;
  return stream.str();
}

auto AqlCall::requestLessDataThan(AqlCall const& other) const noexcept -> bool {
  if (getOffset() > other.getOffset()) {
    return false;
  }
  if (getLimit() > other.getLimit()) {
    return false;
  }
  return needsFullCount() == other.needsFullCount();
}

auto aql::operator<<(std::ostream& out, AqlCall::LimitPrinter const& printer)
    -> std::ostream& {
  return std::visit(
      overload{[&out](size_t const& i) -> std::ostream& { return out << i; },
               [&out](AqlCall::Infinity const&) -> std::ostream& {
                 return out << "unlimited";
               }},
      printer._limit);
}

auto aql::operator<<(std::ostream& out, AqlCall const& call) -> std::ostream& {
  return out << "{ skip: " << call.getOffset()
             << ", softLimit: " << AqlCall::LimitPrinter(call.softLimit)
             << ", hardLimit: " << AqlCall::LimitPrinter(call.hardLimit)
             << ", fullCount: " << std::boolalpha << call.fullCount
             << ", skipCount: " << call.getSkipCount() << " }";
}
