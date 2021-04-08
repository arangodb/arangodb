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

#include "tests-common.h"

#include "gtest/gtest.h"

#include <velocypack/Builder.h>
#include <velocypack/SharedSlice.h>
#include <velocypack/Slice.h>

#include <memory>
#include <variant>

using namespace arangodb;
using namespace arangodb::velocypack;

void initTestCases(std::vector<Builder>& testCases) {
  {
    auto& builder = testCases.emplace_back();
    builder.add(Slice::noneSlice());
  }
  {
    auto& builder = testCases.emplace_back();
    builder.add(Slice::illegalSlice());
  }
  {
    auto& builder = testCases.emplace_back();
    builder.add(Slice::nullSlice());
  }
  {
    auto& builder = testCases.emplace_back();
    builder.add(Slice::trueSlice());
  }
  {
    auto& builder = testCases.emplace_back();
    builder.add(Slice::falseSlice());
  }
  {
    auto& builder = testCases.emplace_back();
    builder.add(Slice::zeroSlice());
  }
  {
    auto& builder = testCases.emplace_back();
    builder.add(Slice::emptyStringSlice());
  }
  {
    auto& builder = testCases.emplace_back();
    builder.add(Slice::emptyArraySlice());
  }
  {
    auto& builder = testCases.emplace_back();
    builder.add(Slice::emptyObjectSlice());
  }
  {
    auto& builder = testCases.emplace_back();
    builder.add(Slice::minKeySlice());
  }
  {
    auto& builder = testCases.emplace_back();
    builder.add(Slice::maxKeySlice());
  }
  {
    auto& builder = testCases.emplace_back();
    builder.add(Value(42));
  }
  {
    auto& builder = testCases.emplace_back();
    builder.add(Value(-42));
  }
  {
    auto& builder = testCases.emplace_back();
    builder.openArray();
    builder.add(Value(42));
    builder.close();
  }
  {
    auto& builder = testCases.emplace_back();
    builder.openArray();
    builder.add(Value("42"));
    builder.close();
  }
  {
    auto& builder = testCases.emplace_back();
    builder.openObject();
    builder.add("foo", Value(42));
    builder.close();
  }
  {
    auto& builder = testCases.emplace_back();
    builder.openObject();
    builder.add("bar", Value("42"));
    builder.close();
  }
  {
    auto& builder = testCases.emplace_back();
    builder.addTagged(42, Value("42"));
  }
  {
    auto& builder = testCases.emplace_back();
    builder.addExternal(Slice::nullSliceData);
  }
  // TODO add something using an attributeTranslator
  // TODO add a UTCDate
  // TODO add a BCD
  // TODO add a custom type
  // TODO add a binary
}

std::vector<Builder> const& testCases() {
  static std::vector<Builder> _testCases{};

  if (_testCases.empty()) {
    initTestCases(_testCases);
  }

  return _testCases;
}

// Iterates over all testcases. Takes a callback `F(Slice, SharedSlice)`.
template<typename F>
std::enable_if_t<std::is_invocable<F, Slice, SharedSlice>::value>
forAllTestCases(F f) {
  for (auto const& builder : testCases()) {
    ASSERT_TRUE(builder.isClosed());
    auto slice = builder.slice();
    //auto sharedSlice = SharedSlice(builder.buffer());
    auto sharedSlice = builder.sharedSlice();
    // both should point to equal data
    ASSERT_EQ(slice.byteSize(), sharedSlice.slice().byteSize());
    ASSERT_EQ(0, memcmp(slice.start(), sharedSlice.slice().start(), slice.byteSize()));
    // Now that we know the data is equal, make slice point to the exact same
    // location.
    // This makes comparisons in the tests easier, because they can be reduced
    // to pointer comparisons.
    slice = sharedSlice.slice();
    ASSERT_EQ(slice.start(), sharedSlice.slice().start());
    using namespace std::string_literals;
    auto sliceString = [&](){
      try {
        return slice.toString();
      } catch (Exception&) {
        return slice.toHex();
      }
    }();
    auto msg = "When testing slice "s + sliceString;
    SCOPED_TRACE(msg.c_str());
    f(slice, sharedSlice);
  }
}

// Iterates over all testcases. Takes a callback `F(SharedSlice&&)`
// (or `F(SharedSlice)`, the slice will be moved).
// Used for refcount tests, so the SharedSlice will be the only owner of its
// buffer.
template<typename F>
std::enable_if_t<std::is_invocable<F, SharedSlice&&>::value>
forAllTestCases(F f) {
  for (auto const& builder : testCases()) {
    Buffer buffer = builder.bufferRef();
    ASSERT_TRUE(builder.isClosed());
    auto sharedSlice = SharedSlice{std::move(buffer)};
    ASSERT_EQ(1, sharedSlice.buffer().use_count());
    using namespace std::string_literals;
    auto sliceString = [&]() {
      try {
        return sharedSlice.toString();
      } catch (Exception& e) {
        return sharedSlice.toHex();
      }
    }();
    auto msg = "When testing slice "s + sliceString;
    SCOPED_TRACE(msg.c_str());
    f(std::move(sharedSlice));
  }
}

/**
 * @brief Can hold either a value or an exception, and is constructed by
 *        executing a callback. Holds either the value the callback returns, or
 *        the velocypack::Exception it throws.
 *        Can then be used to compare the result/exception with another instance.
 *
 *        Also allows to compare a Slice with a SharedSlice by comparing the
 *        data pointers.
 */
template<typename V>
class ResultOrException {
 public:
  template<typename F>
  explicit ResultOrException(F f) {
    try {
      variant.template emplace<V>(f());
    } catch (Exception& e) {
      variant.template emplace<Exception>(e);
    }
  }

