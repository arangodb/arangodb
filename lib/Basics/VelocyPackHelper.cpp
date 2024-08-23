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

#include "VelocyPackHelper.h"

#include "Basics/Exceptions.h"
#include "Basics/NumberUtils.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Basics/memory.h"
#include "Basics/operating-system.h"
#include "Basics/system-compiler.h"
#include "Logger/LogMacros.h"

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <string_view>

#include <velocypack/AttributeTranslator.h>
#include <velocypack/Collection.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-common.h>

namespace {

constexpr std::string_view idRef("id");
constexpr std::string_view cidRef("cid");

static std::unique_ptr<VPackAttributeTranslator> translator;
static std::unique_ptr<VPackCustomTypeHandler> customTypeHandler;

// statically computed table of type weights
// the weight for type MinKey must be lowest, the weight for type MaxKey must be
// highest the table contains a special value -50 to indicate that the value is
// an external which must be resolved further the type Custom has the same
// weight as the String type, because the Custom type is used to store _id
// (which is a string)
static int8_t const typeWeights[256] = {
    0 /* 0x00 */,   5 /* 0x01 */,  5 /* 0x02 */, 5 /* 0x03 */,  5 /* 0x04 */,
    5 /* 0x05 */,   5 /* 0x06 */,  5 /* 0x07 */, 5 /* 0x08 */,  5 /* 0x09 */,
    6 /* 0x0a */,   6 /* 0x0b */,  6 /* 0x0c */, 6 /* 0x0d */,  6 /* 0x0e */,
    6 /* 0x0f */,   6 /* 0x10 */,  6 /* 0x11 */, 6 /* 0x12 */,  5 /* 0x13 */,
    6 /* 0x14 */,   0 /* 0x15 */,  0 /* 0x16 */, -1 /* 0x17 */, 0 /* 0x18 */,
    1 /* 0x19 */,   1 /* 0x1a */,  2 /* 0x1b */, 3 /* 0x1c */,  -50 /* 0x1d */,
    -99 /* 0x1e */, 99 /* 0x1f */, 2 /* 0x20 */, 2 /* 0x21 */,  2 /* 0x22 */,
    2 /* 0x23 */,   2 /* 0x24 */,  2 /* 0x25 */, 2 /* 0x26 */,  2 /* 0x27 */,
    2 /* 0x28 */,   2 /* 0x29 */,  2 /* 0x2a */, 2 /* 0x2b */,  2 /* 0x2c */,
    2 /* 0x2d */,   2 /* 0x2e */,  2 /* 0x2f */, 2 /* 0x30 */,  2 /* 0x31 */,
    2 /* 0x32 */,   2 /* 0x33 */,  2 /* 0x34 */, 2 /* 0x35 */,  2 /* 0x36 */,
    2 /* 0x37 */,   2 /* 0x38 */,  2 /* 0x39 */, 2 /* 0x3a */,  2 /* 0x3b */,
    2 /* 0x3c */,   2 /* 0x3d */,  2 /* 0x3e */, 2 /* 0x3f */,  4 /* 0x40 */,
    4 /* 0x41 */,   4 /* 0x42 */,  4 /* 0x43 */, 4 /* 0x44 */,  4 /* 0x45 */,
    4 /* 0x46 */,   4 /* 0x47 */,  4 /* 0x48 */, 4 /* 0x49 */,  4 /* 0x4a */,
    4 /* 0x4b */,   4 /* 0x4c */,  4 /* 0x4d */, 4 /* 0x4e */,  4 /* 0x4f */,
    4 /* 0x50 */,   4 /* 0x51 */,  4 /* 0x52 */, 4 /* 0x53 */,  4 /* 0x54 */,
    4 /* 0x55 */,   4 /* 0x56 */,  4 /* 0x57 */, 4 /* 0x58 */,  4 /* 0x59 */,
    4 /* 0x5a */,   4 /* 0x5b */,  4 /* 0x5c */, 4 /* 0x5d */,  4 /* 0x5e */,
    4 /* 0x5f */,   4 /* 0x60 */,  4 /* 0x61 */, 4 /* 0x62 */,  4 /* 0x63 */,
    4 /* 0x64 */,   4 /* 0x65 */,  4 /* 0x66 */, 4 /* 0x67 */,  4 /* 0x68 */,
    4 /* 0x69 */,   4 /* 0x6a */,  4 /* 0x6b */, 4 /* 0x6c */,  4 /* 0x6d */,
    4 /* 0x6e */,   4 /* 0x6f */,  4 /* 0x70 */, 4 /* 0x71 */,  4 /* 0x72 */,
    4 /* 0x73 */,   4 /* 0x74 */,  4 /* 0x75 */, 4 /* 0x76 */,  4 /* 0x77 */,
    4 /* 0x78 */,   4 /* 0x79 */,  4 /* 0x7a */, 4 /* 0x7b */,  4 /* 0x7c */,
    4 /* 0x7d */,   4 /* 0x7e */,  4 /* 0x7f */, 4 /* 0x80 */,  4 /* 0x81 */,
    4 /* 0x82 */,   4 /* 0x83 */,  4 /* 0x84 */, 4 /* 0x85 */,  4 /* 0x86 */,
    4 /* 0x87 */,   4 /* 0x88 */,  4 /* 0x89 */, 4 /* 0x8a */,  4 /* 0x8b */,
    4 /* 0x8c */,   4 /* 0x8d */,  4 /* 0x8e */, 4 /* 0x8f */,  4 /* 0x90 */,
    4 /* 0x91 */,   4 /* 0x92 */,  4 /* 0x93 */, 4 /* 0x94 */,  4 /* 0x95 */,
    4 /* 0x96 */,   4 /* 0x97 */,  4 /* 0x98 */, 4 /* 0x99 */,  4 /* 0x9a */,
    4 /* 0x9b */,   4 /* 0x9c */,  4 /* 0x9d */, 4 /* 0x9e */,  4 /* 0x9f */,
    4 /* 0xa0 */,   4 /* 0xa1 */,  4 /* 0xa2 */, 4 /* 0xa3 */,  4 /* 0xa4 */,
    4 /* 0xa5 */,   4 /* 0xa6 */,  4 /* 0xa7 */, 4 /* 0xa8 */,  4 /* 0xa9 */,
    4 /* 0xaa */,   4 /* 0xab */,  4 /* 0xac */, 4 /* 0xad */,  4 /* 0xae */,
    4 /* 0xaf */,   4 /* 0xb0 */,  4 /* 0xb1 */, 4 /* 0xb2 */,  4 /* 0xb3 */,
    4 /* 0xb4 */,   4 /* 0xb5 */,  4 /* 0xb6 */, 4 /* 0xb7 */,  4 /* 0xb8 */,
    4 /* 0xb9 */,   4 /* 0xba */,  4 /* 0xbb */, 4 /* 0xbc */,  4 /* 0xbd */,
    4 /* 0xbe */,   4 /* 0xbf */,  4 /* 0xc0 */, 4 /* 0xc1 */,  4 /* 0xc2 */,
    4 /* 0xc3 */,   4 /* 0xc4 */,  4 /* 0xc5 */, 4 /* 0xc6 */,  4 /* 0xc7 */,
    2 /* 0xc8 */,   2 /* 0xc9 */,  2 /* 0xca */, 2 /* 0xcb */,  2 /* 0xcc */,
    2 /* 0xcd */,   2 /* 0xce */,  2 /* 0xcf */, 2 /* 0xd0 */,  2 /* 0xd1 */,
    2 /* 0xd2 */,   2 /* 0xd3 */,  2 /* 0xd4 */, 2 /* 0xd5 */,  2 /* 0xd6 */,
    2 /* 0xd7 */,   0 /* 0xd8 */,  0 /* 0xd9 */, 0 /* 0xda */,  0 /* 0xdb */,
    0 /* 0xdc */,   0 /* 0xdd */,  0 /* 0xde */, 0 /* 0xdf */,  0 /* 0xe0 */,
    0 /* 0xe1 */,   0 /* 0xe2 */,  0 /* 0xe3 */, 0 /* 0xe4 */,  0 /* 0xe5 */,
    0 /* 0xe6 */,   0 /* 0xe7 */,  0 /* 0xe8 */, 0 /* 0xe9 */,  0 /* 0xea */,
    0 /* 0xeb */,   0 /* 0xec */,  0 /* 0xed */, 0 /* 0xee */,  0 /* 0xef */,
    4 /* 0xf0 */,   4 /* 0xf1 */,  4 /* 0xf2 */, 4 /* 0xf3 */,  4 /* 0xf4 */,
    4 /* 0xf5 */,   4 /* 0xf6 */,  4 /* 0xf7 */, 4 /* 0xf8 */,  4 /* 0xf9 */,
    4 /* 0xfa */,   4 /* 0xfb */,  4 /* 0xfc */, 4 /* 0xfd */,  4 /* 0xfe */,
    4 /* 0xff */,
};

}  // namespace

