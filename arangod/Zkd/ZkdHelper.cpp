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
/// @author Tobias Gödderz
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include "ZkdHelper.h"

#include "Basics/ScopeGuard.h"
#include "Basics/debugging.h"
#include "Containers/SmallVector.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <optional>
#include <tuple>

using namespace arangodb;
using namespace arangodb::zkd;

zkd::byte_string zkd::operator"" _bs(const char* const str, std::size_t len) {
  using namespace std::string_literals;

  std::string normalizedInput{};
  normalizedInput.reserve(len);
  for (char const* p = str; *p != '\0'; ++p) {
    switch (*p) {
      case '0':
      case '1':
        normalizedInput += *p;
        break;
      case ' ':
      case '\'':
        // skip whitespace and single quotes
        break;
      default:
        throw std::invalid_argument{"Unexpected character "s + *p +
                                    " in byte string: " + str};
    }
  }

  if (normalizedInput.empty()) {
    throw std::invalid_argument{"Empty byte string"};
  }

  auto result = byte_string{};

  char const* p = normalizedInput.c_str();
  for (auto bitIdx = 0; *p != '\0'; bitIdx = 0) {
    result += std::byte{0};
    for (; *p != '\0' && bitIdx < 8; ++bitIdx) {
      switch (*p) {
        case '0':
          break;
        case '1': {
          auto const bitPos = 7 - bitIdx;
          result.back() |= (std::byte{1} << bitPos);
          break;
        }
        default:
          throw std::invalid_argument{"Unexpected character "s + *p +
                                      " in byte string: " + str};
      }

      ++p;
      // skip whitespace and single quotes
      while (*p == ' ' || *p == '\'') {
        ++p;
      }
    }
  }

  return result;
}

zkd::byte_string zkd::operator"" _bss(const char* str, std::size_t len) {
  return byte_string{reinterpret_cast<const std::byte*>(str), len};
}

zkd::BitReader::BitReader(zkd::BitReader::iterator begin, zkd::BitReader::iterator end)
    : _current(begin), _end(end) {}

auto zkd::BitReader::next() -> std::optional<zkd::Bit> {
  if (_nibble >= 8) {
    if (_current == _end) {
      return std::nullopt;
    }
    _value = *_current;
    _nibble = 0;
    ++_current;
  }

  auto flag = std::byte{1u} << (7u - _nibble);
  auto bit = (_value & flag) != std::byte{0} ? Bit::ONE : Bit::ZERO;
  _nibble += 1;
  return bit;
}

auto zkd::BitReader::read_big_endian_bits(unsigned bits) -> uint64_t {
  uint64_t result = 0;
  for (size_t i = 0; i < bits; i++) {
    uint64_t bit = uint64_t{1} << (bits - i - 1);
    if (next_or_zero() == Bit::ONE) {
      result |= bit;
    }
  }
  return result;
}

zkd::ByteReader::ByteReader(iterator begin, iterator end)
    : _current(begin), _end(end) {}

auto zkd::ByteReader::next() -> std::optional<std::byte> {
  if (_current == _end) {
    return std::nullopt;
  }
  return *(_current++);
}

void zkd::BitWriter::append(Bit bit) {
  if (bit == Bit::ONE) {
    _value |= std::byte{1} << (7U - _nibble);
  }
  _nibble += 1;
  if (_nibble == 8) {
    _buffer.push_back(_value);
    _value = std::byte{0};
    _nibble = 0;
  }
}

void zkd::BitWriter::write_big_endian_bits(uint64_t v, unsigned bits) {
  for (size_t i = 0; i < bits; i++) {
    auto b = (v & (uint64_t{1} << (bits - i - 1))) == 0 ? Bit::ZERO : Bit::ONE;
    append(b);
  }
}

auto zkd::BitWriter::str() && -> zkd::byte_string {
  if (_nibble > 0) {
    _buffer.push_back(_value);
  }
  _nibble = 0;
  _value = std::byte{0};
  return std::move(_buffer);
}

void zkd::BitWriter::reserve(std::size_t amount) { _buffer.reserve(amount); }

zkd::RandomBitReader::RandomBitReader(byte_string_view ref) : _ref(ref) {}