  template<typename W>
  bool operator==(ResultOrException<W> const& other) const {
    using LeftType = std::decay_t<decltype(std::get<0>(variant))>;
    using RightType = std::decay_t<decltype(std::get<0>(other.variant))>;
    static_assert(std::is_same_v<std::decay_t<V>, LeftType>);
    static_assert(std::is_same_v<std::decay_t<W>, RightType>);
    auto constexpr leftIsSlice = std::is_same_v<Slice, LeftType>;
    auto constexpr rightIsSharedSlice = std::is_same_v<SharedSlice, RightType>;
    // We only allow comparisons of either equal types V == W, or comparing
    // a Slice (left) with a SharedSlice (right).
    static_assert(leftIsSlice == rightIsSharedSlice);
    auto constexpr comparingSliceWithSharedSlice = leftIsSlice && rightIsSharedSlice;
    static_assert(std::is_same_v<V, W> || comparingSliceWithSharedSlice);

    if (variant.index() != other.variant.index()) {
      return false;
    }

    switch(variant.index()) {
      case 0: {
        auto const& left = std::get<0>(variant);
        auto const& right = std::get<0>(other.variant);
        if constexpr (comparingSliceWithSharedSlice) {
          // Compare slice with shared slice by pointer equality
          return left.start() == right.buffer().get();
        } else {
          // Compare other values with operator==()
          return left == right;
        }
      }
      case 1: {
        auto const& left = std::get<1>(variant);
        auto const& right = std::get<1>(other.variant);
        return left.errorCode() == right.errorCode();
      }
      case std::variant_npos:
        throw std::exception();
    }

    throw std::exception();
  }

  friend std::ostream& operator<<(std::ostream& out, ResultOrException const& that) {
    auto const& variant = that.variant;
    switch(variant.index()) {
      case 0: {
        auto const& value = std::get<0>(variant);
        return out << ::testing::PrintToString(value);
      }
      case 1: {
        auto const& ex = std::get<1>(variant);
        return out << ex;
      }
      case std::variant_npos:
        throw std::exception();
    }

    throw std::exception();
  }

  std::variant<V, Exception> variant;
};

template<typename F> ResultOrException(F) -> ResultOrException<std::invoke_result_t<F>>;

#define R(a) ResultOrException([&](){ return a; })

// Compare either the results, or the exceptions if thrown.
// I'd actually prefer to write
// ASSERT_EQ(R(left), R(right))
// inline everywhere, but we'd need googletest 1.10 for that to work.
#define ASSERT_EQ_EX(left, right) \
  do { \
    auto l = R(left); \
    auto r = R(right); \
    ASSERT_EQ(l, r); \
  } while(false)

TEST(SharedSliceAgainstSliceTest, value) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.value(), sharedSlice.value());
  });
}

TEST(SharedSliceAgainstSliceTest, getFirstTag) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getFirstTag(), sharedSlice.getFirstTag());
  });
}

TEST(SharedSliceAgainstSliceTest, getTags) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getTags(), sharedSlice.getTags());
  });
}

TEST(SharedSliceAgainstSliceTest, hasTag) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.hasTag(42), sharedSlice.hasTag(42));
  });
}

TEST(SharedSliceAgainstSliceTest, valueStart) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.valueStart(), sharedSlice.valueStart().get());
  });
}

TEST(SharedSliceAgainstSliceTest, start) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.start(), sharedSlice.start().get());
  });
}

TEST(SharedSliceAgainstSliceTest, startAs) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.startAs<void*>(), sharedSlice.startAs<void*>().get());
  });
}

TEST(SharedSliceAgainstSliceTest, head) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.head(), sharedSlice.head());
  });
}

TEST(SharedSliceAgainstSliceTest, begin) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.begin(), sharedSlice.begin().get());
  });
}

TEST(SharedSliceAgainstSliceTest, end) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.end(), sharedSlice.end().get());
  });
}

TEST(SharedSliceAgainstSliceTest, type) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.type(), sharedSlice.type());
  });
}

TEST(SharedSliceAgainstSliceTest, typeName) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.typeName(), sharedSlice.typeName());
  });
}

TEST(SharedSliceAgainstSliceTest, hash) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.hash(), sharedSlice.hash());
  });
}

TEST(SharedSliceAgainstSliceTest, hash32) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.hash32(), sharedSlice.hash32());
  });
}

TEST(SharedSliceAgainstSliceTest, hashSlow) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.hashSlow(), sharedSlice.hashSlow());
  });
}

TEST(SharedSliceAgainstSliceTest, normalizedHash) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.normalizedHash(), sharedSlice.normalizedHash());
  });
}

TEST(SharedSliceAgainstSliceTest, normalizedHash32) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.normalizedHash32(), sharedSlice.normalizedHash32());
  });
}

TEST(SharedSliceAgainstSliceTest, hashString) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isString()) {
      ASSERT_EQ(slice.hashString(), sharedSlice.hashString());
    }
  });
}

TEST(SharedSliceAgainstSliceTest, hashString32) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isString()) {
      ASSERT_EQ(slice.hashString32(), sharedSlice.hashString32());
    }
  });
}

TEST(SharedSliceAgainstSliceTest, isType) {
  auto const types = std::vector<ValueType>{
      ValueType::None,   ValueType::Illegal,  ValueType::Null,
      ValueType::Bool,   ValueType::Array,    ValueType::Object,
      ValueType::Double, ValueType::UTCDate,  ValueType::External,
      ValueType::MinKey, ValueType::MaxKey,   ValueType::Int,
      ValueType::UInt,   ValueType::SmallInt, ValueType::String,
      ValueType::Binary, ValueType::BCD,      ValueType::Custom,
      ValueType::Tagged};

  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const type : types) {
      using namespace std::string_literals;
      SCOPED_TRACE("When testing type "s + valueTypeName(type));
      ASSERT_EQ(slice.isType(type), sharedSlice.isType(type));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, isNone) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isNone(), sharedSlice.isNone());
  });
}