namespace arangodb::basics {

template<bool useUtf8, typename Comparator,
         VelocyPackHelper::SortingMethod sortingMethod>
int VelocyPackHelper::compareObjects(VPackSlice lhs, VPackSlice rhs,
                                     VPackOptions const* options) {
  // compare two velocypack objects
  std::set<std::string_view, Comparator> keys;
  VPackCollection::unorderedKeys(lhs, keys);
  VPackCollection::unorderedKeys(rhs, keys);
  for (auto const& key : keys) {
    VPackSlice lhsValue = lhs.get(key).resolveExternal();
    if (lhsValue.isNone()) {
      // not present => null
      lhsValue = VPackSlice::nullSlice();
    }
    VPackSlice rhsValue = rhs.get(key).resolveExternal();
    if (rhsValue.isNone()) {
      // not present => null
      rhsValue = VPackSlice::nullSlice();
    }

    int result = VelocyPackHelper::compareInternal<sortingMethod>(
        lhsValue, rhsValue, useUtf8, options, &lhs, &rhs);

    if (result != 0) {
      return result;
    }
  }

  return 0;
}

// a default custom type handler that prevents throwing exceptions when
// custom types are encountered during Slice.toJson() and family
struct DefaultCustomTypeHandler final : public VPackCustomTypeHandler {
  void dump(VPackSlice const&, VPackDumper* dumper,
            VPackSlice const&) override {
    LOG_TOPIC("723df", WARN, arangodb::Logger::FIXME)
        << "DefaultCustomTypeHandler called";
    dumper->appendString("hello from CustomTypeHandler");
  }
  std::string toString(VPackSlice const&, VPackOptions const*,
                       VPackSlice const&) override {
    LOG_TOPIC("a01a7", WARN, arangodb::Logger::FIXME)
        << "DefaultCustomTypeHandler called";
    return "hello from CustomTypeHandler";
  }
};

/*static*/ arangodb::velocypack::Options
    VelocyPackHelper::strictRequestValidationOptions;
/*static*/ arangodb::velocypack::Options
    VelocyPackHelper::looseRequestValidationOptions;

/// @brief static initializer for all VPack values
void VelocyPackHelper::initialize() {
  LOG_TOPIC("bbce8", TRACE, arangodb::Logger::FIXME) << "initializing vpack";

  // initialize attribute translator
  ::translator = std::make_unique<VPackAttributeTranslator>();

  // these attribute names will be translated into short integer values
  ::translator->add(StaticStrings::KeyString, KeyAttribute - AttributeBase);
  ::translator->add(StaticStrings::RevString, RevAttribute - AttributeBase);
  ::translator->add(StaticStrings::IdString, IdAttribute - AttributeBase);
  ::translator->add(StaticStrings::FromString, FromAttribute - AttributeBase);
  ::translator->add(StaticStrings::ToString, ToAttribute - AttributeBase);

  ::translator->seal();

  // set the attribute translator in the global options
  VPackOptions::Defaults.attributeTranslator = ::translator.get();
  VPackOptions::Defaults.unsupportedTypeBehavior =
      VPackOptions::ConvertUnsupportedType;

  ::customTypeHandler = std::make_unique<DefaultCustomTypeHandler>();

  VPackOptions::Defaults.customTypeHandler = ::customTypeHandler.get();

  // false here, but will be set when converting to JSON for HTTP xfer
  VPackOptions::Defaults.escapeUnicode = false;

  // allow dumping of Object attributes in "arbitrary" order (i.e. non-sorted
  // order)
  VPackOptions::Defaults.dumpAttributesInIndexOrder = false;

  // disallow tagged values and BCDs. they are not used in ArangoDB as of now
  VPackOptions::Defaults.disallowTags = true;
  VPackOptions::Defaults.disallowBCD = true;

  // allow at most 200 levels of nested arrays/objects
  // must be 200 + 1 because in velocypack the threshold value is exclusive,
  // not inclusive.
  VPackOptions::Defaults.nestingLimit = 200 + 1;

  // set up options for validating incoming end-user requests
  strictRequestValidationOptions = VPackOptions::Defaults;
  strictRequestValidationOptions.checkAttributeUniqueness = true;
  // note: this value may be overriden by configuration!
  strictRequestValidationOptions.validateUtf8Strings = true;
  strictRequestValidationOptions.disallowExternals = true;
  strictRequestValidationOptions.disallowCustom = true;
  strictRequestValidationOptions.disallowTags = true;
  strictRequestValidationOptions.disallowBCD = true;
  strictRequestValidationOptions.unsupportedTypeBehavior =
      VPackOptions::FailOnUnsupportedType;

  // set up options for validating requests, without UTF-8 validation
  looseRequestValidationOptions = VPackOptions::Defaults;
  looseRequestValidationOptions.checkAttributeUniqueness = true;
  looseRequestValidationOptions.validateUtf8Strings = false;
  looseRequestValidationOptions.disallowExternals = true;
  looseRequestValidationOptions.disallowCustom = true;
  looseRequestValidationOptions.disallowTags = true;
  looseRequestValidationOptions.disallowBCD = true;
  looseRequestValidationOptions.unsupportedTypeBehavior =
      VPackOptions::FailOnUnsupportedType;

  // run quick selfs test with the attribute translator
  TRI_ASSERT(
      VPackSlice(::translator->translate(StaticStrings::KeyString)).getUInt() ==
      KeyAttribute - AttributeBase);
  TRI_ASSERT(
      VPackSlice(::translator->translate(StaticStrings::RevString)).getUInt() ==
      RevAttribute - AttributeBase);
  TRI_ASSERT(
      VPackSlice(::translator->translate(StaticStrings::IdString)).getUInt() ==
      IdAttribute - AttributeBase);
  TRI_ASSERT(VPackSlice(::translator->translate(StaticStrings::FromString))
                 .getUInt() == FromAttribute - AttributeBase);
  TRI_ASSERT(
      VPackSlice(::translator->translate(StaticStrings::ToString)).getUInt() ==
      ToAttribute - AttributeBase);

  TRI_ASSERT(VPackSlice(::translator->translate(KeyAttribute - AttributeBase))
                 .copyString() == StaticStrings::KeyString);
  TRI_ASSERT(VPackSlice(::translator->translate(RevAttribute - AttributeBase))
                 .copyString() == StaticStrings::RevString);
  TRI_ASSERT(VPackSlice(::translator->translate(IdAttribute - AttributeBase))
                 .copyString() == StaticStrings::IdString);
  TRI_ASSERT(VPackSlice(::translator->translate(FromAttribute - AttributeBase))
                 .copyString() == StaticStrings::FromString);
  TRI_ASSERT(VPackSlice(::translator->translate(ToAttribute - AttributeBase))
                 .copyString() == StaticStrings::ToString);
}

/// @brief return the (global) attribute translator instance
arangodb::velocypack::AttributeTranslator* VelocyPackHelper::getTranslator() {
  return ::translator.get();
}

bool VelocyPackHelper::AttributeSorterUTF8StringView::operator()(
    std::string_view l, std::string_view r) const {
  // use UTF-8-based comparison of attribute names
  return TRI_compare_utf8(l.data(), l.size(), r.data(), r.size()) < 0;
}

bool VelocyPackHelper::AttributeSorterBinaryStringView::operator()(
    std::string_view l, std::string_view r) const noexcept {
  // use binary comparison of attribute names
  size_t cmpLength = (std::min)(l.size(), r.size());
  int res = memcmp(l.data(), r.data(), cmpLength);
  if (res < 0) {
    return true;
  }
  if (res == 0) {
    return l.size() < r.size();
  }
  return false;
}

size_t VelocyPackHelper::VPackHash::operator()(VPackSlice slice) const {
  return static_cast<size_t>(slice.normalizedHash());
}

size_t VelocyPackHelper::VPackStringHash::operator()(
    VPackSlice slice) const noexcept {
  return static_cast<size_t>(slice.hashString());
}

bool VelocyPackHelper::VPackEqual::operator()(VPackSlice lhs,
                                              VPackSlice rhs) const {
  return VelocyPackHelper::equal(lhs, rhs, false, _options);
}

static inline int8_t TypeWeight(VPackSlice& slice) {
again:
  int8_t w = ::typeWeights[slice.head()];
  if (ADB_UNLIKELY(w == -50)) {
    slice = slice.resolveExternal();
    goto again;
  }
  return w;
}

bool VelocyPackHelper::VPackStringEqual::operator()(
    VPackSlice lhs, VPackSlice rhs) const noexcept {
  auto const lh = lhs.head();
  auto const rh = rhs.head();

  if (lh != rh) {
    return false;
  }

  VPackValueLength size;
  if (lh == 0xbf) {
    // long UTF-8 String
    size = static_cast<VPackValueLength>(
        velocypack::readIntegerFixed<VPackValueLength, 8>(lhs.begin() + 1));
    if (size != static_cast<VPackValueLength>(
                    velocypack::readIntegerFixed<VPackValueLength, 8>(
                        rhs.begin() + 1))) {
      return false;
    }
    return (memcmp(lhs.start() + 1 + 8, rhs.start() + 1 + 8,
                   static_cast<size_t>(size)) == 0);
  }

  size = static_cast<VPackValueLength>(lh - 0x40);
  return (memcmp(lhs.start() + 1, rhs.start() + 1, static_cast<size_t>(size)) ==
          0);
}

int VelocyPackHelper::compareNumberValuesLegacy(VPackValueType lhsType,
                                                VPackSlice lhs,
                                                VPackSlice rhs) {
  // This function is only for legacy code. The problem is that it casts
  // all integer types to double, which can lose precision. See
  // `VelocyPackHelper::compareNumberValuesCorrectly` for a correct
  // implementation. This is only used for legacy vpack indexes.
  if (lhsType == rhs.type()) {
    // both types are equal
    if (lhsType == VPackValueType::Int || lhsType == VPackValueType::SmallInt) {
      // use exact comparisons. no need to cast to double
      int64_t l = lhs.getIntUnchecked();
      int64_t r = rhs.getIntUnchecked();
      if (l == r) {
        return 0;
      }
      return (l < r ? -1 : 1);
    }

    if (lhsType == VPackValueType::UInt) {
      // use exact comparisons. no need to cast to double
      uint64_t l = lhs.getUIntUnchecked();
      uint64_t r = rhs.getUIntUnchecked();
      if (l == r) {
        return 0;
      }
      return (l < r ? -1 : 1);
    }
    // fallthrough to double comparison
  }

  double l = lhs.getNumericValue<double>();
  double r = rhs.getNumericValue<double>();
  if (l == r) {
    return 0;
  }
  return (l < r ? -1 : 1);
}

namespace {

template<typename T>
int comp(T a, T b) {
  if (a == b) {
    return VelocyPackHelper::cmp_equal;
  }
  return a < b ? VelocyPackHelper::cmp_less : VelocyPackHelper::cmp_greater;
}

// We use the following constants below for case distinctions,
// we use static asserts to ensure that the `double` implementation
// is actually IEEE 754 with 64 bits, otherwise our comparison method
// will be faulty:

constexpr uint64_t uint64_2_63 = uint64_t{1} << 63;

static_assert(53 == std::numeric_limits<double>::digits);
static_assert(63 == std::numeric_limits<int64_t>::digits);

}  // namespace

// The following function deserves an explanation: We want to compare
// numerically. If i is negative, we are good, since all unsigned numbers
// are numerically non-negative. Otherwise, we know that i can be cast
// statically to uint64_t and we can compare there.
int VelocyPackHelper::compareInt64UInt64(int64_t i, uint64_t u) {
  if (i < 0) {
    return VelocyPackHelper::cmp_less;
  }
  return comp<uint64_t>(static_cast<uint64_t>(i), u);
}

// This function deserves an explanation: Not all possible values of
// uint64_t can be represented faithfully as double (IEEE 754 64bit).
// At the same time, there are lots of values of double which cannot
// be represented as uint64_t. Therefore, proper numerical comparison
// is somewhat of a challenge. We cannot just cast one to the other
// and then compare. First we need to handle NaN separately. Then, we
// need to carefully cast u to double and keep track what we lost due to
// the limited precision of double. Then we can evaluate the result.
// This method has been evaluated using godbolt. Only change if you
// know what you are doing!
int VelocyPackHelper::compareUInt64Double(uint64_t u, double d) {
  if (std::isnan(d)) [[unlikely]] {
    return VelocyPackHelper::cmp_less;
  }
  // We essentially want to cast to double, but we want to explicitly
  // round downwards if necessary and also keep the low bits which we
  // lose due to limited precision of doubles. Therefore we determine
  // the number of leading zero bits and from this the number of low bits
  // we will lose on the conversion. Including the topmost one bit, a
  // IEEE 754 double can store 53 bits of precision. Therefore, we can
  // accomodate clz+53 bits in the double (which could be all 64 bits!).
  // We can then mask the low bits out and do a static cast:
  //  u    = 0 ... 0 1 ? ... ? 1 0 ... 0
  //         \ clz / \  <= 53  / \rbits/
  //  mask = 0 ...         ... 0 1 ... 1
  auto clz = std::countl_zero(u);
  auto rbits = 64 - std::min(64, clz + 53);
  auto const mask = (std::uint64_t{1} << rbits) - 1;
  auto ud = static_cast<double>(u & ~mask);
  // Now ud is u cast to double with rounding down. If ud and d are not
  // equal as doubles, then the comparison result is correct. If not, we
  // need to distinguish if we lost bits above (in which case u was actually
  // larger than d numerically), or not (in which case they represent the
  // same integral value):
  if (ud == d) {
    return (mask & u) != 0 ? VelocyPackHelper::cmp_greater
                           : VelocyPackHelper::cmp_equal;
  }
  return ud < d ? VelocyPackHelper::cmp_less : VelocyPackHelper::cmp_greater;
}

// The following function deserves an explanation. We want to compare
// numerically. We want to delegate to the above comparison function
// for unsigned integers by negating both arguments if the integer is
// negative and then flipping the result. The only difficulty we are
// facing is for signed integers here, because we cannot just take the
// negative of an int64_t because of the twos-complement implementation
// and the one negative value which does not have a positive counterpart.
// We also have to handle NaN separately.
int VelocyPackHelper::compareInt64Double(int64_t i, double d) {
  if (std::isnan(d)) [[unlikely]] {
    return VelocyPackHelper::cmp_less;
  }
  if (i < 0) {
    uint64_t u =
        (i == std::numeric_limits<int64_t>::min() ? uint64_2_63
                                                  : static_cast<uint64_t>(-i));
    return -compareUInt64Double(u, -d);
  }
  return compareUInt64Double(static_cast<uint64_t>(i), d);
}

int VelocyPackHelper::compareNumberValuesCorrectly(VPackValueType lhsType,

                                                   VPackSlice lhs,
                                                   VPackSlice rhs) {
  VPackValueType rhsType = rhs.type();
  if (lhsType == rhsType) {
    // both types are equal
    if (lhsType == VPackValueType::Int || lhsType == VPackValueType::SmallInt) {
      // use exact comparisons. no need to cast to double
      int64_t l = lhs.getIntUnchecked();
      int64_t r = rhs.getIntUnchecked();
      return comp<int64_t>(l, r);
    }

    if (lhsType == VPackValueType::UInt) {
      // use exact comparisons. no need to cast to double
      uint64_t l = lhs.getUIntUnchecked();
      uint64_t r = rhs.getUIntUnchecked();
      return comp<uint64_t>(l, r);
    }

    if (lhsType == VPackValueType::Double) {
      double l = lhs.getDouble();
      double r = rhs.getDouble();
      if (std::isnan(l)) [[unlikely]] {
        if (std::isnan(r)) [[unlikely]] {
          return VelocyPackHelper::cmp_equal;
        }
        return VelocyPackHelper::cmp_greater;
      } else if (std::isnan(r)) [[unlikely]] {
        return VelocyPackHelper::cmp_less;
      }
      // No NaN on either side!
      return comp<double>(l, r);
    }
  }

  // Formally, we now have to face 12 different cases, since each side
  // can be one of SmallInt, Int, UInt, double but the two are
  // different types. Let's reduce this to only 3 cases by reducing
  // SmallInt and Int to i64, UInt to u64 and by reordering
  // to have only I ~ U, I ~ D and U ~ D:

  union Number {
    int64_t i;
    uint64_t u;
    double d;
  };

  Number l;
  Number r;
  enum Type {
    kSignedIntegral = 0,
    kUnsignedIntegral = 1,
    kDouble = 2,
    kNumTypes = 3
  };
  Type lhst;
  switch (lhsType) {
    case VPackValueType::SmallInt:
    case VPackValueType::Int:
      l.i = lhs.getIntUnchecked();
      lhst = Type::kSignedIntegral;
      break;
    case VPackValueType::UInt:
      l.u = lhs.getUIntUnchecked();
      lhst = Type::kUnsignedIntegral;
      break;
    case VPackValueType::Double:
      l.d = lhs.getNumericValue<double>();
      lhst = Type::kDouble;
      break;
    default:    // does not happen, just to please the compiler!
      l.u = 0;  // treat anything else as 0
      lhst = Type::kUnsignedIntegral;
      break;
  }
  Type rhst;
  switch (rhsType) {
    case VPackValueType::SmallInt:
    case VPackValueType::Int:
      r.i = rhs.getIntUnchecked();
      rhst = Type::kSignedIntegral;
      break;
    case VPackValueType::UInt:
      r.u = rhs.getUIntUnchecked();
      rhst = Type::kUnsignedIntegral;
      break;
    case VPackValueType::Double:
      r.d = rhs.getNumericValue<double>();
      rhst = Type::kDouble;
      break;
    default:    // does not happen, just to please the compiler!
      r.u = 0;  // treat anything else as 0
      rhst = Type::kUnsignedIntegral;
      break;
  }

  switch (lhst + rhst * Type::kNumTypes) {
    case kUnsignedIntegral + kSignedIntegral* Type::kNumTypes:
      return -compareInt64UInt64(r.i, l.u);
    case kDouble + kSignedIntegral* Type::kNumTypes:
      return -compareInt64Double(r.i, l.d);
    case kSignedIntegral + kUnsignedIntegral* Type::kNumTypes:
      return compareInt64UInt64(l.i, r.u);
    case kDouble + kUnsignedIntegral* Type::kNumTypes:
      return -compareUInt64Double(r.u, l.d);
    case kSignedIntegral + kDouble* Type::kNumTypes:
      return compareInt64Double(l.i, r.d);
    case kUnsignedIntegral + kDouble* Type::kNumTypes:
      return compareUInt64Double(l.u, r.d);
    case kSignedIntegral + kSignedIntegral* Type::kNumTypes:
      // Note that since we have SmallInt and Int it is
      // indeed possible that both sides are signed integers, although
      // we have checked before that the types are not equal!
      return comp<int64_t>(l.i, r.i);
    default:  // does not happen!
      TRI_ASSERT(false);
      return 0;
  }
}

/// @brief compares two VelocyPack string values
int VelocyPackHelper::compareStringValues(char const* left, VPackValueLength nl,
                                          char const* right,
                                          VPackValueLength nr, bool useUTF8) {
  int res;
  if (useUTF8) {
    res = TRI_compare_utf8(left, static_cast<size_t>(nl), right,
                           static_cast<size_t>(nr));
  } else {
    size_t len = static_cast<size_t>(nl < nr ? nl : nr);
    res = memcmp(left, right, len);
  }

  if (res != 0) {
    return (res < 0 ? -1 : 1);
  }

  // res == 0
  if (nl == nr) {
    return 0;
  }
  // res == 0, but different string lengths
  return (nl < nr ? -1 : 1);
}

/// @brief returns a string sub-element, or throws if <name> does not exist
/// or it is not a string
std::string VelocyPackHelper::checkAndGetStringValue(VPackSlice slice,
                                                     std::string_view name) {
  TRI_ASSERT(slice.isObject());
  if (!slice.hasKey(name)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("attribute '", name, "' was not found"));
  }
  VPackSlice sub = slice.get(name);
  if (!sub.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("attribute '", name, "' is not a string"));
  }
  return sub.copyString();
}

