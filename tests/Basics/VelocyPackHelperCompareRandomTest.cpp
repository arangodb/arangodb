////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024 ArangoDB GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include <bitset>
#include <cstring>
#include <thread>

#include <fmt/core.h>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

#include "Aql/AqlValue.h"
#include "Basics/VelocyPackHelper.h"
#include "Containers/Enumerate.h"
#include "Random/RandomGenerator.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"

using namespace arangodb;
using namespace arangodb::velocypack;

// TODO Implement generating close values of a given Number, both in the same
//      type and in others

// TODO Adapt the probability distributions, so generated numbers are more
//      interesting (e.g. lots of doubles |d| < 1 probably aren't helpful).

// TODO This test should run multiple threads, so we can get more coverage in
//      the same time.

// TODO We could probably get by with generating fewer random numbers, if it's
//      too expensive.

struct VPackHelperRandomTest : public testing::Test {
  // TODO randomize seed
  std::uint64_t const seed = 42;
  void SetUp() override {
    RecordProperty("seed", fmt::format("{}", seed));
    RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
    RandomGenerator::seed(seed);
  }

  enum class NumberValueType : std::underlying_type_t<ValueType> {
    Double = static_cast<std::underlying_type_t<ValueType>>(ValueType::Double),
    Int = static_cast<std::underlying_type_t<ValueType>>(ValueType::Int),
    UInt = static_cast<std::underlying_type_t<ValueType>>(ValueType::UInt),
    SmallInt =
        static_cast<std::underlying_type_t<ValueType>>(ValueType::SmallInt),
  };
  struct Number {
    std::uint64_t significand{};
    std::int16_t exponent{};
    // signum is -1 for negative numbers, 1 for positive, and 0 for 0.
    std::int8_t signum{};
    NumberValueType type{};
    // vpack representation of this number
    std::uint8_t vpBuffer[9] = {};

    [[nodiscard]] auto valueType() const noexcept -> ValueType {
      return static_cast<ValueType>(type);
    }
    [[nodiscard]] auto slice() const noexcept -> Slice {
      return Slice(vpBuffer);
    }
    void check() const;

    [[nodiscard]] auto operator<=>(Number const& right) const
        -> std::weak_ordering {
      Number const& left = *this;
      auto const signumCmp = left.signum <=> right.signum;
      if (signumCmp != std::strong_ordering::equal) {
        return signumCmp;
      }
      auto const commonSignum = left.signum;  // left.signum == right.signum
      if (commonSignum == 0) {                // both numbers are 0
        return std::strong_ordering::equal;
      }
      // both numbers are either strictly positive, or strictly negative, and
      // in particular both are non-zero.

      auto const leftBits = 64 - std::countl_zero(left.significand);
      auto const rightBits = 64 - std::countl_zero(right.significand);
      auto const leftShift = leftBits >= rightBits ? 0 : rightBits - leftBits;
      auto const rightShift = rightBits >= leftBits ? 0 : leftBits - rightBits;
      // shift the smaller significand, so both have the same number of bits,
      // without losing digits. Adjust the exponent to compensate.
      auto const leftSignificand = left.significand << leftShift;
      auto const rightSignificand = right.significand << rightShift;
      auto const leftExponent = left.exponent - leftShift;
      auto const rightExponent = right.exponent - rightShift;

      auto const exponentCmp = leftExponent <=> rightExponent;
      if (exponentCmp != std::strong_ordering::equal) {
        if (commonSignum > 0) {
          return exponentCmp <=> 0;
        } else {
          return 0 <=> exponentCmp;
        }
      }

      auto const significandCmp = leftSignificand <=> rightSignificand;
      if (commonSignum > 0) {
        return significandCmp <=> 0;
      } else {
        return 0 <=> significandCmp;
      }
    }

    template<VPackHelperRandomTest::NumberValueType type>
    void writeVPack();
  };

  static auto genType() -> NumberValueType {
    auto n = RandomGenerator::interval(std::uint32_t{3});
    // TODO Possibly tune the probabilities (e.g. make SmallInt much less
    //      likely: it only has 16 distinct values!)
    switch (n) {
      case 0:
        return NumberValueType::Double;
      case 1:
        return NumberValueType::Int;
      case 2:
        return NumberValueType::UInt;
      case 3:
        return NumberValueType::SmallInt;
      default:
        ADB_UNREACHABLE;
    }
  }