auto zkd::RandomBitReader::getBit(std::size_t index) const -> Bit {
  auto byte = index / 8;
  auto nibble = index % 8;

  if (byte >= _ref.size()) {
    return Bit::ZERO;
  }

  auto b = (_ref[byte] >> (7 - nibble)) & 1_b;
  return b != 0_b ? Bit::ONE : Bit::ZERO;
}

auto zkd::RandomBitReader::bits() const -> std::size_t {
  return 8 * _ref.size();
}

zkd::RandomBitManipulator::RandomBitManipulator(byte_string& ref) : _ref(ref) {}

auto zkd::RandomBitManipulator::getBit(std::size_t index) const -> Bit {
  auto byte = index / 8;
  auto nibble = index % 8;

  if (byte >= _ref.size()) {
    return Bit::ZERO;
  }

  auto b = _ref[byte] & (1_b << (7 - nibble));
  return b != 0_b ? Bit::ONE : Bit::ZERO;
}

auto zkd::RandomBitManipulator::setBit(std::size_t index, Bit value) -> void {
  auto byte = index / 8;
  auto nibble = index % 8;

  if (byte >= _ref.size()) {
    _ref.resize(byte + 1);
  }
  auto bit = 1_b << (7 - nibble);
  if (value == Bit::ONE) {
    _ref[byte] |= bit;
  } else {
    _ref[byte] &= ~bit;
  }
}

auto zkd::RandomBitManipulator::bits() const -> std::size_t {
  return 8 * _ref.size();
}

auto zkd::interleave(std::vector<zkd::byte_string> const& vec) -> zkd::byte_string {
  std::size_t max_size = 0;
  std::vector<BitReader> reader;
  reader.reserve(vec.size());

  for (auto const& str : vec) {
    if (str.size() > max_size) {
      max_size = str.size();
    }
    reader.emplace_back(str);
  }

  BitWriter bitWriter;
  bitWriter.reserve(vec.size() * max_size);

  for (size_t i = 0; i < 8 * max_size; i++) {
    for (auto& it : reader) {
      auto b = it.next();
      bitWriter.append(b.value_or(Bit::ZERO));
    }
  }

  return std::move(bitWriter).str();
}

auto zkd::transpose(byte_string_view bs, std::size_t dimensions)
    -> std::vector<zkd::byte_string> {
  assert(dimensions > 0);
  BitReader reader(bs);
  std::vector<BitWriter> writer;
  writer.resize(dimensions);

  while (true) {
    for (auto& w : writer) {
      auto b = reader.next();
      if (!b.has_value()) {
        goto break_loops;
      }
      w.append(b.value());
    }
  }
break_loops:

  std::vector<zkd::byte_string> result;
  std::transform(writer.begin(), writer.end(), std::back_inserter(result),
                 [](auto& bs) { return std::move(bs).str(); });
  return result;
}

auto zkd::compareWithBox(byte_string_view cur, byte_string_view min, byte_string_view max,
                         std::size_t dimensions) -> std::vector<CompareResult> {
  if (dimensions == 0) {
    auto msg = std::string{"dimensions argument to "};
    msg += __func__;
    msg += " must be greater than zero.";
    throw std::invalid_argument{msg};
  }
  std::vector<CompareResult> result;
  result.resize(dimensions);
  zkd::compareWithBoxInto(cur, min, max, dimensions, result);
  return result;
}

void zkd::compareWithBoxInto(byte_string_view cur, byte_string_view min,
                             byte_string_view max, std::size_t dimensions,
                             std::vector<CompareResult>& result) {
  TRI_ASSERT(result.size() == dimensions);
  std::fill(result.begin(), result.end(), CompareResult{});
  std::size_t max_size = std::max({cur.size(), min.size(), max.size()});

  BitReader cur_reader(cur);
  BitReader min_reader(min);
  BitReader max_reader(max);

  auto const isLargerThanMin = [&result](auto const dim) {
    return result[dim].saveMin != CompareResult::max;
  };
  auto const isLowerThanMax = [&result](auto const dim) {
    return result[dim].saveMax != CompareResult::max;
  };

  std::size_t step = 0;
  std::size_t dim = 0;

  for (std::size_t i = 0; i < 8 * max_size; i++) {
    TRI_ASSERT(step == i / dimensions);
    TRI_ASSERT(dim == i % dimensions);

    auto cur_bit = cur_reader.next().value_or(Bit::ZERO);
    auto min_bit = min_reader.next().value_or(Bit::ZERO);
    auto max_bit = max_reader.next().value_or(Bit::ZERO);

    if (result[dim].flag == 0) {
      if (!isLargerThanMin(dim)) {
        if (cur_bit == Bit::ZERO && min_bit == Bit::ONE) {
          result[dim].outStep = step;
          result[dim].flag = -1;
        } else if (cur_bit == Bit::ONE && min_bit == Bit::ZERO) {
          result[dim].saveMin = step;
        }
      }

      if (!isLowerThanMax(dim)) {
        if (cur_bit == Bit::ONE && max_bit == Bit::ZERO) {
          result[dim].outStep = step;
          result[dim].flag = 1;
        } else if (cur_bit == Bit::ZERO && max_bit == Bit::ONE) {
          result[dim].saveMax = step;
        }
      }
    }
    dim += 1;
    if (dim >= dimensions) {
      dim = 0;
      step += 1;
    }
  }
}