void VelocyPackHelper::ensureStringValue(VPackSlice slice,
                                         std::string_view name) {
  TRI_ASSERT(slice.isObject());
  if (!slice.hasKey(name)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("attribute '", name, "' was not found"));
  }
  VPackSlice sub = slice.get(name);
  if (!sub.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("attribute '", name, "' is not a string"));
  }
}

/// @brief returns a string value, or the default value if it is not a
/// string
std::string VelocyPackHelper::getStringValue(VPackSlice slice,
                                             std::string const& defaultValue) {
  if (!slice.isString()) {
    return defaultValue;
  }
  return slice.copyString();
}

uint64_t VelocyPackHelper::stringUInt64(VPackSlice slice) {
  if (slice.isString()) {
    VPackValueLength length;
    char const* p = slice.getString(length);
    return arangodb::NumberUtils::atoi_zero<uint64_t>(p, p + length);
  }
  if (slice.isNumber()) {
    return slice.getNumericValue<uint64_t>();
  }
  return 0;
}

/// @brief parses a json file to VelocyPack
VPackBuilder VelocyPackHelper::velocyPackFromFile(std::string const& path) {
  size_t length;
  char* content = TRI_SlurpFile(path.c_str(), &length);
  if (content != nullptr) {
    auto guard = scopeGuard([&content]() noexcept { TRI_Free(content); });
    // The Parser might throw;
    VPackBuilder builder;
    VPackParser parser(builder);
    parser.parse(reinterpret_cast<uint8_t const*>(content), length);
    return builder;
  }
  THROW_ARANGO_EXCEPTION(TRI_errno());
}

