////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_TRANSACTION_STORE_H
#define IRESEARCH_TRANSACTION_STORE_H

#include "index_writer.hpp"
#include "utils/async_utils.hpp"
#include "utils/bitvector.hpp"
#include "utils/hash_utils.hpp"
#include "utils/object_pool.hpp"

NS_ROOT

class store_writer; // forward declaration
class transaction_store; // forward declaration

////////////////////////////////////////////////////////////////////////////////
/// @brief interface for an index reader over a transaction store
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API store_reader final
    : public sub_reader,
      private atomic_shared_ptr_helper<const sub_reader> {
 public:
  typedef atomic_shared_ptr_helper<const sub_reader> atomic_utils;
  typedef store_reader element_type; // type same as self
  typedef store_reader ptr; // pointer to self

  store_reader() = default; // allow creation of an uninitialized ptr
  store_reader(const store_reader& other) NOEXCEPT;
  store_reader& operator=(const store_reader& other) NOEXCEPT;

  explicit operator bool() const NOEXCEPT { return bool(impl_); }

  store_reader& operator*() NOEXCEPT { return *this; }
  const store_reader& operator*() const NOEXCEPT { return *this; }
  store_reader* operator->() NOEXCEPT { return this; }
  const store_reader* operator->() const NOEXCEPT { return this; }

  virtual const column_meta* column(const string_ref& name) const override {
    return impl_->column(name);
  }

  virtual column_iterator::ptr columns() const override {
    return impl_->columns();
  }

  virtual const columnstore_reader::column_reader* column_reader(
      field_id field
  ) const override {
    return impl_->column_reader(field);
  }

  virtual uint64_t docs_count() const override { return impl_->docs_count(); }

  virtual doc_iterator::ptr docs_iterator() const override {
    return impl_->docs_iterator();
  }

  virtual const term_reader* field(const string_ref& field) const override {
    return impl_->field(field);
  }

  virtual field_iterator::ptr fields() const override {
    return impl_->fields();
  }

  virtual uint64_t live_docs_count() const override {
    return impl_->live_docs_count();
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief open a new instance based on the latest state of the store
  ////////////////////////////////////////////////////////////////////////////////
  virtual store_reader reopen() const;

  void reset() NOEXCEPT { impl_.reset(); }

  virtual const sub_reader& operator[](size_t i) const override {
    return (*impl_)[i];
  }

  virtual size_t size() const override { return impl_->size(); }

 private:
  friend transaction_store; // for access to the private constructor
  typedef std::shared_ptr<const sub_reader> impl_ptr;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  impl_ptr impl_;
  IRESEARCH_API_PRIVATE_VARIABLES_END

  store_reader(impl_ptr&& impl) NOEXCEPT;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief An in-memory small-transaction optimized container/writer for indexed
///        documents. Conceptually a writer and reader all in one object.
///        Thread safe.
///        a document can be in one of three states:
///         * not commited - not visible to readers
///         * commited - visible to all subsequently opened readers
///         * moved - visible only to previously opened readers
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API transaction_store: private util::noncopyable {
 public:
  // 'make(...)' method wrapper for irs::bstring for use with object pools
  struct IRESEARCH_API bstring_builder {
    typedef std::shared_ptr<irs::bstring> ptr;
    DECLARE_FACTORY();
  };

  // 'make(...)' method wrapper for irs::column_meta for use with object pools
  struct column_meta_builder {
    typedef std::shared_ptr<irs::column_meta> ptr;
    DECLARE_FACTORY();
  };

  // 'make(...)' method wrapper for irs::field_meta for use with object pools
  struct field_meta_builder {
    typedef std::shared_ptr<irs::field_meta> ptr;
    DECLARE_FACTORY();
  };

  struct document_t {
    bstring_builder::ptr buf_; // document data buffer
    doc_id_t doc_id_; // conceptually const so that it may be read concurrently
    document_t(doc_id_t doc_id): doc_id_(doc_id) {}
  };

  struct document_entry_t: public document_t {
    size_t offset_; // column/term data starting offset in document buffer
    document_entry_t(const document_t& doc, size_t offset)
      : document_t(doc), offset_(offset) {
    }
  };

  transaction_store(size_t pool_size = DEFAULT_POOL_SIZE);

  ////////////////////////////////////////////////////////////////////////////
  /// @brief remove all unused entries in internal data structures
  ////////////////////////////////////////////////////////////////////////////
  void cleanup();

  ////////////////////////////////////////////////////////////////////////////
  /// @brief remove all unused entries in internal data structures
  /// @note  in-progress transactions will not be able to commit succesfully
  ////////////////////////////////////////////////////////////////////////////
  void clear();

  ////////////////////////////////////////////////////////////////////////////
  /// @brief export all completed transactions into the reader
  /// @note in-progress transaction commit() will fail
  /// @return reader with the flushed state or false on flush failure
  ////////////////////////////////////////////////////////////////////////////
  store_reader flush();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create an index reader over already commited documents in the store
  ////////////////////////////////////////////////////////////////////////////////
  store_reader reader() const;

 private:
  friend store_reader store_reader::reopen() const;
  friend store_writer;

  typedef unbounded_object_pool<bstring_builder> bstring_pool_t;
  typedef unbounded_object_pool<column_meta_builder> column_meta_pool_t;
  typedef unbounded_object_pool<field_meta_builder> field_meta_pool_t;
  class store_reader_helper; // forward declaration for internal use

  template<typename T>
  class ref_t: private util::noncopyable {
   public:
    ref_t(): value_(nullptr) {}
    ref_t(T& value): value_(&value) { ++(value.refs_); }
    ~ref_t() { if (value_) { --(value_->refs_); } }
    ref_t(ref_t&& other) NOEXCEPT: value_(nullptr) { *this = std::move(other); }
    ref_t(const ref_t& other) NOEXCEPT: value_(nullptr) { *this = other; }
    ref_t& operator=(const ref_t& other) {
      if (this != &other) {
        if (value_) {
          --(value_->refs_);
        }

        value_ = other.value_;

        if (value_) {
          ++(value_->refs_);
        }
      }

      return *this;
    }
    ref_t& operator=(ref_t&& other) {
      if (this != &other) {
        if (value_) {
          --(value_->refs_);
        }

        value_ = other.value_;
        other.value_ = nullptr;
      }

      return *this;
    }
    operator bool() const { return nullptr != value_; }
    T& operator*() const { return *value_; }
    T* operator->() const { return value_; }

   private:
    T* value_;
  };

  struct column_t: private util::noncopyable { // no copy because of ref tracking
    std::vector<document_entry_t> entries_;
    std::atomic<size_t> refs_{}; // ref tracking for term addition/write-pending operations
  };

  struct column_named_t: public column_t {
    const column_meta_builder::ptr meta_;
    column_named_t(
        column_meta_pool_t& pool, const string_ref& name, field_id id
    ): meta_(pool.emplace().release()) {
      if (meta_) {
        *meta_ = column_meta(name, id); // reset value
      }
    }
  };

  struct postings_t {
    std::vector<document_entry_t> entries_;
    term_meta meta_;
    const bstring_builder::ptr name_;
    postings_t(
        bstring_pool_t& pool, const bytes_ref& name
    ): name_(pool.emplace().release()) {
      if (name_) {
        *name_ = name; // reset value
      }
    }
  };

  struct terms_t: private util::noncopyable { // no copy because of ref tracking
    const field_meta_builder::ptr meta_;
    ref_t<column_t> norm_col_ref_;
    std::atomic<size_t> refs_{}; // ref tracking for term addition/write-pending operations
    std::unordered_map<hashed_bytes_ref, postings_t> terms_;
    terms_t(
        field_meta_pool_t& pool, const string_ref& name, const flags& features
    ): meta_(pool.emplace().release()) {
      if (meta_) {
        *meta_ = field_meta(name, features); // reset value
      }
    }
  };

  typedef ref_t<column_named_t> column_ref_t;
  typedef ref_t<terms_t> field_ref_t;
  typedef std::shared_ptr<bool> reusable_t;

  static const size_t DEFAULT_POOL_SIZE = 128; // arbitrary value
  bstring_pool_t bstring_pool_;
  column_meta_pool_t column_meta_pool_;
  std::unordered_map<hashed_string_ref, column_named_t> columns_named_; // user columns
  std::unordered_map<field_id, column_t> columns_unnamed_; // system columns
  field_meta_pool_t field_meta_pool_;
  std::unordered_map<hashed_string_ref, terms_t> fields_;
  size_t generation_; // current commit generation
  std::mutex generation_mutex_; // prevent generation modification during writer commit with removals/updates and flush (used before aquiring write lock on mutex_)
  mutable async_utils::read_write_mutex mutex_; // mutex for 'columns_', 'fields_', 'generation_', 'visible_docs_'
  reusable_t reusable_;
  bitvector used_column_ids_; // true == column id is in use by some column

  // doc_id states: used -> valid -> visible
  // if 'used' <= do not reassign until flush() marks it as not used
  // if !'used' <= available for assignment
  // if 'valid' && 'used' <=  candidate for flush() once 'visible'
  // if !'valid' && 'used' <= free during flush(), do not flush()
  // if 'visible' && 'valid' && 'used' <= candidate for flush(), free after flush()
  // if !'visible' && 'valid' && 'used' <= do not flush(), do not free during flush()
  bitvector used_doc_ids_; // true == doc_id in use (in 'fields_'/'columns_'), false == doc_id can be reused
  bitvector valid_doc_ids_; // true == doc_id part of some tx (implies 'used'), false == doc_id will never be comitted
  bitvector visible_docs_; // true == commited (implies 'valid'), false == not commited or removed

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find an existing column or create a new column
  /// @return column matching 'name', false == error
  ////////////////////////////////////////////////////////////////////////////////
  column_ref_t get_column(const hashed_string_ref& name);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find the first unused field_id (column id) and mark it used
  /// @return reserved field_id or type_limits<type_t::field_id_t>::invalid() on
  ///         error
  ////////////////////////////////////////////////////////////////////////////////
  field_id get_column_id();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find the first unused doc_id starting from 'start' and mark it used
  /// @return reserved doc_id or type_limits<type_t::doc_id_t>::invalid() on error
  ////////////////////////////////////////////////////////////////////////////////
  doc_id_t get_doc_id(doc_id_t start);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief find an existing field or create a new field
  /// @return field matching 'name' with a superset of 'features', false == error
  ////////////////////////////////////////////////////////////////////////////////
  field_ref_t get_field(const hashed_string_ref& name, const flags& features);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief interface for an index writer over a transaction store
///        an object that represents a single ongoing transaction
///        non-thread safe
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API store_writer final: private util::noncopyable {
  class bstring_output; // forward declaration

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @class document
  /// @brief Facade for the insertion logic
  //////////////////////////////////////////////////////////////////////////////
  class document: private util::noncopyable {
   public:
    struct state_t {
      std::unordered_map<void*, size_t> offsets_; // offsets within 'state_'
      bstring_output& out_;
      state_t(bstring_output& out): out_(out) {}
    };

    ////////////////////////////////////////////////////////////////////////////
    /// @brief constructor
    ////////////////////////////////////////////////////////////////////////////
    document(
        store_writer& writer,
        transaction_store::document_t& doc,
        bstring_output& out,
        state_t& state
    ) NOEXCEPT: doc_(doc), out_(out), state_(state), valid_(true), writer_(writer) {
      state_.offsets_.clear(); // reset for this document
      state_.out_.seek(0); // reset for this document
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief inserts the specified field into the document according to the
    ///        specified ACTION
    /// @note 'Field' type type must satisfy the Field concept
    /// @param field attribute to be inserted
    /// @return true, if field was successfully insterted
    ////////////////////////////////////////////////////////////////////////////
    template<typename Action, typename Field>
    bool insert(Action& action, Field& field) {
      return valid_ = valid_ && writer_.insert(action, doc_, out_, state_, field);
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief inserts the specified field (denoted by the pointer) into the
    ///        document according to the specified ACTION
    /// @note 'Field' type type must satisfy the Field concept
    /// @note pointer must not be nullptr
    /// @param field attribute to be inserted
    /// @return true, if field was successfully insterted
    ////////////////////////////////////////////////////////////////////////////
    template<typename Action, typename Field>
    bool insert(Action& action, Field* field) {
      assert(field);
      return insert(action, *field);
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief inserts the specified range of fields, denoted by the [begin;end)
    ///        into the document according to the specified ACTION
    /// @note 'Iterator' underline value type must satisfy the Field concept
    /// @param begin the beginning of the fields range
    /// @param end the end of the fields range
    /// @return true, if the range was successfully insterted
    ////////////////////////////////////////////////////////////////////////////
    template<typename Action, typename Iterator>
    bool insert(Action& action, Iterator begin, Iterator end) {
      for (; valid() && begin != end; ++begin) {
        insert(action, *begin);
      }

      return valid();
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @return current state of the object
    /// @note for the case when the object is in an invalid state all further
    ///       operations will not take any effect
    ////////////////////////////////////////////////////////////////////////////
    bool valid() const NOEXCEPT { return valid_; }

   private:
    friend store_writer;
    transaction_store::document_t& doc_;
    bstring_output& out_;
    state_t& state_;
    bool valid_;
    store_writer& writer_;
  }; // document

  store_writer(transaction_store& store) NOEXCEPT;

  ////////////////////////////////////////////////////////////////////////////
  /// @brief rolls-back any uncommited data
  ////////////////////////////////////////////////////////////////////////////
  ~store_writer();

  ////////////////////////////////////////////////////////////////////////////
  /// @brief make all buffered changes visible for readers
  ////////////////////////////////////////////////////////////////////////////
  bool commit();

  ////////////////////////////////////////////////////////////////////////////
  /// @brief inserts document to be filled by the specified functor into index
  /// @note changes are not visible until commit()
  /// @note the specified 'func' should return false in order to break the
  ///       insertion loop
  /// @param func the insertion logic
  /// @return status of the last insert operation
  ////////////////////////////////////////////////////////////////////////////
  template<typename Func>
  bool insert(Func func) {
    bitvector doc_ids;

    if (!insert(doc_ids, func)) {
      return false; // doc_ids will be rolled vack since they are not in valid_doc_ids_
    }

    valid_doc_ids_ |= doc_ids; // consider documents as candidates for commit

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////
  /// @brief marks documents matching filter for removal
  /// @note that changes are not visible until commit()
  /// @note that filter must be valid until commit()
  ///
  /// @param filter the document filter
  ////////////////////////////////////////////////////////////////////////////
  void remove(const filter& filter);

  ////////////////////////////////////////////////////////////////////////////
  /// @brief marks documents matching filter for removal
  /// @note changes are not visible until commit()
  ///
  /// @param filter the document filter
  ////////////////////////////////////////////////////////////////////////////
  void remove(filter::ptr&& filter);

  ////////////////////////////////////////////////////////////////////////////
  /// @brief marks documents matching filter for removal
  /// @note changes are not visible until commit()
  ///
  /// @param filter the document filter
  ////////////////////////////////////////////////////////////////////////////
  void remove(const std::shared_ptr<filter>& filter);

  ////////////////////////////////////////////////////////////////////////////
  /// @brief replaces documents matching filter with the document
  ///        to be filled by the specified functor
  /// @note that changes are not visible until commit()
  /// @note that filter must be valid until commit()
  /// @param filter the document filter
  /// @param func the insertion logic
  /// @return all fields/attributes successfully insterted
  ////////////////////////////////////////////////////////////////////////////
  template<typename Filter, typename Func>
  bool update(Filter&& filter, Func func) {
    bitvector doc_ids;

    if (!insert(doc_ids, func)) {
      return false;
    }

    remove(std::forward<Filter>(filter));
    modification_queries_.back().documents_ = std::move(doc_ids); // noexcept

    return true;
  }

 private:
  // a data_output implementation backed by an output_iterator
  struct bstring_data_output: public data_output {
    bstring_output& out_;
    bstring_data_output(bstring_output& out): out_(out) {}
    virtual void close() override {}
    virtual void write_byte(byte_type b) override { write_bytes(&b, 1); }
    virtual void write_bytes(const byte_type* b, size_t size) override {
      out_.write(b, size);
    }
  };

  // an output_iterator implementation backed by a bstring
  class IRESEARCH_API bstring_output
    : std::iterator<std::output_iterator_tag, byte_type, void, void, void> {
   public:
    bstring_output(bstring& buf): buf_(buf), pos_(0) { ensure(pos_); }
    bstring::value_type& operator[](size_t i) { ensure(i); return buf_[i]; }
    bstring::value_type& operator*() { return buf_[pos_]; }
    bstring_output& operator++() { ensure(++pos_); return *this; }
    bstring_output& operator+=(size_t i) { ensure(pos_ += i); return *this; }
    size_t file_pointer() const NOEXCEPT { return pos_; }
    void seek(size_t pos) NOEXCEPT { pos_ = pos; }
    void write(const byte_type* value, size_t size);

   private:
    bstring& buf_;
    size_t pos_;

    void ensure(size_t pos);
  };

  struct modification_context {
    bitvector documents_; // replacement document IDs
    std::shared_ptr<const filter> filter_; // keep a handle to the filter for the case when this object has ownership
    const doc_id_t generation_;
    modification_context(const filter& match_filter, doc_id_t generation)
      : filter_(&match_filter, [](const filter*)->void{}), generation_(generation) {}
    modification_context(const std::shared_ptr<filter>& match_filter, doc_id_t generation)
      : filter_(match_filter), generation_(generation) {}
    modification_context(filter::ptr&& match_filter, doc_id_t generation)
      : filter_(std::move(match_filter)), generation_(generation) {}
    modification_context(modification_context&& other) NOEXCEPT
      : documents_(std::move(other.documents_)), filter_(std::move(other.filter_)), generation_(other.generation_) {}
  }; // modification_context

  typedef std::vector<modification_context> modification_requests_t;

  modification_requests_t modification_queries_; // sequential list of modification requests (remove/update)
  doc_id_t next_doc_id_; // next potential minimal doc_id, also the current modification/update generation, i.e. removals applied to all doc_ids < generation
  const transaction_store::reusable_t reusable_; // marker to allow commit into store
  transaction_store& store_;
  bitvector used_doc_ids_; // true == doc_id allocated to this writer, false == doc_id not allocated to this writer
  bitvector valid_doc_ids_; // true == commit pending, false == do not commit

  ////////////////////////////////////////////////////////////////////////////
  /// @brief insert documents provided by 'func' into store
  /// @param doc_ids - track inserted doc_ids
  /// @return success if all documents inserted successfuly
  ////////////////////////////////////////////////////////////////////////////
  template<typename Func>
  bool insert(bitvector& doc_ids, Func func) {
    REGISTER_TIMER_DETAILED();
    auto state_buf = store_.bstring_pool_.emplace().release();

    if (!state_buf) {
      return false; // treat as a rollback
    }

    bstring_output state_out(*state_buf);
    document::state_t doc_state(state_out);

    for (bool has_next = true; has_next;) {
      next_doc_id_ = store_.get_doc_id(next_doc_id_);

      if (!type_limits<type_t::doc_id_t>::valid(next_doc_id_)) {
        return false; // unable to reserve doc_id
      }

      used_doc_ids_.set(next_doc_id_); // mark as used by this transaction

      transaction_store::document_t doc(next_doc_id_);

      ++next_doc_id_; // prepare for next document
      doc.buf_ = store_.bstring_pool_.emplace().release();

      if (!doc.buf_) {
        return false; // treat as a rollback
      }

      bstring_output out(*(doc.buf_));
      document doc_wrapper(*this, doc, out, doc_state);

      irs::write<uint8_t>(doc_wrapper.out_, 0); // reserved for internal use (force all offsets to not be 0)
      has_next = func(doc_wrapper);

      if (!doc_wrapper.valid()) {
        return false; // treat as a rollback
      }

      doc_ids.set(doc.doc_id_);
    }

    return true;
  }

  template<typename Field>
  bool insert(
      const action::index_t&,
      transaction_store::document_t& doc,
      bstring_output& out,
      store_writer::document::state_t& state,
      Field& field
  ) {
    REGISTER_TIMER_DETAILED();
    const auto name = make_hashed_ref(
      static_cast<const string_ref&>(field.name()),
      std::hash<string_ref>()
    );

    auto& tokens = static_cast<token_stream&>(field.get_tokens());
    const auto& features = static_cast<const flags&>(field.features());
    auto start = out.file_pointer();

    if (index(out, state, name, features, doc, tokens)) {
      return true;
    }

    out.seek(start); // discard aded data

    return false;
  }

  template<typename Field>
  bool insert(
      const action::index_store_t&,
      transaction_store::document_t& doc,
      bstring_output& out,
      store_writer::document::state_t& state,
      Field& field
  ) {
    REGISTER_TIMER_DETAILED();
    const auto name = make_hashed_ref(
      static_cast<const string_ref&>(field.name()),
      std::hash<string_ref>()
    );

    auto& tokens = static_cast<token_stream&>(field.get_tokens());
    const auto& features = static_cast<const flags&>(field.features());
    auto start = out.file_pointer();
    bstring_data_output field_out(out);

    if (field.write(field_out)
        && store(out, state, name, doc, start)
        && index(out, state, name, features, doc, tokens)) {
      return true;
    }

    out.seek(start); // discard aded data

    return false;
  }

  template<typename Field>
  bool insert(
      const action::store_t&,
      transaction_store::document_t& doc,
      bstring_output& out,
      store_writer::document::state_t& state,
      Field& field
  ) {
    REGISTER_TIMER_DETAILED();
    const auto name = make_hashed_ref(
      static_cast<const string_ref&>(field.name()),
      std::hash<string_ref>()
    );
    assert(doc.buf_);
    auto start = out.file_pointer();
    bstring_data_output field_out(out);

    if (field.write(field_out)
        && store(out, state, name, doc, start)) {
      return true;
    }

    out.seek(start); // discard aded data

    return false;
  }

  bool index(
    bstring_output& out,
    document::state_t& state,
    const hashed_string_ref& field_name,
    const flags& field_features,
    transaction_store::document_t& doc,
    token_stream& tokens
  );

  bool store(
    bstring_output& out,
    document::state_t& state,
    const hashed_string_ref& column_name,
    transaction_store::document_t& doc,
    size_t buf_offset
  );
};

NS_END // ROOT

#endif
