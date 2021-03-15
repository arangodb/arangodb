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

#include "shared.hpp"
#include "sorted_column.hpp"
#include "comparer.hpp"
#include "utils/type_limits.hpp"
#include "utils/misc.hpp"
#include "utils/lz4compression.hpp"

namespace iresearch {

std::pair<doc_map, field_id> sorted_column::flush(
    columnstore_writer& writer,
    doc_id_t max,
    const comparer& less
) {
  assert(index_.size() <= max);
  assert(index_.empty() || index_.back().first <= max);

  // temporarily push sentinel
  index_.emplace_back(doc_limits::eof(), data_buf_.size());

  // prepare order array (new -> old)
  // first - position in 'index_', eof() - not present
  // second - old document id, 'doc_limit::min()'-based
  std::vector<std::pair<irs::doc_id_t, irs::doc_id_t>> new_old(
    doc_limits::min() + max, std::make_pair(doc_limits::eof(), 0)
  );

  doc_id_t new_doc_id = irs::doc_limits::min();
  for (size_t i = 0, size = index_.size()-1; i < size; ++i) {
    const auto doc_id = index_[i].first; // 'doc_limit::min()'-based doc_id

    while (new_doc_id < doc_id) {
      new_old[new_doc_id].second = new_doc_id;
      ++new_doc_id;
    }

    new_old[new_doc_id].first = doc_id_t(i);
    new_old[new_doc_id].second = new_doc_id;
    ++new_doc_id;
  }

  auto get_value = [this](irs::doc_id_t doc) {
    return doc_limits::eof(doc)
      ? bytes_ref::NIL
      : bytes_ref(data_buf_.c_str() + index_[doc].second,
                  index_[doc+1].second - index_[doc].second);
  };

  auto comparer = [&less, &get_value] (
      const std::pair<doc_id_t, doc_id_t>& lhs,
      const std::pair<doc_id_t, doc_id_t>& rhs) {
    if (lhs.first == rhs.first) {
      return false;
    }

    return less(get_value(lhs.first), get_value(rhs.first));
  };

  doc_map docmap;
  auto begin = new_old.begin() + irs::doc_limits::min();

  // perform extra check to avoid qsort worst case complexity
  if (!std::is_sorted(begin, new_old.end(), comparer)) {
    std::sort(begin, new_old.end(), comparer);

    docmap.resize(new_old.size(), doc_limits::eof());

    for (size_t i = irs::doc_limits::min(), size = docmap.size(); i < size; ++i) {
      docmap[new_old[i].second] = doc_id_t(i);
    }
  }

  // flush sorted data
  auto column = writer.push_column(info_);
  auto& column_writer = column.second;

  new_doc_id = doc_limits::min();
  for (auto end = new_old.end(); begin != end; ++begin) {
    auto& stream = column_writer(new_doc_id++);

    if (!doc_limits::eof(begin->first)) {
      write_value(stream, begin->first);
    }
  };

  clear(); // data have been flushed

  return std::pair<doc_map, field_id>(
    std::piecewise_construct,
    std::forward_as_tuple(std::move(docmap)),
    std::forward_as_tuple(column.first)
  );
}

void sorted_column::flush_already_sorted(
    const columnstore_writer::values_writer_f& writer) {
  for (size_t i = 0, size = index_.size() - 1; i < size; ++i) { // -1 for sentinel
    write_value(writer(index_[i].first), i);
  }
}

bool sorted_column::flush_dense(
    const columnstore_writer::values_writer_f& writer,
    const doc_map& docmap,
    flush_buffer_t& buffer) {
  assert(!docmap.empty());

  const size_t total = docmap.size() - 1; // -1 for first element
  const size_t size = index_.size() - 1; // -1 for sentinel

  if (!use_dense_sort(size, total)) {
    return false;
  }

  buffer.clear();
  buffer.resize(
    total,
    std::make_pair(doc_limits::eof(), doc_limits::invalid())
  );

  for (size_t i = 0; i < size; ++i) {
    buffer[docmap[index_[i].first] - doc_limits::min()].first = doc_id_t(i);
  }

  // flush sorted data
  irs::doc_id_t doc = doc_limits::min();
  for (const auto& entry : buffer) {
    if (!doc_limits::eof(entry.first)) {
      write_value(writer(doc), entry.first);
    }
    ++doc;
  };

  return true;
}

void sorted_column::flush_sparse(
    const columnstore_writer::values_writer_f& writer,
    const doc_map& docmap,
    flush_buffer_t& buffer) {
  assert(!docmap.empty());

  const size_t size = index_.size() - 1; // -1 for sentinel

  buffer.resize(size);

  for (size_t i = 0; i < size; ++i) {
    buffer[i] = std::make_pair(doc_id_t(i), docmap[index_[i].first]);
  }

  std::sort(
    buffer.begin(), buffer.end(),
    [] (const std::pair<size_t, doc_id_t>& lhs,
        const std::pair<size_t, doc_id_t>& rhs) {
      return lhs.second < rhs.second;
  });

  // flush sorted data
  for (const auto& entry: buffer) {
    write_value(writer(entry.second), entry.first);
  };
}

field_id sorted_column::flush(
    columnstore_writer& writer,
    const doc_map& docmap,
    std::vector<std::pair<doc_id_t, doc_id_t>>& buffer) {
  assert(docmap.size() < irs::doc_limits::eof());

  if (index_.empty()) {
    return field_limits::invalid();
  }

  auto column = writer.push_column(info_);
  auto& column_writer = column.second;

  // temporarily push sentinel
  index_.emplace_back(doc_limits::eof(), data_buf_.size());

  if (docmap.empty()) {
    flush_already_sorted(column_writer);
  } else if (!flush_dense(column_writer, docmap, buffer)) {
    flush_sparse(column_writer, docmap, buffer);
  }

  clear(); // data have been flushed

  return column.first;
}

}
