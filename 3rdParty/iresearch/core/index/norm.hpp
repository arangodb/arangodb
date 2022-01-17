////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_NORM_H
#define IRESEARCH_NORM_H

#include "shared.hpp"
#include "analysis/token_attributes.hpp"
#include "store/store_utils.hpp"
#include "utils/lz4compression.hpp"

namespace iresearch {

struct NormReaderContext {
  bool Reset(const sub_reader& segment,
             field_id column,
             const document& doc);
  bool Valid() const noexcept {
    return doc != nullptr;
  }

  bytes_ref header;
  doc_iterator::ptr it;
  const irs::payload* payload{};
  const document* doc{};
};

static_assert(std::is_nothrow_move_constructible_v<NormReaderContext>);
static_assert(std::is_nothrow_move_assignable_v<NormReaderContext>);

class IRESEARCH_API Norm : public attribute {
 public:
  using Context = NormReaderContext;

  // DO NOT CHANGE NAME
  static constexpr string_ref type_name() noexcept {
    return "norm";
  }

  FORCE_INLINE static constexpr float_t DEFAULT() noexcept {
    return 1.f;
  }

  static feature_writer::ptr MakeWriter(range<const bytes_ref> payload);

  static auto MakeReader(Context&& ctx) {
    assert(ctx.it);
    assert(ctx.payload);
    assert(ctx.doc);

    return [ctx = std::move(ctx)]() mutable -> float_t {
      if (const auto doc = ctx.doc->value;
          doc != ctx.it->seek(doc)) {
        return Norm::DEFAULT();
      }
      bytes_ref_input in{ctx.payload->value};
      return read_zvfloat(in);
    };
  }
};

static_assert(std::is_nothrow_move_constructible_v<Norm>);
static_assert(std::is_nothrow_move_assignable_v<Norm>);

enum class Norm2Version : byte_type {
  kMin = 0
};

enum class Norm2Encoding : byte_type {
  Byte = sizeof(byte_type),
  Short = sizeof(uint16_t),
  Int = sizeof(uint32_t)
};

class IRESEARCH_API Norm2Header final {
 public:
  constexpr static size_t ByteSize() noexcept {
    return sizeof(Norm2Version) + sizeof(encoding_) +
           sizeof(min_) + sizeof(max_);
  }

  constexpr static bool CheckNumBytes(size_t num_bytes) noexcept {
    return num_bytes == sizeof(byte_type) ||
           num_bytes == sizeof(uint16_t) ||
           num_bytes == sizeof(uint32_t);
  }

  explicit Norm2Header(Norm2Encoding encoding) noexcept
    : encoding_{encoding} {
    assert(CheckNumBytes(static_cast<size_t>(encoding_)));
  }

  void Reset(uint32_t value) noexcept {
    min_ = std::min(min_, value);
    max_ = std::max(max_, value);
  }

  void Reset(const Norm2Header& hdr) noexcept;

  size_t MaxNumBytes() const noexcept {
    if (max_ <= std::numeric_limits<byte_type>::max()) {
      return sizeof(byte_type);
    } else if (max_ <= std::numeric_limits<uint16_t>::max()) {
      return sizeof(uint16_t);
    } else {
      return sizeof(uint32_t);
    }
  }

  size_t NumBytes() const noexcept {
    return static_cast<size_t>(encoding_);
  }

  static void Write(const Norm2Header& hdr, bstring& out);
  static std::optional<Norm2Header> Read(irs::bytes_ref payload) noexcept;

 private:
  uint32_t min_{std::numeric_limits<uint32_t>::max()};
  uint32_t max_{std::numeric_limits<uint32_t>::min()};
  Norm2Encoding encoding_; // Number of bytes used for encoding
};

template<typename T>
class Norm2Writer final : public feature_writer {
 public:
  static_assert(std::is_same_v<T, byte_type> ||
                std::is_same_v<T, uint16_t>  ||
                std::is_same_v<T, uint32_t>);

