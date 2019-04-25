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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "shared.hpp"
#include "sorted_column.hpp"
#include "utils/type_limits.hpp"
#include "utils/misc.hpp"

NS_LOCAL



NS_END

NS_ROOT

//FIXME make docmap accessible via doc, rather than doc-doc_limits::min()
//FIXME optimize dense case

void sorted_column::write_value(data_output& out, const size_t idx) {
  const auto begin = index_[idx].second;
  const auto end = index_[idx+1].second;
  assert(begin <= end);

  out.write_bytes(data_buf_.c_str() + begin, end - begin);
}

std::pair<doc_map, field_id> sorted_column::flush(
    columnstore_writer& writer,
    doc_id_t max,
    const comparer& less
) {
  assert(index_.size() <= max);

  // temporarily push sentinel
  index_.emplace_back(doc_limits::eof(), data_buf_.size());

  // prepare order array (new -> old)
  // first - position in 'index_', eof() - not present
  // second - old document id, 0-based
  std::vector<std::pair<irs::doc_id_t, irs::doc_id_t>> new_old(
    max, std::make_pair(doc_limits::eof(), 0)
  );

  doc_id_t new_doc_id = 0;
  for (size_t i = 0, size = index_.size()-1; i < size; ++i) {
    const auto doc_id = index_[i].first - doc_limits::min(); // 0-based doc_id

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

  // perform extra check to avoid qsort worst case complexity
  if (!std::is_sorted(new_old.begin(), new_old.end(), comparer)) {
    std::sort(new_old.begin(), new_old.end(), comparer);

    docmap.resize(max);
    for (size_t i = 0, size = new_old.size(); i < size; ++i) {
      docmap[new_old[i].second] = doc_id_t(i);
    }
  }

  // flush sorted data
  auto column = writer.push_column();
  auto& column_writer = column.second;

  new_doc_id = doc_limits::min();
  for (const auto& entry: new_old) {
    auto& stream = column_writer(new_doc_id++);

    if (!doc_limits::eof(entry.first)) {
      write_value(stream, entry.first);
    }
  };

  // data have been flushed
  clear();

  return std::pair<doc_map, field_id>(
    std::piecewise_construct,
    std::forward_as_tuple(std::move(docmap)),
    std::forward_as_tuple(column.first)
  );
}

field_id sorted_column::flush(
    columnstore_writer& writer,
    const doc_map& docmap
) {
  auto column = writer.push_column();
  auto& column_writer = column.second;

  const size_t size = index_.size();

  // temporarily push sentinel
  index_.emplace_back(doc_limits::eof(), data_buf_.size());

  if (docmap.empty()) { // already sorted
    // temporarily push sentinel
    for (size_t i = 0; i < size; ++i) {
      write_value(column_writer(index_[i].first), i);
    }
  } else {
    assert(size <= docmap.size());

    std::vector<std::pair<doc_id_t, doc_id_t>> sorted_index(size);

    for (size_t i = 0; i < size; ++i) {
      //FIXME remove +/- doc_limits::min()
      sorted_index[i] = std::make_pair(doc_id_t(i), docmap[index_[i].first - doc_limits::min()] + doc_limits::min());
    }

    std::sort(
      sorted_index.begin(), sorted_index.end(),
      [] (const std::pair<size_t, doc_id_t>& lhs,
          const std::pair<size_t, doc_id_t>& rhs) {
        return lhs.second < rhs.second;
    });

    // flush sorted data
    for (const auto& entry: sorted_index) {
      write_value(column_writer(entry.second), entry.first);
    };
  }

  // data have been flushed
  clear();

  return column.first;
}

NS_END