TEST(SharedSliceAgainstSliceTest, isIllegal) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isIllegal(), sharedSlice.isIllegal());
  });
}

TEST(SharedSliceAgainstSliceTest, isNull) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isNull(), sharedSlice.isNull());
  });
}

TEST(SharedSliceAgainstSliceTest, isBool) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isBool(), sharedSlice.isBool());
  });
}

TEST(SharedSliceAgainstSliceTest, isBoolean) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isBoolean(), sharedSlice.isBoolean());
  });
}

TEST(SharedSliceAgainstSliceTest, isTrue) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isTrue(), sharedSlice.isTrue());
  });
}

TEST(SharedSliceAgainstSliceTest, isFalse) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isFalse(), sharedSlice.isFalse());
  });
}

TEST(SharedSliceAgainstSliceTest, isArray) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isArray(), sharedSlice.isArray());
  });
}

TEST(SharedSliceAgainstSliceTest, isObject) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isObject(), sharedSlice.isObject());
  });
}

TEST(SharedSliceAgainstSliceTest, isDouble) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isDouble(), sharedSlice.isDouble());
  });
}

TEST(SharedSliceAgainstSliceTest, isUTCDate) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isUTCDate(), sharedSlice.isUTCDate());
  });
}

TEST(SharedSliceAgainstSliceTest, isExternal) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isExternal(), sharedSlice.isExternal());
  });
}

TEST(SharedSliceAgainstSliceTest, isMinKey) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isMinKey(), sharedSlice.isMinKey());
  });
}

TEST(SharedSliceAgainstSliceTest, isMaxKey) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isMaxKey(), sharedSlice.isMaxKey());
  });
}

TEST(SharedSliceAgainstSliceTest, isInt) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isInt(), sharedSlice.isInt());
  });
}

TEST(SharedSliceAgainstSliceTest, isUInt) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isUInt(), sharedSlice.isUInt());
  });
}

TEST(SharedSliceAgainstSliceTest, isSmallInt) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isSmallInt(), sharedSlice.isSmallInt());
  });
}

TEST(SharedSliceAgainstSliceTest, isString) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isString(), sharedSlice.isString());
  });
}

TEST(SharedSliceAgainstSliceTest, isBinary) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isBinary(), sharedSlice.isBinary());
  });
}

TEST(SharedSliceAgainstSliceTest, isBCD) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isBCD(), sharedSlice.isBCD());
  });
}

TEST(SharedSliceAgainstSliceTest, isCustom) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isCustom(), sharedSlice.isCustom());
  });
}

TEST(SharedSliceAgainstSliceTest, isTagged) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isTagged(), sharedSlice.isTagged());
  });
}

TEST(SharedSliceAgainstSliceTest, isInteger) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isInteger(), sharedSlice.isInteger());
  });
}

TEST(SharedSliceAgainstSliceTest, isNumber) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isNumber(), sharedSlice.isNumber());
  });
}

TEST(SharedSliceAgainstSliceTest, isNumberT) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isNumber<uint64_t>(), sharedSlice.isNumber<uint64_t>());
    ASSERT_EQ(slice.isNumber<int64_t>(), sharedSlice.isNumber<int64_t>());
    ASSERT_EQ(slice.isNumber<uint8_t>(), sharedSlice.isNumber<uint8_t>());
  });
}

TEST(SharedSliceAgainstSliceTest, isSorted) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ(slice.isSorted(), sharedSlice.isSorted());
  });
}

TEST(SharedSliceAgainstSliceTest, getBool) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getBool(), sharedSlice.getBool());
  });
}

TEST(SharedSliceAgainstSliceTest, getBoolean) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getBoolean(), sharedSlice.getBoolean());
  });
}

TEST(SharedSliceAgainstSliceTest, getDouble) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getDouble(), sharedSlice.getDouble());
  });
}

TEST(SharedSliceAgainstSliceTest, at) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.at(0), sharedSlice.at(0));
    ASSERT_EQ_EX(slice.at(1), sharedSlice.at(1));
  });
}

TEST(SharedSliceAgainstSliceTest, operatorIndexPValueLength) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.operator[](0), sharedSlice.operator[](0));
    ASSERT_EQ_EX(slice.operator[](1), sharedSlice.operator[](1));
  });
}

TEST(SharedSliceAgainstSliceTest, length) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.length(), sharedSlice.length());
  });
}

TEST(SharedSliceAgainstSliceTest, keyAt) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.keyAt(0), sharedSlice.keyAt(0));
    ASSERT_EQ_EX(slice.keyAt(1), sharedSlice.keyAt(1));
  });
}

TEST(SharedSliceAgainstSliceTest, valueAt) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.valueAt(0), sharedSlice.valueAt(0));
    ASSERT_EQ_EX(slice.valueAt(1), sharedSlice.valueAt(1));
  });
}

