
#include "velocypack/velocypack-common.h"
#include "velocypack/AttributeTranslator.h"
#include "velocypack/Basics.h"
#include "velocypack/Buffer.h"
#include "velocypack/Builder.h"
#include "velocypack/Collection.h"
#include "velocypack/Compare.h"
#include "velocypack/Dumper.h"
#include "velocypack/Exception.h"
#include "velocypack/HashedStringRef.h"
#include "velocypack/HexDump.h"
#include "velocypack/Iterator.h"
#include "velocypack/Options.h"
#include "velocypack/Parser.h"
#include "velocypack/Sink.h"
#include "velocypack/Slice.h"
#include "velocypack/SliceContainer.h"
#include "velocypack/StringRef.h"
#include "velocypack/Validator.h"
#include "velocypack/Value.h"
#include "velocypack/ValueType.h"
#include "velocypack/Version.h"

#include "gtest/gtest.h"

using namespace arangodb::velocypack;

// helper for catching VPack-specific exceptions
#define ASSERT_VELOCYPACK_EXCEPTION(operation, code) \
  try {                                              \
    (operation);                                     \
    ASSERT_FALSE(true);                              \
  } catch (Exception const& ex) {                    \
    ASSERT_EQ(code, ex.errorCode());                 \
  } catch (...) {                                    \
    ASSERT_FALSE(true);                              \
  }

// don't complain if this function is not called
static void dumpDouble(double, uint8_t*) VELOCYPACK_UNUSED;

static void dumpDouble(double x, uint8_t* p) {
  uint64_t u;
  memcpy(&u, &x, sizeof(double));
  for (size_t i = 0; i < 8; i++) {
    p[i] = u & 0xff;
    u >>= 8;
  }
}

// don't complain if this function is not called
static void checkDump(Slice, std::string const&) VELOCYPACK_UNUSED;

static void checkDump(Slice s, std::string const& knownGood) {
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(s);
  ASSERT_EQ(knownGood, buffer);
}

// don't complain if this function is not called
static void checkBuild(Slice, ValueType, ValueLength) VELOCYPACK_UNUSED;

// With the following function we check type determination and size
// of the produced VPack value:
static void checkBuild(Slice s, ValueType t, ValueLength byteSize) {
  ASSERT_EQ(t, s.type());
  ASSERT_TRUE(s.isType(t));
  ValueType other =
      (t == ValueType::String) ? ValueType::Int : ValueType::String;
  ASSERT_FALSE(s.isType(other));
  ASSERT_FALSE(other == s.type());

  ASSERT_EQ(byteSize, s.byteSize());

  switch (t) {
    case ValueType::None:
      ASSERT_TRUE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::Illegal:
      ASSERT_FALSE(s.isNone());
      ASSERT_TRUE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::Null:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_TRUE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::Bool:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_TRUE(s.isBool());
      ASSERT_TRUE(s.isFalse() || s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::Double:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_TRUE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_TRUE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::Array:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_TRUE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::Object:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_TRUE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::External:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_TRUE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::UTCDate:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_TRUE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::Int:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_TRUE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_TRUE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::UInt:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_TRUE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_TRUE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::SmallInt:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_TRUE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_TRUE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::String:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_TRUE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::Binary:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_TRUE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::BCD:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_TRUE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::MinKey:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_TRUE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::MaxKey:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_TRUE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::Custom:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_TRUE(s.isCustom());
      ASSERT_FALSE(s.isTagged());
      break;
    case ValueType::Tagged:
      ASSERT_FALSE(s.isNone());
      ASSERT_FALSE(s.isIllegal());
      ASSERT_FALSE(s.isNull());
      ASSERT_FALSE(s.isBool());
      ASSERT_FALSE(s.isFalse());
      ASSERT_FALSE(s.isTrue());
      ASSERT_FALSE(s.isDouble());
      ASSERT_FALSE(s.isArray());
      ASSERT_FALSE(s.isObject());
      ASSERT_FALSE(s.isExternal());
      ASSERT_FALSE(s.isUTCDate());
      ASSERT_FALSE(s.isInt());
      ASSERT_FALSE(s.isUInt());
      ASSERT_FALSE(s.isSmallInt());
      ASSERT_FALSE(s.isString());
      ASSERT_FALSE(s.isBinary());
      ASSERT_FALSE(s.isNumber());
      ASSERT_FALSE(s.isBCD());
      ASSERT_FALSE(s.isMinKey());
      ASSERT_FALSE(s.isMaxKey());
      ASSERT_FALSE(s.isCustom());
      ASSERT_TRUE(s.isTagged());
      break;
  }
}

#if __cplusplus >= 201703L

template<typename T, typename U>
bool haveSameOwnership(std::shared_ptr<T> const& left, std::shared_ptr<U> const& right) {
  static thread_local auto owner_less = std::owner_less<void>{};
  return !owner_less(left, right) && !owner_less(right, left);
}

inline bool haveSameOwnership(SharedSlice const& leftSlice, SharedSlice const& rightSlice) {
  auto const& left = leftSlice.buffer();
  auto const& right = rightSlice.buffer();
  return haveSameOwnership(left, right);
}

#endif
