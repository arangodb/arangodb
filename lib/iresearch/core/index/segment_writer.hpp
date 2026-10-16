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

#include "analysis/token_stream.hpp"
#include "index/buffered_column.hpp"
#include "index/column_info.hpp"
#include "index/field_data.hpp"
#include "index/index_reader.hpp"
#include "utils/bitset.hpp"
#include "utils/compression.hpp"
#include "utils/directory_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/timer_utils.hpp"
#include "utils/type_limits.hpp"

#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_set.h>

namespace irs {

class Comparer;
struct SegmentMeta;

// Defines how the inserting field should be processed
enum class Action {
  // Field should be indexed only
  // Field must satisfy 'Field' concept
  INDEX = 1,

  // Field should be stored only
  // Field must satisfy 'Attribute' concept
  STORE = 2,

  // Field should be stored in sorted order
  // Field must satisfy 'Attribute' concept
  STORE_SORTED = 4
};

ENABLE_BITMASK_ENUM(Action);

struct DocsMask final {
  ManagedBitset set;
  uint32_t count{0};
};

// Interface for an index writer over a directory
// an object that represents a single ongoing transaction
// non-thread safe
class segment_writer : public ColumnProvider, util::noncopyable {
 private:
  // Disallow using public constructor
  struct ConstructToken final {
    explicit ConstructToken() = default;
  };

 public:
  struct DocContext final {
    uint64_t tick{0};
    size_t query_id{writer_limits::kInvalidOffset};
  };

  static std::unique_ptr<segment_writer> make(
    directory& dir, const SegmentWriterOptions& options);

  // begin document-write transaction
  // Return doc_id_t as per doc_limits
  doc_id_t begin(DocContext ctx);

  template<Action action, typename Field>
  bool insert(Field&& field) {
    if (IRS_LIKELY(valid_)) {
      if constexpr (Action::INDEX == action) {
        return index(std::forward<Field>(field));
      }

      if constexpr (Action::STORE == action) {
        return store(std::forward<Field>(field));
      }

      if constexpr (Action::STORE_SORTED == action) {
        return store_sorted(std::forward<Field>(field));
      }

      if constexpr ((Action::INDEX | Action::STORE) == action) {
        return index_and_store<false>(std::forward<Field>(field));
      }

      if constexpr ((Action::INDEX | Action::STORE_SORTED) == action) {
        return index_and_store<true>(std::forward<Field>(field));
      }

      IRS_ASSERT(false);  // unsupported action
      valid_ = false;
    }

    return false;
  }

  // Commit document-write transaction
  void commit() {
    if (valid_) {
      finish();
    } else {
      rollback();
    }
  }

  // Return approximate amount of memory actively in-use by this instance
  size_t memory_active() const noexcept;

  // Return approximate amount of memory reserved by this instance
  size_t memory_reserved() const noexcept;

  // doc_id the document id as returned by begin(...)
  // Return success
  bool remove(doc_id_t doc_id) noexcept;

  // Rollback document-write transaction,
  // implicitly noexcept since we reserve memory in 'begin'
  void rollback() noexcept {
    // mark as removed since not fully inserted
    remove(LastDocId());
    valid_ = false;
  }

  std::span<DocContext> docs_context() noexcept { return docs_context_; }

  [[nodiscard]] DocMap flush(IndexSegment& segment, DocsMask& docs_mask);

  const std::string& name() const noexcept { return seg_name_; }
  size_t buffered_docs() const noexcept { return docs_context_.size(); }
  bool initialized() const noexcept { return initialized_; }
  bool valid() const noexcept { return valid_; }
  void reset() noexcept;
  void reset(const SegmentMeta& meta);

  doc_id_t LastDocId() const noexcept {
    IRS_ASSERT(buffered_docs() <= doc_limits::eof());
    return doc_limits::min() + static_cast<doc_id_t>(buffered_docs()) - 1;
  }

  segment_writer(ConstructToken, directory& dir,
                 const SegmentWriterOptions& options) noexcept;

  const column_reader* column(field_id id) const final {
    const auto it = column_ids_.find(id);
    if (it != column_ids_.end()) {
      return it->second;
    }
    return nullptr;
  }

  const column_reader* column(std::string_view name) const final {
    const auto it = columns_.find(hashed_string_view{name});
    if (it != columns_.end()) {
      return it->cached;
    }
    return nullptr;
  }

 private:
  struct stored_column : util::noncopyable {
    struct hash {
      using is_transparent = void;

      size_t operator()(const hashed_string_view& value) const noexcept {
        return value.hash();
      }

      size_t operator()(const stored_column& value) const noexcept {
        return value.name_hash;
      }
    };

    struct eq {
      using is_transparent = void;

      bool operator()(const stored_column& lhs,
                      const stored_column& rhs) const noexcept {
        return lhs.name == rhs.name;
      }

      bool operator()(const stored_column& lhs,
                      const hashed_string_view& rhs) const noexcept {
        return lhs.name == rhs;
      }

      bool operator()(const hashed_string_view& lhs,
                      const stored_column& rhs) const noexcept {
        return this->operator()(rhs, lhs);
      }
    };

    stored_column(
      const hashed_string_view& name, columnstore_writer& columnstore,
      IResourceManager& rm, const ColumnInfoProvider& column_info,
      std::deque<cached_column, ManagedTypedAllocator<cached_column>>&
        cached_columns,
      bool cache);

