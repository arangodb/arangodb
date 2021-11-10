////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_ASSERT_FORMAT_H
#define IRESEARCH_ASSERT_FORMAT_H

#include "doc_generator.hpp"
#include "index/field_meta.hpp"
#include "formats/formats.hpp"

namespace tests {

struct position {
  position(uint32_t pos, uint32_t start,
           uint32_t end, const irs::bytes_ref& pay)
    : pos{pos}, start{start},
      end{end}, payload{pay} {
  }

  bool operator<(const position& rhs) const {
    return pos < rhs.pos;
  }

  uint32_t pos;
  uint32_t start;
  uint32_t end;
  irs::bstring payload;
};

class posting {
 public:
  explicit posting(irs::doc_id_t id)
    : id_{id} {
  }
  posting(irs::doc_id_t id, std::set<position>&& positions)
    : positions_(std::move(positions)), id_(id) {
  }
  posting(posting&& rhs) noexcept = default;
  posting& operator=(posting&& rhs) noexcept = default;

  void insert(uint32_t pos, uint32_t offs_start, const irs::attribute_provider& attrs);

  bool operator<(const posting& rhs) const {
    return id_ < rhs.id_;
  }

  const std::set<position>& positions() const { return positions_; }
  irs::doc_id_t id() const { return id_; }
  size_t size() const { return positions_.size(); }

 private:
  friend struct term;

  std::set<position> positions_;
  irs::doc_id_t id_;
};

struct term {
  explicit term(irs::bytes_ref data);

  posting& insert(irs::doc_id_t id);

  bool operator<(const term& rhs) const;

  uint64_t docs_count() const { return postings.size(); }

  void sort(const std::map<irs::doc_id_t, irs::doc_id_t>& docs);

  std::set<posting> postings;
  irs::bstring value;
};

struct field : public irs::field_meta {
  struct feature_info {
    irs::field_id id;
    irs::feature_handler_f handler;
  };

  struct field_stats : irs::field_stats {
    uint32_t pos{};
    uint32_t offs{};
  };

  field(const irs::string_ref& name,
        irs::IndexFeatures index_features,
        const irs::features_t& features);
  field(field&& rhs) = default;
  field& operator=(field&& rhs) = default;

  term& insert(irs::bytes_ref term);
  term* find(irs::bytes_ref term);
  size_t remove(irs::bytes_ref term);
  void sort(const std::map<irs::doc_id_t, irs::doc_id_t>& docs);

  irs::bytes_ref min() const;
  irs::bytes_ref max() const;
  uint64_t total_freq() const;

  irs::seek_term_iterator::ptr iterator() const;

  std::vector<feature_info> feature_infos;
  std::set<term> terms;
  std::unordered_set<irs::doc_id_t> docs;
  field_stats stats;
};

class column_values {
 public:
  void insert(irs::doc_id_t key, irs::bytes_ref value);

  auto begin() const { return values_.begin(); }
  auto end() const { return values_.end(); }
  auto size() const { return values_.size(); }
  auto empty() const { return values_.empty(); }

  void sort(const std::map<irs::doc_id_t, irs::doc_id_t>& docs);

 private:
  std::map<irs::doc_id_t, irs::bstring> values_;
};

class index_segment: irs::util::noncopyable {
 public:
  using field_map_t = std::map<irs::string_ref, field>;
  using columns_t = std::deque<column_values>; // pointers remain valid
  using columns_meta_t = std::map<std::string, irs::field_id>;

  explicit index_segment(const irs::field_features_t& features)
    : field_features_{features} {
  }
  index_segment(index_segment&& rhs) = default;
  index_segment& operator=(index_segment&& rhs) = default;

  irs::doc_id_t doc_count() const noexcept { return count_; }
  size_t size() const noexcept { return fields_.size(); }
  auto& doc_mask() const noexcept { return doc_mask_; }
  auto& fields() const noexcept { return fields_; }
  auto& columns() noexcept { return columns_; }
  auto& columns() const noexcept { return columns_; }
  auto& columns_meta() const noexcept { return columns_meta_; }
  auto& columns_meta() noexcept { return columns_meta_; }
  auto& pk() const noexcept { return sort_; };

  template<typename IndexedFieldIterator, typename StoredFieldIterator>
  void insert(
      IndexedFieldIterator indexed_begin, IndexedFieldIterator indexed_end,
      StoredFieldIterator stored_begin, StoredFieldIterator stored_end,
      const ifield* sorted = nullptr);

  void insert(const document& doc) {
    insert(std::begin(doc.indexed), std::end(doc.indexed),
           std::begin(doc.stored), std::end(doc.stored),
           doc.sorted.get());
  }

  void sort(const irs::comparer& comparator);

  void clear() noexcept {
    fields_.clear();
    count_ = 0;
  }

 private:
  index_segment(const index_segment& rhs) noexcept = delete;
  index_segment& operator=(const index_segment& rhs) noexcept = delete;

  irs::doc_id_t doc() const noexcept {
    return count_ + irs::doc_limits::min();
  }

  void insert_indexed(const ifield& field);
  void insert_stored(const ifield& field);
  void insert_sorted(const ifield& field);
  void compute_features();

  irs::field_features_t field_features_;
  columns_meta_t columns_meta_;
  std::vector<std::pair<irs::bstring, irs::doc_id_t>> sort_;
  std::vector<const field*> id_to_field_;
  std::set<field*> doc_fields_;
  field_map_t fields_;
  columns_t columns_;
  irs::document_mask doc_mask_;
  irs::bstring buf_;
  irs::doc_id_t count_{};
};

template<typename IndexedFieldIterator, typename StoredFieldIterator>
void index_segment::insert(
    IndexedFieldIterator indexed_begin, IndexedFieldIterator indexed_end,
    StoredFieldIterator stored_begin, StoredFieldIterator stored_end,
    const ifield* sorted /*= nullptr*/) {
  // reset field per-document state
  doc_fields_.clear();
  for (auto it = indexed_begin; it != indexed_end; ++it) {
    auto field = fields_.find(it->name());

    if (field != fields_.end()) {
      field->second.stats = {};
    }
  }

  for (; indexed_begin != indexed_end; ++indexed_begin) {
    insert_indexed(*indexed_begin);
  }

  for (; stored_begin != stored_end; ++stored_begin) {
    insert_stored(*stored_begin);
  }

  if (sorted) {
    insert_sorted(*sorted);
  }

  compute_features();

  ++count_;
}

using index_t = std::vector<index_segment>;

void assert_columnstore(
  const irs::directory& dir,
  irs::format::ptr codec,
  const index_t& expected_index,
  size_t skip = 0); // do not validate the first 'skip' segments

void assert_columnstore(
  irs::index_reader::ptr actual_index,
  const index_t& expected_index,
  size_t skip = 0); // do not validate the first 'skip' segments

void assert_index(
  irs::index_reader::ptr actual_index,
  const index_t& expected_index,
  irs::IndexFeatures features,
  size_t skip = 0, // do not validate the first 'skip' segments
  irs::automaton_table_matcher* matcher = nullptr);

void assert_index(
  const irs::directory& dir,
  irs::format::ptr codec,
  const index_t& index,
  irs::IndexFeatures features,
  size_t skip = 0, // no not validate the first 'skip' segments
  irs::automaton_table_matcher* matcher = nullptr);

} // tests

#endif
