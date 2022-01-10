////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#pragma once
#include <cstddef>
#include <iosfwd>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace arangodb::zkd {

inline static std::byte operator"" _b(unsigned long long b) {
  return std::byte{(unsigned char)b};
}

using byte_string = std::basic_string<std::byte>;
using byte_string_view = std::basic_string_view<std::byte>;

byte_string operator"" _bs(const char* str, std::size_t len);
byte_string operator"" _bss(const char* str, std::size_t len);

auto interleave(std::vector<byte_string> const& vec) -> byte_string;
auto transpose(byte_string_view bs, std::size_t dimensions)
    -> std::vector<byte_string>;

struct alignas(32) CompareResult {
  static constexpr auto max = std::numeric_limits<std::size_t>::max();

  signed flag = 0;
  std::size_t outStep = CompareResult::max;
  std::size_t saveMin = CompareResult::max;
  std::size_t saveMax = CompareResult::max;
};

std::ostream& operator<<(std::ostream& ostream, CompareResult const& string);

auto compareWithBox(byte_string_view cur, byte_string_view min,
                    byte_string_view max, std::size_t dimensions)
    -> std::vector<CompareResult>;
void compareWithBoxInto(byte_string_view cur, byte_string_view min,
                        byte_string_view max, std::size_t dimensions,
                        std::vector<CompareResult>& result);
auto testInBox(byte_string_view cur, byte_string_view min, byte_string_view max,
               std::size_t dimensions) -> bool;

auto getNextZValue(byte_string_view cur, byte_string_view min,
                   byte_string_view max, std::vector<CompareResult>& cmpResult)
    -> std::optional<byte_string>;

template<typename T>
auto to_byte_string_fixed_length(T) -> zkd::byte_string;
template<typename T>
auto from_byte_string_fixed_length(byte_string_view) -> T;
template<>
byte_string to_byte_string_fixed_length<double>(double x);

enum class Bit { ZERO = 0, ONE = 1 };

class BitReader {
 public:
  using iterator = typename byte_string_view::const_iterator;

  explicit BitReader(iterator begin, iterator end);
  explicit BitReader(byte_string const& str)
      : BitReader(byte_string_view{str}) {}
  explicit BitReader(byte_string_view v) : BitReader(v.cbegin(), v.cend()) {}

  auto next() -> std::optional<Bit>;
  auto next_or_zero() -> Bit { return next().value_or(Bit::ZERO); }

  auto read_big_endian_bits(unsigned bits) -> uint64_t;

 private:
  iterator _current;
  iterator _end;
  std::byte _value{};
  std::size_t _nibble = 8;
};

class ByteReader {
 public:
  using iterator = typename byte_string::const_iterator;

  explicit ByteReader(iterator begin, iterator end);

  auto next() -> std::optional<std::byte>;

 private:
  iterator _current;
  iterator _end;
};

class BitWriter {
 public:
  void append(Bit bit);
  void write_big_endian_bits(uint64_t, unsigned bits);

  auto str() && -> byte_string;

  void reserve(std::size_t amount);

 private:
  std::size_t _nibble = 0;
  std::byte _value = std::byte{0};
  byte_string _buffer;
};

struct RandomBitReader {
  explicit RandomBitReader(byte_string_view ref);

  [[nodiscard]] auto getBit(std::size_t index) const -> Bit;

  [[nodiscard]] auto bits() const -> std::size_t;

 private:
  byte_string_view _ref;
};

struct RandomBitManipulator {
  explicit RandomBitManipulator(byte_string& ref);

  [[nodiscard]] auto getBit(std::size_t index) const -> Bit;

  auto setBit(std::size_t index, Bit value) -> void;

  [[nodiscard]] auto bits() const -> std::size_t;

 private:
  byte_string& _ref;
};

template<typename T>
void into_bit_writer_fixed_length(BitWriter&, T);
template<typename T>
auto from_bit_reader_fixed_length(BitReader&) -> T;

struct floating_point {
  bool positive : 1;
  uint64_t exp : 11;
  uint64_t base : 52;
};

auto destruct_double(double x) -> floating_point;

auto construct_double(floating_point const& fp) -> double;

std::ostream& operator<<(std::ostream& os, struct floating_point const& fp);

}  // namespace arangodb::zkd

std::ostream& operator<<(std::ostream& ostream,
                         arangodb::zkd::byte_string const& string);
std::ostream& operator<<(std::ostream& ostream,
                         arangodb::zkd::byte_string_view string);