    std::string name;
    size_t name_hash;
    column_output* writer{};
    cached_column* cached{};
    mutable field_id id{field_limits::invalid()};
  };

  // FIXME consider refactor this
  // we can't use flat_hash_set as stored_column stores 'this' in non-cached
  // case
  using stored_columns =
    absl::node_hash_set<stored_column, stored_column::hash, stored_column::eq,
                        ManagedTypedAllocator<stored_column>>;

  struct sorted_column : util::noncopyable {
    explicit sorted_column(const ColumnInfoProvider& column_info,
                           columnstore_writer::column_finalizer_f finalizer,
                           IResourceManager& rm) noexcept
      : stream{column_info({}), rm},  // compression for sorted column
        finalizer{std::move(finalizer)} {}

    field_id id{field_limits::invalid()};
    irs::BufferedColumn stream;
    columnstore_writer::column_finalizer_f finalizer;
  };

  bool index(const hashed_string_view& name, const doc_id_t doc,
             IndexFeatures index_features, const features_t& features,
             token_stream& tokens);

  template<typename Writer>
  bool store_sorted(const doc_id_t doc, Writer& writer) {
    IRS_ASSERT(doc < doc_limits::eof());

    if (IRS_UNLIKELY(!fields_.comparator())) {
      // can't store sorted field without a comparator
      valid_ = false;
      return false;
    }

    auto& out = sorted_stream(doc);

    if (IRS_LIKELY(writer.write(out))) {
      return true;
    }

    out.reset();

    valid_ = false;
    return false;
  }

  template<typename Writer>
  bool store(const hashed_string_view& name, const doc_id_t doc,
             Writer& writer) {
    IRS_ASSERT(doc < doc_limits::eof());

    auto& out = stream(name, doc);

    if (IRS_LIKELY(writer.write(out))) {
      return true;
    }

    out.reset();

    valid_ = false;
    return false;
  }

  template<typename Field>
  bool store(Field&& field) {
    REGISTER_TIMER_DETAILED();

    const hashed_string_view field_name{
      static_cast<std::string_view>(field.name())};

    // user should check return of begin() != eof()
    IRS_ASSERT(LastDocId() < doc_limits::eof());
    const auto doc_id = LastDocId();

    return store(field_name, doc_id, field);
  }

  template<typename Field>
  bool store_sorted(Field&& field) {
    REGISTER_TIMER_DETAILED();

    // user should check return of begin() != eof()
    IRS_ASSERT(LastDocId() < doc_limits::eof());
    const auto doc_id = LastDocId();

    return store_sorted(doc_id, field);
  }

  template<typename Field>
  bool index(Field&& field) {
    REGISTER_TIMER_DETAILED();

    const hashed_string_view field_name{
      static_cast<std::string_view>(field.name())};

    auto& tokens = static_cast<token_stream&>(field.get_tokens());
    const auto& features = static_cast<const features_t&>(field.features());
    const IndexFeatures index_features = field.index_features();

    // user should check return of begin() != eof()
    IRS_ASSERT(LastDocId() < doc_limits::eof());
    const auto doc_id = LastDocId();

    return index(field_name, doc_id, index_features, features, tokens);
  }

  template<bool Sorted, typename Field>
  bool index_and_store(Field&& field) {
    REGISTER_TIMER_DETAILED();

    const hashed_string_view field_name{
      static_cast<std::string_view>(field.name())};

    auto& tokens = static_cast<token_stream&>(field.get_tokens());
    const auto& features = static_cast<const features_t&>(field.features());
    const IndexFeatures index_features = field.index_features();

    // user should check return of begin() != eof()
    IRS_ASSERT(LastDocId() < doc_limits::eof());
    const auto doc_id = LastDocId();

    if (IRS_UNLIKELY(
          !index(field_name, doc_id, index_features, features, tokens))) {
      return false;  // indexing failed
    }

    if constexpr (Sorted) {
      return store_sorted(doc_id, field);
    }

    return store(field_name, doc_id, field);
  }

  // Returns stream for storing attributes in sorted order
  column_output& sorted_stream(const doc_id_t doc_id) {
    sort_.stream.Prepare(doc_id);
    return sort_.stream;
  }

  // Returns stream for storing attributes
  column_output& stream(const hashed_string_view& name, const doc_id_t doc);

  // Finishes document
  void finish() {
    REGISTER_TIMER_DETAILED();
    for (const auto* field : doc_) {
      field->compute_features();
    }
  }

  // Flushes indexed fields to directory
  void FlushFields(flush_state& state);

  ScorersView scorers_;
  std::deque<cached_column, ManagedTypedAllocator<cached_column>>
    cached_columns_;  // pointers remain valid
  absl::flat_hash_map<field_id, cached_column*> column_ids_;
  sorted_column sort_;
  ManagedVector<DocContext> docs_context_;
  // invalid/removed doc_ids (e.g. partially indexed due to indexing failure)
  DocsMask docs_mask_;
  fields_data fields_;
  stored_columns columns_;
  std::vector<const field_data*> doc_;  // document fields
  std::string seg_name_;
  field_writer::ptr field_writer_;
  const ColumnInfoProvider* column_info_;
  columnstore_writer::ptr col_writer_;
  TrackingDirectory dir_;
  bool initialized_{false};
  bool valid_{true};  // current state
};

}  // namespace irs