static bool PrintVelocyPack(int fd, VPackSlice slice, bool appendNewline) {
  if (slice.isNone()) {
    return false;
  }

  std::string buffer;
  velocypack::StringSink sink(&buffer);
  try {
    velocypack::Dumper dumper(&sink);
    dumper.dump(slice);
  } catch (...) {
    // Writing failed
    return false;
  }

  if (buffer.empty()) {
    // should not happen
    return false;
  }

  if (appendNewline) {
    // add the newline here so we only need one write operation in the ideal
    // case
    buffer.push_back('\n');
  }

  char const* p = buffer.data();
  size_t n = buffer.size();

  while (0 < n) {
    auto m = TRI_WRITE(fd, p, static_cast<TRI_write_t>(n));

    if (m <= 0) {
      return false;
    }

    n -= m;
    p += m;
  }

  return true;
}

/// @brief writes a VelocyPack to a file
bool VelocyPackHelper::velocyPackToFile(std::string const& filename,
                                        VPackSlice slice, bool syncFile) {
  std::string const tmp = filename + ".tmp";

  // remove a potentially existing temporary file
  if (TRI_ExistsFile(tmp.c_str())) {
    TRI_UnlinkFile(tmp.c_str());
  }

  int fd = TRI_CREATE(tmp.c_str(),
                      O_CREAT | O_TRUNC | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

  if (fd < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_TOPIC("35198", WARN, arangodb::Logger::FIXME)
        << "cannot create json file '" << tmp << "': " << TRI_LAST_ERROR_STR;
    return false;
  }

  if (!PrintVelocyPack(fd, slice, true)) {
    TRI_CLOSE(fd);
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_TOPIC("549f4", WARN, arangodb::Logger::FIXME)
        << "cannot write to json file '" << tmp << "': " << TRI_LAST_ERROR_STR;
    TRI_UnlinkFile(tmp.c_str());
    return false;
  }

  if (syncFile) {
    LOG_TOPIC("0acab", TRACE, arangodb::Logger::FIXME)
        << "syncing tmp file '" << tmp << "'";

    if (!TRI_fsync(fd)) {
      TRI_CLOSE(fd);
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC("fd628", WARN, arangodb::Logger::FIXME)
          << "cannot sync saved json '" << tmp << "': " << TRI_LAST_ERROR_STR;
      TRI_UnlinkFile(tmp.c_str());
      return false;
    }
  }

  if (int res = TRI_CLOSE(fd); res < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_TOPIC("3f835", WARN, arangodb::Logger::FIXME)
        << "cannot close saved file '" << tmp << "': " << TRI_LAST_ERROR_STR;
    TRI_UnlinkFile(tmp.c_str());
    return false;
  }

  if (auto res = TRI_RenameFile(tmp.c_str(), filename.c_str());
      res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    LOG_TOPIC("7f5c9", WARN, arangodb::Logger::FIXME)
        << "cannot rename saved file '" << tmp << "' to '" << filename
        << "': " << TRI_LAST_ERROR_STR;
    TRI_UnlinkFile(tmp.c_str());

    return false;
  }

  if (syncFile) {
    // also sync target directory
    std::string const dir = TRI_Dirname(filename);
    fd = TRI_OPEN(dir.c_str(), O_RDONLY | TRI_O_CLOEXEC);
    if (fd < 0) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC("fd84e", WARN, arangodb::Logger::FIXME)
          << "cannot sync directory '" << filename
          << "': " << TRI_LAST_ERROR_STR;
    } else {
      if (fsync(fd) < 0) {
        TRI_set_errno(TRI_ERROR_SYS_ERROR);
        LOG_TOPIC("6b8f6", WARN, arangodb::Logger::FIXME)
            << "cannot sync directory '" << filename
            << "': " << TRI_LAST_ERROR_STR;
      }
      int res = TRI_CLOSE(fd);
      if (res < 0) {
        TRI_set_errno(TRI_ERROR_SYS_ERROR);
        LOG_TOPIC("7ceee", WARN, arangodb::Logger::FIXME)
            << "cannot close directory '" << filename
            << "': " << TRI_LAST_ERROR_STR;
      }
    }
  }

  return true;
}