  static thread_local velocypack::Builder reusableBuilder;

  template<NumberValueType type>
  static auto genNumber() -> Number {
    auto number = Number{.type = type};

    constexpr auto isSigned = type != NumberValueType::UInt;

    // significand
    if constexpr (type == NumberValueType::SmallInt) {
      auto const n = RandomGenerator::interval(-6, 9);
      if (n < 0) {
        number.signum = -1;
      } else if (n > 0) {
        number.signum = 1;
      }
      number.significand = std::abs(n);
    } else {
      constexpr auto maxSignificantBits =  //
          type == NumberValueType::Double ? 53
          : type == NumberValueType::UInt ? 64
          : type == NumberValueType::Int  ? 63
                                          : 0;

      auto const significantBits =
          RandomGenerator::interval(std::uint32_t{maxSignificantBits});
      auto const remainingBits = maxSignificantBits - significantBits;

      auto const maxSignificand =
          significantBits == 64 ? std::numeric_limits<std::uint64_t>::max()
                                : (std::uint64_t{1} << significantBits) - 1;
      number.significand = RandomGenerator::interval(maxSignificand);
      number.significand <<=
          RandomGenerator::interval(std::min<std::uint32_t>(remainingBits, 63));
    }

    // exponent
    if constexpr (type == NumberValueType::Double) {
      if (number.significand != 0) {
        auto const bitsAfterFirstOne =
            64 - std::countl_zero(number.significand) - 1;
        // In order to normalize the significand, multiply it with
        // 2**normalizeExponent
        auto const normalizeExponent = -bitsAfterFirstOne;
        // TODO change probability distribution, like for the significand
        //      (choose the number of bits uniformly first)
        //        number.exponent =
        //            normalizeExponent + RandomGenerator::interval(-1022,
        //            1023);
        number.exponent =
            normalizeExponent + RandomGenerator::interval(-64, 64);
      }
    }

    // signum (SmallInt sets signum earlier)
    if constexpr (isSigned && type != NumberValueType::SmallInt) {
      if (number.significand != 0) {
        number.signum = RandomGenerator::interval(std::uint32_t{1}) * 2 - 1;
      }
    } else if (type == NumberValueType::UInt) {
      number.signum = number.significand == 0 ? 0 : 1;
    }

    // TODO remove the check when everything works
    number.check();
    number.writeVPack<type>();

    return number;
  }

  static auto genNumber() -> Number {
    auto type = genType();
    switch (type) {
      case NumberValueType::Double:
        return genNumber<NumberValueType::Double>();
      case NumberValueType::Int:
        return genNumber<NumberValueType::Int>();
      case NumberValueType::UInt:
        return genNumber<NumberValueType::UInt>();
      case NumberValueType::SmallInt:
        return genNumber<NumberValueType::SmallInt>();
    }
  }
};

auto to_string(VPackHelperRandomTest::NumberValueType type)
    -> std::string_view {
  switch (type) {
    case VPackHelperRandomTest::NumberValueType::Double:
      return "Double";
    case VPackHelperRandomTest::NumberValueType::Int:
      return "Int";
    case VPackHelperRandomTest::NumberValueType::UInt:
      return "UInt";
    case VPackHelperRandomTest::NumberValueType::SmallInt:
      return "SmallInt";
  }
}
auto to_string(VPackHelperRandomTest::Number const& number) -> std::string {
  return fmt::format(
      "{{.type={}, .signum={}, .exponent={}, .significand={}, slice={}}}",
      to_string(number.type), number.signum, number.exponent,
      number.significand, number.slice().toString());
}

auto operator<<(std::ostream& ostream,
                VPackHelperRandomTest::Number const& number) -> std::ostream& {
  return ostream << to_string(number);
}

void VPackHelperRandomTest::Number::check() const {
  TRI_ASSERT((signum == 0) == (significand == 0)) << *this;
  TRI_ASSERT(signum != 0 || exponent == 0) << *this;
}

