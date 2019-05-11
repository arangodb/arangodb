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

#ifndef IRESEARCH_TL_DOC_WRITER_H
#define IRESEARCH_TL_DOC_WRITER_H

#include "field_data.hpp"
#include "sorted_column.hpp"
#include "analysis/token_stream.hpp"
#include "formats/formats.hpp"
#include "utils/bitvector.hpp"
#include "utils/directory_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/type_limits.hpp"

NS_ROOT

class comparer;
struct segment_meta;
class segment_writer;

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
  /// @brief Field should be indexed and stored
  /// @note Field must satisfy 'Field' concept
  ////////////////////////////////////////////////////////////////////////////
  INDEX_AND_STORE = 3, // MSVC2013 doesn't support constexpr

  ////////////////////////////////////////////////////////////////////////////
  /// @brief Field should be stored in sorted order
  /// @note Field must satisfy 'Attribute' concept
  ////////////////////////////////////////////////////////////////////////////
  STORE_SORTED = 4,

  ////////////////////////////////////////////////////////////////////////////
  /// @brief Field should be indexed and stored in sorted order
  /// @note Field must satisfy 'Field' concept
  ////////////////////////////////////////////////////////////////////////////
  INDEX_AND_STORE_SORTED = 5, // MSVC2013 doesn't support constexpr
}; // Action

ENABLE_BITMASK_ENUM(Action);

template<Action action>
struct action_helper {
  template<typename Field>
  static bool insert(segment_writer& /*writer*/, Field& /*field*/) {
    // unsupported action
    assert(false);
    return false;
  }
}; // action_helper