template<VelocyPackHelper::SortingMethod sortingMethod>
int VelocyPackHelper::compareInternal(VPackSlice lhs, VPackSlice rhs,
                                      bool useUTF8, VPackOptions const* options,
                                      VPackSlice const* lhsBase,
                                      VPackSlice const* rhsBase) {
  {
    // will resolve externals and modify both lhs & rhs...
    int8_t lWeight = TypeWeight(lhs);
    int8_t rWeight = TypeWeight(rhs);

    if (lWeight != rWeight) {
      return (lWeight < rWeight ? -1 : 1);
    }

    TRI_ASSERT(lWeight == rWeight);
  }

  // lhs and rhs have equal weights

  // note that the following code would be redundant because it was already
  // checked that lhs & rhs have the same TypeWeight, which is 0 for none.
  //  and for TypeWeight 0 we always return value 0
  // if (lhs.isNone() || rhs.isNone()) {
  //   // if rhs is none. we cannot be sure here that both are nones.
  //   // there can also exist the situation that lhs is a none and rhs is a
  //   // null value
  //   // (or vice versa). Anyway, the compare value is the same for both,
  //   return 0;
  // }

  auto lhsType = lhs.type();

  switch (lhsType) {
    case VPackValueType::Null:
      return 0;
    case VPackValueType::Bool: {
      TRI_ASSERT(lhs.isBoolean());
      TRI_ASSERT(rhs.isBoolean());
      bool left = lhs.isTrue();
      if (left == rhs.isTrue()) {
        return 0;
      }
      if (!left) {
        TRI_ASSERT(rhs.isTrue());
        return -1;
      }
      TRI_ASSERT(rhs.isFalse());
      return 1;
    }
    case VPackValueType::Double:
    case VPackValueType::Int:
    case VPackValueType::UInt:
    case VPackValueType::SmallInt: {
      if (sortingMethod == SortingMethod::Correct) {
        return compareNumberValuesCorrectly(lhsType, lhs, rhs);
      } else {
        return compareNumberValuesLegacy(lhsType, lhs, rhs);
      }
    }
    case VPackValueType::UTCDate: {
      // We know that the other type also has to be UTCDate, since only
      // UTCDate has weight 3:
      TRI_ASSERT(rhs.type() == VPackValueType::UTCDate);
      int64_t l = lhs.getUTCDate();
      int64_t r = rhs.getUTCDate();
      return comp<int64_t>(l, r);
    }
    case VPackValueType::String:
    case VPackValueType::Custom: {
      VPackValueLength nl;
      VPackValueLength nr;
      char const* left;
      char const* right;
      std::string lhsString;
      std::string rhsString;

      if (lhs.isCustom()) {
        if (lhsBase == nullptr || options == nullptr ||
            options->customTypeHandler == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "Could not extract custom attribute.");
        }
        lhsString =
            options->customTypeHandler->toString(lhs, options, *lhsBase);
        left = lhsString.data();
        nl = lhsString.size();
      } else {
        left = lhs.getStringUnchecked(nl);
      }

      if (rhs.isCustom()) {
        if (rhsBase == nullptr || options == nullptr ||
            options->customTypeHandler == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "Could not extract custom attribute.");
        }
        rhsString =
            options->customTypeHandler->toString(rhs, options, *rhsBase);
        right = rhsString.data();
        nr = rhsString.size();
      } else {
        right = rhs.getStringUnchecked(nr);
      }

      TRI_ASSERT(left != nullptr);
      TRI_ASSERT(right != nullptr);

      return compareStringValues(left, nl, right, nr, useUTF8);
    }
    case VPackValueType::Array: {
      VPackArrayIterator al(lhs);
      VPackArrayIterator ar(rhs);

      VPackValueLength const n = (std::max)(al.size(), ar.size());
      for (VPackValueLength i = 0; i < n; ++i) {
        VPackSlice lhsValue;
        VPackSlice rhsValue;

        if (i < al.size()) {
          lhsValue = al.value();
          al.next();
        }
        if (i < ar.size()) {
          rhsValue = ar.value();
          ar.next();
        }

        int result = compareInternal<sortingMethod>(lhsValue, rhsValue, useUTF8,
                                                    options, &lhs, &rhs);
        if (result != 0) {
          return result;
        }
      }

      return 0;
    }
    case VPackValueType::Object: {
      if (useUTF8) {
        return compareObjects<true, AttributeSorterUTF8StringView,
                              sortingMethod>(lhs, rhs, options);
      }
      return compareObjects<false, AttributeSorterBinaryStringView,
                            sortingMethod>(lhs, rhs, options);
    }
    case VPackValueType::Illegal:
    case VPackValueType::MinKey:
    case VPackValueType::MaxKey:
    case VPackValueType::None:
      // uncommon cases are compared at the end
      return 0;
    default:
      // Contains all other ValueTypes of VelocyPack.
      // They are not used in ArangoDB so this cannot occur
      TRI_ASSERT(false);
      return 0;
  }
}