TEST(SharedSliceAgainstSliceTest, getNthValue) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isObject()) { // other types will run into an assertion
      ASSERT_EQ_EX(slice.getNthValue(0), sharedSlice.getNthValue(0));
      ASSERT_EQ_EX(slice.getNthValue(1), sharedSlice.getNthValue(1));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, getPVector) {
  using namespace std::string_literals;
  auto paths = std::vector<std::vector<std::string>>{{"foo"s}, {"bar"s}};
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const& path : paths) {
      ASSERT_EQ_EX(slice.get(path), sharedSlice.get(path));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, getPStringRef) {
  using namespace std::string_literals;
  auto attrs = std::vector<std::string>{"foo"s, "bar"s};
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const& attr : attrs) {
      ASSERT_EQ_EX(slice.get(StringRef(attr)), sharedSlice.get(StringRef(attr)));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, getPString) {
  using namespace std::string_literals;
  auto attrs = std::vector<std::string>{"foo"s, "bar"s};
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const& attr : attrs) {
      ASSERT_EQ_EX(slice.get(attr), sharedSlice.get(attr));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, getPCharPtr) {
  using namespace std::string_literals;
  auto attrs = std::vector<std::string>{"foo"s, "bar"s};
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const& attr : attrs) {
      ASSERT_EQ_EX(slice.get(attr.c_str()), sharedSlice.get(attr.c_str()));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, getPCharPtrLen) {
  using namespace std::string_literals;
  auto attrs = std::vector<std::string>{"foo"s, "bar"s};
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const& attr : attrs) {
      ASSERT_EQ_EX(slice.get(attr.c_str(), attr.length()), sharedSlice.get(attr.c_str(), attr.length()));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, operatorIndexPStringRef) {
  using namespace std::string_literals;
  auto attrs = std::vector<std::string>{"foo"s, "bar"s};
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const& attr : attrs) {
      ASSERT_EQ_EX(slice.operator[](StringRef(attr)), sharedSlice.operator[](StringRef(attr)));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, operatorIndexPString) {
  using namespace std::string_literals;
  auto attrs = std::vector<std::string>{"foo"s, "bar"s};
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const& attr : attrs) {
      ASSERT_EQ_EX(slice.operator[](attr), sharedSlice.operator[](attr));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, hasKeyPStringRef) {
  using namespace std::string_literals;
  auto attrs = std::vector<std::string>{"foo"s, "bar"s};
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const& attr : attrs) {
      ASSERT_EQ_EX(slice.hasKey(StringRef(attr)), sharedSlice.hasKey(StringRef(attr)));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, hasKeyPString) {
  using namespace std::string_literals;
  auto attrs = std::vector<std::string>{"foo"s, "bar"s};
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const& attr : attrs) {
      ASSERT_EQ_EX(slice.hasKey(attr), sharedSlice.hasKey(attr));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, hasKeyPCharPtr) {
  using namespace std::string_literals;
  auto attrs = std::vector<std::string>{"foo"s, "bar"s};
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const& attr : attrs) {
      ASSERT_EQ_EX(slice.hasKey(attr.c_str()), sharedSlice.hasKey(attr.c_str()));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, hasKeyPCharPtrLen) {
  using namespace std::string_literals;
  auto attrs = std::vector<std::string>{"foo"s, "bar"s};
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const& attr : attrs) {
      ASSERT_EQ_EX(slice.hasKey(attr.c_str(), attr.length()), sharedSlice.hasKey(attr.c_str(), attr.length()));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, hasKeyPVector) {
  using namespace std::string_literals;
  auto paths = std::vector<std::vector<std::string>>{{"foo"s}, {"bar"s}};
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    for (auto const& path : paths) {
      ASSERT_EQ_EX(slice.hasKey(path), sharedSlice.hasKey(path));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, getExternal) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getExternal(), sharedSlice.getExternal().get());
  });
}

TEST(SharedSliceAgainstSliceTest, resolveExternal) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.resolveExternal(), sharedSlice.resolveExternal());
  });
}

TEST(SharedSliceAgainstSliceTest, resolveExternals) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.resolveExternals(), sharedSlice.resolveExternals());
  });
}

TEST(SharedSliceAgainstSliceTest, isEmptyArray) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.isEmptyArray(), sharedSlice.isEmptyArray());
  });
}

TEST(SharedSliceAgainstSliceTest, isEmptyObject) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.isEmptyObject(), sharedSlice.isEmptyObject());
  });
}

TEST(SharedSliceAgainstSliceTest, translate) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.translate(), sharedSlice.translate());
  });
}

TEST(SharedSliceAgainstSliceTest, getInt) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getInt(), sharedSlice.getInt());
  });
}

TEST(SharedSliceAgainstSliceTest, getUInt) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getUInt(), sharedSlice.getUInt());
  });
}

TEST(SharedSliceAgainstSliceTest, getSmallInt) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getSmallInt(), sharedSlice.getSmallInt());
  });
}

TEST(SharedSliceAgainstSliceTest, getNumber) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getNumber<uint64_t>(), sharedSlice.getNumber<uint64_t>());
    ASSERT_EQ_EX(slice.getNumber<int64_t>(), sharedSlice.getNumber<int64_t>());
    ASSERT_EQ_EX(slice.getNumber<uint8_t>(), sharedSlice.getNumber<uint8_t>());
  });
}

TEST(SharedSliceAgainstSliceTest, getNumericValue) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getNumericValue<uint64_t>(), sharedSlice.getNumericValue<uint64_t>());
    ASSERT_EQ_EX(slice.getNumericValue<int64_t>(), sharedSlice.getNumericValue<int64_t>());
    ASSERT_EQ_EX(slice.getNumericValue<uint8_t>(), sharedSlice.getNumericValue<uint8_t>());
  });
}

TEST(SharedSliceAgainstSliceTest, getUTCDate) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getUTCDate(), sharedSlice.getUTCDate());
  });
}

TEST(SharedSliceAgainstSliceTest, getString) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ValueLength left;
    ValueLength right;
    ASSERT_EQ_EX(slice.getString(left), sharedSlice.getString(right).get());
    // The argument is only set when getString is successful
    if (slice.isString()) {
      ASSERT_EQ(left, right);
    }
  });
}

