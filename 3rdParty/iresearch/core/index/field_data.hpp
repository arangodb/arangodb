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

#include "field_meta.hpp"
#include "postings.hpp"
#include "formats/formats.hpp"

#include "index/iterators.hpp"

#include "utils/block_pool.hpp"
#include "utils/memory.hpp"
#include "utils/noncopyable.hpp"

#include <vector>
#include <tuple>
#include <unordered_map>

namespace iresearch {

struct field_writer;
class token_stream;
class analyzer;
struct offset;
struct payload;
class format;
struct directory;
class comparer;
struct flush_state;

typedef block_pool<size_t, 8192> int_block_pool;

namespace detail {
class term_iterator;
class doc_iterator;
class sorting_doc_iterator;
}

IRESEARCH_API bool memcmp_less(
  const byte_type* lhs,
  size_t lhs_size,
  const byte_type* rhs,
  size_t rhs_size
) noexcept;

inline bool memcmp_less(
    const bytes_ref& lhs,
    const bytes_ref& rhs
) noexcept {
  return memcmp_less(lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size());
}

class IRESEARCH_API field_data : util::noncopyable {
 public:
  field_data(
    const string_ref& name,
    byte_block_pool::inserter& byte_writer,
    int_block_pool::inserter& int_writer,
    bool random_access
  );

  doc_id_t doc() const noexcept { return last_doc_; }

  // returns number of terms in a field within a document
  size_t size() const noexcept { return len_; }

  const field_meta& meta() const noexcept { return meta_; }

  data_output& norms(columnstore_writer& writer);

  // returns false if field contains indexed data
  bool empty() const noexcept {
    return !doc_limits::valid(last_doc_);
  }

  bool invert(token_stream& tokens, const flags& features, doc_id_t id);

  bool prox_random_access() const noexcept {
    return TERM_PROCESSING_TABLES[1] == proc_table_;
  }

 private:
  friend class detail::term_iterator;
  friend class detail::doc_iterator;
  friend class detail::sorting_doc_iterator;
  friend class fields_data;

  typedef void(field_data::*process_term_f)(posting&, doc_id_t, const payload*, const offset*);

  static const process_term_f TERM_PROCESSING_TABLES[2][2];

  void reset(doc_id_t doc_id);

  void new_term(posting& p, doc_id_t did, const payload* pay, const offset* offs);
  void add_term(posting& p, doc_id_t did, const payload* pay, const offset* offs);

  void new_term_random_access(posting& p, doc_id_t did, const payload* pay, const offset* offs);
  void add_term_random_access(posting& p, doc_id_t did, const payload* pay, const offset* offs);

  columnstore_writer::values_writer_f norms_;
  field_meta meta_;
  postings terms_;
  byte_block_pool::inserter* byte_writer_;
  int_block_pool::inserter* int_writer_;
  const process_term_f* proc_table_;
  doc_id_t last_doc_{ type_limits<type_t::doc_id_t>::invalid() };
  uint32_t pos_;
  uint32_t last_pos_;
  uint32_t len_; // total number of terms
  uint32_t num_overlap_; // number of overlapped terms
  uint32_t offs_;
  uint32_t last_start_offs_;
  uint32_t max_term_freq_; // maximum number of terms in a field across all indexed documents 
  uint32_t unq_term_cnt_;
};

class IRESEARCH_API fields_data: util::noncopyable {
 public:
  typedef std::unordered_map<hashed_string_ref, field_data> fields_map;

  explicit fields_data(const comparer* comparator);

  const comparer* comparator() const noexcept {
    return comparator_;
  }

  field_data& emplace(const hashed_string_ref& name);

  //////////////////////////////////////////////////////////////////////////////
  /// @return approximate amount of memory actively in-use by this instance
  //////////////////////////////////////////////////////////////////////////////
  size_t memory_active() const noexcept {
    return byte_writer_.pool_offset()
      + int_writer_.pool_offset() * sizeof(int_block_pool::value_type)
      + fields_.size() * sizeof(fields_map::value_type);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return approximate amount of memory reserved by this instance
  //////////////////////////////////////////////////////////////////////////////
  size_t memory_reserved() const noexcept {
    return sizeof(fields_data) + byte_pool_.size() + int_pool_.size();
  }

  size_t size() const { return fields_.size(); }
  fields_data& operator+=(const flags& features) {
    features_ |= features;
    return *this;
  }
  const flags& features() { return features_; }
  void flush(field_writer& fw, flush_state& state);
  void reset() noexcept;

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  const comparer* comparator_;
  fields_map fields_;
  byte_block_pool byte_pool_;
  byte_block_pool::inserter byte_writer_;
  int_block_pool int_pool_; // FIXME why don't to use std::vector<size_t>?
  int_block_pool::inserter int_writer_;
  flags features_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

}

#endif