auto zkd::testInBox(byte_string_view cur, byte_string_view min,
                    byte_string_view max, std::size_t dimensions) -> bool {
  if (dimensions == 0) {
    auto msg = std::string{"dimensions argument to "};
    msg += __func__;
    msg += " must be greater than zero.";
    throw std::invalid_argument{msg};
  }

  std::size_t max_size = std::max({cur.size(), min.size(), max.size()});

  BitReader cur_reader(cur);
  BitReader min_reader(min);
  BitReader max_reader(max);

  ::arangodb::containers::SmallVector<std::pair<bool, bool>>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<std::pair<bool, bool>> isLargerLowerThanMinMax{a};
  isLargerLowerThanMinMax.resize(dimensions);

  unsigned dim = 0;
  unsigned finished_dims = 2 * dimensions;
  for (std::size_t i = 0; i < 8 * max_size; i++) {
    auto cur_bit = cur_reader.next().value_or(Bit::ZERO);
    auto min_bit = min_reader.next().value_or(Bit::ZERO);
    auto max_bit = max_reader.next().value_or(Bit::ZERO);

    if (!isLargerLowerThanMinMax[dim].first) {
      if (cur_bit == Bit::ZERO && min_bit == Bit::ONE) {
        return false;
      } else if (cur_bit == Bit::ONE && min_bit == Bit::ZERO) {
        isLargerLowerThanMinMax[dim].first = true;
        if (--finished_dims == 0) {
          break;
        }
      }
    }

    if (!isLargerLowerThanMinMax[dim].second) {
      if (cur_bit == Bit::ONE && max_bit == Bit::ZERO) {
        return false;
      } else if (cur_bit == Bit::ZERO && max_bit == Bit::ONE) {
        isLargerLowerThanMinMax[dim].second = true;
        if (--finished_dims == 0) {
          break;
        }
      }
    }

    dim += 1;
    if (dim >= dimensions) {
      dim = 0;
    }
  }

  return true;
}

