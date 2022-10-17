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

#ifndef IRESEARCH_SEGMENT_WRITER_H
#define IRESEARCH_SEGMENT_WRITER_H

#include <absl/container/node_hash_set.h>

#include "column_info.hpp"
#include "field_data.hpp"
#include "sorted_column.hpp"
#include "analysis/token_stream.hpp"
#include "formats/formats.hpp"
#include "utils/bitvector.hpp"
#include "utils/compression.hpp"
#include "utils/directory_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/type_limits.hpp"

namespace iresearch {

class comparer;
struct segment_meta;

//////////////////////////////////////////////////////////////////////////////
/// @enum Action
/// @brief defines how the inserting field should be processed
//////////////////////////////////////////////////////////////////////////////
enum class Action {
  ////////////////////////////////////////////////////////////////////////////
  /// @brief Field should be indexed only
  /// @note Field must satisfy 'Field' concept
  ////////////////////////////////////////////////////////////////////////////
  INDEX = 1,

  ////////////////////////////////////////////////////////////////////////////
  /// @brief Field should be stored only
  /// @note Field must satisfy 'Attribute' concept
  ////////////////////////////////////////////////////////////////////////////
  STORE = 2,

  ////////////////////////////////////////////////////////////////////////////
  /// @brief Field should be stored in sorted order
  /// @note Field must satisfy 'Attribute' concept
  ////////////////////////////////////////////////////////////////////////////
  STORE_SORTED = 4
}; // Action

ENABLE_BITMASK_ENUM(Action);

////////////////////////////////////////////////////////////////////////////////
/// @brief interface for an index writer over a directory
///        an object that represents a single ongoing transaction
///        non-thread safe
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API segment_writer : util::noncopyable {
 public:
  struct update_context {
    size_t generation;
    size_t update_id;
  };

  using update_contexts = std::vector<update_context>;

  //////////////////////////////////////////////////////////////////////////////
  /// @class document
  /// @brief Facade for the insertion logic
  //////////////////////////////////////////////////////////////////////////////
  class document: private util::noncopyable {
   public:
    ////////////////////////////////////////////////////////////////////////////
    /// @brief constructor
    ////////////////////////////////////////////////////////////////////////////
    // cppcheck-suppress constParameter
    explicit document(segment_writer& writer) noexcept: writer_(writer) {}

    ////////////////////////////////////////////////////////////////////////////
    /// @return current state of the object
    /// @note if the object is in an invalid state all further operations will
    ///       not take any effect
    ////////////////////////////////////////////////////////////////////////////
    explicit operator bool() const noexcept { return writer_.valid(); }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief inserts the specified field into the document according to the
    ///        specified ACTION
    /// @note 'Field' type type must satisfy the Field concept
    /// @param field attribute to be inserted
    /// @return true, if field was successfully insterted
    ////////////////////////////////////////////////////////////////////////////
    template<Action action, typename Field>
    bool insert(Field&& field) const {
      return writer_.insert<action>(std::forward<Field>(field));
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief inserts the specified field (denoted by the pointer) into the
    ///        document according to the specified ACTION
    /// @note 'Field' type type must satisfy the Field concept
    /// @note pointer must not be nullptr
    /// @param field attribute to be inserted
    /// @return true, if field was successfully insterted
    ////////////////////////////////////////////////////////////////////////////
    template<Action action, typename Field>
    bool insert(Field* field) const {
      return writer_.insert<action>(*field);
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief inserts the specified range of fields, denoted by the [begin;end)
    ///        into the document according to the specified ACTION
    /// @note 'Iterator' underline value type must satisfy the Field concept
    /// @param begin the beginning of the fields range
    /// @param end the end of the fields range
    /// @return true, if the range was successfully insterted
    ////////////////////////////////////////////////////////////////////////////
    template<Action action, typename Iterator>
    bool insert(Iterator begin, Iterator end) const {
      for (; writer_.valid() && begin != end; ++begin) {
        insert<action>(*begin);
      }

      return writer_.valid();
    }

   private:
    segment_writer& writer_;
  }; // document

  DECLARE_UNIQUE_PTR(segment_writer);

  static ptr make(
    directory& dir,
    const column_info_provider_t& column_info,
    const feature_info_provider_t& feature_info,
    const comparer* comparator);

  // begin document-write transaction
  // @return doc_id_t as per type_limits<type_t::doc_id_t>
  doc_id_t begin(const update_context& ctx, size_t reserve_rollback_extra = 0);

  // @param doc_id the document id as returned by begin(...)
  // @return modifiable update_context for the specified doc_id
  update_context& doc_context(doc_id_t doc_id) {
    assert(doc_limits::valid(doc_id));
    assert(doc_id - doc_limits::min() < docs_context_.size());
    return docs_context_[doc_id - doc_limits::min()];
  }

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

      assert(false); // unsupported action
      valid_ = false;
    }

    return false;
  }

  // commit document-write transaction
  void commit() {
    if (valid_) {
      finish();
    } else {
      rollback();
    }
  }

  // @return approximate amount of memory actively in-use by this instance
  size_t memory_active() const noexcept;

  // @return approximate amount of memory reserved by this instance
  size_t memory_reserved() const noexcept;

  // @param doc_id the document id as returned by begin(...)
  // @return success
  bool remove(doc_id_t doc_id);

  // rollbacks document-write transaction,
  // implicitly noexcept since we reserve memory in 'begin'
  void rollback() {
    // mark as removed since not fully inserted
    assert(docs_cached() + doc_limits::min() - 1 < doc_limits::eof()); // user should check return of begin() != eof()
    remove(doc_id_t(docs_cached() + doc_limits::min() - 1)); // -1 for 0-based offset
    valid_ = false;
  }

  void flush(index_meta::index_segment_t& segment);

  const update_contexts& docs_context() noexcept {
    return docs_context_;
  };
  const std::string& name() const noexcept { return seg_name_; }
  size_t docs_cached() const noexcept { return docs_context_.size(); }
  bool initialized() const noexcept { return initialized_; }
  bool valid() const noexcept { return valid_; }
  void reset() noexcept;
  void reset(const segment_meta& meta);

  void tick(uint64_t tick) noexcept { tick_ = tick; }
  uint64_t tick() const noexcept { return tick_; }

 private:
  struct stored_column : util::noncopyable {
    struct hash {
      using is_transparent = void;

      size_t operator()(const hashed_string_ref& value) const noexcept {
        return value.hash();
      }

      size_t operator()(const stored_column& value) const noexcept {
        return value.name_hash;
      }
    };

    struct eq {
      using is_transparent = void;

      bool operator()(const stored_column& lhs, const stored_column& rhs) const noexcept {
        return lhs.name == rhs.name;
      }

      bool operator()(const stored_column& lhs, const hashed_string_ref& rhs) const noexcept {
        return lhs.name == rhs;
      }

      bool operator()(const hashed_string_ref& lhs, const stored_column& rhs) const noexcept {
        return this->operator()(rhs, lhs);
      }
    };

    stored_column(
      const hashed_string_ref& name,
      columnstore_writer& columnstore,
      const column_info_provider_t& column_info,
      std::deque<cached_column>& cached_columns,
      bool cache);

    std::string name;
    size_t name_hash;
    columnstore_writer::values_writer_f writer;
    mutable field_id id{ field_limits::invalid() };
  }; // stored_column

  // FIXME consider refactor this
  // we can't use flat_hash_set as stored_column stores 'this' in non-cached case
  using stored_columns = absl::node_hash_set<
    stored_column,
    stored_column::hash,
    stored_column::eq>;

  struct sorted_column : util::noncopyable {
    explicit sorted_column(
        const column_info_provider_t& column_info,
        columnstore_writer::column_finalizer_f finalizer) noexcept
      : stream(column_info(string_ref::NIL)), // get compression for sorted column
        finalizer{std::move(finalizer)} {
    }

    field_id id{ field_limits::invalid() };
    irs::sorted_column stream;
    columnstore_writer::column_finalizer_f finalizer;
  }; // sorted_column

  segment_writer(
    directory& dir,
    const column_info_provider_t& column_info,
    const feature_info_provider_t& feature_info,
    const comparer* comparator) noexcept;

  bool index(
    const hashed_string_ref& name,
    const doc_id_t doc,
    IndexFeatures index_features,
    const features_t& features,
    token_stream& tokens);

  template<typename Writer>
  bool store_sorted(const doc_id_t doc, Writer& writer) {
    assert(doc < doc_limits::eof());

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
  bool store(
      const hashed_string_ref& name,
      const doc_id_t doc,
      Writer& writer) {
    assert(doc < doc_limits::eof());

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

    const auto field_name = make_hashed_ref(static_cast<const string_ref&>(field.name()));

    assert(docs_cached() + doc_limits::min() - 1 < doc_limits::eof()); // user should check return of begin() != eof()
    const auto doc_id = doc_id_t(docs_cached() + doc_limits::min() - 1); // -1 for 0-based offset

    return store(field_name, doc_id, field);
  }

  template<typename Field>
  bool store_sorted(Field&& field) {
    REGISTER_TIMER_DETAILED();

    assert(docs_cached() + doc_limits::min() - 1 < doc_limits::eof()); // user should check return of begin() != eof()
    const auto doc_id = doc_id_t(docs_cached() + doc_limits::min() - 1); // -1 for 0-based offset

    return store_sorted(doc_id, field);
  }

  template<typename Field>
  bool index(Field&& field) {
    REGISTER_TIMER_DETAILED();

    const auto field_name = make_hashed_ref(static_cast<const string_ref&>(field.name()));

    auto& tokens = static_cast<token_stream&>(field.get_tokens());
    const auto& features = static_cast<const features_t&>(field.features());
    const IndexFeatures index_features = field.index_features();

    assert(docs_cached() + doc_limits::min() - 1 < doc_limits::eof()); // user should check return of begin() != eof()
    const auto doc_id = doc_id_t(docs_cached() + doc_limits::min() - 1); // -1 for 0-based offset

    return index(field_name, doc_id, index_features, features, tokens);
  }

  template<bool Sorted, typename Field>
  bool index_and_store(Field&& field) {
    REGISTER_TIMER_DETAILED();

    const auto field_name = make_hashed_ref(static_cast<const string_ref&>(field.name()));

    auto& tokens = static_cast<token_stream&>(field.get_tokens());
    const auto& features = static_cast<const features_t&>(field.features());
    const IndexFeatures index_features = field.index_features();

    assert(docs_cached() + doc_limits::min() - 1 < doc_limits::eof()); // user should check return of begin() != eof()
    const auto doc_id = doc_id_t(docs_cached() + doc_limits::min() - 1); // -1 for 0-based offset

    if (IRS_UNLIKELY(!index(field_name, doc_id, index_features, features, tokens))) {
      return false; // indexing failed
    }

    if constexpr (Sorted) {
      return store_sorted(doc_id, field);
    }

    return store(field_name, doc_id, field);
  }

  // returns stream for storing attributes in sorted order
  column_output& sorted_stream(const doc_id_t doc_id) {
    sort_.stream.prepare(doc_id);
    return sort_.stream;
  }

  // returns stream for storing attributes
  column_output& stream(
    const hashed_string_ref& name,
    const doc_id_t doc);

  // finishes document
  void finish() {
    REGISTER_TIMER_DETAILED();
    for (const auto* field : doc_) {
      field->compute_features();
    }
  }

  size_t flush_doc_mask(const segment_meta& meta); // flushes document mask to directory, returns number of masked documens
  void flush_fields(const doc_map& docmap); // flushes indexed fields to directory

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::deque<cached_column> cached_columns_; // pointers remain valid
  sorted_column sort_;
  update_contexts docs_context_;
  bitvector docs_mask_; // invalid/removed doc_ids (e.g. partially indexed due to indexing failure)
  fields_data fields_;
  stored_columns columns_;
  std::vector<const stored_column*> sorted_columns_;
  std::vector<const field_data*> doc_; // document fields
  std::string seg_name_;
  field_writer::ptr field_writer_;
  const column_info_provider_t* column_info_;
  columnstore_writer::ptr col_writer_;
  tracking_directory dir_;
  uint64_t tick_{0};
  bool initialized_;
  bool valid_{ true }; // current state
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // segment_writer

}

// segment_writer::ptr
MSVC_ONLY(template class IRESEARCH_API std::unique_ptr<irs::segment_writer>;) // cppcheck-suppress unknownMacro 

#endif // IRESEARCH_SEGMENT_WRITER_H
