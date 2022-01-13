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
#include "store/store_utils.hpp"
#include "utils/bytes_utils.hpp"

namespace {

using namespace irs;

class NormWriter final : public feature_writer {
 public:
  virtual void write(
      const field_stats& stats,
      doc_id_t doc,
      // cppcheck-suppress constParameter
      columnstore_writer::values_writer_f& writer) final {
    if (stats.len > 0) {
      const float_t value = 1.f / float_t(std::sqrt(double_t(stats.len)));
      if (value != Norm::DEFAULT()) {
        auto& stream = writer(doc);
        write_zvfloat(stream, value);
      }
    }
  }

  virtual void write(
      data_output& out,
      bytes_ref payload) {
    if (!payload.empty()) {
      out.write_bytes(payload.c_str(), payload.size());
    }
  }

  virtual void finish(bstring& /*out*/) final { }
};

NormWriter kNormWriter;

}

namespace iresearch {

bool NormReaderContext::Reset(
    const sub_reader& reader,
    field_id column_id,
    const document& doc) {
  const auto* column = reader.column(column_id);

  if (column) {
    auto it = column->iterator(false);
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

bool Norm2ReaderContext::Reset(
    const sub_reader& reader,
    field_id column_id,
    const document& doc) {
  if (NormReaderContext::Reset(reader, column_id, doc)) {
    const auto hdr = Norm2Header::Read(header);
    if (hdr.has_value()) {
      num_bytes = hdr.value().NumBytes();
      return true;
    }
  }

  return false;
}


/*static*/ feature_writer::ptr Norm::MakeWriter(range<const bytes_ref> /*payload*/) {
  return memory::to_managed<feature_writer, false>(&kNormWriter);
}

void Norm2Header::Reset(const Norm2Header& hdr) noexcept {
  min_ = std::min(hdr.min_, min_);
  max_ = std::max(hdr.max_, max_);
  encoding_ = std::max(hdr.encoding_, encoding_);
}

/*static*/ void Norm2Header::Write(const Norm2Header& hdr, bstring& out) {
  out.resize(ByteSize());

  auto* p = out.data();
  *p++ = static_cast<byte_type>(Norm2Version::kMin);
  *p++ = static_cast<byte_type>(hdr.encoding_);
  irs::write(p, hdr.min_);
  irs::write(p, hdr.max_);
}

/*static*/ std::optional<Norm2Header> Norm2Header::Read(bytes_ref payload) noexcept {
  if (IRS_UNLIKELY(payload.size() != ByteSize())) {
    IR_FRMT_ERROR("Invalid 'norm2' header size " IR_SIZE_T_SPECIFIER "",
                  payload.size());
    return std::nullopt;
  }

  const auto* p = payload.c_str();

  if (const byte_type ver = *p++;
      IRS_UNLIKELY(ver != static_cast<byte_type>(Norm2Version::kMin))) {
    IR_FRMT_ERROR("'norm2' header version mismatch, expected '%u', got '%u'",
                  static_cast<uint32_t>(Norm2Version::kMin),
                  static_cast<uint32_t>(ver));
    return std::nullopt;
  }

  const byte_type num_bytes = *p++;
  if (IRS_UNLIKELY(!CheckNumBytes(num_bytes))) {
    IR_FRMT_ERROR("Malformed 'norm2' header, invalid number of bytes '%u'",
                  static_cast<uint32_t>(num_bytes));
    return std::nullopt;
  }

  Norm2Header hdr{Norm2Encoding{num_bytes}};
  hdr.min_ = irs::read<decltype(min_)>(p);
  hdr.max_ = irs::read<decltype(max_)>(p);

  return hdr;
}

/*static*/ feature_writer::ptr Norm2::MakeWriter(range<const bytes_ref> headers) {
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
    case sizeof(byte_type):
      return memory::make_managed<Norm2Writer<byte_type>>();
    case sizeof(uint16_t):
      return memory::make_managed<Norm2Writer<uint16_t>>();
    default:
      assert(max_bytes == sizeof(uint32_t));
      return memory::make_managed<Norm2Writer<uint32_t>>();
  }

  return nullptr;
}

REGISTER_ATTRIBUTE(Norm);
REGISTER_ATTRIBUTE(Norm2);

} // iresearch