TEST(SharedSliceAgainstSliceTest, getStringUnchecked) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isString()) {
      ValueLength left;
      ValueLength right;
      ASSERT_EQ_EX(slice.getStringUnchecked(left),
                   sharedSlice.getStringUnchecked(right).get());
      ASSERT_EQ(left, right);
    }
  });
}

TEST(SharedSliceAgainstSliceTest, getStringLength) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getStringLength(), sharedSlice.getStringLength());
  });
}

TEST(SharedSliceAgainstSliceTest, copyString) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.copyString(), sharedSlice.copyString());
  });
}

TEST(SharedSliceAgainstSliceTest, stringRef) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.stringRef(), sharedSlice.stringRef());
  });
}

TEST(SharedSliceAgainstSliceTest, stringView) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.stringView(), sharedSlice.stringView());
  });
}

TEST(SharedSliceAgainstSliceTest, getBinary) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ValueLength left;
    ValueLength right;
    ASSERT_EQ_EX(slice.getBinary(left), sharedSlice.getBinary(right).get());
    // The argument is only set when getBinary is successful
    if (slice.isBinary()) {
      ASSERT_EQ(left, right);
    }
  });
}

TEST(SharedSliceAgainstSliceTest, getBinaryLength) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.getBinaryLength(), sharedSlice.getBinaryLength());
  });
}

TEST(SharedSliceAgainstSliceTest, copyBinary) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.copyBinary(), sharedSlice.copyBinary());
  });
}

TEST(SharedSliceAgainstSliceTest, byteSize) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.byteSize(), sharedSlice.byteSize());
  });
}

TEST(SharedSliceAgainstSliceTest, valueByteSize) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.valueByteSize(), sharedSlice.valueByteSize());
  });
}

// TODO Maybe it makes sense to add a test for this, but I don't know what this
//      does, exactly.
// TEST(SharedSliceAgainstSliceTest, findDataOffset) {
//   forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
//   });
// }

TEST(SharedSliceAgainstSliceTest, getNthOffset) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isArray() || slice.isObject()) { // avoid assertion
      ASSERT_EQ_EX(slice.getNthOffset(0), sharedSlice.getNthOffset(0));
      ASSERT_EQ_EX(slice.getNthOffset(1), sharedSlice.getNthOffset(1));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, makeKey) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.makeKey(), sharedSlice.makeKey());
  });
}

TEST(SharedSliceAgainstSliceTest, compareStringPStringRef) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.compareString(StringRef("42")), sharedSlice.compareString(StringRef("42")));
    ASSERT_EQ_EX(slice.compareString(StringRef("foo")), sharedSlice.compareString(StringRef("foo")));
    ASSERT_EQ_EX(slice.compareString(StringRef("bar")), sharedSlice.compareString(StringRef("bar")));
  });
}

TEST(SharedSliceAgainstSliceTest, compareStringPString) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    using namespace std::string_literals;
    ASSERT_EQ_EX(slice.compareString("42"s), sharedSlice.compareString("42"s));
    ASSERT_EQ_EX(slice.compareString("foo"s), sharedSlice.compareString("foo"s));
    ASSERT_EQ_EX(slice.compareString("bar"s), sharedSlice.compareString("bar"s));
  });
}

TEST(SharedSliceAgainstSliceTest, compareStringPCharPtrLen) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.compareString("42", 2), sharedSlice.compareString("42", 2));
    ASSERT_EQ_EX(slice.compareString("foo", 3), sharedSlice.compareString("foo", 3));
    ASSERT_EQ_EX(slice.compareString("bar", 3), sharedSlice.compareString("bar", 3));
  });
}

TEST(SharedSliceAgainstSliceTest, compareStringUncheckedPStringRef) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isString()) {
      ASSERT_EQ_EX(slice.compareStringUnchecked(StringRef("42")),
                   sharedSlice.compareStringUnchecked(StringRef("42")));
      ASSERT_EQ_EX(slice.compareStringUnchecked(StringRef("foo")),
                   sharedSlice.compareStringUnchecked(StringRef("foo")));
      ASSERT_EQ_EX(slice.compareStringUnchecked(StringRef("bar")),
                   sharedSlice.compareStringUnchecked(StringRef("bar")));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, compareStringUncheckedPString) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isString()) {
      using namespace std::string_literals;
      ASSERT_EQ_EX(slice.compareStringUnchecked("42"s),
                   sharedSlice.compareStringUnchecked("42"s));
      ASSERT_EQ_EX(slice.compareStringUnchecked("foo"s),
                   sharedSlice.compareStringUnchecked("foo"s));
      ASSERT_EQ_EX(slice.compareStringUnchecked("bar"s),
                   sharedSlice.compareStringUnchecked("bar"s));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, compareStringUncheckedPCharPtrLen) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isString()) {
      ASSERT_EQ_EX(slice.compareStringUnchecked("42", 2),
                   sharedSlice.compareStringUnchecked("42", 2));
      ASSERT_EQ_EX(slice.compareStringUnchecked("foo", 3),
                   sharedSlice.compareStringUnchecked("foo", 3));
      ASSERT_EQ_EX(slice.compareStringUnchecked("bar", 3),
                   sharedSlice.compareStringUnchecked("bar", 3));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, isEqualStringPStringRef) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.isEqualString(StringRef("42")), sharedSlice.isEqualString(StringRef("42")));
    ASSERT_EQ_EX(slice.isEqualString(StringRef("foo")), sharedSlice.isEqualString(StringRef("foo")));
    ASSERT_EQ_EX(slice.isEqualString(StringRef("bar")), sharedSlice.isEqualString(StringRef("bar")));
  });
}