// Instantiate template functions explicitly:
template int
VelocyPackHelper::compareInternal<VelocyPackHelper::SortingMethod::Correct>(
    arangodb::velocypack::Slice lhs, arangodb::velocypack::Slice rhs,
    bool useUTF8, arangodb::velocypack::Options const* options,
    arangodb::velocypack::Slice const* lhsBase,
    arangodb::velocypack::Slice const* rhsBase);

template int
VelocyPackHelper::compareInternal<VelocyPackHelper::SortingMethod::Legacy>(
    arangodb::velocypack::Slice lhs, arangodb::velocypack::Slice rhs,
    bool useUTF8, arangodb::velocypack::Options const* options,
    arangodb::velocypack::Slice const* lhsBase,
    arangodb::velocypack::Slice const* rhsBase);

bool VelocyPackHelper::hasNonClientTypes(velocypack::Slice input) {
  if (input.isExternal()) {
    return true;
  } else if (input.isCustom()) {
    return true;
  } else if (input.isObject()) {
    auto it = VPackObjectIterator(input, true);
    while (it.valid()) {
      if (!it.key(/*translate*/ false).isString()) {
        return true;
      }
      if (hasNonClientTypes(it.value())) {
        return true;
      }
      it.next();
    }
  } else if (input.isArray()) {
    for (auto it : VPackArrayIterator(input)) {
      if (hasNonClientTypes(it)) {
        return true;
      }
    }
  }
  return false;
}

