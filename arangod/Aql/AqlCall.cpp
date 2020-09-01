////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "AqlCall.h"

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <velocypack/Collection.h>
#include <velocypack/Slice.h>

#include <iostream>
#include <map>
#include <string_view>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
// hack for MSVC
auto getStringView(velocypack::Slice slice) -> std::string_view {
  velocypack::StringRef ref = slice.stringRef();
  return std::string_view(ref.data(), ref.size());
}
}  // namespace

auto AqlCall::fromVelocyPack(velocypack::Slice slice) -> ResultT<AqlCall> {
  if (ADB_UNLIKELY(!slice.isObject())) {
    using namespace std::string_literals;
    return Result(TRI_ERROR_TYPE_ERROR,
                  "When deserializating AqlCall: Expected object, got "s + slice.typeName());
  }

  auto expectedPropertiesFound = std::map<std::string_view, bool>{};
  expectedPropertiesFound.emplace(StaticStrings::AqlRemoteLimit, false);
  expectedPropertiesFound.emplace(StaticStrings::AqlRemoteLimitType, false);
  expectedPropertiesFound.emplace(StaticStrings::AqlRemoteFullCount, false);
  expectedPropertiesFound.emplace(StaticStrings::AqlRemoteOffset, false);

  auto limit = AqlCall::Limit{};
  auto limitType = std::optional<AqlCall::LimitType>{};
  auto offset = decltype(AqlCall::offset){0};
  auto fullCount = false;

  auto const readLimit = [](velocypack::Slice slice) -> ResultT<AqlCall::Limit> {
    auto const type = slice.type();
    if (type == velocypack::ValueType::String &&
        slice.isEqualString(StaticStrings::AqlRemoteInfinity)) {
      return AqlCall::Limit{AqlCall::Infinity{}};
    } else if (slice.isInteger()) {
      try {
        return AqlCall::Limit{slice.getNumber<std::size_t>()};
      } catch (velocypack::Exception const& ex) {
        auto message = std::string{"When deserializating AqlCall: "};
        message += "When reading limit: ";
        message += ex.what();
        return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
      }
    } else {
      auto message = std::string{"When deserializating AqlCall: "};
      message += "When reading limit: ";
      if (slice.isString()) {
        message += "Unexpected value '";
        message += getStringView(slice);
        message += "'";
      } else {
        message += "Unexpected type ";
        message += slice.typeName();
      }
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
  };

  auto const readLimitType =
      [](velocypack::Slice slice) -> ResultT<std::optional<AqlCall::LimitType>> {
    if (slice.isNull()) {
      return {std::nullopt};
    }
    if (ADB_UNLIKELY(!slice.isString())) {
      auto message = std::string{
          "When deserializating AqlCall: When reading limitType: "
          "Unexpected type "};
      message += slice.typeName();
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
    auto value = getStringView(slice);
    if (value == StaticStrings::AqlRemoteLimitTypeSoft) {
      return {AqlCall::LimitType::SOFT};
    } else if (value == StaticStrings::AqlRemoteLimitTypeHard) {
      return {AqlCall::LimitType::HARD};
    } else {
      auto message = std::string{
          "When deserializating AqlCall: When reading limitType: "
          "Unexpected value '"};
      message += value;
      message += "'";
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
  };

  auto const readFullCount = [](velocypack::Slice slice) -> ResultT<bool> {
    if (ADB_UNLIKELY(!slice.isBool())) {
      auto message = std::string{
          "When deserializating AqlCall: When reading fullCount: "
          "Unexpected type "};
      message += slice.typeName();
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
    return slice.getBool();
  };

  auto const readOffset = [](velocypack::Slice slice) -> ResultT<decltype(offset)> {
    if (!slice.isInteger()) {
      auto message = std::string{
          "When deserializating AqlCall: When reading offset: "
          "Unexpected type "};
      message += slice.typeName();
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
    try {
      return slice.getNumber<decltype(offset)>();
    } catch (velocypack::Exception const& ex) {
      auto message =
          std::string{"When deserializating AqlCall: When reading offset: "};
      message += ex.what();
      return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
    }
  };

  for (auto const it : velocypack::ObjectIterator(slice)) {
    auto const keySlice = it.key;
    if (ADB_UNLIKELY(!keySlice.isString())) {
      return Result(TRI_ERROR_TYPE_ERROR,
                    "When deserializating AqlCall: Key is not a string");
    }
    auto const key = getStringView(keySlice);

    if (auto propIt = expectedPropertiesFound.find(key);
        ADB_LIKELY(propIt != expectedPropertiesFound.end())) {
      if (ADB_UNLIKELY(propIt->second)) {
        return Result(
            TRI_ERROR_TYPE_ERROR,
            "When deserializating AqlCall: Encountered duplicate key");
      }
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
          << "When deserializating AqlCall: Encountered unexpected key " << key;
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
        "When deserializating AqlCall: limit set, but limitType is missing.");
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

  auto const limitValue =
      std::visit(overload{
                     [](Infinity) {
                       return Value(StaticStrings::AqlRemoteInfinity);
                     },
                     [](std::size_t limit) { return Value(limit); },
                 },
                 limit);
  auto const limitTypeValue = std::invoke([&]() {
    if (!limitType.has_value()) {
      return Value(ValueType::Null);
    } else {
      switch (limitType.value()) {
        case LimitType::SOFT:
          return Value(StaticStrings::AqlRemoteLimitTypeSoft);
        case LimitType::HARD:
          return Value(StaticStrings::AqlRemoteLimitTypeHard);
      }
      // unreachable
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
  });

  builder.openObject();
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
  return std::visit(overload{[&out](size_t const& i) -> std::ostream& {
                               return out << i;
                             },
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