auto zkd::getNextZValue(byte_string_view cur, byte_string_view min,
                        byte_string_view max, std::vector<CompareResult>& cmpResult)
    -> std::optional<byte_string> {
  auto result = byte_string{cur};

  auto const dims = cmpResult.size();

  auto minOutstepIter = std::min_element(cmpResult.begin(), cmpResult.end(),
                                         [&](auto const& a, auto const& b) {
                                           if (a.flag == 0) {
                                             return false;
                                           }
                                           if (b.flag == 0) {
                                             return true;
                                           }
                                           return a.outStep < b.outStep;
                                         });
  TRI_ASSERT(minOutstepIter->flag != 0);
  auto const d = std::distance(cmpResult.begin(), minOutstepIter);

  RandomBitReader nisp(cur);

  std::size_t changeBP = dims * minOutstepIter->outStep + d;

  if (minOutstepIter->flag > 0) {
    bool update_dims = false;
    while (changeBP != 0 && !update_dims) {
      --changeBP;
      if (nisp.getBit(changeBP) == Bit::ZERO) {
        auto dim = changeBP % dims;
        auto step = changeBP / dims;
        if (cmpResult[dim].saveMax <= step) {
          cmpResult[dim].saveMin = step;
          cmpResult[dim].flag = 0;
          update_dims = true;
        }
      }
    }

    if (!update_dims) {
      return std::nullopt;
    }
  }

  {
    RandomBitManipulator rbm(result);
    TRI_ASSERT(rbm.getBit(changeBP) == Bit::ZERO);
    rbm.setBit(changeBP, Bit::ONE);
    TRI_ASSERT(rbm.getBit(changeBP) == Bit::ONE);
  }
  auto resultManipulator = RandomBitManipulator{result};
  auto minReader = RandomBitReader{min};

  // Calculate the next bit position in dimension `dim` (regarding dims)
  // after `bitPos`
  auto const nextGreaterBitInDim = [dims](std::size_t const bitPos, std::size_t const dim) {
    auto const posRem = bitPos % dims;
    auto const posFloor = bitPos - posRem;
    auto const result = dim > posRem ? (posFloor + dim) : posFloor + dims + dim;
    // result must be a bit of dimension `dim`
    TRI_ASSERT(result % dims == dim);
    // result must be strictly greater than bitPos
    TRI_ASSERT(bitPos < result);
    // and the lowest bit with the above properties
    TRI_ASSERT(result <= bitPos + dims);
    return result;
  };

  for (std::size_t dim = 0; dim < dims; dim++) {
    auto& cmpRes = cmpResult[dim];
    if (cmpRes.flag >= 0) {
      auto bp = dims * cmpRes.saveMin + dim;
      if (changeBP >= bp) {
        // “set all bits of dim with bit positions > changeBP to 0”
        for (std::size_t i = nextGreaterBitInDim(changeBP, dim);
             i < resultManipulator.bits(); i += dims) {
          resultManipulator.setBit(i, Bit::ZERO);
        }
      } else {
        // “set all bits of dim with bit positions >  changeBP  to  the  minimum of  the  query  box  in  this dim”
        for (std::size_t i = nextGreaterBitInDim(changeBP, dim);
             i < resultManipulator.bits(); i += dims) {
          resultManipulator.setBit(i, minReader.getBit(i));
        }
      }
    } else {
      // load the minimum for that dimension
      for (std::size_t i = dim; i < resultManipulator.bits(); i += dims) {
        resultManipulator.setBit(i, minReader.getBit(i));
      }
    }
  }

  return result;
}

template <typename T>
auto zkd::to_byte_string_fixed_length(T v) -> zkd::byte_string {
  byte_string result;
  static_assert(std::is_integral_v<T>);
  if constexpr (std::is_unsigned_v<T>) {
    result.reserve(sizeof(T));
    for (size_t i = 0; i < sizeof(T); i++) {
      uint8_t b = 0xff & (v >> (56 - 8 * i));
      result.push_back(std::byte{b});
    }
  } else {
    // we have to add a <positive?> byte
    result.reserve(sizeof(T) + 1);
    if (v < 0) {
      result.push_back(0_b);
    } else {
      result.push_back(0xff_b);
    }
    for (size_t i = 0; i < sizeof(T); i++) {
      uint8_t b = 0xff & (v >> (56 - 8 * i));
      result.push_back(std::byte{b});
    }
  }
  return result;
}

template auto zkd::to_byte_string_fixed_length<uint64_t>(uint64_t) -> zkd::byte_string;
template auto zkd::to_byte_string_fixed_length<int64_t>(int64_t) -> zkd::byte_string;
template auto zkd::to_byte_string_fixed_length<uint32_t>(uint32_t) -> zkd::byte_string;
template auto zkd::to_byte_string_fixed_length<int32_t>(int32_t) -> zkd::byte_string;

inline constexpr auto fp_infinity_expo_biased = (1u << 11) - 1;
inline constexpr auto fp_denorm_expo_biased = 0;
inline constexpr auto fp_min_expo_biased = std::numeric_limits<double>::min_exponent - 1;