  explicit Norm2Writer() noexcept
    : hdr_{Norm2Encoding{sizeof(T)}} {
  }

  virtual void write(
      const field_stats& stats,
      doc_id_t doc,
      // cppcheck-suppress constParameter
      columnstore_writer::values_writer_f& writer) final {
    hdr_.Reset(stats.len);

    auto& stream = writer(doc);
    WriteValue(stream, stats.len);
  }

  virtual void write(data_output& out, bytes_ref payload) {
    uint32_t value;

    switch (payload.size()) {
      case sizeof(irs::byte_type): {
        value = payload.front();
      } break;
      case sizeof(uint16_t): {
        auto* p = payload.c_str();
        value = irs::read<uint16_t>(p);
      } break;
      case sizeof(uint32_t): {
        auto* p = payload.c_str();
        value = irs::read<uint32_t>(p);
      } break;
      default:
        return;
    }

    hdr_.Reset(value);
    WriteValue(out, value);
  }

  virtual void finish(bstring& out) final {
    Norm2Header::Write(hdr_, out);
  }

 private:
  static void WriteValue(data_output& out, uint32_t value) {
    if constexpr (sizeof(T) == sizeof(byte_type)) {
      out.write_byte(static_cast<byte_type>(value & 0xFF));
    }

    if constexpr (sizeof(T) == sizeof(uint16_t)) {
      out.write_short(static_cast<uint16_t>(value & 0xFFFF));
    }

    if constexpr (sizeof(T) == sizeof(uint32_t)) {
      out.write_int(value);
    }
  }

  Norm2Header hdr_;
};

struct Norm2ReaderContext : NormReaderContext {
  bool Reset(const sub_reader& segment,
             field_id column,
             const document& doc);
  bool Valid() const noexcept {
    return NormReaderContext::Valid() && num_bytes;
  }

  uint32_t num_bytes{};
};

class IRESEARCH_API Norm2 : public attribute {
 public:
  using ValueType = uint32_t;
  using Context = Norm2ReaderContext;

  // DO NOT CHANGE NAME
  static constexpr string_ref type_name() noexcept {
    return "iresearch::norm2";
  }

  static feature_writer::ptr MakeWriter(range<const bytes_ref> payload);

  template<typename T>
  static auto MakeReader(Context&& ctx) {
    static_assert(std::is_same_v<T, byte_type> ||
                  std::is_same_v<T, uint16_t>  ||
                  std::is_same_v<T, uint32_t>);
    assert(ctx.num_bytes == sizeof(T));
    assert(ctx.it);
    assert(ctx.payload);
    assert(ctx.doc);

    return [ctx = std::move(ctx)]() -> ValueType {
      if (const doc_id_t doc = ctx.doc->value;
          IRS_LIKELY(doc == ctx.it->seek(doc))) {
        assert(sizeof(T) == ctx.payload->value.size());
        const auto* value = ctx.payload->value.c_str();

        if constexpr (std::is_same_v<T, irs::byte_type>) {
          return *value;
        }

        return irs::read<T>(value);
      }

      // we should investigate why we failed to find a norm2 value for doc
      assert(false);

      return 1;
    };
  }

  template<typename Func>
  static auto MakeReader(Context&& ctx, Func&& func) {
    assert(Norm2Header::CheckNumBytes(ctx.num_bytes));

    switch (ctx.num_bytes) {
      case sizeof(uint8_t):
        return func(MakeReader<uint8_t>(std::move(ctx)));
      case sizeof(uint16_t):
        return func(MakeReader<uint16_t>(std::move(ctx)));
      default:
        return func(MakeReader<uint32_t>(std::move(ctx)));
    }
  }
};

static_assert(std::is_nothrow_move_constructible_v<Norm2>);
static_assert(std::is_nothrow_move_assignable_v<Norm2>);

} // iresearch

#endif // IRESEARCH_NORM_H