TEST(SharedSliceAgainstSliceTest, isEqualStringPString) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    using namespace std::string_literals;
    ASSERT_EQ_EX(slice.isEqualString("42"s), sharedSlice.isEqualString("42"s));
    ASSERT_EQ_EX(slice.isEqualString("foo"s), sharedSlice.isEqualString("foo"s));
    ASSERT_EQ_EX(slice.isEqualString("bar"s), sharedSlice.isEqualString("bar"s));
  });
}

TEST(SharedSliceAgainstSliceTest, isEqualStringUncheckedPStringRef) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isString()) {
      ASSERT_EQ_EX(slice.isEqualStringUnchecked(StringRef("42")),
                   sharedSlice.isEqualStringUnchecked(StringRef("42")));
      ASSERT_EQ_EX(slice.isEqualStringUnchecked(StringRef("foo")),
                   sharedSlice.isEqualStringUnchecked(StringRef("foo")));
      ASSERT_EQ_EX(slice.isEqualStringUnchecked(StringRef("bar")),
                   sharedSlice.isEqualStringUnchecked(StringRef("bar")));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, isEqualStringUncheckedPString) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isString()) {
      using namespace std::string_literals;
      ASSERT_EQ_EX(slice.isEqualStringUnchecked("42"s),
                   sharedSlice.isEqualStringUnchecked("42"s));
      ASSERT_EQ_EX(slice.isEqualStringUnchecked("foo"s),
                   sharedSlice.isEqualStringUnchecked("foo"s));
      ASSERT_EQ_EX(slice.isEqualStringUnchecked("bar"s),
                   sharedSlice.isEqualStringUnchecked("bar"s));
    }
  });
}

TEST(SharedSliceAgainstSliceTest, binaryEquals) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.binaryEquals(slice), sharedSlice.binaryEquals(slice));
    ASSERT_EQ_EX(slice.binaryEquals(slice), sharedSlice.binaryEquals(sharedSlice));
    ASSERT_EQ_EX(slice.binaryEquals(sharedSlice.slice()), sharedSlice.binaryEquals(slice));
    ASSERT_EQ_EX(slice.binaryEquals(sharedSlice.slice()), sharedSlice.binaryEquals(sharedSlice));
  });
}

TEST(SharedSliceAgainstSliceTest, toHex) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.toHex(), sharedSlice.toHex());
  });
}

TEST(SharedSliceAgainstSliceTest, toJson) {
  auto const pretty = [](){
    auto opts = Options{};
    opts.prettyPrint = true;
    return opts;
  }();
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.toJson(), sharedSlice.toJson());
    ASSERT_EQ_EX(slice.toJson(&pretty), sharedSlice.toJson(&pretty));
  });
}

TEST(SharedSliceAgainstSliceTest, toString) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.toString(), sharedSlice.toString());
    // TODO add a check with an options argument, that actually results in a
    // different return value for some input test case.
  });
}

TEST(SharedSliceAgainstSliceTest, hexType) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    ASSERT_EQ_EX(slice.hexType(), sharedSlice.hexType());
  });
}

TEST(SharedSliceAgainstSliceTest, getIntUnchecked) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isInteger()) {
      ASSERT_EQ_EX(slice.getIntUnchecked(), sharedSlice.getIntUnchecked());
    }
  });
}

TEST(SharedSliceAgainstSliceTest, getUIntUnchecked) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isUInt()) {
      ASSERT_EQ_EX(slice.getUIntUnchecked(), sharedSlice.getUIntUnchecked());
    }
  });
}

TEST(SharedSliceAgainstSliceTest, getSmallIntUnchecked) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    if (slice.isInteger()) {
      ASSERT_EQ_EX(slice.getSmallIntUnchecked(), sharedSlice.getSmallIntUnchecked());
    }
  });
}

TEST(SharedSliceAgainstSliceTest, getBCD) {
  forAllTestCases([&](Slice slice, SharedSlice sharedSlice) {
    int8_t leftSign;
    int8_t rightSign;
    int32_t leftExponent;
    int32_t rightExponent;
    ValueLength leftMantissaLength;
    ValueLength rightMantissaLength;
    ASSERT_EQ_EX(slice.getBCD(leftSign, leftExponent, leftMantissaLength), sharedSlice.getBCD(rightSign, rightExponent, rightMantissaLength).get());
    // The arguments are only set when getBCD is successful
    if (slice.isBCD()) {
      ASSERT_EQ(leftSign, rightSign);
      ASSERT_EQ(leftExponent, rightExponent);
      ASSERT_EQ(leftMantissaLength, rightMantissaLength);
    }
  });
}

TEST(SharedSliceRefcountTest, copyConstructor) {
  forAllTestCases([&](SharedSlice&& sharedSliceRef) {
    // We assume to be the only owner of the referenced buffer
    ASSERT_EQ(1, sharedSliceRef.buffer().use_count());

    // Execute copy constructor
    SharedSlice sharedSlice{sharedSliceRef};

    // Use count for both should be two
    ASSERT_GE(2, sharedSliceRef.buffer().use_count());
    ASSERT_GE(2, sharedSlice.buffer().use_count());

    // Both should share ownership
    ASSERT_TRUE(haveSameOwnership(sharedSliceRef, sharedSlice));

    // Both should share the same buffer
    ASSERT_EQ(sharedSliceRef.buffer(), sharedSlice.buffer());
  });
}

