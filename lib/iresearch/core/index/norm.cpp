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

#include "norm.hpp"

#include "shared.hpp"
#include "store/store_utils.hpp"
#include "utils/bytes_utils.hpp"

namespace {

using namespace irs;

class NormWriter : public FeatureWriter {
 public:
  void write(const field_stats& stats, doc_id_t doc,
             column_output& writer) final {
    if (stats.len > 0) {
      const float_t value = 1.f / float_t(std::sqrt(double_t(stats.len)));
      if (value != Norm::DEFAULT()) {
        writer.Prepare(doc);
        write_zvfloat(writer, value);
      }
    }
  }

  void write(data_output& out, bytes_view payload) final {
    if (!payload.empty()) {
      out.write_bytes(payload.data(), payload.size());
    }
  }

  void finish(bstring& /*out*/) final {}
};

NormWriter kNormWriter;

}  // namespace

namespace irs {

bool NormReaderContext::Reset(const ColumnProvider& reader, field_id column_id,
                              const document& doc) {
  const auto* column = reader.column(column_id);

  if (column) {
    auto it = column->iterator(ColumnHint::kNormal);
    if (IRS_LIKELY(it)) {
      auto* payload = irs::get<irs::payload>(*it);
      if (IRS_LIKELY(payload)) {
        this->header = column->payload();
        this->it = std::move(it);
        this->payload = payload;
        this->doc = &doc;
        return true;
      }
    }
  }

  return false;
}

bool Norm2ReaderContext::Reset(const ColumnProvider& reader, field_id column_id,
                               const document& doc) {
  if (NormReaderContext::Reset(reader, column_id, doc)) {
    const auto hdr = Norm2Header::Read(header);
    if (hdr.has_value()) {
      auto& value = hdr.value();
      num_bytes = static_cast<uint32_t>(value.NumBytes());
      max_num_bytes = static_cast<uint32_t>(value.MaxNumBytes());
      return true;
    }
  }

  return false;
}

FeatureWriter::ptr Norm::MakeWriter(std::span<const bytes_view> /*payload*/) {
  return memory::to_managed<FeatureWriter>(kNormWriter);
}

void Norm2Header::Reset(const Norm2Header& hdr) noexcept {
  min_ = std::min(hdr.min_, min_);
  max_ = std::max(hdr.max_, max_);
  encoding_ = std::max(hdr.encoding_, encoding_);
}

void Norm2Header::Write(const Norm2Header& hdr, bstring& out) {
  out.resize(ByteSize());

  auto* p = out.data();
  *p++ = static_cast<byte_type>(Norm2Version::kMin);
  *p++ = static_cast<byte_type>(hdr.encoding_);
  irs::write(p, hdr.min_);
  irs::write(p, hdr.max_);
}

std::optional<Norm2Header> Norm2Header::Read(bytes_view payload) noexcept {
  if (IRS_UNLIKELY(payload.size() != ByteSize())) {
    IRS_LOG_ERROR(absl::StrCat("Invalid 'norm2' header size ", payload.size()));
    return std::nullopt;
  }

  const auto* p = payload.data();

  if (const byte_type ver = *p++;
      IRS_UNLIKELY(ver != static_cast<byte_type>(Norm2Version::kMin))) {
    IRS_LOG_ERROR(absl::StrCat("'norm2' header version mismatch, expected: ",
                               static_cast<uint32_t>(Norm2Version::kMin),
                               ", got: ", static_cast<uint32_t>(ver)));
    return std::nullopt;
  }

  const byte_type num_bytes = *p++;
  if (IRS_UNLIKELY(!CheckNumBytes(num_bytes))) {
    IRS_LOG_ERROR(
      absl::StrCat("Malformed 'norm2' header, invalid number of bytes: ",
                   static_cast<uint32_t>(num_bytes)));
    return std::nullopt;
  }

  Norm2Header hdr{Norm2Encoding{num_bytes}};
  hdr.min_ = irs::read<decltype(min_)>(p);
  hdr.max_ = irs::read<decltype(max_)>(p);

  return hdr;
}

FeatureWriter::ptr Norm2::MakeWriter(std::span<const bytes_view> headers) {
  size_t max_bytes{sizeof(ValueType)};

  if (!headers.empty()) {
    Norm2Header acc{Norm2Encoding::Byte};
    for (auto header : headers) {
      auto hdr = Norm2Header::Read(header);
      if (!hdr.has_value()) {
        return nullptr;
      }
      acc.Reset(hdr.value());
    }
    max_bytes = acc.MaxNumBytes();
  }

  switch (max_bytes) {
    case sizeof(uint8_t):
      return memory::make_managed<Norm2Writer<uint8_t>>();
    case sizeof(uint16_t):
      return memory::make_managed<Norm2Writer<uint16_t>>();
    default:
      IRS_ASSERT(max_bytes == sizeof(uint32_t));
      return memory::make_managed<Norm2Writer<uint32_t>>();
  }
}

REGISTER_ATTRIBUTE(Norm);
REGISTER_ATTRIBUTE(Norm2);

}  // namespace irs
