////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include <vector>

#include "formats/formats.hpp"
#include "index/column_info.hpp"
#include "store/store_utils.hpp"

namespace irs {

class Comparer;

struct BufferedValue {
  BufferedValue() = default;
  BufferedValue(doc_id_t key, size_t begin, size_t size) noexcept
    : key{key}, begin{begin}, size{size} {}

  doc_id_t key{};
  size_t begin{};
  size_t size{};
};

class BufferedColumn final : public column_output, private util::noncopyable {
 public:
  using BufferedValues = ManagedVector<BufferedValue>;
  using Buffer = irs::basic_string<irs::byte_type,
                                   irs::ManagedTypedAllocator<irs::byte_type>>;
  explicit BufferedColumn(const ColumnInfo& info, IResourceManager& rm)
    : data_buf_{{rm}}, index_{{rm}}, info_{info} {}

  void Prepare(doc_id_t key) final {
    IRS_ASSERT(key >= pending_key_);

    if (IRS_LIKELY(pending_key_ != key)) {
      const auto offset = data_buf_.size();

      if (IRS_LIKELY(doc_limits::valid(pending_key_))) {
        IRS_ASSERT(offset >= pending_offset_);
        index_.emplace_back(pending_key_, pending_offset_,
                            offset - pending_offset_);
      }

      pending_key_ = key;
      pending_offset_ = offset;
    }
  }

  void write_byte(byte_type b) final { data_buf_ += b; }

  void write_bytes(const byte_type* b, size_t size) final {
    data_buf_.append(b, size);
  }

  void reset() final {
    if (!doc_limits::valid(pending_key_)) {
      return;
    }

    data_buf_.resize(pending_offset_);
    pending_key_ = doc_limits::invalid();
  }

  bool Empty() const noexcept { return index_.empty(); }

  size_t Size() const noexcept { return index_.size(); }

  void Clear() noexcept {
    data_buf_.clear();
    index_.clear();
    pending_key_ = doc_limits::invalid();
  }

  // 1st - doc map (old->new), empty -> already sorted
  // 2nd - flushed column identifier
  std::pair<DocMap, field_id> Flush(
    columnstore_writer& writer,
    columnstore_writer::column_finalizer_f header_writer,
    doc_id_t docs_count,  // total number of docs in segment
    const Comparer& compare);

  field_id Flush(columnstore_writer& writer,
                 columnstore_writer::column_finalizer_f header_writer,
                 DocMapView docmap, BufferedValues& buffer);

  size_t MemoryActive() const noexcept {
    return data_buf_.size() +
           index_.size() * sizeof(decltype(index_)::value_type);
  }

  size_t MemoryReserved() const noexcept {
    return data_buf_.capacity() +
           index_.capacity() * sizeof(decltype(index_)::value_type);
  }

  const ColumnInfo& Info() const noexcept { return info_; }

  std::span<const BufferedValue> Index() const noexcept { return index_; }

  bytes_view Data() const noexcept { return data_buf_; }

 private:
  friend class BufferedColumnIterator;

  bytes_view GetPayload(const BufferedValue& value) noexcept {
    return {data_buf_.data() + value.begin, value.size};
  }

  void WriteValue(data_output& out, const BufferedValue& value) {
    const auto payload = GetPayload(value);
    out.write_bytes(payload.data(), payload.size());
  }

  bool FlushSparsePrimary(DocMap& docmap, column_output& writer,
                          doc_id_t docs_count, const Comparer& compare);

  void FlushAlreadySorted(column_output& writer);

  bool FlushDense(column_output& writer, DocMapView docmap,
                  BufferedValues& buffer);

  void FlushSparse(column_output& writer, DocMapView docmap);

  Buffer data_buf_;  // FIXME use memory_file or block_pool instead
  BufferedValues index_;
  size_t pending_offset_{};
  doc_id_t pending_key_{doc_limits::invalid()};
  ColumnInfo info_;
};

}  // namespace irs