TEST(SharedSliceRefcountTest, copyAssignment) {
  SharedSlice sharedSlice;
  forAllTestCases([&](SharedSlice&& sharedSliceRef) {
    // We assume to be the only owner of the referenced buffer
    ASSERT_EQ(1, sharedSliceRef.buffer().use_count());

    // Execute copy assignment
    sharedSlice = sharedSliceRef;

    // Use count for both should be two
    ASSERT_GE(2, sharedSliceRef.buffer().use_count());
    ASSERT_GE(2, sharedSlice.buffer().use_count());

    // Both should share ownership
    ASSERT_TRUE(haveSameOwnership(sharedSliceRef, sharedSlice));

    // Both should share the same buffer
    ASSERT_EQ(sharedSliceRef.buffer(), sharedSlice.buffer());
  });
}

TEST(SharedSliceRefcountTest, aliasingCopyConstructor) {
  forAllTestCases([&](SharedSlice&& sharedSliceRef) {
    // We assume to be the only owner of the referenced buffer
    ASSERT_EQ(1, sharedSliceRef.buffer().use_count());

    Builder b;
    b.add(Value(-7));

    auto const slice = b.slice();

    // Execute aliasing copy constructor (nonsensical here - usually, slice
    // should point into the same memory block)
    SharedSlice sharedSlice{sharedSliceRef, slice};

    // Use count for both should be two
    ASSERT_GE(2, sharedSliceRef.buffer().use_count());
    ASSERT_GE(2, sharedSlice.buffer().use_count());

    // Both should share ownership
    ASSERT_TRUE(haveSameOwnership(sharedSliceRef, sharedSlice));

    // The aliased copy should point to different memory than the originating
    // shared slice...
    ASSERT_NE(sharedSliceRef.buffer(), sharedSlice.buffer());
    // ... but to the same memory the originating (raw) slice did, instead.
    ASSERT_EQ(slice.start(), sharedSlice.start().get());
  });
}

TEST(SharedSliceRefcountTest, moveConstructor) {
  forAllTestCases([&](SharedSlice&& sharedSliceRef) {
    // We assume to be the only owner of the referenced buffer
    ASSERT_EQ(1, sharedSliceRef.buffer().use_count());
    auto const origPointer = sharedSliceRef.buffer().get();

    // Execute move constructor
    SharedSlice sharedSlice{std::move(sharedSliceRef)};

    // The passed slice should now point to a valid None slice
    ASSERT_LE(1, sharedSliceRef.buffer().use_count()); // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
    ASSERT_TRUE(sharedSliceRef.isNone());
    // The underlying buffers should be different
    ASSERT_NE(sharedSliceRef.buffer(), sharedSlice.buffer());

    // The slices should not share ownership
    ASSERT_FALSE(haveSameOwnership(sharedSliceRef, sharedSlice));

    // The local sharedSlice should be the only owner of its buffer
    ASSERT_EQ(1, sharedSlice.buffer().use_count());

    // sharedSlice should point to the same buffer as the sharedSliceRef did
    // originally
    ASSERT_EQ(origPointer, sharedSlice.buffer().get());
  });
}

TEST(SharedSliceRefcountTest, aliasingMoveConstructor) {
  forAllTestCases([&](SharedSlice&& sharedSliceRef) {
    // We assume to be the only owner of the referenced buffer
    ASSERT_EQ(1, sharedSliceRef.buffer().use_count());
    auto const origPointer = sharedSliceRef.buffer().get();

    Builder b;
    b.add(Value(-7));

    auto const slice = b.slice();

    // Execute aliasing move constructor (nonsensical here - usually, slice
    // should point into the same memory block)
    SharedSlice sharedSlice{std::move(sharedSliceRef), slice};

    // The passed slice should now point to a valid None slice
    ASSERT_LE(1, sharedSliceRef.buffer().use_count()); // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
    ASSERT_TRUE(sharedSliceRef.isNone());
    // The underlying buffers should be different
    ASSERT_NE(sharedSliceRef.buffer(), sharedSlice.buffer());

    // The slices should not share ownership
    ASSERT_FALSE(haveSameOwnership(sharedSliceRef, sharedSlice));

    // The local sharedSlice should be the only owner of its buffer
    ASSERT_EQ(1, sharedSlice.buffer().use_count());

    // The aliased copy should point to different memory than the originating
    // shared slice...
    ASSERT_NE(origPointer, sharedSlice.buffer().get());
    // ... but to the same memory the originating (raw) slice did, instead.
    ASSERT_EQ(slice.start(), sharedSlice.start().get());
  });
}

TEST(SharedSliceRefcountTest, moveAssignment) {
  SharedSlice sharedSlice;
  forAllTestCases([&](SharedSlice&& sharedSliceRef) {
    // We assume to be the only owner of the referenced buffer
    ASSERT_EQ(1, sharedSliceRef.buffer().use_count());
    auto const origPointer = sharedSliceRef.buffer().get();

    // Execute move assignment
    sharedSlice = std::move(sharedSliceRef);

    // The passed slice should now point to a valid None slice
    ASSERT_LE(1, sharedSliceRef.buffer().use_count()); // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
    ASSERT_TRUE(sharedSliceRef.isNone());
    // The underlying buffers should be different
    ASSERT_NE(sharedSliceRef.buffer(), sharedSlice.buffer());

    // The slices should not share ownership
    ASSERT_FALSE(haveSameOwnership(sharedSliceRef, sharedSlice));

    // The local sharedSlice should be the only owner of its buffer
    ASSERT_EQ(1, sharedSlice.buffer().use_count());

    // sharedSlice should point to the same buffer as the sharedSliceRef did
    // originally
    ASSERT_EQ(origPointer, sharedSlice.buffer().get());
  });
}

