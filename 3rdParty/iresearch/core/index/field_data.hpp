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

NS_ROOT

struct field_writer;
class token_stream;
class analyzer;
struct offset;
struct payload;
class format;
struct directory;

typedef block_pool<size_t, 8192> int_block_pool;

NS_BEGIN( detail ) 
class term_iterator;
class doc_iterator;
NS_END

class IRESEARCH_API field_data : util::noncopyable {
 public:
  field_data(
    const string_ref& name,
    byte_block_pool::inserter* byte_writer,
    int_block_pool::inserter* int_writer
  );

  ~field_data();

  doc_id_t doc() const { return last_doc_; }

  // returns number of terms in a field within a document
  size_t size() const { return len_; }

  const field_meta& meta() const { return meta_; }

  data_output& norms(columnstore_writer& writer);

  // returns false if field contains indexed data
  bool empty() const {
    return !type_limits<type_t::doc_id_t>::valid(last_doc_);
  }

  bool invert(token_stream& tokens, const flags& features, doc_id_t id);

 private:
  friend class detail::term_iterator;
  friend class detail::doc_iterator;
  friend class fields_data;

  void init(doc_id_t doc_id);

  void new_term(posting& p, doc_id_t did, const payload* pay, const offset* offs);

  void add_term(posting& p, doc_id_t did, const payload* pay, const offset* offs);

  void write_prox(posting& p, int_block_pool::iterator& where,
                  uint32_t prox, const payload* pay);

  void write_offset(posting& p, int_block_pool::iterator& where,
                    const offset* offs);

  columnstore_writer::values_writer_f norms_;
  field_meta meta_;
  postings terms_;
  byte_block_pool::inserter* byte_writer_;
  int_block_pool::inserter* int_writer_;
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

struct flush_state;

class IRESEARCH_API fields_data: util::noncopyable {
 public:
  typedef std::unordered_map<hashed_string_ref, field_data> fields_map;

  fields_data();

  field_data& get(const hashed_string_ref& name);

  //////////////////////////////////////////////////////////////////////////////
  /// @return approximate amount of memory actively in-use by this instance
  //////////////////////////////////////////////////////////////////////////////
  size_t memory_active() const NOEXCEPT {
    return byte_writer_.pool_offset()
      + int_writer_.pool_offset() * sizeof(int_block_pool::value_type)
      + fields_.size() * sizeof(fields_map::value_type);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @return approximate amount of memory reserved by this instance
  //////////////////////////////////////////////////////////////////////////////
  size_t memory_reserved() const NOEXCEPT {
    return sizeof(fields_data) + byte_pool_.size() + int_pool_.size();
  }

  size_t size() const { return fields_.size(); }
  fields_data& operator+=(const flags& features) {
    features_ |= features;
    return *this;
  }
  const flags& features() { return features_; }
  void flush(field_writer& fw, flush_state& state);
  void reset() NOEXCEPT;

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  fields_map fields_;
  byte_block_pool byte_pool_;
  byte_block_pool::inserter byte_writer_;
  int_block_pool int_pool_;
  int_block_pool::inserter int_writer_;
  flags features_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

NS_END

#endif