template<VPackHelperRandomTest::NumberValueType type>
void VPackHelperRandomTest::Number::writeVPack() {
  reusableBuilder.clear();
  if constexpr (type == NumberValueType::Double) {
    auto d = signum * std::ldexp(significand, exponent);
    reusableBuilder.add(Value(d, static_cast<ValueType>(type)));
  } else if constexpr (type == NumberValueType::UInt) {
    reusableBuilder.add(Value(significand, static_cast<ValueType>(type)));
  } else if constexpr (type == NumberValueType::Int) {
    if (signum == -1 && significand == 0) [[unlikely]] {
      // there is no -0 in two's complement; use ::lowest() instead, which
      // is also not yet part of the generated values, as its representation
      // is 1 bit longer.
      reusableBuilder.add(Value(std::numeric_limits<std::int64_t>::lowest(),
                                static_cast<ValueType>(type)));
    } else {
      reusableBuilder.add(Value(signum * static_cast<std::int64_t>(significand),
                                static_cast<ValueType>(type)));
    }
  } else if constexpr (type == NumberValueType::SmallInt) {
    reusableBuilder.add(Value(signum * static_cast<std::int32_t>(significand),
                              static_cast<ValueType>(type)));
  }
  std::memcpy(vpBuffer, reusableBuilder.slice().start(),
              reusableBuilder.slice().byteSize());
}

thread_local Builder VPackHelperRandomTest::reusableBuilder;

TEST_F(VPackHelperRandomTest, test_me) {
  auto left = Number{
      .significand = 34359738368,
      .exponent = 0,
      .signum = -1,
      .type = NumberValueType::Int,
  };  // slice=-34359738368
  left.writeVPack<NumberValueType::Int>();
  auto right = Number{
      .significand = 264904856511168,
      .exponent = -13,
      .signum = -1,
      .type = NumberValueType::Double,
  };  // slice=-32337018617.085938
  right.writeVPack<NumberValueType::Double>();
  EXPECT_EQ(left <=> right, std::weak_ordering::less);
  EXPECT_EQ(right <=> left, std::weak_ordering::greater);
}

// TODO maybe add a handful of tests for Number::operator<=>

static auto compareVPack(VPackHelperRandomTest::Number const& left,
                         VPackHelperRandomTest::Number const& right)
    -> std::strong_ordering {
  auto res = basics::VelocyPackHelper::compareNumberValuesCorrectly(
      left.slice().type(), left.slice(), right.slice());
  if (res < 0) {
    return std::strong_ordering::less;
  }
  if (res > 0) {
    return std::strong_ordering::greater;
  }
  return std::strong_ordering::equal;
}

static auto compareAqlValue(VPackHelperRandomTest::Number const& left,
                            VPackHelperRandomTest::Number const& right)
    -> std::strong_ordering {
  velocypack::Options options;
  arangodb::aql::AqlValue leftValue{left.slice()};
  arangodb::aql::AqlValue rightValue{right.slice()};
  auto res =
      arangodb::aql::AqlValue::Compare(&options, leftValue, rightValue, true);
  if (res < 0) {
    return std::strong_ordering::less;
  }
  if (res > 0) {
    return std::strong_ordering::greater;
  }
  return std::strong_ordering::equal;
}

TEST_F(VPackHelperRandomTest, test_vpackcmps) {
  auto lambda = [&](std::uint64_t seed) {
    RandomGenerator::seed(seed);
    SCOPED_TRACE(fmt::format("seed={}", seed));
    constexpr auto num = 10000;
    auto numbers = std::multiset<Number>{};
    for (auto i = 0; i < num; ++i) {
      auto n = genNumber();
      // TODO generate and add a few numbers near `n`, also of different types
      numbers.emplace(n);
    }

    auto groupedNumbers = std::vector<std::vector<Number>>();

    auto last = std::optional<Number>();
    for (auto const& n : numbers) {
      if (last.has_value() and (n <=> *last) == 0) {
        groupedNumbers.back().emplace_back(n);
      } else {
        groupedNumbers.emplace_back(std::vector({n}));
      }
      last = n;
    }

    for (auto const& [li, leftGroup] : enumerate(groupedNumbers)) {
      for (auto const& left : leftGroup) {
        for (auto const& [ri, rightGroup] : enumerate(groupedNumbers)) {
          for (auto const& right : rightGroup) {
            EXPECT_EQ(compareVPack(left, right), li <=> ri)
                << left << " " << right;
            EXPECT_EQ(compareAqlValue(left, right), li <=> ri)
                << left << " " << right;
          }
        }
      }
    }
  };

  constexpr auto numberOfThreads = 8;

  std::vector<std::jthread> _threads;
  _threads.reserve(numberOfThreads);
  std::generate_n(std::back_inserter(_threads), numberOfThreads,
                  [&, i = 0]() mutable {
                    return std::jthread{lambda, seed + i++};
                  });
}
