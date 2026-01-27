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

#include "buffered_column.hpp"

#include "index/comparer.hpp"
#include "shared.hpp"
#include "utils/type_limits.hpp"

namespace irs {

bool BufferedColumn::FlushSparsePrimary(DocMap& docmap, column_output& writer,
                                        doc_id_t docs_count,
                                        const Comparer& compare) {
  auto comparer = [&](const auto& lhs, const auto& rhs) {
    return compare.Compare(GetPayload(lhs), GetPayload(rhs));
  };

  if (std::is_sorted(index_.begin(), index_.end(),
                     [&](const auto& lhs, const auto& rhs) {
                       return comparer(lhs, rhs) < 0;
                     })) {
    return false;
  }

  docmap.resize(doc_limits::min() + docs_count);

  std::vector<size_t> sorted_index(index_.size());
  std::iota(sorted_index.begin(), sorted_index.end(), 0);
  std::sort(sorted_index.begin(), sorted_index.end(),
            [&](size_t lhs, size_t rhs) {
              IRS_ASSERT(lhs < index_.size());
              IRS_ASSERT(rhs < index_.size());
              if (const auto r = comparer(index_[lhs], index_[rhs]); r) {
                return r < 0;
              }
              return lhs < rhs;
            });

  doc_id_t new_doc = doc_limits::min();

  for (size_t idx : sorted_index) {
    const auto* value = &index_[idx];

    doc_id_t min = doc_limits::min();
    if (IRS_LIKELY(idx)) {
      min += std::prev(value)->key;
    }

    for (const doc_id_t max = value->key; min < max; ++min) {
      docmap[min] = new_doc++;
    }

    docmap[min] = new_doc;
    writer.Prepare(new_doc);
    WriteValue(writer, *value);
    ++new_doc;
  }

  // Ensure that all docs up to new_doc are remapped without gaps
  IRS_ASSERT(std::all_of(docmap.begin() + 1, docmap.begin() + new_doc,
                         [](doc_id_t doc) { return doc_limits::valid(doc); }));
  // Ensure we reached the last doc in sort column
  IRS_ASSERT((std::prev(index_.end(), 1)->key + 1) == new_doc);
  // Handle docs without sort value that are placed after last filled sort doc
  for (auto begin = std::next(docmap.begin(), new_doc); begin != docmap.end();
       ++begin) {
    IRS_ASSERT(!doc_limits::valid(*begin));
    *begin = new_doc++;
  }

  return true;
}

std::pair<DocMap, field_id> BufferedColumn::Flush(
  columnstore_writer& writer, columnstore_writer::column_finalizer_f finalizer,
  doc_id_t docs_count, const Comparer& compare) {
  IRS_ASSERT(index_.size() <= docs_count);
  IRS_ASSERT(index_.empty() || index_.back().key <= docs_count);

  Prepare(doc_limits::eof());  // Insert last pending value

  if (IRS_UNLIKELY(index_.empty())) {
    return {DocMap{index_.get_allocator()}, field_limits::invalid()};
  }

  DocMap docmap{index_.get_allocator()};
  auto [column_id, column_writer] =
    writer.push_column(info_, std::move(finalizer));

  if (!FlushSparsePrimary(docmap, column_writer, docs_count, compare)) {
    FlushAlreadySorted(column_writer);
  }

  return {std::move(docmap), column_id};
}

void BufferedColumn::FlushAlreadySorted(column_output& writer) {
  for (const auto& value : index_) {
    writer.Prepare(value.key);
    WriteValue(writer, value);
  }
}

bool BufferedColumn::FlushDense(column_output& writer, DocMapView docmap,
                                BufferedValues& buffer) {
  IRS_ASSERT(!docmap.empty());

  const size_t total = docmap.size() - 1;  // -1 for the first element
  const size_t size = index_.size();

  if (!UseDenseSort(size, total)) {
    return false;
  }

  buffer.clear();
  buffer.resize(total);
  for (const auto& old_value : index_) {
    const auto new_key = docmap[old_value.key];
    auto& new_value = buffer[new_key - doc_limits::min()];
    new_value.key = new_key;
    new_value.begin = old_value.begin;
    new_value.size = old_value.size;
  }

  index_.clear();

  for (const auto& new_value : buffer) {
    if (doc_limits::valid(new_value.key)) {
      writer.Prepare(new_value.key);
      WriteValue(writer, new_value);
      index_.emplace_back(new_value);
    }
  }

  return true;
}

void BufferedColumn::FlushSparse(column_output& writer, DocMapView docmap) {
  IRS_ASSERT(!docmap.empty());

  for (auto& value : index_) {
    value.key = docmap[value.key];
  }

  std::sort(index_.begin(), index_.end(),
            [](const auto& lhs, const auto& rhs) noexcept {
              return lhs.key < rhs.key;
            });

  FlushAlreadySorted(writer);
}

field_id BufferedColumn::Flush(columnstore_writer& writer,
                               columnstore_writer::column_finalizer_f finalizer,
                               DocMapView docmap, BufferedValues& buffer) {
  IRS_ASSERT(docmap.size() < irs::doc_limits::eof());

  Prepare(doc_limits::eof());  // Insert last pending value

  if (IRS_UNLIKELY(index_.empty())) {
    return field_limits::invalid();
  }

  auto [column_id, column_writer] =
    writer.push_column(info_, std::move(finalizer));

  if (docmap.empty()) {
    FlushAlreadySorted(column_writer);
  } else if (!FlushDense(column_writer, docmap, buffer)) {
    FlushSparse(column_writer, docmap);
  }

  return column_id;
}

}  // namespace irs
