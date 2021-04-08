////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_AQL_CALL_H
#define ARANGOD_AQL_AQL_CALL_H 1

#include "Aql/ExecutionBlock.h"
#include "Basics/Common.h"
#include "Basics/ResultT.h"
#include "Basics/overload.h"

#include <algorithm>
#include <cstddef>
#include <iosfwd>
#include <tuple>
#include <variant>

namespace arangodb::velocypack {
class Slice;
}

namespace arangodb::aql {

struct AqlCall {
  // TODO We currently have softLimit and hardLimit, where both can be a number
  //      or Infinity - but not both may be non-infinite at the same time.
  //      In addition, fullCount does only make sense together with a hard
  //      limit.
  //      The data structures and APIs should reflect that. E.g.:
  //      Infinity | SoftLimit { count : Int } | HardLimit { count : Int, fullCount : Bool }
  //      On a less important case, softLimit = 0 and offset = 0 do not occur together,
  //      but it's probably not worth implementing that in terms of data structures.
  class Infinity {};
  using Limit = std::variant<std::size_t, Infinity>;

  /**
   * @brief We need to implement this wrappter class only for a MSVC compiler insufficency:
   * For some reason (see bug-report here: https://developercommunity.visualstudio.com/content/problem/1031281/improper-c4244-warning-in-variant-code.html)
   * the MSVC compiler decides on every operator<< usage if this implementation could be used.
   * This causes every operator<<(numberType) to test this implementation (and discard it afterwards), however if the
   * Number type is too large, this will result in a valid compilation unit, emitting this warning (possible dataloss e.g. double -> size_t), which is neither used
   * nor compiled, but the error is reported.
   * As we disallow any warnings in the build this will stop compilation here.
   *
   * Remove this as soon as the MSVC compiler is fixed.
   * 
   * So this wrapper class will be wrapped arround every limit to print now.
   */

  struct LimitPrinter {
    explicit LimitPrinter(Limit const& limit) : _limit(limit) {}
    ~LimitPrinter() = default;

    // Never allow any kind of copying
    LimitPrinter(LimitPrinter const& other) = delete;
    LimitPrinter(LimitPrinter&& other) = delete;

    Limit _limit;
  };

  AqlCall() = default;
  // Replacements for struct initialization
  // cppcheck-suppress *
  explicit constexpr AqlCall(size_t off, Limit soft = Infinity{},
                             Limit hard = Infinity{}, bool fc = false)
      : offset{off}, softLimit{soft}, hardLimit{hard}, fullCount{fc} {}

  enum class LimitType { SOFT, HARD };
  // cppcheck-suppress *
  constexpr AqlCall(size_t off, bool fc, Infinity)
      : offset{off}, softLimit{Infinity{}}, hardLimit{Infinity{}}, fullCount{fc} {}
  // cppcheck-suppress *
  constexpr AqlCall(size_t off, bool fc, size_t limit, LimitType limitType)
      : offset{off},
        softLimit{limitType == LimitType::SOFT ? Limit{limit} : Limit{Infinity{}}},
        hardLimit{limitType == LimitType::HARD ? Limit{limit} : Limit{Infinity{}}},
        fullCount{fc} {}

  static auto fromVelocyPack(velocypack::Slice) -> ResultT<AqlCall>;
  void toVelocyPack(velocypack::Builder&) const;

  auto toString() const -> std::string;

  // TODO Remove me, this will not be necessary later
  static AqlCall SimulateSkipSome(std::size_t toSkip) {
    return AqlCall{/*offset*/ toSkip, /*softLimit*/ 0u, /*hardLimit*/ AqlCall::Infinity{}, /*fullCount*/ false};
  }

  // TODO Remove me, this will not be necessary later
  static AqlCall SimulateGetSome(std::size_t atMost) {
    return AqlCall{/*offset*/ 0, /*softLimit*/ atMost, /*hardLimit*/ AqlCall::Infinity{}, /*fullCount*/ false};
  }

  // TODO Remove me, this will not be necessary later
  static bool IsSkipSomeCall(AqlCall const& call) {
    return call.getOffset() > 0;
  }

  // TODO Remove me, this will not be necessary later
  static bool IsGetSomeCall(AqlCall const& call) {
    return call.getLimit() > 0 && call.getOffset() == 0;
  }

  // TODO Remove me, this will not be necessary later
  static bool IsFullCountCall(AqlCall const& call) {
    return call.hasHardLimit() && call.getLimit() == 0 &&
           call.getOffset() == 0 && call.needsFullCount();
  }

  static bool IsFastForwardCall(AqlCall const& call) {
    return call.hasHardLimit() && call.getLimit() == 0 &&
           call.getOffset() == 0 && !call.needsFullCount();
  }

  std::size_t offset{0};
  // TODO: The defaultBatchSize function could move into this file instead
  // TODO We must guarantee that at most one of those is not Infinity.
  //      To do that, we should replace softLimit and hardLimit with
  //        Limit limit;
  //        bool isHardLimit;
  //      .
  Limit softLimit{Infinity{}};
  Limit hardLimit{Infinity{}};
  bool fullCount{false};
  std::size_t skippedRows{0};

  std::size_t getOffset() const { return offset; }

  // TODO I think this should return the actual limit without regards to the batch size,
  //      so we can use it to calculate upstream calls. The batch size should be applied
  //      when allocating blocks only!
  std::size_t getLimit() const {
    return clampToLimit(ExecutionBlock::DefaultBatchSize);
  }

