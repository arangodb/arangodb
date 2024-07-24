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

#include <cstdint>
#include <cstring>

#include <fmt/core.h>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

#include "Basics/VelocyPackHelper.h"
#include "Random/RandomGenerator.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"

using namespace arangodb;
using namespace arangodb::velocypack;

// TODO Implement generating close values of a given Number, both in the same
//      type and in others

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
    std::int8_t signum{};
    NumberValueType type{};
    std::uint8_t vpBuffer[9] = {};

    [[nodiscard]] auto valueType() const noexcept -> ValueType {
      return static_cast<ValueType>(type);
    }
    [[nodiscard]] auto slice() const noexcept -> Slice {
      return Slice(vpBuffer);
    }

    [[nodiscard]] auto operator<=>(Number const& right) {
      // Number const& left = *this;
      // TODO implement comparison
    }
  };

  static auto genType() -> NumberValueType {
    auto n = RandomGenerator::interval(std::uint32_t{3});
    // TODO Possibly tune the probabilities (e.g. make SmallInt less likely)
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
          RandomGenerator::interval(std::uint32_t{remainingBits});
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
        number.exponent =
            normalizeExponent + RandomGenerator::interval(-1022, 1023);
      }
    }

    // signum (SmallInt sets signum earlier)
    if constexpr (isSigned && type != NumberValueType::SmallInt) {
      if (number.significand != 0) {
        number.signum = RandomGenerator::interval(std::uint32_t{1}) * 2 - 1;
      }
    }

    // vpBuffer (velocypack representation)
    reusableBuilder.clear();
    if constexpr (type == NumberValueType::Double) {
      auto d = number.signum * std::ldexp(number.significand, number.exponent);
      reusableBuilder.add(Value(d, static_cast<ValueType>(type)));
    } else if constexpr (type == NumberValueType::UInt) {
      reusableBuilder.add(
          Value(number.significand, static_cast<ValueType>(type)));
    } else if constexpr (type == NumberValueType::Int) {
      if (number.signum == -1 && number.significand == 0) [[unlikely]] {
        // there is no -0 in two's complement; use ::lowest() instead, which
        // is also not yet part of the generated values, as its representation
        // is 1 bit longer.
        reusableBuilder.add(Value(std::numeric_limits<std::int64_t>::lowest(),
                                  static_cast<ValueType>(type)));
      } else {
        reusableBuilder.add(
            Value(number.signum * static_cast<std::int64_t>(number.significand),
                  static_cast<ValueType>(type)));
      }
    } else if constexpr (type == NumberValueType::SmallInt) {
      reusableBuilder.add(
          Value(number.signum * static_cast<std::int32_t>(number.significand),
                static_cast<ValueType>(type)));
    }
    std::memcpy(number.vpBuffer, reusableBuilder.slice().start(),
                reusableBuilder.slice().byteSize());

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

thread_local Builder VPackHelperRandomTest::reusableBuilder;

TEST_F(VPackHelperRandomTest, test) {
  SCOPED_TRACE(fmt::format("seed={}", seed));
    for(auto i = 0; i < 1000; ++i) {
      auto n = genNumber();
      std::cout << n.slice().typeName() << " " << n.slice().toString() << "\n";
    }
}
