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

#ifndef IRESEARCH_SORTED_COLUMN_H
#define IRESEARCH_SORTED_COLUMN_H

#include "column_info.hpp"
#include "formats/formats.hpp"
#include "store/store_utils.hpp"

#include <vector>

namespace iresearch {

typedef std::vector<doc_id_t> doc_map;

class comparer;

class sorted_column final : public irs::columnstore_writer::column_output {
 public:
  typedef std::vector<std::pair<doc_id_t, doc_id_t>> flush_buffer_t;

  explicit sorted_column(const column_info& info)
    : info_(info) {
  }

  void prepare(doc_id_t key) {
    assert(index_.empty() || key >= index_.back().first);

    if (index_.empty() || index_.back().first != key) {
      index_.emplace_back(key, data_buf_.size());
    }
  }

  virtual void close() override {
    // NOOP
  }

  virtual void write_byte(byte_type b) override {
    data_buf_ += b;
  }

  virtual void write_bytes(const byte_type* b, size_t size) override {
    data_buf_.append(b, size);
  }

  virtual void reset() override {
    if (index_.empty()) {
      return;
    }

    data_buf_.resize(index_.back().second);
    index_.pop_back();
  }

  bool empty() const noexcept {
    return index_.empty();
  }

  size_t size() const noexcept {
    return index_.size();
  }

  void clear() noexcept {
    data_buf_.clear();
    index_.clear();
  }

  // 1st - doc map (old->new), empty -> already sorted
  // 2nd - flushed column identifier
  std::pair<doc_map, field_id> flush(
    columnstore_writer& writer,
    doc_id_t max, // total number of docs in segment
    const comparer& less
  );

  field_id flush(
    columnstore_writer& writer,
    const doc_map& docmap,
    flush_buffer_t& buffer
  );

  size_t memory_active() const noexcept {
    return data_buf_.size() + index_.size()*sizeof(decltype(index_)::value_type);
  }

  size_t memory_reserved() const noexcept {
    return data_buf_.capacity() + index_.capacity()*sizeof(decltype(index_)::value_type);
  }

  const column_info& info() const noexcept {
    return info_;
  }

 private:
  void write_value(data_output& out, const size_t idx) {
    assert(idx + 1 < index_.size());
    const auto begin = index_[idx].second;
    const auto end = index_[idx+1].second;
    assert(begin <= end);

    out.write_bytes(data_buf_.c_str() + begin, end - begin);
  }

  void flush_already_sorted(
    const columnstore_writer::values_writer_f& writer
  );

  bool flush_dense(
    const columnstore_writer::values_writer_f& writer,
    const doc_map& docmap,
    flush_buffer_t& buffer
  );

  void flush_sparse(
    const columnstore_writer::values_writer_f& writer,
    const doc_map& docmap,
    flush_buffer_t& buffer
  );

  bstring data_buf_; // FIXME use memory_file or block_pool instead
  std::vector<std::pair<irs::doc_id_t, size_t>> index_; // doc_id + offset in 'data_buf_'
  column_info info_;
}; // sorted_column

} // ROOT

#endif // IRESEARCH_SORTED_COLUMN_H