auto zkd::destruct_double(double x) -> floating_point {
  TRI_ASSERT(!std::isnan(x));

  bool positive = true;
  int exp = 0;
  double base = frexp(x, &exp);

  // handle negative values
  if (base < 0) {
    positive = false;
    base = -base;
  }

  if (std::isinf(base)) {
    // deal with +- infinity
    return {positive, fp_infinity_expo_biased, 0};
  }

  auto int_base = uint64_t((uint64_t{1} << 53) * base);

  if (exp < std::numeric_limits<double>::min_exponent) {
    // handle denormalized case
    auto divide_by = std::numeric_limits<double>::min_exponent - exp;

    exp = fp_denorm_expo_biased;

    int_base >>= divide_by;
    return {positive, fp_denorm_expo_biased, int_base};
  } else {
    // TRI_ASSERT(int_base & (uint64_t{1} << 53));
    uint64_t biased_exp = exp - fp_min_expo_biased;
    if (int_base == 0) {
      // handle zero case, assign smallest exponent
      biased_exp = 0;
    }

    return {positive, biased_exp, int_base};
  }
}

auto zkd::construct_double(floating_point const& fp) -> double {
  if (fp.exp == fp_infinity_expo_biased) {
    // first handle infinity
    auto base = std::numeric_limits<double>::infinity();
    return fp.positive ? base : -base;
  }

  uint64_t int_base = fp.base;

  int exp = int(fp.exp) + fp_min_expo_biased;

  if (fp.exp != fp_denorm_expo_biased) {
    int_base |= uint64_t{1} << 52;
  } else {
    exp = std::numeric_limits<double>::min_exponent;
  }

  double base = (double)int_base / double(uint64_t{1} << 53);

  if (!fp.positive) {
    base = -base;
  }
  return std::ldexp(base, exp);
}

std::ostream& zkd::operator<<(std::ostream& os, struct floating_point const& fp) {
  os << (fp.positive ? "p" : "n") << fp.exp << "E" << fp.base;
  return os;
}

template <>
void zkd::into_bit_writer_fixed_length<double>(BitWriter& bw, double x) {
  auto [p, exp, base] = destruct_double(x);

  bw.append(p ? Bit::ONE : Bit::ZERO);

  if (!p) {
    exp ^= (uint64_t{1} << 11) - 1;
    base ^= (uint64_t{1} << 52) - 1;
  }

  bw.write_big_endian_bits(exp, 11);
  bw.write_big_endian_bits(base, 52);
}

template <>
auto zkd::to_byte_string_fixed_length<double>(double x) -> byte_string {
  BitWriter bw;
  zkd::into_bit_writer_fixed_length(bw, x);
  return std::move(bw).str();
}

template <>
auto zkd::from_bit_reader_fixed_length<double>(BitReader& r) -> double {
  bool isPositive = r.next_or_zero() == Bit::ONE;

  auto exp = r.read_big_endian_bits(11);
  auto base = r.read_big_endian_bits(52);
  if (!isPositive) {
    exp ^= (uint64_t{1} << 11) - 1;
    base ^= (uint64_t{1} << 52) - 1;
  }

  return construct_double({isPositive, exp, base});
}

template <typename T>
auto zkd::from_byte_string_fixed_length(byte_string_view bs) -> T {
  T result = 0;
  static_assert(std::is_integral_v<T>);
  if constexpr (std::is_unsigned_v<T>) {
    for (size_t i = 0; i < sizeof(T); i++) {
      result |= std::to_integer<uint64_t>(bs[i]) << (56 - 8 * i);
    }
  } else {
    abort();
  }

  return result;
}

template auto zkd::from_byte_string_fixed_length<uint64_t>(byte_string_view) -> uint64_t;

template <>
auto zkd::from_byte_string_fixed_length<double>(byte_string_view bs) -> double {
  BitReader r(bs);
  return from_bit_reader_fixed_length<double>(r);
}

std::ostream& operator<<(std::ostream& ostream, zkd::byte_string const& string) {
  return ::operator<<(ostream, byte_string_view{string});
}

std::ostream& operator<<(std::ostream& ostream, byte_string_view string) {
  ostream << "[0x ";
  bool first = true;
  for (auto const& it : string) {
    if (!first) {
      ostream << " ";
    }
    first = false;
    ostream << std::hex << std::setfill('0') << std::setw(2)
            << std::to_integer<unsigned>(it);
  }
  ostream << "]";
  return ostream;
}

std::ostream& zkd::operator<<(std::ostream& ostream, zkd::CompareResult const& cr) {
  ostream << "CR{";
  ostream << "flag=" << cr.flag;
  ostream << ", saveMin=" << cr.saveMin;
  ostream << ", saveMax=" << cr.saveMax;
  ostream << ", outStep=" << cr.outStep;
  ostream << "}";
  return ostream;
}
