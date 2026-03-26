////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "minhash_token_stream.hpp"

#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

#include "analysis/analyzers.hpp"
#include "analysis/token_streams.hpp"
#include "utils/log.hpp"
#include "utils/vpack_utils.hpp"

#include <absl/strings/internal/escaping.h>

namespace {

// CityHash64 implementation from Abseil library (city.cc).
//
// The code is copied to avoid potential problems related to Abseil update.
// That is important because produced tokens are stored in the index as terms.

// Some primes between 2^63 and 2^64 for various uses.
static constexpr uint64_t k0 = 0xc3a5c85c97cb3127ULL;
static constexpr uint64_t k1 = 0xb492b66fbe98f273ULL;
static constexpr uint64_t k2 = 0x9ae16a3b2f90404fULL;

#ifdef ABSL_IS_BIG_ENDIAN
#define uint32_in_expected_order(x) (absl::gbswap_32(x))
#define uint64_in_expected_order(x) (absl::gbswap_64(x))
#else
#define uint32_in_expected_order(x) (x)
#define uint64_in_expected_order(x) (x)
#endif

static uint64_t Fetch64(const char* p) {
  return uint64_in_expected_order(ABSL_INTERNAL_UNALIGNED_LOAD64(p));
}

static uint32_t Fetch32(const char* p) {
  return uint32_in_expected_order(ABSL_INTERNAL_UNALIGNED_LOAD32(p));
}

static uint64_t ShiftMix(uint64_t val) { return val ^ (val >> 47); }

// Bitwise right rotate.  Normally this will compile to a single
// instruction, especially if the shift is a manifest constant.
static uint64_t Rotate(uint64_t val, int shift) {
  // Avoid shifting by 64: doing so yields an undefined result.
  return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
}

static uint64_t HashLen16(uint64_t u, uint64_t v, uint64_t mul) {
  // Murmur-inspired hashing.
  uint64_t a = (u ^ v) * mul;
  a ^= (a >> 47);
  uint64_t b = (v ^ a) * mul;
  b ^= (b >> 47);
  b *= mul;
  return b;
}

static uint64_t HashLen16(uint64_t u, uint64_t v) {
  const uint64_t kMul = 0x9ddfea08eb382d69ULL;
  return HashLen16(u, v, kMul);
}

static uint64_t HashLen0to16(const char* s, size_t len) {
  if (len >= 8) {
    uint64_t mul = k2 + len * 2;
    uint64_t a = Fetch64(s) + k2;
    uint64_t b = Fetch64(s + len - 8);
    uint64_t c = Rotate(b, 37) * mul + a;
    uint64_t d = (Rotate(a, 25) + b) * mul;
    return HashLen16(c, d, mul);
  }
  if (len >= 4) {
    uint64_t mul = k2 + len * 2;
    uint64_t a = Fetch32(s);
    return HashLen16(len + (a << 3), Fetch32(s + len - 4), mul);
  }
  if (len > 0) {
    uint8_t a = s[0];
    uint8_t b = s[len >> 1];
    uint8_t c = s[len - 1];
    uint32_t y = static_cast<uint32_t>(a) + (static_cast<uint32_t>(b) << 8);
    uint32_t z = static_cast<uint32_t>(len) + (static_cast<uint32_t>(c) << 2);
    return ShiftMix(y * k2 ^ z * k0) * k2;
  }
  return k2;
}

// This probably works well for 16-byte strings as well, but it may be overkill
// in that case.
static uint64_t HashLen17to32(const char* s, size_t len) {
  uint64_t mul = k2 + len * 2;
  uint64_t a = Fetch64(s) * k1;
  uint64_t b = Fetch64(s + 8);
  uint64_t c = Fetch64(s + len - 8) * mul;
  uint64_t d = Fetch64(s + len - 16) * k2;
  return HashLen16(Rotate(a + b, 43) + Rotate(c, 30) + d,
                   a + Rotate(b + k2, 18) + c, mul);
}

// Return a 16-byte hash for 48 bytes.  Quick and dirty.
// Callers do best to use "random-looking" values for a and b.
static std::pair<uint64_t, uint64_t> WeakHashLen32WithSeeds(
  uint64_t w, uint64_t x, uint64_t y, uint64_t z, uint64_t a, uint64_t b) {
  a += w;
  b = Rotate(b + a + z, 21);
  uint64_t c = a;
  a += x;
  a += y;
  b += Rotate(a, 44);
  return std::make_pair(a + z, b + c);
}

// Return a 16-byte hash for s[0] ... s[31], a, and b.  Quick and dirty.
static std::pair<uint64_t, uint64_t> WeakHashLen32WithSeeds(const char* s,
                                                            uint64_t a,
                                                            uint64_t b) {
  return WeakHashLen32WithSeeds(Fetch64(s), Fetch64(s + 8), Fetch64(s + 16),
                                Fetch64(s + 24), a, b);
}

// Return an 8-byte hash for 33 to 64 bytes.
static uint64_t HashLen33to64(const char* s, size_t len) {
  uint64_t mul = k2 + len * 2;
  uint64_t a = Fetch64(s) * k2;
  uint64_t b = Fetch64(s + 8);
  uint64_t c = Fetch64(s + len - 24);
  uint64_t d = Fetch64(s + len - 32);
  uint64_t e = Fetch64(s + 16) * k2;
  uint64_t f = Fetch64(s + 24) * 9;
  uint64_t g = Fetch64(s + len - 8);
  uint64_t h = Fetch64(s + len - 16) * mul;
  uint64_t u = Rotate(a + g, 43) + (Rotate(b, 30) + c) * 9;
  uint64_t v = ((a + g) ^ d) + f + 1;
  uint64_t w = absl::gbswap_64((u + v) * mul) + h;
  uint64_t x = Rotate(e + f, 42) + c;
  uint64_t y = (absl::gbswap_64((v + w) * mul) + g) * mul;
  uint64_t z = e + f + c;
  a = absl::gbswap_64((x + z) * mul + y) + b;
  b = ShiftMix((z + a) * mul + d + h) * mul;
  return b + x;
}

uint64_t CityHash64(const char* s, size_t len) {
  if (len <= 32) {
    if (len <= 16) {
      return HashLen0to16(s, len);
    } else {
      return HashLen17to32(s, len);
    }
  } else if (len <= 64) {
    return HashLen33to64(s, len);
  }

  // For strings over 64 bytes we hash the end first, and then as we
  // loop we keep 56 bytes of state: v, w, x, y, and z.
  uint64_t x = Fetch64(s + len - 40);
  uint64_t y = Fetch64(s + len - 16) + Fetch64(s + len - 56);
  uint64_t z = HashLen16(Fetch64(s + len - 48) + len, Fetch64(s + len - 24));
  std::pair<uint64_t, uint64_t> v =
    WeakHashLen32WithSeeds(s + len - 64, len, z);
  std::pair<uint64_t, uint64_t> w =
    WeakHashLen32WithSeeds(s + len - 32, y + k1, x);
  x = x * k1 + Fetch64(s);

  // Decrease len to the nearest multiple of 64, and operate on 64-byte chunks.
  len = (len - 1) & ~static_cast<size_t>(63);
  do {
    x = Rotate(x + y + v.first + Fetch64(s + 8), 37) * k1;
    y = Rotate(y + v.second + Fetch64(s + 48), 42) * k1;
    x ^= w.second;
    y += v.first + Fetch64(s + 40);
    z = Rotate(z + w.first, 33) * k1;
    v = WeakHashLen32WithSeeds(s, v.second * k1, x + w.first);
    w = WeakHashLen32WithSeeds(s + 32, z + w.second, y + Fetch64(s + 16));
    std::swap(z, x);
    s += 64;
    len -= 64;
  } while (len != 0);
  return HashLen16(HashLen16(v.first, w.first) + ShiftMix(y) * k1 + z,
                   HashLen16(v.second, w.second) + x);
}

}  // namespace
namespace irs::analysis {
namespace {

using namespace arangodb;

constexpr std::string_view kParseError =
  ", failed to parse options for MinHashTokenStream";
constexpr offset kEmptyOffset;

constexpr uint32_t kMinHashes = 1;
constexpr std::string_view kNumHashes = "numHashes";

bool ParseNumHashes(velocypack::Slice input, uint32_t& num_hashes) {
  IRS_ASSERT(input.isObject());
  input = input.get(kNumHashes);
  if (!input.isNumber<uint32_t>()) {
    IRS_LOG_ERROR(absl::StrCat(
      kNumHashes, " attribute must be positive integer", kParseError));
    return false;
  }
  num_hashes = input.getNumber<uint32_t>();
  if (num_hashes < kMinHashes) {
    IRS_LOG_ERROR(absl::StrCat(kNumHashes, " attribute must be at least ",
                               kMinHashes, kParseError));
    return false;
  }
  return true;
}

bool ParseOptions(velocypack::Slice slice,
                  MinHashTokenStream::Options& options) {
  if (!slice.isObject()) {
    return false;
  }
  if (!ParseNumHashes(slice, options.num_hashes)) {
    return false;
  }
  if (!analyzers::MakeAnalyzer(slice, options.analyzer)) {
    IRS_LOG_ERROR(absl::StrCat("Invalid analyzer definition in ",
                               slice_to_string(slice), kParseError));
    return false;
  }
  return true;
}

std::shared_ptr<velocypack::Builder> ParseArgs(std::string_view args) try {
  return velocypack::Parser::fromJson(args.data(), args.size());
} catch (const std::exception& e) {
  IRS_LOG_ERROR(absl::StrCat("Caught exception: ", e.what(), kParseError));
  return {};
} catch (...) {
  IRS_LOG_ERROR(absl::StrCat("Caught unknown exception", kParseError));
  return {};
}

analyzer::ptr MakeImpl(velocypack::Slice slice) {
  if (MinHashTokenStream::Options opts; ParseOptions(slice, opts)) {
    return std::make_unique<MinHashTokenStream>(std::move(opts));
  }
  return {};
}

bool NormalizeImpl(velocypack::Slice input, velocypack::Builder& output) {
  if (!input.isObject()) {
    return false;
  }
  uint32_t num_hashes = 0;
  if (!ParseNumHashes(input, num_hashes)) {
    return false;
  }
  velocypack::ObjectBuilder scope{&output};
  output.add(kNumHashes, velocypack::Value{num_hashes});
  if (!analyzers::NormalizeAnalyzer(input, output)) {
    IRS_LOG_ERROR(absl::StrCat("Invalid analyzer definition in ",
                               slice_to_string(input), kParseError));
    return false;
  }
  return true;
}

analyzer::ptr MakeVPack(std::string_view args) {
  if (args.empty()) {
    IRS_LOG_ERROR(absl::StrCat("Empty arguments", kParseError));
    return {};
  }
  velocypack::Slice slice{reinterpret_cast<const uint8_t*>(args.data())};
  return MakeImpl(slice);
}

analyzer::ptr MakeJson(std::string_view args) {
  if (args.empty()) {
    IRS_LOG_ERROR(absl::StrCat("Empty arguments", kParseError));
    return {};
  }
  auto builder = ParseArgs(args);
  if (!builder) {
    return {};
  }
  return MakeImpl(builder->slice());
}

bool NormalizeVPack(std::string_view args, std::string& definition) {
  if (args.empty()) {
    IRS_LOG_ERROR(absl::StrCat("Empty arguments", kParseError));
    return false;
  }
  velocypack::Slice input{reinterpret_cast<const uint8_t*>(args.data())};
  velocypack::Builder output;
  if (!NormalizeImpl(input, output)) {
    return false;
  }
  definition.assign(output.slice().startAs<char>(), output.slice().byteSize());
  return true;
}

bool NormalizeJson(std::string_view args, std::string& definition) {
  if (args.empty()) {
    IRS_LOG_ERROR(absl::StrCat("Empty arguments", kParseError));
    return false;
  }
  auto input = ParseArgs(args);
  if (!input) {
    return {};
  }
  velocypack::Builder output;
  if (!NormalizeImpl(input->slice(), output)) {
    return false;
  }
  definition = output.toString();
  return !definition.empty();
}

}  // namespace

void MinHashTokenStream::init() {
  REGISTER_ANALYZER_VPACK(irs::analysis::MinHashTokenStream, MakeVPack,
                          NormalizeVPack);
  REGISTER_ANALYZER_JSON(irs::analysis::MinHashTokenStream, MakeJson,
                         NormalizeJson);
}

MinHashTokenStream::MinHashTokenStream(Options&& opts)
  : opts_{std::move(opts)}, minhash_{opts.num_hashes} {
  if (!opts_.analyzer) {
    // Fallback to default implementation
    opts_.analyzer = std::make_unique<string_token_stream>();
  }

  term_ = irs::get<term_attribute>(*opts_.analyzer);

  if (IRS_UNLIKELY(!term_)) {
    opts_.analyzer = std::make_unique<empty_analyzer>();
  }

  offset_ = irs::get<offset>(*opts_.analyzer);

  std::get<term_attribute>(attrs_).value = {
    reinterpret_cast<const byte_type*>(buf_.data()), buf_.size()};
}

bool MinHashTokenStream::next() {
  if (begin_ == end_) {
    return false;
  }

  const auto value = absl::little_endian::FromHost(*begin_);

  [[maybe_unused]] const size_t length =
    absl::strings_internal::Base64EscapeInternal(
      reinterpret_cast<const uint8_t*>(&value), sizeof value, buf_.data(),
      buf_.size(), absl::strings_internal::kBase64Chars, false);
  IRS_ASSERT(length == buf_.size());

  std::get<increment>(attrs_).value = std::exchange(next_inc_.value, 0);
  ++begin_;

  return true;
}

bool MinHashTokenStream::reset(std::string_view data) {
  begin_ = end_ = {};
  if (opts_.analyzer->reset(data)) {
    ComputeSignature();
    return true;
  }
  return false;
}

void MinHashTokenStream::ComputeSignature() {
  minhash_.Clear();
  next_inc_.value = 1;

  if (opts_.analyzer->next()) {
    IRS_ASSERT(term_);

    const offset* offs = offset_ ? offset_ : &kEmptyOffset;
    auto& [start, end] = std::get<offset>(attrs_);
    start = offs->start;
    end = offs->end;

    do {
      const std::string_view value = ViewCast<char>(term_->value);
      const auto hash_value = ::CityHash64(value.data(), value.size());

      minhash_.Insert(hash_value);
      end = offs->end;
    } while (opts_.analyzer->next());

    begin_ = std::begin(minhash_);
    end_ = std::end(minhash_);
  }
}

}  // namespace irs::analysis
