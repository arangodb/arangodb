#ifndef ZKD_TREE_LIBRARY_H
#define ZKD_TREE_LIBRARY_H

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace zkd {

inline static std::byte operator"" _b(unsigned long long b) {
  return std::byte{(unsigned char) b};
}
/*
struct byte_string : public std::basic_string<std::byte> {
  using std::basic_string<std::byte>::basic_string;
  using std::basic_string<std::byte>::operator=;

  //byte_string(std::basic_string<std::byte> str) : std::basic_string<std::byte>(std::move(str)) {}

  template<typename T>
  auto operator+(T&& other) -> byte_string {
    return byte_string(static_cast<std::basic_string<std::byte>&>(*this) + std::forward<T>(other));
  }

  auto as_string_view() const -> std::string_view {
    return std::string_view(reinterpret_cast<const char *>(data()), size());
  }
};*/
using byte_string = std::basic_string<std::byte>;
using byte_string_view = std::basic_string_view<std::byte>;

byte_string operator"" _bs(const char* str, std::size_t len);
byte_string operator"" _bss(const char* str, std::size_t len);

auto interleave(std::vector<byte_string> const& vec) -> byte_string;
auto transpose(byte_string_view bs, std::size_t dimensions) -> std::vector<byte_string>;

struct CompareResult {
  static constexpr auto max = std::numeric_limits<unsigned>::max();

  signed flag = 0;
  unsigned outStep = CompareResult::max;
  unsigned saveMin = CompareResult::max;
  unsigned saveMax = CompareResult::max;
};

std::ostream& operator<<(std::ostream& ostream, CompareResult const& string);

auto compareWithBox(byte_string_view cur, byte_string_view min, byte_string_view max, std::size_t dimensions)
-> std::vector<CompareResult>;
auto testInBox(byte_string_view cur, byte_string_view min, byte_string_view max, std::size_t dimensions)
-> bool;

auto getNextZValue(byte_string_view cur, byte_string_view min, byte_string_view max, std::vector<CompareResult>& cmpResult)
-> std::optional<byte_string>;

template<typename T>
auto to_byte_string_fixed_length(T) -> zkd::byte_string;
template<typename T>
auto from_byte_string_fixed_length(byte_string_view) -> T;
template<>
byte_string to_byte_string_fixed_length<double>(double x);

enum class Bit {
  ZERO = 0,
  ONE = 1
};

class BitReader {
 public:
  using iterator = typename byte_string_view::const_iterator;

  explicit BitReader(iterator begin, iterator end);
  explicit BitReader(byte_string const& str) : BitReader(byte_string_view{str}) {}
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

  auto getBit(unsigned index) -> Bit;

 private:
  byte_string_view ref;
};

struct RandomBitManipulator {
  explicit RandomBitManipulator(byte_string& ref);

  auto getBit(unsigned index) -> Bit;

  auto setBit(unsigned index, Bit value) -> void;

 private:
  byte_string& ref;
};


template<typename T>
void into_bit_writer_fixed_length(BitWriter&, T);

} // namespace zkd

std::ostream& operator<<(std::ostream& ostream, zkd::byte_string const& string);
std::ostream& operator<<(std::ostream& ostream, zkd::byte_string_view string);

#endif //ZKD_TREE_LIBRARY_H
