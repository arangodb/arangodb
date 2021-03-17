#include "ZkdHelper.h"

#include <Basics/debugging.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <optional>

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
        throw std::invalid_argument{"Unexpected character "s + *p + " in byte string: " + str};
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
          throw std::invalid_argument{"Unexpected character "s + *p + " in byte string: " + str};
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

namespace {

uint64_t space_out_2(uint32_t v) {
    uint64_t x = v;
    x = (x | (x << 8)) & 0x00FF00FFul;
    x = (x | (x << 4)) & 0x0F0F0F0Ful;
    x = (x | (x << 2)) & 0x33333333ul;
    x = (x | (x << 1)) & 0x55555555ul;

    return x;
}

uint64_t space_out_3(uint16_t v) {
    uint64_t x = v;
    x = (x | (x << 16)) & 0x00FF0000FF0000FFul;
    x = (x | (x << 8)) & 0x000000F00F00F00Ful;
    x = (x | (x << 4)) & 0x30C30C30C30C30C3ul;
    x = (x | (x << 2)) & 0x0249249249249249ul;

    return x;
}

uint64_t space_out_4(uint16_t v) {
    uint64_t x = v;
    x = (x | (x << 24)) & 0x000000FF000000FFul;
    x = (x | (x << 12)) & 0x000F000F000F000Ful;
    x = (x | (x << 6)) & 0x0303030303030303ul;
    x = (x | (x << 3)) & 0x1111111111111111ul;

    return x;
}
}