void VelocyPackHelper::sanitizeNonClientTypes(
    velocypack::Slice input, velocypack::Slice base,
    velocypack::Builder& output, velocypack::Options const& options,
    bool allowUnindexed) {
  if (input.isExternal()) {
    // recursively resolve externals
    sanitizeNonClientTypes(input.resolveExternal(), base, output, options,
                           allowUnindexed);
  } else if (input.isCustom()) {
    if (options.customTypeHandler == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "cannot sanitize vpack without custom type handler");
    }
    std::string custom =
        options.customTypeHandler->toString(input, &options, base);
    output.add(VPackValue(custom));
  } else if (input.isObject()) {
    output.openObject(allowUnindexed);
    for (auto it : VPackObjectIterator(input, true)) {
      output.add(VPackValue(it.key.stringView()));
      sanitizeNonClientTypes(it.value, input, output, options, allowUnindexed);
    }
    output.close();
  } else if (input.isArray()) {
    output.openArray(allowUnindexed);
    for (auto it : VPackArrayIterator(input)) {
      sanitizeNonClientTypes(it, input, output, options, allowUnindexed);
    }
    output.close();
  } else {
    output.add(input);
  }
}

/// @brief extract the collection id from VelocyPack
uint64_t VelocyPackHelper::extractIdValue(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return 0;
  }
  VPackSlice id = slice.get(::idRef);
  if (id.isNone()) {
    // pre-3.1 compatibility
    id = slice.get(::cidRef);
  }

  if (id.isString()) {
    // string cid, e.g. "9988488"
    VPackValueLength l;
    char const* p = id.getStringUnchecked(l);
    return NumberUtils::atoi_zero<uint64_t>(p, p + l);
  } else if (id.isNumber()) {
    // numeric cid, e.g. 9988488
    return id.getNumericValue<uint64_t>();
  } else if (!id.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid value for 'id' attribute");
  }

  TRI_ASSERT(id.isNone());
  return 0;
}

arangodb::LoggerStream& operator<<(arangodb::LoggerStream& logger,
                                   VPackSlice const& slice) {
  size_t const cutoff = 100;
  std::string sliceStr(slice.toJson());
  bool longer = sliceStr.size() > cutoff;
  if (longer) {
    logger << sliceStr.substr(cutoff) << "...";
  } else {
    logger << sliceStr;
  }
  return logger;
}

}  // namespace arangodb::basics
