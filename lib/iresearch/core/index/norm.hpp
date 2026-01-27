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

#pragma once

#include "analysis/token_attributes.hpp"
#include "index/field_meta.hpp"
#include "store/store_utils.hpp"
#include "utils/lz4compression.hpp"

namespace irs {

struct NormReaderContext {
  bool Reset(const ColumnProvider& segment, field_id column,
             const document& doc);
  bool Valid() const noexcept { return doc != nullptr; }

  bytes_view header;
  doc_iterator::ptr it;
  const irs::payload* payload{};
  const document* doc{};
};

static_assert(std::is_nothrow_move_constructible_v<NormReaderContext>);
static_assert(std::is_nothrow_move_assignable_v<NormReaderContext>);

class Norm : public attribute {
 public:
  using Context = NormReaderContext;

  // DO NOT CHANGE NAME
  static constexpr std::string_view type_name() noexcept { return "norm"; }

  IRS_FORCE_INLINE static constexpr float_t DEFAULT() noexcept { return 1.f; }

  static FeatureWriter::ptr MakeWriter(std::span<const bytes_view> payload);

  static auto MakeReader(Context&& ctx) {
    IRS_ASSERT(ctx.it);
    IRS_ASSERT(ctx.payload);
    IRS_ASSERT(ctx.doc);

    return [ctx = std::move(ctx)]() mutable -> float_t {
      if (const auto doc = ctx.doc->value; doc != ctx.it->seek(doc)) {
        return Norm::DEFAULT();
      }
      bytes_view_input in{ctx.payload->value};
      return read_zvfloat(in);
    };
  }
};

static_assert(std::is_nothrow_move_constructible_v<Norm>);
static_assert(std::is_nothrow_move_assignable_v<Norm>);

enum class Norm2Version : uint8_t { kMin = 0 };

enum class Norm2Encoding : uint8_t {
  Byte = sizeof(uint8_t),
  Short = sizeof(uint16_t),
  Int = sizeof(uint32_t)
};

class Norm2Header final {
 public:
  constexpr static size_t ByteSize() noexcept {
    return sizeof(Norm2Version) + sizeof(encoding_) + sizeof(min_) +
           sizeof(max_);
  }

  constexpr static bool CheckNumBytes(size_t num_bytes) noexcept {
    return num_bytes == sizeof(uint8_t) || num_bytes == sizeof(uint16_t) ||
           num_bytes == sizeof(uint32_t);
  }

  explicit Norm2Header(Norm2Encoding encoding) noexcept : encoding_{encoding} {
    IRS_ASSERT(CheckNumBytes(static_cast<size_t>(encoding_)));
  }

  void Reset(uint32_t value) noexcept {
    min_ = std::min(min_, value);
    max_ = std::max(max_, value);
  }

  void Reset(const Norm2Header& hdr) noexcept;

  size_t MaxNumBytes() const noexcept {
    if (max_ <= std::numeric_limits<uint8_t>::max()) {
      return sizeof(uint8_t);
    } else if (max_ <= std::numeric_limits<uint16_t>::max()) {
      return sizeof(uint16_t);
    } else {
      return sizeof(uint32_t);
    }
  }

  size_t NumBytes() const noexcept { return static_cast<size_t>(encoding_); }

  static void Write(const Norm2Header& hdr, bstring& out);
  static std::optional<Norm2Header> Read(irs::bytes_view payload) noexcept;

 private:
  uint32_t min_{std::numeric_limits<uint32_t>::max()};
  uint32_t max_{std::numeric_limits<uint32_t>::min()};
  Norm2Encoding encoding_;  // Number of bytes used for encoding
};

template<typename T>
class Norm2Writer : public FeatureWriter {
 public:
  static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                std::is_same_v<T, uint32_t>);

  explicit Norm2Writer() noexcept : hdr_{Norm2Encoding{sizeof(T)}} {}

  void write(const field_stats& stats, doc_id_t doc,
             // cppcheck-suppress constParameter
             column_output& writer) final {
    hdr_.Reset(stats.len);

    writer.Prepare(doc);
    WriteValue(writer, stats.len);
  }

  void write(data_output& out, bytes_view payload) final {
    uint32_t value;

    switch (payload.size()) {
      case sizeof(uint8_t): {
        value = payload.front();
      } break;
      case sizeof(uint16_t): {
        auto* p = payload.data();
        value = irs::read<uint16_t>(p);
      } break;
      case sizeof(uint32_t): {
        auto* p = payload.data();
        value = irs::read<uint32_t>(p);
      } break;
      default:
        return;
    }

    hdr_.Reset(value);
    WriteValue(out, value);
  }

  void finish(bstring& out) final { Norm2Header::Write(hdr_, out); }

 private:
  static void WriteValue(data_output& out, uint32_t value) {
    if constexpr (sizeof(T) == sizeof(uint8_t)) {
      out.write_byte(static_cast<uint8_t>(value & 0xFF));
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
  bool Reset(const ColumnProvider& segment, field_id column,
             const document& doc);
  bool Valid() const noexcept {
    return NormReaderContext::Valid() && num_bytes;
  }

  uint32_t num_bytes{};
  uint32_t max_num_bytes{};
};

class Norm2 : public attribute {
 public:
  using ValueType = uint32_t;
  using Context = Norm2ReaderContext;

  // DO NOT CHANGE NAME
  static constexpr std::string_view type_name() noexcept {
    return "iresearch::norm2";
  }

  static FeatureWriter::ptr MakeWriter(std::span<const bytes_view> payload);

  template<typename T>
  static auto MakeReader(Context&& ctx) {
    static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                  std::is_same_v<T, uint32_t>);
    IRS_ASSERT(ctx.num_bytes == sizeof(T));
    IRS_ASSERT(ctx.it);
    IRS_ASSERT(ctx.payload);
    IRS_ASSERT(ctx.doc);

    return [ctx = std::move(ctx)]() -> ValueType {
      if (const doc_id_t doc = ctx.doc->value;
          IRS_LIKELY(doc == ctx.it->seek(doc))) {
        IRS_ASSERT(sizeof(T) == ctx.payload->value.size());
        const auto* value = ctx.payload->value.data();

        if constexpr (std::is_same_v<T, uint8_t>) {
          return *value;
        } else {
          return irs::read<T>(value);
        }
      }

      // we should investigate why we failed to find a norm2 value for doc
      IRS_ASSERT(false);

      return 1;
    };
  }

  template<typename Func>
  static auto MakeReader(Context&& ctx, Func&& func) {
    IRS_ASSERT(Norm2Header::CheckNumBytes(ctx.num_bytes));

    switch (ctx.num_bytes) {
      case sizeof(uint8_t):
        return func(MakeReader<uint8_t>(std::move(ctx)));
      case sizeof(uint16_t):
        return func(MakeReader<uint16_t>(std::move(ctx)));
      default:
        return func(MakeReader<uint32_t>(std::move(ctx)));
    }
  }

  ValueType value{};
};

static_assert(std::is_nothrow_move_constructible_v<Norm2>);
static_assert(std::is_nothrow_move_assignable_v<Norm2>);

}  // namespace irs