TEST(SharedSliceRefcountTest, destructor) {
  forAllTestCases([&](SharedSlice&& sharedSliceRef) {
    std::weak_ptr<uint8_t const> weakPtr;
    {
      SharedSlice sharedSlice{std::move(sharedSliceRef)};
      // We assume to be the only owner of the referenced buffer
      ASSERT_EQ(1, sharedSlice.buffer().use_count());
      weakPtr = decltype(weakPtr)(sharedSlice.buffer());
      ASSERT_EQ(1, weakPtr.use_count());
    }
    ASSERT_EQ(0, weakPtr.use_count());
  });
}

TEST(SharedSliceRefcountTest, valueStart) {
  forAllTestCases([&](SharedSlice sharedSlice) {
    ASSERT_EQ(1, sharedSlice.buffer().use_count());
    auto result = sharedSlice.valueStart();
    ASSERT_EQ(2, sharedSlice.buffer().use_count());
    ASSERT_EQ(2, result.use_count());
    ASSERT_TRUE(haveSameOwnership(sharedSlice.buffer(), result));
  });
}

TEST(SharedSliceRefcountTest, start) {
  forAllTestCases([&](SharedSlice sharedSlice) {
    ASSERT_EQ(1, sharedSlice.buffer().use_count());
    auto result = sharedSlice.start();
    ASSERT_EQ(2, sharedSlice.buffer().use_count());
    ASSERT_EQ(2, result.use_count());
    ASSERT_TRUE(haveSameOwnership(sharedSlice.buffer(), result));
  });
}

TEST(SharedSliceRefcountTest, startAs) {
  forAllTestCases([&](SharedSlice sharedSlice) {
    ASSERT_EQ(1, sharedSlice.buffer().use_count());
    auto result = sharedSlice.startAs<void*>();
    ASSERT_EQ(2, sharedSlice.buffer().use_count());
    ASSERT_EQ(2, result.use_count());
    ASSERT_TRUE(haveSameOwnership(sharedSlice.buffer(), result));
  });
}

TEST(SharedSliceRefcountTest, begin) {
  forAllTestCases([&](SharedSlice sharedSlice) {
    ASSERT_EQ(1, sharedSlice.buffer().use_count());
    auto result = sharedSlice.begin();
    ASSERT_EQ(2, sharedSlice.buffer().use_count());
    ASSERT_EQ(2, result.use_count());
    ASSERT_TRUE(haveSameOwnership(sharedSlice.buffer(), result));
  });
}

TEST(SharedSliceRefcountTest, end) {
  forAllTestCases([&](SharedSlice sharedSlice) {
    ASSERT_EQ(1, sharedSlice.buffer().use_count());
    auto result = sharedSlice.end();
    ASSERT_EQ(2, sharedSlice.buffer().use_count());
    ASSERT_EQ(2, result.use_count());
    ASSERT_TRUE(haveSameOwnership(sharedSlice.buffer(), result));
  });
}

TEST(SharedSliceRefcountTest, getExternal) {
  forAllTestCases([&](SharedSlice sharedSlice) {
    if (sharedSlice.isExternal()) {
      ASSERT_EQ(1, sharedSlice.buffer().use_count());
      auto result = sharedSlice.getExternal();
      ASSERT_EQ(2, sharedSlice.buffer().use_count());
      ASSERT_EQ(2, result.use_count());
      ASSERT_TRUE(haveSameOwnership(sharedSlice.buffer(), result));
    }
  });
}

TEST(SharedSliceRefcountTest, getString) {
  forAllTestCases([&](SharedSlice sharedSlice) {
    if (sharedSlice.isString()) {
      ASSERT_EQ(1, sharedSlice.buffer().use_count());
      ValueLength length;
      auto result = sharedSlice.getString(length);
      ASSERT_EQ(2, sharedSlice.buffer().use_count());
      ASSERT_EQ(2, result.use_count());
      ASSERT_TRUE(haveSameOwnership(sharedSlice.buffer(), result));
    }
  });
}

TEST(SharedSliceRefcountTest, getStringUnchecked) {
  forAllTestCases([&](SharedSlice sharedSlice) {
    if (sharedSlice.isString()) {
      ASSERT_EQ(1, sharedSlice.buffer().use_count());
      ValueLength length;
      auto result = sharedSlice.getStringUnchecked(length);
      ASSERT_EQ(2, sharedSlice.buffer().use_count());
      ASSERT_EQ(2, result.use_count());
      ASSERT_TRUE(haveSameOwnership(sharedSlice.buffer(), result));
    }
  });
}

TEST(SharedSliceRefcountTest, getBinary) {
  forAllTestCases([&](SharedSlice sharedSlice) {
    if (sharedSlice.isBinary()) {
      ASSERT_EQ(1, sharedSlice.buffer().use_count());
      ValueLength length;
      auto result = sharedSlice.getBinary(length);
      ASSERT_EQ(2, sharedSlice.buffer().use_count());
      ASSERT_EQ(2, result.use_count());
      ASSERT_TRUE(haveSameOwnership(sharedSlice.buffer(), result));
    }
  });
}

TEST(SharedSliceRefcountTest, getBCD) {
  forAllTestCases([&](SharedSlice sharedSlice) {
    if (sharedSlice.isBCD()) {
      ASSERT_EQ(1, sharedSlice.buffer().use_count());
      int8_t sign;
      int32_t exponent;
      ValueLength mantissaLength;
      auto result = sharedSlice.getBCD(sign, exponent, mantissaLength);
      ASSERT_EQ(2, sharedSlice.buffer().use_count());
      ASSERT_EQ(2, result.use_count());
      ASSERT_TRUE(haveSameOwnership(sharedSlice.buffer(), result));
    }
  });
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
