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

#pragma once

#include <deque>
#include <vector>

#include "formats/formats.hpp"
#include "index/buffered_column.hpp"
#include "index/field_meta.hpp"
#include "index/index_features.hpp"
#include "index/iterators.hpp"
#include "index/postings.hpp"
#include "utils/block_pool.hpp"
#include "utils/hash_set_utils.hpp"
#include "utils/memory.hpp"
#include "utils/noncopyable.hpp"

namespace irs {

struct field_writer;
class token_stream;
struct offset;
struct payload;
class format;
struct directory;
class Comparer;
struct flush_state;

namespace analysis {
class analyzer;
}

using int_block_pool = block_pool<size_t, 8192, ManagedTypedAllocator<size_t>>;

namespace detail {
class term_iterator;
class doc_iterator;
class sorting_doc_iterator;
}  // namespace detail

// Represents a mapping between cached column data
// and a pointer to column identifier
class cached_column final : public column_reader {
 public:
  cached_column(field_id* id, ColumnInfo info,
                columnstore_writer::column_finalizer_f finalizer,
                IResourceManager& rm) noexcept
    : id_{id}, stream_{info, rm}, finalizer_{std::move(finalizer)} {}

  BufferedColumn& Stream() noexcept { return stream_; }
  const BufferedColumn& Stream() const noexcept { return stream_; }

  void Flush(columnstore_writer& writer, DocMapView docmap,
             BufferedColumn::BufferedValues& buffer) {
    auto finalizer = [this, finalizer = std::move(finalizer_)](
                       bstring& out) mutable -> std::string_view {
      name_ = finalizer(payload_);
      out = payload_;  // FIXME(gnusi): avoid double-copy
      return name_;
    };

    *id_ = stream_.Flush(writer, std::move(finalizer), docmap, buffer);
  }

  field_id id() const noexcept final { return *id_; }
  std::string_view name() const noexcept final { return name_; }
  bytes_view payload() const noexcept final { return payload_; }

  doc_iterator::ptr iterator(ColumnHint hint) const final;

  doc_id_t size() const final {
    IRS_ASSERT(stream_.Size() < doc_limits::eof());
    return static_cast<doc_id_t>(stream_.Size());
  }

 private:
  field_id* id_;
  std::string_view name_;
  bstring payload_;
  BufferedColumn stream_;
  columnstore_writer::column_finalizer_f finalizer_;
};

class field_data : util::noncopyable {
 public:
  field_data(std::string_view name, const features_t& features,
             const FeatureInfoProvider& feature_columns,
             std::deque<cached_column, ManagedTypedAllocator<cached_column>>&
               cached_columns,
             const feature_set_t& cached_features, columnstore_writer& columns,
             byte_block_pool::inserter& byte_writer,
             int_block_pool::inserter& int_writer, IndexFeatures index_features,
             bool random_access);

  doc_id_t doc() const noexcept { return last_doc_; }

  const field_meta& meta() const noexcept { return meta_; }

  // returns false if field contains indexed data
  bool empty() const noexcept { return !doc_limits::valid(last_doc_); }

  bool invert(token_stream& tokens, doc_id_t id);

  const field_stats& stats() const noexcept { return stats_; }

  bool seen() const noexcept { return seen_; }
  void seen(bool value) noexcept { seen_ = value; }

  IndexFeatures requested_features() const noexcept {
    return requested_features_;
  }

  void compute_features() const {
    for (auto& entry : features_) {
      IRS_ASSERT(entry.handler);
      entry.handler->write(stats_, doc(), *entry.writer);
    }
  }

  bool has_features() const noexcept { return !features_.empty(); }

 private:
  friend class detail::term_iterator;
  friend class detail::doc_iterator;
  friend class detail::sorting_doc_iterator;
  friend class fields_data;

  struct feature_info {
    feature_info(FeatureWriter::ptr handler, column_output& writer)
      : handler{std::move(handler)}, writer{&writer} {}

    FeatureWriter::ptr handler;
    column_output* writer;
  };

  using process_term_f = void (field_data::*)(posting&, doc_id_t,
                                              const payload*, const offset*);

  void reset(doc_id_t doc_id);

  void new_term(posting& p, doc_id_t did, const payload* pay,
                const offset* offs);
  void add_term(posting& p, doc_id_t did, const payload* pay,
                const offset* offs);

  void new_term_random_access(posting& p, doc_id_t did, const payload* pay,
                              const offset* offs);
  void add_term_random_access(posting& p, doc_id_t did, const payload* pay,
                              const offset* offs);

  static constexpr process_term_f kTermProcessingTables[2][2] = {
    // sequential access: [0] - new term, [1] - add term
    {&field_data::add_term, &field_data::new_term},
    // random access: [0] - new term, [1] - add term
    {&field_data::add_term_random_access, &field_data::new_term_random_access}};

  bool prox_random_access() const noexcept {
    return kTermProcessingTables[1] == proc_table_;
  }

  mutable std::vector<feature_info> features_;
  mutable field_meta meta_;
  postings terms_;
  byte_block_pool::inserter* byte_writer_;
  int_block_pool::inserter* int_writer_;
  const process_term_f* proc_table_;
  field_stats stats_;
  IndexFeatures requested_features_{};
  doc_id_t last_doc_{doc_limits::invalid()};
  uint32_t pos_;
  uint32_t last_pos_;
  uint32_t offs_;
  uint32_t last_start_offs_;
  bool seen_{false};
};

class fields_data : util::noncopyable {
 private:
  struct FieldEq : ValueRefEq<field_data*> {
    using is_transparent = void;
    using Self::operator();

    bool operator()(const Ref& lhs,
                    const hashed_string_view& rhs) const noexcept {
      return lhs.ref->meta().name == rhs;
    }

    bool operator()(const hashed_string_view& lhs,
                    const Ref& rhs) const noexcept {
      return this->operator()(rhs, lhs);
    }
  };

  using fields_map = flat_hash_set<FieldEq>;

 public:
  using postings_ref_t = std::vector<const posting*>;

  explicit fields_data(
    const FeatureInfoProvider& feature_info,
    std::deque<cached_column, ManagedTypedAllocator<cached_column>>&
      cached_columns,
    const feature_set_t& cached_features, IResourceManager& rm,
    const Comparer* comparator);

  const Comparer* comparator() const noexcept { return comparator_; }

  field_data* emplace(const hashed_string_view& name,
                      IndexFeatures index_features, const features_t& features,
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
  const Comparer* comparator_;
  const FeatureInfoProvider* feature_info_;
  std::deque<field_data, ManagedTypedAllocator<field_data>>
    fields_;  // pointers remain valid
  std::deque<cached_column, ManagedTypedAllocator<cached_column>>*
    cached_columns_;  // pointers remain valid
  const feature_set_t* cached_features_;
  fields_map fields_map_;
  postings_ref_t sorted_postings_;
  std::vector<const field_data*> sorted_fields_;
  byte_block_pool byte_pool_;
  byte_block_pool::inserter byte_writer_;
  int_block_pool int_pool_;  // FIXME why don't to use std::vector<size_t>?
  int_block_pool::inserter int_writer_;
};

}  // namespace irs