////////////////////////////////////////////////////////////////////////////////
/// @brief interface for an index writer over a directory
///        an object that represents a single ongoing transaction
///        non-thread safe
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API segment_writer: util::noncopyable {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @class document
  /// @brief Facade for the insertion logic
  //////////////////////////////////////////////////////////////////////////////
  class document: private util::noncopyable {
   public:
    ////////////////////////////////////////////////////////////////////////////
    /// @brief constructor
    ////////////////////////////////////////////////////////////////////////////
    explicit document(segment_writer& writer) NOEXCEPT: writer_(writer) {}

    ////////////////////////////////////////////////////////////////////////////
    /// @brief destructor
    ////////////////////////////////////////////////////////////////////////////
    ~document() = default;

    ////////////////////////////////////////////////////////////////////////////
    /// @return current state of the object
    /// @note if the object is in an invalid state all further operations will
    ///       not take any effect
    ////////////////////////////////////////////////////////////////////////////
    explicit operator bool() const NOEXCEPT { return writer_.valid(); }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief inserts the specified field into the document according to the
    ///        specified ACTION
    /// @note 'Field' type type must satisfy the Field concept
    /// @param field attribute to be inserted
    /// @return true, if field was successfully insterted
    ////////////////////////////////////////////////////////////////////////////
    template<Action action, typename Field>
    bool insert(Field& field) const {
      return writer_.insert<action>(field);
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
  DECLARE_FACTORY(directory& dir, const comparer* comparator);

  struct update_context {
    size_t generation;
    size_t update_id;
  };

  typedef std::vector<update_context> update_contexts;

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
  bool insert(Field& field) {
    return valid_ = valid_ && action_helper<action>::insert(*this, field);
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
  size_t memory_active() const NOEXCEPT;

  // @return approximate amount of memory reserved by this instance
  size_t memory_reserved() const NOEXCEPT;

  // @param doc_id the document id as returned by begin(...)
  // @return success
  bool remove(doc_id_t doc_id);

  // rollbacks document-write transaction,
  // implicitly NOEXCEPT since we reserve memory in 'begin'
  void rollback() {
    // mark as removed since not fully inserted
    assert(docs_cached() + doc_limits::min() - 1 < doc_limits::eof()); // user should check return of begin() != eof()
    remove(doc_id_t(docs_cached() + doc_limits::min() - 1)); // -1 for 0-based offset
    valid_ = false;
  }

  void flush(index_meta::index_segment_t& segment);

  const std::string& name() const NOEXCEPT { return seg_name_; }
  size_t docs_cached() const NOEXCEPT { return docs_context_.size(); }
  bool initialized() const NOEXCEPT { return initialized_; }
  bool valid() const NOEXCEPT { return valid_; }
  void reset() NOEXCEPT;
  void reset(const segment_meta& meta);

 private:
  template<Action action>
  friend struct action_helper;

  struct stored_column : util::noncopyable {
    stored_column(
      const string_ref& name,
      columnstore_writer& columnstore,
      bool cache
    );

    std::string name;
    irs::sorted_column stream;
    columnstore_writer::values_writer_f writer;
    field_id id{ field_limits::invalid() };
  }; // stored_column

  struct sorted_column : util::noncopyable {
    sorted_column() = default;

    irs::sorted_column stream;
    field_id id{ field_limits::invalid() };
  }; // sorted_column

  segment_writer(directory& dir, const comparer* comparator) NOEXCEPT;

  bool index(
    const hashed_string_ref& name,
    const doc_id_t doc,
    const flags& features,
    token_stream& tokens
  );

  template<typename Writer>
  bool store_sorted(
      const doc_id_t doc,
      Writer& writer
  ) {
    assert(doc < doc_limits::eof());

    if (!fields_.comparator()) {
      // can't store sorted field without a comparator
      return false;
    }

    auto& out = sorted_stream(doc);

    if (writer.write(out)) {
      return true;
    }

    out.reset();

    return false;
  }

  template<typename Writer>
  bool store(
      const hashed_string_ref& name,
      const doc_id_t doc,
      Writer& writer
  ) {
    assert(doc < doc_limits::eof());

    auto& out = stream(name, doc);

    if (writer.write(out)) {
      return true;
    }

    out.reset();

    return false;
  }

  template<typename Field>
  bool store_worker(Field& field) {
    REGISTER_TIMER_DETAILED();

    const auto name = make_hashed_ref(
      static_cast<const string_ref&>(field.name()),
      std::hash<irs::string_ref>()
    );

    assert(docs_cached() + doc_limits::min() - 1 < doc_limits::eof()); // user should check return of begin() != eof()
    const auto doc_id = doc_id_t(docs_cached() + doc_limits::min() - 1); // -1 for 0-based offset

    return store(name, doc_id, field);
  }

  template<typename Field>
  bool store_sorted_worker(Field& field) {
    REGISTER_TIMER_DETAILED();

    assert(docs_cached() + doc_limits::min() - 1 < doc_limits::eof()); // user should check return of begin() != eof()
    const auto doc_id = doc_id_t(docs_cached() + doc_limits::min() - 1); // -1 for 0-based offset

    return store_sorted(doc_id, field);
  }

  // adds document field
  template<typename Field>
  bool index_worker(Field& field) {
    REGISTER_TIMER_DETAILED();

    const auto name = make_hashed_ref(
      static_cast<const string_ref&>(field.name()),
      std::hash<irs::string_ref>()
    );

    auto& tokens = static_cast<token_stream&>(field.get_tokens());
    const auto& features = static_cast<const flags&>(field.features());

    assert(docs_cached() + doc_limits::min() - 1 < doc_limits::eof()); // user should check return of begin() != eof()
    const auto doc_id = doc_id_t(docs_cached() + doc_limits::min() - 1); // -1 for 0-based offset

    return index(name, doc_id, features, tokens);
  }

  template<bool Sorted, typename Field>
  bool index_and_store_worker(Field& field) {
    REGISTER_TIMER_DETAILED();

    const auto name = make_hashed_ref(
      static_cast<const string_ref&>(field.name()),
      std::hash<irs::string_ref>()
    );

    auto& tokens = static_cast<token_stream&>(field.get_tokens());
    const auto& features = static_cast<const flags&>(field.features());

    assert(docs_cached() + doc_limits::min() - 1 < doc_limits::eof()); // user should check return of begin() != eof()
    const auto doc_id = doc_id_t(docs_cached() + doc_limits::min() - 1); // -1 for 0-based offset

    if (!index(name, doc_id, features, tokens)) {
      return false; // indexing failed
    }

    if (Sorted) {
      return store_sorted(doc_id, field);
    }

    return store(name, doc_id, field);
  }

  // returns stream for storing attributes in sorted order
  columnstore_writer::column_output& sorted_stream(const doc_id_t doc_id);

  // returns stream for storing attributes
  columnstore_writer::column_output& stream(
    const hashed_string_ref& name,
    const doc_id_t doc
  );

  void finish(); // finishes document

  size_t flush_doc_mask(const segment_meta& meta); // flushes document mask to directory, returns number of masked documens
  void flush_column_meta(const segment_meta& meta); // flushes column meta to directory
  void flush_fields(const doc_map& docmap); // flushes indexed fields to directory

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  sorted_column sort_;
  update_contexts docs_context_;
  bitvector docs_mask_; // invalid/removed doc_ids (e.g. partially indexed due to indexing failure)
  fields_data fields_;
  std::unordered_map<hashed_string_ref, stored_column> columns_;
  std::unordered_set<field_data*> norm_fields_; // document fields for normalization
  std::string seg_name_;
  field_writer::ptr field_writer_;
  column_meta_writer::ptr col_meta_writer_;
  columnstore_writer::ptr col_writer_;
  tracking_directory dir_;
  bool initialized_;
  bool valid_{ true }; // current state
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // segment_writer

template<>
struct action_helper<Action::INDEX> {
  template<typename Field>
  static bool insert(segment_writer& writer, Field& field) {
    return writer.index_worker(field);
  }
}; // action_helper

template<>
struct action_helper<Action::STORE> {
  template<typename Field>
  static bool insert(segment_writer& writer, Field& field) {
    return writer.store_worker(field);
  }
}; // action_helper

template<>
struct action_helper<Action::STORE_SORTED> {
  template<typename Field>
  static bool insert(segment_writer& writer, Field& field) {
    return writer.store_sorted_worker(field);
  }
}; // action_helper

template<>
struct action_helper<Action::INDEX_AND_STORE> {
  template<typename Field>
  static bool insert(segment_writer& writer, Field& field) {
    return writer.index_and_store_worker<false>(field);
  }
}; // action_helper

template<>
struct action_helper<Action::INDEX_AND_STORE_SORTED> {
  template<typename Field>
  static bool insert(segment_writer& writer, Field& field) {
    return writer.index_and_store_worker<true>(field);
  }
}; // action_helper

NS_END

MSVC_ONLY(template class IRESEARCH_API std::unique_ptr<irs::segment_writer>;) // segment_writer::ptr

#endif