zkd::byte_string zkd::operator"" _bss(const char* str, std::size_t len) {
  return byte_string{ reinterpret_cast<const std::byte*>(str), len};
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
    _value |= std::byte{1} << (7u - _nibble);
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

void zkd::BitWriter::reserve(std::size_t amount) {
  _buffer.reserve(amount);
}

zkd::RandomBitReader::RandomBitReader(byte_string_view ref) : ref(ref) {}

auto zkd::RandomBitReader::getBit(std::size_t index) -> Bit {
  auto byte = index / 8;
  auto nibble = index % 8;

  if (byte >= ref.size()) {
    return Bit::ZERO;
  }

  auto b = ref[byte] & (1_b << (7 - nibble));
  return b != 0_b ? Bit::ONE : Bit::ZERO;
}

zkd::RandomBitManipulator::RandomBitManipulator(byte_string& ref) : ref(ref) {}

auto zkd::RandomBitManipulator::getBit(std::size_t index) -> Bit {
  auto byte = index / 8;
  auto nibble = index % 8;

  if (byte >= ref.size()) {
    return Bit::ZERO;
  }

  auto b = ref[byte] & (1_b << (7 - nibble));
  return b != 0_b ? Bit::ONE : Bit::ZERO;
}

auto zkd::RandomBitManipulator::setBit(std::size_t index, Bit value) -> void {
  auto byte = index / 8;
  auto nibble = index % 8;

  if (byte >= ref.size()) {
    ref.resize(byte + 1);
  }
  auto bit = 1_b << (7 - nibble);
  ref[byte] = (ref[byte] & ~bit) | (value == Bit::ONE ? bit : 0_b);
}

auto zkd::interleave(std::vector<zkd::byte_string> const& vec) -> zkd::byte_string {
  std::size_t max_size = 0;
  std::vector<BitReader> reader;
  reader.reserve(vec.size());

  for (auto& str : vec) {
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

auto zkd::transpose(byte_string_view bs, std::size_t dimensions) -> std::vector<zkd::byte_string> {
  assert(dimensions > 0);
  BitReader reader(bs);
  std::vector<BitWriter> writer;
  writer.resize(dimensions);

  while (true) {
    for (auto& w : writer) {
      auto b = reader.next();
      if (!b.has_value()) {
        goto fuckoff_cxx;
      }
      w.append(b.value());
    }
  }
  fuckoff_cxx:

  std::vector<zkd::byte_string> result;
  std::transform(writer.begin(), writer.end(), std::back_inserter(result), [](auto& bs) {
    return std::move(bs).str();
  });
  return result;
}


constexpr std::size_t max_dimensions = 64;

auto zkd::compareWithBox(byte_string_view cur, byte_string_view min, byte_string_view max, std::size_t dimensions)
-> std::vector<CompareResult> {
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

void zkd::compareWithBoxInto(byte_string_view cur, byte_string_view min, byte_string_view max,
                    std::size_t dimensions, std::vector<CompareResult>& result) {

  std::size_t max_size = std::max(std::max(cur.size(), min.size()), max.size());

  BitReader cur_reader(cur);
  BitReader min_reader(min);
  BitReader max_reader(max);

  bool isLargerThanMin[max_dimensions] = {};
  bool isLowerThanMax[max_dimensions] = {};

  for (std::size_t i = 0; i < 8 * max_size; i++) {
    unsigned step = i / dimensions;
    unsigned dim = i % dimensions;

    auto cur_bit = cur_reader.next().value_or(Bit::ZERO);
    auto min_bit = min_reader.next().value_or(Bit::ZERO);
    auto max_bit = max_reader.next().value_or(Bit::ZERO);

    if (result[dim].flag != 0) {
      continue;
    }

    if (!isLargerThanMin[dim]) {
      if (cur_bit == Bit::ZERO && min_bit == Bit::ONE) {
        result[dim].outStep = step;
        result[dim].flag = -1;
      } else if (cur_bit == Bit::ONE && min_bit == Bit::ZERO) {
        isLargerThanMin[dim] = true;
        result[dim].saveMin = step;
      }
    }

    if (!isLowerThanMax[dim]) {
      if (cur_bit == Bit::ONE && max_bit == Bit::ZERO) {
        result[dim].outStep = step;
        result[dim].flag = 1;
      } else if (cur_bit == Bit::ZERO && max_bit == Bit::ONE) {
        isLowerThanMax[dim] = true;
        result[dim].saveMax = step;
      }
    }
  }

}

auto zkd::testInBox(byte_string_view cur, byte_string_view min, byte_string_view max, std::size_t dimensions)
-> bool {

  if (dimensions == 0 && dimensions <= max_dimensions) {
    auto msg = std::string{"dimensions argument to "};
    msg += __func__;
    msg += " must be greater than zero.";
    throw std::invalid_argument{msg};
  }

  std::size_t max_size = std::max(std::max(cur.size(), min.size()), max.size());

  BitReader cur_reader(cur);
  BitReader min_reader(min);
  BitReader max_reader(max);

  bool isLargerThanMin[max_dimensions] = {};
  bool isLowerThanMax[max_dimensions] = {};

  for (std::size_t i = 0; i < 8 * max_size; i++) {
    unsigned step = i / dimensions;
    unsigned dim = i % dimensions;

    auto cur_bit = cur_reader.next().value_or(Bit::ZERO);
    auto min_bit = min_reader.next().value_or(Bit::ZERO);
    auto max_bit = max_reader.next().value_or(Bit::ZERO);

    if (!isLargerThanMin[dim]) {
      if (cur_bit == Bit::ZERO && min_bit == Bit::ONE) {
        return false;
      } else if (cur_bit == Bit::ONE && min_bit == Bit::ZERO) {
        isLargerThanMin[dim] = true;
      }
    }

    if (!isLowerThanMax[dim]) {
      if (cur_bit == Bit::ONE && max_bit == Bit::ZERO) {
        return false;
      } else if (cur_bit == Bit::ZERO && max_bit == Bit::ONE) {
        isLowerThanMax[dim] = true;
      }
    }
  }

  return true;
}

auto zkd::getNextZValue(byte_string_view cur, byte_string_view min, byte_string_view max, std::vector<CompareResult>& cmpResult)
-> std::optional<byte_string> {

  auto result = byte_string{cur};

  auto const dims = cmpResult.size();

  auto minOutstepIter = std::min_element(cmpResult.begin(), cmpResult.end(), [&](auto const& a, auto const& b) {
    if (a.flag == 0) {
      return false;
    }
    if (b.flag == 0) {
      return true;
    }
    return a.outStep < b.outStep;
  });
  assert(minOutstepIter->flag != 0);
  auto const d = std::distance(cmpResult.begin(), minOutstepIter);

  RandomBitReader nisp(cur);

  std::size_t changeBP = dims * minOutstepIter->outStep + d;

  if (minOutstepIter->flag > 0) {
    while (changeBP != 0) {
      --changeBP;
      if (nisp.getBit(changeBP) == Bit::ZERO) {
        auto dim = changeBP % dims;
        auto step = changeBP / dims;
        if (cmpResult[dim].saveMax <= step) {
          cmpResult[dim].saveMin = step;
          cmpResult[dim].flag = 0;
          goto update_dims;
        }
      }
    }

    return std::nullopt;
  }

  update_dims:
  RandomBitManipulator rbm(result);
  assert(rbm.getBit(changeBP) == Bit::ZERO);
  rbm.setBit(changeBP, Bit::ONE);
  assert(rbm.getBit(changeBP) == Bit::ONE);

  auto min_trans = transpose(min, dims);
  auto next_v = transpose(result, dims);

  for (unsigned dim = 0; dim < dims; dim++) {
    auto& cmpRes = cmpResult[dim];
    if (cmpRes.flag >= 0) {
      auto bp = dims * cmpRes.saveMin + dim;
      if (changeBP >= bp) {
        // “set all bits of dim with bit positions > changeBP to 0”
        BitReader br(next_v[dim]);
        BitWriter bw;
        size_t i = 0;
        while (auto bit = br.next()) {
          if (i * dims + dim > changeBP) {
            break;
          }
          bw.append(bit.value());
          i++;
        }
        next_v[dim] = std::move(bw).str();
      } else {
        // “set all bits of dim with bit positions >  changeBP  to  the  minimum  of  the  query  box  in  this dim”
        BitReader br(next_v[dim]);
        BitWriter bw;
        size_t i = 0;
        while (auto bit = br.next()) {
          if (i * dims + dim > changeBP) {
            break;
          }
          bw.append(bit.value());
          i++;
        }
        BitReader br_min(min_trans[dim]);
        for (size_t j = 0; j < i; ++j) {
          br_min.next();
        }
        for (; auto bit = br_min.next(); ++i) {
          bw.append(bit.value());
        }
        next_v[dim] = std::move(bw).str();
      }
    } else {
      // load the minimum for that dimension
      next_v[dim] = min_trans[dim];
    }
  }

  return interleave(next_v);
}

template<typename T>
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
    base = - base;
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
    return {positive, fp_denorm_expo_biased, int_base };
  } else {
    //TRI_ASSERT(int_base & (uint64_t{1} << 53));
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

  double base = (double) int_base / double(uint64_t{1} << 53);

  if (!fp.positive) {
    base = -base;
  }
  return std::ldexp(base, exp);
}

std::ostream& zkd::operator<<(std::ostream& os, struct floating_point const& fp) {
  os << (fp.positive ? "p" : "n") << fp.exp << "E" << fp.base;
  return os;
}

template<>
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

template<>
auto zkd::to_byte_string_fixed_length<double>(double x) -> byte_string {
  BitWriter bw;
  zkd::into_bit_writer_fixed_length(bw, x);
  return std::move(bw).str();
}



template<>
auto zkd::from_bit_reader_fixed_length<double>(BitReader& r) -> double{
  bool isPositive = r.next_or_zero() == Bit::ONE;

  auto exp = r.read_big_endian_bits(11);
  auto base = r.read_big_endian_bits(52);
  if (!isPositive) {
    exp ^= (uint64_t{1} << 11) - 1;
    base ^= (uint64_t{1} << 52) - 1;
  }

  return construct_double({isPositive, exp, base});
}

template<typename T>
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

template<>
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
    ostream << std::hex << std::setfill('0') << std::setw(2) << std::to_integer<unsigned>(it);
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
