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

#ifndef IRESEARCH_FIELD_DATA_H
#define IRESEARCH_FIELD_DATA_H

#include <vector>

#include "formats/formats.hpp"
#include "index/field_meta.hpp"
#include "index/postings.hpp"
#include "index/sorted_column.hpp"
#include "index/index_features.hpp"
#include "index/iterators.hpp"
#include "utils/block_pool.hpp"
#include "utils/memory.hpp"
#include "utils/noncopyable.hpp"
#include "utils/hash_set_utils.hpp"

namespace iresearch {

struct field_writer;
class token_stream;
struct offset;
struct payload;
class format;
struct directory;
class comparer;
struct flush_state;

namespace analysis {
class analyzer;
}

typedef block_pool<size_t, 8192> int_block_pool;

namespace detail {
class term_iterator;
class doc_iterator;
class sorting_doc_iterator;
}

// represents a mapping between cached column data
// and a pointer to column identifier
struct cached_column {
  cached_column(field_id* id, column_info info) noexcept
    : id{id}, stream{info} {
  }

  field_id* id;
  sorted_column stream;
};

class IRESEARCH_API field_data : util::noncopyable {
 public:
  field_data(
    const string_ref& name,
    const features_t& features,
    const field_features_t& field_features,
    const feature_column_info_provider_t& feature_columns,
    std::deque<cached_column>& cached_columns,
    columnstore_writer& columns,
    byte_block_pool::inserter& byte_writer,
    int_block_pool::inserter& int_writer,
    IndexFeatures index_features,
    bool random_access);

  doc_id_t doc() const noexcept { return last_doc_; }

  const field_meta& meta() const noexcept { return meta_; }

  // returns false if field contains indexed data
  bool empty() const noexcept {
    return !doc_limits::valid(last_doc_);
  }

  bool invert(token_stream& tokens, doc_id_t id);

  const field_stats& stats() const noexcept { return stats_; }

  bool seen() const noexcept { return seen_; }
  void seen(bool value) noexcept { seen_ = value; }

  void compute_features() const {
    for (auto& entry : features_) {
      entry.handler(stats_, doc(), entry.writer);
    }
  }

  bool has_features() const noexcept {
    return !features_.empty();
  }

 private:
  friend class detail::term_iterator;
  friend class detail::doc_iterator;
  friend class detail::sorting_doc_iterator;
  friend class fields_data;

  struct feature_info {
    feature_info(
        feature_handler_f handler,
        columnstore_writer::values_writer_f writer)
      : handler{handler},
        writer{std::move(writer)} {
    }

    feature_handler_f handler;
    columnstore_writer::values_writer_f writer;
  };

  using process_term_f = void(field_data::*)(
    posting&, doc_id_t,
    const payload*, const offset*);

  static const process_term_f TERM_PROCESSING_TABLES[2][2];

  void reset(doc_id_t doc_id);

  void new_term(posting& p, doc_id_t did, const payload* pay, const offset* offs);
  void add_term(posting& p, doc_id_t did, const payload* pay, const offset* offs);

  void new_term_random_access(posting& p, doc_id_t did, const payload* pay, const offset* offs);
  void add_term_random_access(posting& p, doc_id_t did, const payload* pay, const offset* offs);

  bool prox_random_access() const noexcept {
    return TERM_PROCESSING_TABLES[1] == proc_table_;
  }

  mutable std::vector<feature_info> features_;
  mutable field_meta meta_;
  postings terms_;
  byte_block_pool::inserter* byte_writer_;
  int_block_pool::inserter* int_writer_;
  const process_term_f* proc_table_;
  field_stats stats_;
  doc_id_t last_doc_{ doc_limits::invalid() };
  uint32_t pos_;
  uint32_t last_pos_;
  uint32_t offs_;
  uint32_t last_start_offs_;
  bool seen_{false};
}; // field_data

class IRESEARCH_API fields_data: util::noncopyable {
 private:
  struct field_ref_eq : value_ref_eq<field_data*> {
    using self_t::operator();

    bool operator()(const ref_t& lhs, const hashed_string_ref& rhs) const noexcept {
      return lhs.second->meta().name == rhs;
    }

    bool operator()(const hashed_string_ref& lhs, const ref_t& rhs) const noexcept {
      return this->operator()(rhs, lhs);
    }
  };

  using fields_map = flat_hash_set<field_ref_eq>;

 public:
  using postings_ref_t = std::vector<const posting*>;

  explicit fields_data(
    const field_features_t& field_features,
    const feature_column_info_provider_t& feature_columns,
    std::deque<cached_column>& cached_features,
    const comparer* comparator);

  const comparer* comparator() const noexcept {
    return comparator_;
  }

  field_data* emplace(const hashed_string_ref& name,
                      IndexFeatures index_features,
                      const features_t& features,
                      columnstore_writer& columns);

  //////////////////////////////////////////////////////////////////////////////
  /// @return approximate amount of memory actively in-use by this instance
  //////////////////////////////////////////////////////////////////////////////
  size_t memory_active() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @return approximate amount of memory reserved by this instance
  //////////////////////////////////////////////////////////////////////////////
  size_t memory_reserved() const noexcept;

  size_t size() const { return fields_.size(); }
  void flush(field_writer& fw, flush_state& state);
  void reset() noexcept;

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  const comparer* comparator_;
  const field_features_t* field_features_;
  const feature_column_info_provider_t* feature_columns_;
  std::deque<field_data> fields_; // pointers remain valid
  std::deque<cached_column>* cached_features_; // pointers remain valid
  fields_map fields_map_;
  postings_ref_t sorted_postings_;
  std::vector<const field_data*> sorted_fields_;
  byte_block_pool byte_pool_;
  byte_block_pool::inserter byte_writer_;
  int_block_pool int_pool_; // FIXME why don't to use std::vector<size_t>?
  int_block_pool::inserter int_writer_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // fields_data

} // iresearch

#endif