  std::size_t clampToLimit(size_t limit) const {  // By default we use batchsize
    // We are not allowed to go above softLimit
    if (std::holds_alternative<std::size_t>(softLimit)) {
      limit = (std::min)(std::get<std::size_t>(softLimit), limit);
    }
    // We are not allowed to go above hardLimit
    if (std::holds_alternative<std::size_t>(hardLimit)) {
      limit = (std::min)(std::get<std::size_t>(hardLimit), limit);
    }
    return limit;
  }

  void didSkip(std::size_t n) noexcept {
    if (n <= offset) {
      offset -= n;
    } else {
      TRI_ASSERT(fullCount);
      // We might have skip,(produce?),fullCount
      // in a single call here.
      offset = 0;
    }
    skippedRows += n;
  }

  [[nodiscard]] std::size_t getSkipCount() const noexcept {
    return skippedRows;
  }

  // TODO this is the same as shouldSkip(), remove one of them.
  [[nodiscard]] bool needSkipMore() const noexcept {
    return (0 < getOffset()) || (getLimit() == 0 && needsFullCount());
  }

  void didProduce(std::size_t n) {
    auto minus = overload{
        [n](size_t& i) {
          TRI_ASSERT(n <= i);
          i -= n;
        },
        [](auto) {},
    };
    std::visit(minus, softLimit);
    std::visit(minus, hardLimit);
  }

  void resetSkipCount() noexcept;

  bool hasLimit() const { return hasHardLimit() || hasSoftLimit(); }

  bool hasHardLimit() const {
    return !std::holds_alternative<AqlCall::Infinity>(hardLimit);
  }

  bool hasSoftLimit() const {
    return !std::holds_alternative<AqlCall::Infinity>(softLimit);
  }

  bool needsFullCount() const { return fullCount; }

  // TODO this is the same as needSkipMore(), remove one of them.
  bool shouldSkip() const {
    return getOffset() > 0 || (getLimit() == 0 && needsFullCount());
  }

  auto requestLessDataThan(AqlCall const& other) const noexcept -> bool;
};

constexpr bool operator<(AqlCall::Limit const& a, AqlCall::Limit const& b) {
  if (std::holds_alternative<AqlCall::Infinity>(a)) {
    return false;
  }
  if (std::holds_alternative<AqlCall::Infinity>(b)) {
    return true;
  }
  return std::get<size_t>(a) < std::get<size_t>(b);
}

constexpr bool operator<(AqlCall::Limit const& a, size_t b) {
  if (std::holds_alternative<AqlCall::Infinity>(a)) {
    return false;
  }
  return std::get<size_t>(a) < b;
}

constexpr bool operator<(size_t a, AqlCall::Limit const& b) {
  if (std::holds_alternative<AqlCall::Infinity>(b)) {
    return true;
  }
  return a < std::get<size_t>(b);
}

constexpr bool operator>(size_t a, AqlCall::Limit const& b) {
  return b < a;
}

constexpr bool operator>(AqlCall::Limit const& a, size_t b) {
  return b < a;
}

constexpr AqlCall::Limit operator+(AqlCall::Limit const& a, size_t n) {
  return std::visit(
      overload{[n](size_t const& i) -> AqlCall::Limit { return i + n; },
               [](AqlCall::Infinity inf) -> AqlCall::Limit { return inf; }},
      a);
}

constexpr AqlCall::Limit operator+(size_t n, AqlCall::Limit const& a) {
  return a + n;
}

constexpr AqlCall::Limit operator+(AqlCall::Limit const& a, AqlCall::Limit const& b) {
  return std::visit(
      overload{[&a](size_t const& b_) -> AqlCall::Limit { return a + b_; },
               [](AqlCall::Infinity inf) -> AqlCall::Limit { return inf; }},
      a);
}

constexpr bool operator==(AqlCall::Limit const& a, size_t n) {
  return std::visit(overload{[n](size_t const& i) -> bool { return i == n; },
                             [](auto inf) -> bool { return false; }},
                    a);
}

constexpr bool operator==(size_t n, AqlCall::Limit const& a) { return a == n; }

constexpr bool operator==(AqlCall::Limit const& a,
                          arangodb::aql::AqlCall::Infinity const& n) {
  return std::visit(overload{[](size_t const& i) -> bool { return false; },
                             [](auto inf) -> bool { return true; }},
                    a);
}

constexpr bool operator==(arangodb::aql::AqlCall::Infinity const& n,
                          AqlCall::Limit const& a) {
  return a == n;
}

constexpr bool operator==(AqlCall::Limit const& a, AqlCall::Limit const& b) {
  return std::visit(overload{[&b](size_t const& i) -> bool { return i == b; },
                             [&b](auto inf) -> bool { return inf == b; }},
                    a);
}

constexpr bool operator==(AqlCall const& left, AqlCall const& right) {
  return left.hardLimit == right.hardLimit && left.softLimit == right.softLimit &&
         left.offset == right.offset && left.fullCount == right.fullCount &&
         left.skippedRows == right.skippedRows;
}

auto operator<<(std::ostream& out, const arangodb::aql::AqlCall::LimitPrinter& limit)
    -> std::ostream&;

auto operator<<(std::ostream& out, const arangodb::aql::AqlCall& call) -> std::ostream&;

}  // namespace arangodb::aql

#endif
