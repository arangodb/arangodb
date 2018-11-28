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

#include "composite_reader_impl.hpp"
#include "search/bitset_doc_iterator.hpp"
#include "store/store_utils.hpp"
#include "utils/string_utils.hpp"

#include "transaction_store.hpp"

NS_LOCAL

static const size_t DEFAULT_BUFFER_SIZE = 512; // arbitrary value

///////////////////////////////////////////////////////////////////////////////
/// @brief write to 'out' array of data pointed by 'value' of length 'size'
///        optimization for writing to a contiguous memory block
/// @note OutputIterator::operator*() must return byte_type& to a contiguous
///       block of memory
/// @return bytes written
////////////////////////////////////////////////////////////////////////////////
template<typename OutputIterator, typename T>
size_t write_bytes_block(OutputIterator& out, const T* value, size_t size) {
  // ensure the following is defined: byte_type& OutputIterator::operator*()
  typedef typename std::enable_if<
    std::is_same<decltype(((OutputIterator*)(nullptr))->operator*()), irs::byte_type&>::value,
    irs::byte_type&
  >::type ref_type;
  auto start = out; // do not dereference yet since space reservation could change address

  size = sizeof(T) * size;
  out += size; // reserve space

  ref_type dst = *start; // assign to ensure std::enable_if<..> is applied

  assert(size_t(std::distance(&(*start), &(*out))) == size); // std::memcpy() only valid for contiguous blocks
  std::memcpy(&dst, value, size); // optimization for writing to a contiguous memory block

  return size;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write to 'out' raw byte representation of data in 'value'
///        optimization for writing to a contiguous memory block
/// @return bytes written
////////////////////////////////////////////////////////////////////////////////
template<typename OutputIterator, typename T>
size_t write_bytes_block(OutputIterator& out, const T& value) {
  return write_bytes_block(out, &value, 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write to 'out' data in 'value'
///        optimization for writing to a contiguous memory block
///        same output format as irs::vwrite_string
////////////////////////////////////////////////////////////////////////////////
template<typename OutputIterator, typename StringType>
void vwrite_string_block(OutputIterator& out, const StringType& value) {
  irs::vwrite<uint64_t>(out, value.size());
  write_bytes_block(out, value.c_str(), value.size());
}

// state at start of each unique column
struct column_stats_t {
  size_t offset = 0; // offset to previous column data start in buffer
};

// state at start of each unique document (only once since state is per-document)
struct doc_stats_t {
  float_t norm = irs::norm::DEFAULT();
  uint32_t term_count = 0; // number of terms in a document
};

// state at start of each unique field (for all terms)
struct field_stats_t {
  uint32_t pos = irs::integer_traits<uint32_t>::const_max; // current term position
  uint32_t pos_last = 0; // previous term position
  uint32_t max_term_freq = 0; // highest term frequency in a field across all terms/documents
  uint32_t num_overlap = 0; // number of overlapped terms
  uint32_t offs_start_base = 0; // current field term-offset base (i.e. previous same-field last term-offset-end) for computation of absolute offset
  uint32_t offs_start_term = 0; // current field previous term-offset start (offsets may overlap)
  uint32_t unq_term_count = 0; // number of unique terms
};

// state at start of each unique term
struct term_stats_t {
  size_t offset = 0; // offset to previous term data start in buffer
  uint32_t term_freq = 0; // term frequency in a document
};

struct document_less_t {
  typedef irs::transaction_store::document_t document_t;

  bool operator() (const document_t& doc, irs::doc_id_t id) const {
    return doc.doc_id_ < id;
  }

  bool operator() (irs::doc_id_t id, const document_t& doc) const {
    return id < doc.doc_id_;
  }

  bool operator() (const document_t& lhs, const document_t& rhs) const {
    return lhs.doc_id_ < rhs.doc_id_;
  }
};

const document_less_t DOC_LESS;

struct empty_seek_term_iterator: public irs::seek_term_iterator {
  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return irs::attribute_view::empty_instance();
  }

  virtual bool next() override { return false; }

  virtual irs::doc_iterator::ptr postings(const irs::flags&) const override {
    return irs::doc_iterator::empty();
  }

  virtual void read() override { }
  virtual bool seek(const irs::bytes_ref&) override { return false; }

  virtual bool seek(const irs::bytes_ref&, const seek_cookie&) override {
    return false;
  }

  virtual seek_cookie::ptr cookie() const override { return nullptr; }

  virtual irs::SeekResult seek_ge(const irs::bytes_ref&) override {
    return irs::SeekResult::END;
  }

  virtual const irs::bytes_ref& value() const override {
    return irs::bytes_ref::NIL;
  }
};

// ----------------------------------------------------------------------------
// --SECTION--                                      store_reader implementation
// ----------------------------------------------------------------------------

class store_reader_impl final: public irs::sub_reader {
 public:
  typedef irs::transaction_store::document_entry_t document_entry_t;
  typedef std::vector<document_entry_t> document_entries_t;

  struct column_reader_t: public irs::columnstore_reader::column_reader {
    document_entries_t entries_;
    column_reader_t(document_entries_t&& entries) NOEXCEPT
      : entries_(std::move(entries)) {}
    virtual irs::doc_iterator::ptr iterator() const override;
    virtual size_t size() const override { return entries_.size(); }
    virtual irs::columnstore_reader::values_reader_f values() const override;
    virtual bool visit(
      const irs::columnstore_reader::values_visitor_f& visitor
    ) const override;
  };

  struct named_column_reader_t: public column_reader_t {
    const irs::transaction_store::column_meta_builder::ptr meta_; // copy from 'store' because column in store may disapear
    named_column_reader_t(
        const irs::transaction_store::column_meta_builder::ptr& meta,
        document_entries_t&& entries
    ): column_reader_t(std::move(entries)), meta_(meta) { assert(meta_); }
  };

  typedef std::map<irs::string_ref, named_column_reader_t> columns_named_t;
  typedef std::map<irs::field_id, column_reader_t> columns_unnamed_t;

  struct term_entry_t {
    document_entries_t entries_;
    irs::term_meta meta_; // copy from 'store' because term in store may disapear
    const irs::transaction_store::bstring_builder::ptr name_; // copy from 'store' because term in store may disapear
    term_entry_t(
        const irs::transaction_store::bstring_builder::ptr& name,
        const irs::term_meta& meta,
        document_entries_t&& entries
    ): entries_(std::move(entries)), meta_(meta), name_(name) { assert(name_); }
  };

  typedef std::map<irs::bytes_ref, term_entry_t> term_entries_t;

  struct term_reader_t: public irs::term_reader {
    irs::attribute_view attrs_;
    uint64_t doc_count_;
    irs::bytes_ref max_term_{ irs::bytes_ref::NIL };
    const irs::transaction_store::field_meta_builder::ptr meta_; // copy from 'store' because field in store may disapear
    irs::bytes_ref min_term_{ irs::bytes_ref::NIL };
    term_entries_t terms_;

    term_reader_t(const irs::transaction_store::field_meta_builder::ptr& meta)
      : meta_(meta) { assert(meta_); }
    term_reader_t(term_reader_t&& other) NOEXCEPT;
    virtual const irs::attribute_view& attributes() const NOEXCEPT override {
      return attrs_;
    }
    virtual uint64_t docs_count() const override { return doc_count_; }
    virtual irs::seek_term_iterator::ptr iterator() const override;
    virtual const irs::bytes_ref& max() const override { return max_term_; }
    virtual const irs::field_meta& meta() const override { return *meta_; }
    virtual const irs::bytes_ref& min() const override { return min_term_; }
    virtual size_t size() const override { return terms_.size(); }
  };

  typedef std::map<irs::string_ref, term_reader_t> fields_t;

  virtual const irs::column_meta* column(const irs::string_ref& name) const override;
  virtual irs::column_iterator::ptr columns() const override;
  virtual const irs::columnstore_reader::column_reader* column_reader(irs::field_id field) const override;
  using irs::sub_reader::docs_count;
  virtual uint64_t docs_count() const override { return documents_.size(); } // +1 for invalid doc if non empty
  virtual irs::doc_iterator::ptr docs_iterator() const override;
  virtual const irs::term_reader* field(const irs::string_ref& field) const override;
  virtual irs::field_iterator::ptr fields() const override;
  virtual uint64_t live_docs_count() const override { return documents_.count(); }
  virtual const sub_reader& operator[](size_t i) const override {
    assert(!i);
    return *this;
  }
  virtual size_t size() const override { return 1; } // only 1 segment

 private:
  friend irs::store_reader irs::store_reader::reopen() const;
  friend bool irs::store_writer::commit();
  friend irs::store_reader irs::transaction_store::reader() const;
  typedef std::unordered_map<irs::field_id, const column_reader_t*> column_by_id_t;

  const columns_named_t columns_named_;
  const columns_unnamed_t columns_unnamed_;
  const column_by_id_t column_by_id_;
  const irs::bitvector documents_;
  const fields_t fields_;
  const size_t generation_;
  const irs::transaction_store& store_;

  store_reader_impl(
    const irs::transaction_store& store,
    irs::bitvector&& documents,
    fields_t&& fields,
    columns_named_t&& columns_named,
    columns_unnamed_t&& columns_unnamed,
    size_t generation
  );
};

class store_column_iterator final: public irs::column_iterator {
 public:
  store_column_iterator(const store_reader_impl::columns_named_t& map)
    : itr_(map.begin()), map_(map), value_(nullptr) {}

  virtual bool next() override {
    if (map_.end() == itr_) {
      value_ = nullptr;

      return false; // already at end
    }

    value_ = itr_->second.meta_.get();
    ++itr_;

    return true;
  }

  virtual bool seek(const irs::string_ref& name) override {
    itr_ = map_.lower_bound(name);

    return next();
  }

  virtual const irs::column_meta& value() const override {
    static const irs::column_meta invalid;

    return value_ ? *value_ : invalid;
  }

 private:
  store_reader_impl::columns_named_t::const_iterator itr_;
  const store_reader_impl::columns_named_t& map_;
  const irs::column_meta* value_;
};

class store_doc_iterator_base: public irs::doc_iterator {
 public:
  store_doc_iterator_base(
      const store_reader_impl::document_entries_t& entries,
      size_t attr_count = 0
  ): attrs_(attr_count),
     entry_(nullptr),
     entries_(entries),
     next_itr_(entries.begin()) {
    doc_.value = irs::type_limits<irs::type_t::doc_id_t>::invalid();
  }

  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return attrs_;
  }

  virtual bool next() override {
    do {
      if (next_itr_ == entries_.end()) {
        entry_ = nullptr;
        doc_.value = irs::type_limits<irs::type_t::doc_id_t>::eof();

        return false;
      }

      entry_ = &*next_itr_;
      doc_.value = entry_->doc_id_;
      ++next_itr_;
    } while (!load_attributes()); // skip invalid doc ids

    return true;
  }

  virtual irs::doc_id_t seek(irs::doc_id_t doc) override {
    next_itr_ = std::lower_bound(entries_.begin(), entries_.end(), doc, DOC_LESS);
    next();

    return value();
  }

  virtual irs::doc_id_t value() const override { return doc_.value; }

 protected:
  irs::attribute_view attrs_;
  irs::document doc_;
  const store_reader_impl::document_entry_t* entry_;
  const store_reader_impl::document_entries_t& entries_;
  store_reader_impl::document_entries_t::const_iterator next_itr_;

  virtual bool load_attributes() = 0;
};

class store_columnstore_iterator final: public store_doc_iterator_base {
 public:
  store_columnstore_iterator(
      const store_reader_impl::document_entries_t& entries
  ): store_doc_iterator_base(entries, 1) { // +1 for payload_iterator
    attrs_.emplace(doc_payload_);
  }

  virtual bool next() override {
    if (store_doc_iterator_base::next()) {
      return true;
    }

    doc_payload_.value_ = nullptr;

    return false;
  }

 protected:
  bool load_attributes() override {
    if (!entry_ || !(entry_->buf_)) {
      return false;
    }

    // @see store_writer::store(...) for format definition
    doc_payload_.data_ = *(entry_->buf_);
    doc_payload_.offset_ = entry_->offset_;

    return true;
  }

 private:
  struct payload_iterator: public irs::payload_iterator {
    irs::bytes_ref data_;
    size_t offset_ = 0; // initially no data
    irs::bytes_ref value_;

    virtual bool next() override {
      if (!offset_) {
        value_ = irs::bytes_ref::NIL;

        return false;
      }

      auto* ptr = data_.c_str() + offset_;
      auto next_offset = irs::read<uint64_t>(ptr); // read next value offset
      auto size = irs::vread<uint64_t>(ptr); // read value size
      auto start = offset_ - size;

      assert(offset_ >= size); // otherwise there is an error somewhere in the code
      assert(data_.size() >= start + size); // otherwise there is an error somewhere in the code
      offset_ = next_offset;
      value_ = irs::bytes_ref(data_.c_str() + start, size);

      return true;
    }

    virtual const irs::bytes_ref& value() const override { return value_; }
  };

  payload_iterator doc_payload_;
};

class store_doc_iterator final: public store_doc_iterator_base {
 public:
  store_doc_iterator(
      const store_reader_impl::document_entries_t& entries,
      const irs::flags& field_features,
      const irs::flags& requested_features
  ): store_doc_iterator_base(entries, 3), // +1 for document, +1 for frequency, +1 for position
     doc_pos_(field_features, requested_features, entry_),
     load_frequency_(requested_features.check<irs::frequency>()) {

    attrs_.emplace(doc_);

    if (load_frequency_) {
      attrs_.emplace(doc_freq_);
    }

    if (requested_features.check<irs::position>()
        && field_features.check<irs::position>()) {
      attrs_.emplace(doc_pos_);
    }
  }

 protected:
  bool load_attributes() override {
    if (!load_frequency_) {
      return true; // nothing to do
    }

    if (!entry_ || !(entry_->buf_)) {
      return false;
    }

    // @see store_writer::index(...) for format definition
    auto next_offset = entry_->offset_;

    doc_freq_.value = 0; // reset for new entry

    // length of linked list == term frequency in current document
    do {
      assert(entry_->buf_->size() >= next_offset); // otherwise there is an error somewhere in the code
      auto* ptr = &((*(entry_->buf_))[next_offset]);

      ++doc_freq_.value;
      next_offset = irs::read<uint64_t>(ptr);
    } while (next_offset);

    doc_pos_.clear(); // reset impl to new doc

    return true;
  }

 private:
  struct position_t: public irs::position {
    const store_reader_impl::document_entry_t*& entry_;
    bool has_offs_;
    bool has_pay_;
    size_t next_;
    irs::offset offs_;
    irs::position::value_t pos_;
    irs::payload pay_;

    position_t(
        const irs::flags& field_features,
        const irs::flags& requested_features,
        const store_reader_impl::document_entry_t*& entry
    ): position(2), // offset + payload
       entry_(entry),
       has_offs_(field_features.check<irs::offset>()),
       has_pay_(field_features.check<irs::payload>()) {
      if (has_offs_ && requested_features.check<irs::offset>()) {
        attrs_.emplace(offs_);
      }

      if (has_pay_ && requested_features.check<irs::payload>()) {
        attrs_.emplace(pay_);
      }

      clear();
    }

    virtual void clear() override {
      next_ = entry_ ? entry_->offset_ : 0; // 0 indicates end of list in format definition
      offs_.clear();
      pos_ = irs::type_limits<irs::type_t::pos_t>::invalid();
      pay_.clear();
    }

    virtual irs::position::value_t value() const override { return pos_; }

    virtual bool next() override {
      if (!next_ || !entry_ || !entry_->buf_ || next_ >= entry_->buf_->size()) {
        next_ = 0; // 0 indicates end of list in format definition
        offs_.clear();
        pos_ = irs::type_limits<irs::type_t::pos_t>::eof();
        pay_.clear();

        return false;
      }

      // @see store_writer::index(...) for format definition
      assert(entry_->buf_->size() > next_);
      auto* ptr = entry_->buf_->c_str() + next_;

      next_ = irs::read<uint64_t>(ptr); // read 'next-pointer'
      pos_ = irs::vread<uint32_t>(ptr);

      if (has_offs_) {
        offs_.start = irs::vread<uint32_t>(ptr);
        offs_.end = irs::vread<uint32_t>(ptr);
      }

      pay_.value = !irs::read<uint8_t>(ptr)
        ? irs::bytes_ref::NIL
        : irs::vread_string<irs::bytes_ref>(ptr);
        ;

      return true;
    }
  };

  irs::frequency doc_freq_;
  position_t doc_pos_;
  bool load_frequency_; // should the frequency attribute be updated (optimization)
};

class store_field_iterator final: public irs::field_iterator {
 public:
  store_field_iterator(const store_reader_impl::fields_t& map)
    : itr_(map.begin()), map_(map), value_(nullptr) {}

  virtual bool next() override {
    if (map_.end() == itr_) {
      value_ = nullptr;

      return false; // already at end
    }

    value_ = &(itr_->second);
    ++itr_;

    return true;
  };

  virtual bool seek(const irs::string_ref& name) override {
    itr_ = map_.lower_bound(name);

    return next();
  }

  virtual const irs::term_reader& value() const override {
    assert(value_);

    return *value_;
  }

 private:
  store_reader_impl::fields_t::const_iterator itr_;
  const store_reader_impl::fields_t& map_;
  const irs::term_reader* value_;
};

class store_term_iterator: public irs::seek_term_iterator {
 public:
  store_term_iterator(
      const irs::flags& field_features,
      const store_reader_impl::term_entries_t& terms
  ): field_features_(field_features),
     term_entry_(nullptr),
     attrs_(2), // term_meta + frequency
     next_itr_(terms.begin()),
     terms_(terms) {
    attrs_.emplace(meta_);

    if (field_features_.check<irs::frequency>()) {
      attrs_.emplace(freq_);
    }
  }

  const irs::attribute_view& attributes() const NOEXCEPT override {
    return attrs_;
  }

  virtual seek_term_iterator::seek_cookie::ptr cookie() const override {
    return irs::memory::make_unique<cookie_t>(terms_.find(value()));
  }

  virtual bool next() override {
    if (next_itr_ == terms_.end()) {
      term_ = irs::bytes_ref::NIL;
      term_entry_ = nullptr;

      return false;
    }

    term_ = next_itr_->first;
    term_entry_ = &(next_itr_->second);
    ++next_itr_;

    return true;
  }

  virtual irs::doc_iterator::ptr postings(
      const irs::flags& features
  ) const override {
    return !term_entry_ || term_entry_->entries_.empty()
      ? irs::doc_iterator::empty()
      : irs::doc_iterator::make<store_doc_iterator>(
          term_entry_->entries_,
          field_features_,
          features
        )
      ;
  }

  virtual void read() override {
    if (!term_entry_) {
      return; // nothing to do
    }

    freq_.value = term_entry_->meta_.freq;
    meta_ = term_entry_->meta_;
  }

  virtual bool seek(const irs::bytes_ref& term) override {
    return irs::SeekResult::FOUND == seek_ge(term);
  }

  virtual bool seek(
    const irs::bytes_ref& /*term*/,
    const irs::seek_term_iterator::seek_cookie& cookie
  ) override {
    #ifdef IRESEARCH_DEBUG
      const auto& state = dynamic_cast<const cookie_t&>(cookie);
    #else
      const auto& state = static_cast<const cookie_t&>(cookie);
    #endif

    next_itr_ = state.itr_;

    return next();
  }

  virtual irs::SeekResult seek_ge(const irs::bytes_ref& term) override {
    next_itr_ = terms_.lower_bound(term);

    if (!next()) {
      return irs::SeekResult::END;
    }

    return value() == term
      ? irs::SeekResult::FOUND
      : irs::SeekResult::NOT_FOUND
      ;
  }

  virtual const irs::bytes_ref& value() const override { return term_; }

 protected:
  const irs::flags& field_features_;
  const store_reader_impl::term_entry_t* term_entry_;

 private:
  struct cookie_t final: public irs::seek_term_iterator::seek_cookie {
    store_reader_impl::term_entries_t::const_iterator itr_;
    cookie_t(const store_reader_impl::term_entries_t::const_iterator& itr)
      : itr_(itr) {}
  };

  irs::attribute_view attrs_;
  irs::frequency freq_;
  irs::term_meta meta_;
  store_reader_impl::term_entries_t::const_iterator next_itr_;
  irs::bytes_ref term_;
  const store_reader_impl::term_entries_t& terms_;
};

irs::doc_iterator::ptr store_reader_impl::column_reader_t::iterator() const {
  return entries_.empty()
    ? irs::doc_iterator::empty()
    : irs::doc_iterator::make<store_columnstore_iterator>(entries_)
    ;
}

irs::columnstore_reader::values_reader_f store_reader_impl::column_reader_t::values() const {
  if (entries_.empty()) {
    return irs::columnstore_reader::empty_reader();
  }

  // FIXME TODO find a better way to concatinate values, or modify API to return separate values
  auto reader = [this](
      irs::doc_id_t key, irs::bytes_ref& value, irs::bstring& value_buf
  )->bool {
    auto itr = std::lower_bound(entries_.begin(), entries_.end(), key, DOC_LESS);

    if (itr == entries_.end() || itr->doc_id_ != key || !itr->buf_) {
      return false; // no entry >= doc
    }

    bool multi_valued = false;
    auto next_offset = itr->offset_;

    value = irs::bytes_ref::NIL;
    value_buf.clear();

    while(next_offset) {
      assert(itr->buf_->size() >= next_offset); // otherwise there is an error somewhere in the code
      auto* ptr = &((*(itr->buf_))[next_offset]);
      auto offset = irs::read<uint64_t>(ptr); // read next value offset
      auto size = irs::vread<uint64_t>(ptr); // read value size
      auto start = next_offset - size;

      if (next_offset < size) {
        return false; // invalid data size
      }

      multi_valued |= offset > 0; // if have another value part
      next_offset = offset;

      if (multi_valued) {
        value_buf.append(itr->buf_->data() + start, size); // concatinate multi_valued attributes
        value = value_buf;
      } else {
        value = irs::bytes_ref(itr->buf_->data() + start, size);
      }
    }

    return true;
  };

  return std::bind(
    std::move(reader),
    std::placeholders::_1,
    std::placeholders::_2,
    irs::bstring()
  );
}

bool store_reader_impl::column_reader_t::visit(
    const irs::columnstore_reader::values_visitor_f& visitor
) const {
  for (auto& entry: entries_) {
    if (!(entry.buf_)) {
      continue;
    }

    auto doc_id = entry.doc_id_;

    for(auto next_offset = entry.offset_; next_offset;) {
      assert(entry.buf_->size() >= next_offset); // otherwise there is an error somewhere in the code
      auto* ptr = &((*(entry.buf_))[next_offset]);
      auto offset = next_offset;

      next_offset = irs::read<uint64_t>(ptr); // read next value offset

      auto size = irs::vread<uint64_t>(ptr); // read value size
      auto start = offset - size;

      if (offset < size) {
        break; // invalid data size
      }

      if (!visitor(doc_id, irs::bytes_ref(entry.buf_->data() + start, size))) {
        return false;
      }
    }
  }

  return true;
}

store_reader_impl::term_reader_t::term_reader_t(term_reader_t&& other) NOEXCEPT
  : attrs_(std::move(other.attrs_)),
    doc_count_(std::move(other.doc_count_)),
    max_term_(std::move(other.max_term_)),
    meta_(std::move(other.meta_)),
    min_term_(std::move(other.min_term_)),
    terms_(std::move(other.terms_)) {
}

irs::seek_term_iterator::ptr store_reader_impl::term_reader_t::iterator() const {
  return terms_.empty()
    ? irs::seek_term_iterator::make<empty_seek_term_iterator>()
    : irs::seek_term_iterator::make<store_term_iterator>(meta_->features, terms_);
}

store_reader_impl::store_reader_impl(
    const irs::transaction_store& store,
    irs::bitvector&& documents,
    fields_t&& fields,
    columns_named_t&& columns_named,
    columns_unnamed_t&& columns_unnamed,
    size_t generation
): columns_named_(std::move(columns_named)),
   columns_unnamed_(std::move(columns_unnamed)),
   documents_(std::move(documents)),
   fields_(std::move(fields)),
   generation_(generation),
   store_(store) {
  auto& column_by_id = const_cast<column_by_id_t&>(column_by_id_); // initialize map

  for (auto& entry: columns_named_) {
    auto& column = entry.second;

    column_by_id.emplace(column.meta_->id, &column);
  }

  for (auto& entry: columns_unnamed_) {
    column_by_id.emplace(entry.first, &(entry.second));
  }
}

const irs::column_meta* store_reader_impl::column(
    const irs::string_ref& name
) const {
  auto itr = columns_named_.find(name);

  return itr == columns_named_.end() ? nullptr : itr->second.meta_.get();
}

irs::column_iterator::ptr store_reader_impl::columns() const {
  auto ptr = irs::memory::make_unique<store_column_iterator>(columns_named_);

  return irs::memory::make_managed<irs::column_iterator, true>(std::move(ptr));
}

const irs::columnstore_reader::column_reader* store_reader_impl::column_reader(
    irs::field_id field
) const {
  auto itr = column_by_id_.find(field);

  return itr == column_by_id_.end() ? nullptr : itr->second;
}

irs::doc_iterator::ptr store_reader_impl::docs_iterator() const {
  return irs::memory::make_shared<irs::bitset_doc_iterator>(documents_);
}

const irs::term_reader* store_reader_impl::field(
    const irs::string_ref& field
) const {
  auto itr = fields_.find(field);

  return itr == fields_.end() ? nullptr : &(itr->second);
}

irs::field_iterator::ptr store_reader_impl::fields() const {
  auto ptr = irs::memory::make_unique<store_field_iterator>(fields_);

  return irs::memory::make_managed<irs::field_iterator, true>(std::move(ptr));
}

NS_END

NS_ROOT

class transaction_store::store_reader_helper {
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill reader state only for the specified documents
  /// @note caller must have read lock on store.mutex_
  ////////////////////////////////////////////////////////////////////////////////
  static size_t get_reader_state_unsafe(
      store_reader_impl::fields_t& fields,
      store_reader_impl::columns_named_t& columns_named,
      store_reader_impl::columns_unnamed_t& columns_unnamed,
      const transaction_store& store,
      const bitvector& documents
  ) {
    fields.clear();
    columns_named.clear();
    columns_unnamed.clear();

    // copy over non-empty columns into an ordered map
    for (auto& columns_entry: store.columns_named_) {
      store_reader_impl::document_entries_t entries;

      // copy over valid documents
      for (auto& entry: columns_entry.second.entries_) {
        if (entry.buf_ && documents.test(entry.doc_id_)) {
          entries.emplace_back(entry);
        }
      }

      if (entries.empty()) {
        continue; // no docs in column, skip
      }

      std::sort(entries.begin(), entries.end(), DOC_LESS); // sort by doc_id
      columns_named.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(columns_entry.first), // key
        std::forward_as_tuple(columns_entry.second.meta_, std::move(entries)) // value
      );
    }

    // copy over non-empty columns into an ordered map
    for (auto& columns_entry: store.columns_unnamed_) {
      store_reader_impl::document_entries_t entries;

      // copy over valid documents
      for (auto& entry: columns_entry.second.entries_) {
        if (entry.buf_ && documents.test(entry.doc_id_)) {
          entries.emplace_back(entry);
        }
      }

      if (entries.empty()) {
        continue; // no docs in column, skip
      }

      std::sort(entries.begin(), entries.end(), DOC_LESS); // sort by doc_id
      columns_unnamed.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(columns_entry.first), // key
        std::forward_as_tuple(std::move(entries)) // value
      );
    }

    // copy over non-empty fields into an ordered map
    for (auto& field_entry: store.fields_) {
      bitvector field_docs;
      store_reader_impl::term_reader_t terms(field_entry.second.meta_);

      // copy over non-empty terms into an ordered map
      for (auto& term_entry: field_entry.second.terms_) {
        store_reader_impl::document_entries_t postings;

        // copy over valid postings
        for (auto& entry: term_entry.second.entries_) {
          if (entry.buf_ && documents.test(entry.doc_id_)) {
            field_docs.set(entry.doc_id_);
            postings.emplace_back(entry);
          }
        }

        if (postings.empty()) {
          continue; // no docs in term, skip
        }

        std::sort(postings.begin(), postings.end(), DOC_LESS); // sort by doc_id

        auto& term = terms.terms_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(term_entry.first), // key
          std::forward_as_tuple(term_entry.second.name_, term_entry.second.meta_, std::move(postings)) // value
        ).first->first;

        if (terms.min_term_.null() || terms.min_term_ > term) {
          terms.min_term_ = term; // point at term in reader map
        }

        if (terms.max_term_.null() || terms.max_term_ < term) {
          terms.max_term_ = term; // point at term in reader map
        }
      }

      if (terms.terms_.empty()) {
        continue; // no terms in field, skip
      }

      terms.doc_count_ = field_docs.count();
      map_utils::try_emplace(
        fields,
        field_entry.first, // key
        std::move(terms) // value
      );
    }

    return store.generation_; // obtain store generation while under lock
  }
};

store_reader::store_reader(impl_ptr&& impl) NOEXCEPT
  : impl_(std::move(impl)) {
}

store_reader::store_reader(const store_reader& other) NOEXCEPT {
  *this = other;
}

store_reader& store_reader::operator=(const store_reader& other) NOEXCEPT {
  if (this != &other) {
    // make a copy
    impl_ptr impl = atomic_utils::atomic_load(&other.impl_);

    atomic_utils::atomic_store(&impl_, impl);
  }

  return *this;
}

store_reader store_reader::reopen() const {
  // make a copy
  impl_ptr impl = atomic_utils::atomic_load(&impl_);

  #ifdef IRESEARCH_DEBUG
    auto& reader_impl = dynamic_cast<const store_reader_impl&>(*impl);
  #else
    auto& reader_impl = static_cast<const store_reader_impl&>(*impl);
  #endif

  {
    async_utils::read_write_mutex::read_mutex mutex(reader_impl.store_.mutex_);
    SCOPED_LOCK(mutex);

    if (reader_impl.store_.generation_ == reader_impl.generation_) {
      return store_reader(std::move(impl)); // reuse same reader since there were no changes in store
    }
  }

  return reader_impl.store_.reader(); // store changed, create new reader
}

void store_writer::bstring_output::ensure(size_t pos) {
  if (pos >= buf_.size()) {
    irs::string_utils::oversize(buf_, irs::math::roundup_power2(pos + 1)); // 2*size growth policy, +1 for offset->size
  }
}

void store_writer::bstring_output::write(const byte_type* value, size_t size) {
  write_bytes_block(*this, value, size);
}

store_writer::store_writer(transaction_store& store) NOEXCEPT
  : next_doc_id_(type_limits<type_t::doc_id_t>::min()), store_(store) {
  async_utils::read_write_mutex::write_mutex mutex(store_.mutex_);
  SCOPED_LOCK(mutex);
  const_cast<transaction_store::reusable_t&>(reusable_) = store.reusable_; // init under lock
}

store_writer::~store_writer() {
  async_utils::read_write_mutex::write_mutex mutex(store_.mutex_);
  SCOPED_LOCK(mutex);

  // invalidate in 'store_.valid_doc_ids_' anything in 'used_doc_ids_'
  store_.valid_doc_ids_ -= used_doc_ids_; // free reserved doc_ids
}

bool store_writer::commit() {
  REGISTER_TIMER_DETAILED();

  // ensure doc_ids held by the transaction are always released
  auto cleanup = irs::make_finally([this]()->void {
    async_utils::read_write_mutex::write_mutex mutex(store_.mutex_);
    SCOPED_LOCK(mutex); // reobtain lock, ok since 'store_.commit_flush_mutex_' held

    // invalidate in 'store_.valid_doc_ids_' anything still in 'used_doc_ids_'
    store_.valid_doc_ids_ -= used_doc_ids_; // free reserved doc_ids
    // cannot remove from 'store_.used_doc_ids_' because docs are still in postings

    // reset for next run
    modification_queries_.clear();
    next_doc_id_ = type_limits<type_t::doc_id_t>::min();
    used_doc_ids_.clear();
    valid_doc_ids_.clear();
  });
  bitvector invalid_doc_ids;
  DEFER_SCOPED_LOCK_NAMED(store_.generation_mutex_, modified_lock); // prevent concurrent removals/updates and flush

  // apply removals
  if (!modification_queries_.empty()) {
    modified_lock.lock(); // prevent concurrent removals/updates (ensure reader stays valid until store state update is complete)

    async_utils::read_write_mutex::read_mutex mutex(store_.mutex_);
    SCOPED_LOCK(mutex); // reading 'store_.visible_docs_' and get_reader_state_unsafe(...)
    bitvector candidate_documents = used_doc_ids_; // all documents since some of them might be updates

    candidate_documents |= store_.visible_docs_; // all documents used by writer + all visible documents in store

    store_reader_impl::columns_named_t columns_named;
    store_reader_impl::columns_unnamed_t columns_unnamed;
    bitvector documents = store_.visible_docs_; // all visible doc ids from store + all visible doc ids from writer up to the current update generation
    store_reader_impl::fields_t fields;
    auto generation = transaction_store::store_reader_helper::get_reader_state_unsafe(
      fields, columns_named, columns_unnamed, store_, candidate_documents
    );
    bitvector processed_documents; // all visible doc ids from writer up to the current update generation
    store_reader_impl reader(
      store_,
      std::move(candidate_documents),
      std::move(fields),
      std::move(columns_named),
      std::move(columns_unnamed),
      generation
    );

    for (auto& entry: modification_queries_) {
      if (!entry.filter_) {
        continue; // skip null filters since they will not match anything (valid for indexing error during replacement insertion)
      }

      auto filter = entry.filter_->prepare(reader);

      if (!filter) {
        return false; // failed to prepare filter
      }

      auto itr = filter->execute(reader);

      if (!itr) {
        return false; // failed to execute filter
      }

      bool seen = false;

      processed_documents = valid_doc_ids_;
      processed_documents.resize(entry.generation_, true); // compute valid documents up to entry.generation_ (preserve capacity to bypass malloc)
      documents |= processed_documents; // include all visible documents from writer < entry.generation_

      while (itr->next()) {
        auto doc_id = itr->value(); // match was found

        // consider only documents visible up to this point in time
        if (documents.test(doc_id)) {
          seen = true; // mark query as seen if at least one document matched
          invalid_doc_ids.set(doc_id); // mark doc_id as no longer visible/used
        }
      }

      documents -= invalid_doc_ids; // clear no longer visible doc_ids from next generation pass
      valid_doc_ids_ -= invalid_doc_ids; // clear no longer visible doc_ids from writer

      // for successful updates mark replacement documents as visible
      if (seen) {
        documents |= entry.documents_;
        valid_doc_ids_ |= entry.documents_;
      }
    }
  }

  async_utils::read_write_mutex::write_mutex mutex(store_.mutex_);
  SCOPED_LOCK(mutex); // modifying 'store_.visible_docs_'

  if (!*reusable_) {
    return false; // this writer should not commit since store has been cleared or flushed with removals/updates applied by writer
  }

  // ensure modification operations below are noexcept
  store_.visible_docs_.reserve(std::max(valid_doc_ids_.size(), invalid_doc_ids.size()));
  used_doc_ids_.reserve(std::max(valid_doc_ids_.size(), invalid_doc_ids.size()));

  ++(store_.generation_); // mark store state as modified
  store_.visible_docs_ |= valid_doc_ids_; // commit doc_ids
  store_.visible_docs_ -= invalid_doc_ids; // commit removals
  used_doc_ids_ -= valid_doc_ids_; // exclude 'valid' from 'used' (so commited docs would remain valid when transaction is cleaned up)
  used_doc_ids_ |= invalid_doc_ids; // include 'invalid' into 'used' (so removed docs would be invalidated when transaction is cleaned up)

  return true;
}

bool store_writer::index(
    bstring_output& out,
    document::state_t& state,
    const hashed_string_ref& field_name,
    const flags& field_features,
    transaction_store::document_t& doc,
    token_stream& tokens
) {
  REGISTER_TIMER_DETAILED();
  auto& attrs = tokens.attributes();
  auto& term = attrs.get<term_attribute>();
  auto& inc = attrs.get<increment>();
  auto& offs = attrs.get<offset>();
  auto& pay = attrs.get<payload>();

  if (!inc) {
    IR_FRMT_ERROR(
      "field '%s' missing required token_stream attribute '%s'",
      field_name.c_str(), increment::type().name().c_str()
    );
    return false;
  }

  if (!term) {
    IR_FRMT_ERROR(
      "field '%s' missing required token_stream attribute '%s'",
      field_name.c_str(), term_attribute::type().name().c_str()
    );
    return false;
  }

  auto field = store_.get_field(field_name, field_features);

  if (!field) {
    IR_FRMT_ERROR(
      "failed to reserve field '%s' for token insertion",
      field_name.c_str()
    );
    return false;
  }

  bool has_freq = field->meta_->features.check<frequency>();
  bool has_offs = has_freq && field->meta_->features.check<offset>() && offs;
  bool has_pay = has_offs && pay;
  bool has_pos = field->meta_->features.check<position>();
  auto& doc_state_offset = map_utils::try_emplace(
    state.offsets_,
    &doc, // key
    irs::integer_traits<size_t>::max() // invalid offset
  ).first->second;

  // write out document stats if they were not written yet
  if (irs::integer_traits<size_t>::const_max == doc_state_offset) {
    static const doc_stats_t initial;

    doc_state_offset = state.out_.file_pointer();
    write_bytes_block(state.out_, initial);
  }

  auto& field_state_offset = map_utils::try_emplace(
    state.offsets_,
    &*field, // key
    irs::integer_traits<size_t>::max() // invalid offset
  ).first->second;

  // write out field stats if they were not written yet
  if (irs::integer_traits<size_t>::const_max == field_state_offset) {
    static const field_stats_t initial;

    field_state_offset = state.out_.file_pointer();
    write_bytes_block(state.out_, initial);
  }

  while (tokens.next()) {
    size_t term_state_offset; // offset to term state in state buffer

    // insert term if not present and insert document buffer into its postings
    {
      static auto generator = [](
        const hashed_bytes_ref& key,
        const transaction_store::postings_t& value
      ) NOEXCEPT ->hashed_bytes_ref {
        // reuse hash but point ref at value if buffer was allocated succesfully
        return value.name_ ? hashed_bytes_ref(key.hash(), *(value.name_)) : key;
      };

      async_utils::read_write_mutex::write_mutex mutex(store_.mutex_);
      SCOPED_LOCK(mutex);
      auto field_term_itr = map_utils::try_emplace_update_key(
        field->terms_,
        generator,
        make_hashed_ref(term->value(), std::hash<irs::bytes_ref>()), // key
        store_.bstring_pool_, term->value() // value
      );
      auto& field_term = field_term_itr.first->second;

      // new term was inserted which failed to initialize its buffer
      if (field_term_itr.second && !field_term.name_) {
        field->terms_.erase(field_term_itr.first);
        IR_FRMT_ERROR(
          "failed to allocate buffer for term name while indexing new term: %s",
          std::string(ref_cast<char>(term->value()).c_str(), term->value().size()).c_str()
        );

        return false;
      }

      auto& term_state_offset_ref = map_utils::try_emplace(
        state.offsets_,
        &field_term, // key
        irs::integer_traits<size_t>::max() // invalid offset
      ).first->second;

      // if this is the first time this term was seen for this document
      if (irs::integer_traits<size_t>::const_max == term_state_offset_ref) {
        field_term.entries_.emplace_back(doc, out.file_pointer()); // term offset in buffer

        static const term_stats_t initial;

        term_state_offset_ref = state.out_.file_pointer();
        write_bytes_block(state.out_, initial);
        ++(reinterpret_cast<field_stats_t&>(state.out_[field_state_offset]).unq_term_count);
      }

      term_state_offset = term_state_offset_ref; // remember offset outside this block
    }

    // get references to meta once state is no longer resized
    auto& document_state = reinterpret_cast<doc_stats_t&>(state.out_[doc_state_offset]);
    auto& field_state = reinterpret_cast<field_stats_t&>(state.out_[field_state_offset]);

    field_state.pos += inc->value;

    if (field_state.pos < field_state.pos_last) {
      IR_FRMT_ERROR("invalid position %u < %u", field_state.pos, field_state.pos_last);
      return false; // doc_id will be marked not valid by caller, hence in rollback state
    }

    if (!(inc->value)) {
      ++(field_state.num_overlap); // pos == pos_last
    }

    field_state.pos_last = field_state.pos;

    if (has_offs) {
      const auto offs_start = field_state.offs_start_base + offs->start;
      const auto offs_end = field_state.offs_start_base + offs->end;

      // current term absolute offset start is before the previous term absolute
      // offset start or term-offset-end < term-offset-start
      if (offs_start < field_state.offs_start_term || offs_end < offs_start) {
        IR_FRMT_ERROR("invalid offset start=%u end=%u", offs_start, offs_end);
        return false; // doc_id will be marked not valid by caller, hence in rollback state
      }

      field_state.offs_start_term = offs_start;
    }

    if (0 == ++(document_state.term_count)) {
      IR_FRMT_ERROR("too many token in field, document '" IR_UINT32_T_SPECIFIER "'", doc.doc_id_);
      return false; // doc_id will be marked not valid by caller, hence in rollback state
    }

    auto& term_state = reinterpret_cast<term_stats_t&>(state.out_[term_state_offset]);
    auto term_start = out.file_pointer(); // remeber start of current term

    //...........................................................................
    // encoded buffer header definition:
    // byte - reserved for internal use
    //
    // encoded block format definition:
    // long   - pointer to the next entry for same doc-term, last == 0 (length of linked list == term frequency in current document)
    // vint   - position     (present if field.meta.features.check<position>() == true)
    // vint   - offset start (present if field.meta.features.check<offset>() == true)
    // vint   - offset end   (present if field.meta.features.check<offset>() == true)
    // byte   - 0 => nullptr payload, !0 => payload follows next
    // string - size + payload (present if previous byte != 0)
    //...........................................................................
    irs::write<uint64_t>(out, 0); // write out placeholder for next entry
    field_state.max_term_freq = std::max(++term_state.term_freq, field_state.max_term_freq);

    if (has_pos) {
      irs::vwrite<uint32_t>(out, field_state.pos); // write out position
    }

    if (has_offs) {
      // add field_state.offs_start_base to shift offsets for repeating terms (no offset overlap)
      irs::vwrite<uint32_t>(out, field_state.offs_start_base + offs->start); // write offset start
      irs::vwrite<uint32_t>(out, field_state.offs_start_base + offs->end); // write offset end
    }

    irs::write<uint8_t>(out, has_pay ? 1 : 0); // write has-payload flag

    if (has_pay) {
      vwrite_string_block(out, pay->value);
    }

    if (term_state.offset) {
      auto* ptr = &(out[term_state.offset]);

      irs::write<uint64_t>(ptr, term_start); // update 'next-pointer' of previous entry
    }

    term_state.offset = term_start; // remeber start of current term
  }

  // get references to meta once state is no longer resized
  auto& document_state = reinterpret_cast<doc_stats_t&>(state.out_[doc_state_offset]);
  auto& field_state = reinterpret_cast<field_stats_t&>(state.out_[field_state_offset]);

  if (offs) {
    field_state.offs_start_base += offs->end; // ending offset of last term
  }

  if (field->meta_->features.check<norm>()) {
    document_state.norm =
      1.f / float_t(std::sqrt(double_t(document_state.term_count)));
  }

  return true;
}

void store_writer::remove(const filter& filter) {
  modification_queries_.emplace_back(filter, next_doc_id_);
}

void store_writer::remove(filter::ptr&& filter) {
  if (filter) {
    modification_queries_.emplace_back(std::move(filter), next_doc_id_); // skip empty filters
  }
}

void store_writer::remove(const std::shared_ptr<filter>& filter) {
  if (filter) {
    modification_queries_.emplace_back(filter, next_doc_id_); // skip empty filters
  }
}

bool store_writer::store(
    bstring_output& out,
    document::state_t& state,
    const hashed_string_ref& column_name,
    transaction_store::document_t& doc,
    size_t buf_offset
) {
  REGISTER_TIMER_DETAILED();
  auto column = store_.get_column(column_name);

  if (!column) {
    IR_FRMT_ERROR(
      "failed to reserve column '%s' for data insertion",
      column_name.c_str()
    );
    return false;
  }

  auto& column_state_offset = map_utils::try_emplace(
    state.offsets_,
    &*column, // key
    irs::integer_traits<size_t>::max() // invalid offset
  ).first->second;

  // if this is the first time this column was seen for this document
  if (irs::integer_traits<size_t>::const_max == column_state_offset) {
    {
      async_utils::read_write_mutex::write_mutex mutex(store_.mutex_);
      SCOPED_LOCK(mutex);
      column->entries_.emplace_back(doc, out.file_pointer()); // column offset in buffer
    }

    static const column_stats_t initial;

    column_state_offset = state.out_.file_pointer(); // same as in 'entries_'
    write_bytes(state.out_, initial);
  }

  auto& column_state = reinterpret_cast<column_stats_t&>(state.out_[column_state_offset]);
  auto column_start = out.file_pointer(); // remeber start of current column

  //...........................................................................
  // encoded buffer header definition:
  // byte - reserved for internal use
  //
  // encoded block format definition:
  // bytes  - user data
  // long   - pointer to the next entry (points at its 'next-pointer') for same doc-term, last == 0 (length of linked list == column frequency in current document)
  // vlong  - delta length of user data up to 'next-pointer'
  //...........................................................................
  irs::write<uint64_t>(out, 0); // write out placeholder for next entry
  irs::vwrite<uint64_t>(out, column_start - buf_offset); // write out delta to start of data

  if (column_state.offset) {
    auto* ptr = &(out[column_state.offset]);

    irs::write<uint64_t>(ptr, column_start); // update 'next-pointer' of previous entry
  }

  column_state.offset = column_start; // remeber start of current column

  return true;
}

/*static*/ transaction_store::bstring_builder::ptr transaction_store::bstring_builder::make() {
  return irs::memory::make_shared<bstring>(DEFAULT_BUFFER_SIZE, '\0');
}

/*static*/ transaction_store::column_meta_builder::ptr transaction_store::column_meta_builder::make() {
  return irs::memory::make_shared<column_meta>();
}

/*static*/ transaction_store::field_meta_builder::ptr transaction_store::field_meta_builder::make() {
  return irs::memory::make_shared<field_meta>();
}

transaction_store::transaction_store(size_t pool_size /*= DEFAULT_POOL_SIZE*/)
  : bstring_pool_(pool_size),
    column_meta_pool_(pool_size),
    field_meta_pool_(pool_size),
    generation_(0),
    reusable_(memory::make_shared<bool>(true)),
    used_doc_ids_(type_limits<type_t::doc_id_t>::invalid() + 1),
    visible_docs_(type_limits<type_t::doc_id_t>::invalid() + 1) { // same size as used_doc_ids_
  used_doc_ids_.set(type_limits<type_t::doc_id_t>::invalid()); // mark as always in-use
}

void transaction_store::cleanup() {
  async_utils::read_write_mutex::write_mutex mutex(mutex_);
  SCOPED_LOCK(mutex);

  used_doc_ids_ &= valid_doc_ids_; // remove invalid ids from 'used'

  // remove unused records from named user columns
  for (auto itr = columns_named_.begin(), end = columns_named_.end(); itr != end;) {
    auto& column = itr->second;
    size_t last = column.entries_.size() - 1;

    for (auto i = column.entries_.size(); i;) {
      auto& record = column.entries_[--i];

      if (used_doc_ids_.test(record.doc_id_)) {
        continue; // record still in use
      }

      record = std::move(column.entries_[last--]); // replace unused record
      column.entries_.pop_back(); // remove moved record
    }

    if (!column.entries_.empty() || column.refs_) {
      ++itr;
      continue; // column still in use
    }

    used_column_ids_.unset(column.meta_->id); // release column id
    itr = columns_named_.erase(itr);
  }

  // remove unused records from system columns
  for (auto itr = columns_unnamed_.begin(), end = columns_unnamed_.end(); itr != end;) {
    auto& column = itr->second;
    auto id = itr->first;
    size_t last = column.entries_.size() - 1;

    for (auto i = column.entries_.size(); i;) {
      auto& record = column.entries_[--i];

      if (used_doc_ids_.test(record.doc_id_)) {
        continue; // record still in use
      }

      record = std::move(column.entries_[last--]); // replace unused record
      column.entries_.pop_back(); // remove moved record
    }

    if (!column.entries_.empty() || column.refs_) {
      ++itr;
      continue; // column still in use
    }

    used_column_ids_.unset(id); // release column id
    itr = columns_unnamed_.erase(itr);
  }

  // remove unused records from fields
  for (auto itr = fields_.begin(), end = fields_.end(); itr != end;) {
    auto& field = itr->second;

    for (auto term_itr = field.terms_.begin(), terms_end = field.terms_.end();
         term_itr != terms_end;
        ) {
      auto& term = term_itr->second;
      size_t last = term.entries_.size() - 1;

      for (auto i = term.entries_.size(); i;) {
        auto& record = term.entries_[--i];

        if (used_doc_ids_.test(record.doc_id_)) {
          continue; // record still in use
        }

        record = std::move(term.entries_[last--]); // replace unused record
        term.entries_.pop_back(); // remove moved record
      }

      if (!term.entries_.empty()) {
        ++term_itr;
        continue; // term still in use
      }

      term_itr = field.terms_.erase(term_itr);
    }

    if (!field.terms_.empty() || field.refs_) {
      ++itr;
      continue; // field still in use
    }

    itr = fields_.erase(itr);
  }
}

void transaction_store::clear() {
  async_utils::read_write_mutex::write_mutex mutex(mutex_);
  SCOPED_LOCK(mutex);
  auto reusable = memory::make_shared<bool>(true); // create marker for next generation

  *reusable_ = false; // prevent existing writers from commiting into the store
  reusable_ = std::move(reusable); // mark new generation
  visible_docs_.clear(); // mark all documents are non-visible
}

store_reader transaction_store::flush() {
  SCOPED_LOCK(generation_mutex_); // lock generation modification until end of flush (or in-progress writer commit with removals/updates will have an inconsistent reader)
  async_utils::read_write_mutex::write_mutex mutex(mutex_);
  SCOPED_LOCK(mutex);
  auto reader = transaction_store::reader();

  // if a reader with a flushed state was created successfully
  if (reader) {
    ++generation_; // mark state as modified
    valid_doc_ids_ -= visible_docs_; // remove flushed ids from 'valid'
    visible_docs_.clear(); // remove flushed ids from 'visible'
  }

  return reader;
}

transaction_store::column_ref_t transaction_store::get_column(
    const hashed_string_ref& name
) {
  REGISTER_TIMER_DETAILED();
  static auto generator = [](
    const hashed_string_ref& key,
    const transaction_store::column_named_t& value
  ) NOEXCEPT ->hashed_string_ref {
    // reuse hash but point ref at value if buffer was allocated succesfully
    return value.meta_ ? hashed_string_ref(key.hash(), value.meta_->name) : key;
  };

  async_utils::read_write_mutex::write_mutex mutex(mutex_);
  SCOPED_LOCK(mutex);

  auto itr = map_utils::try_emplace_update_key(
    columns_named_,
    generator,
    name, // key
    column_meta_pool_, name, type_limits<type_t::field_id_t>::invalid() // value
  );
  auto& column = itr.first->second;

  // new column was inserted, assign column id
  if (itr.second) {
    // new column was inserted which failed to initialize its meta
    if (!column.meta_) {
      columns_named_.erase(itr.first);
      IR_FRMT_ERROR(
        "failed to allocate buffer for column meta while indexing new column: %s",
        std::string(name.c_str(), name.size()).c_str()
      );
    }

    column.meta_->id = get_column_id();

    if (!type_limits<type_t::field_id_t>::valid(column.meta_->id)) {
      columns_named_.erase(itr.first);

      return column_ref_t();
    }
  }

  // increment ref counter under write lock to coordinate with flush(...)
  return column_ref_t(column);
}

field_id transaction_store::get_column_id() {
  REGISTER_TIMER_DETAILED();
  field_id start = 0;
  async_utils::read_write_mutex::write_mutex mutex(mutex_);
  SCOPED_LOCK(mutex);

  while (type_limits<type_t::field_id_t>::valid(start)) {
    if (!used_column_ids_.test(start)) {
      used_column_ids_.set(start);

      return start;
    }

    // optimization to skip over words with all bits set
    start = *(used_column_ids_.begin() + bitset::word(start)) == bitset::word_t(-1)
      ? field_id(bitset::bit_offset(bitset::word(start) + 1)) // set to first bit of next word (will not overflow due toc heck above)
      : (start + 1)
      ;
  }

  return type_limits<type_t::field_id_t>::invalid();
}

doc_id_t transaction_store::get_doc_id(doc_id_t start) {
  REGISTER_TIMER_DETAILED();
  if (start == type_limits<type_t::doc_id_t>::eof() ||
      start == type_limits<type_t::doc_id_t>::invalid()) {
    return type_limits<type_t::doc_id_t>::invalid();
  }

  async_utils::read_write_mutex::write_mutex mutex(mutex_);
  SCOPED_LOCK(mutex);

  while (!type_limits<type_t::doc_id_t>::eof(start)) {
    if (!used_doc_ids_.test(start)) {
      visible_docs_.reserve(start); // ensure all allocation happends here
      used_doc_ids_.set(start);
      valid_doc_ids_.set(start);

      return start;
    }

    // optimization to skip over words with all bits set
    start = *(used_doc_ids_.begin() + bitset::word(start)) == bitset::word_t(-1)
      ? bitset::bit_offset(bitset::word(start) + 1) // set to first bit of next word
      : (start + 1)
      ;
  }

  return type_limits<type_t::doc_id_t>::invalid();
}

transaction_store::field_ref_t transaction_store::get_field(
    const hashed_string_ref& name,
    const flags& features
) {
  REGISTER_TIMER_DETAILED();
  static auto generator = [](
    const hashed_string_ref& key,
    const transaction_store::terms_t& value
  ) NOEXCEPT ->hashed_string_ref {
    // reuse hash but point ref at value if buffer was allocated succesfully
    return value.meta_ ? hashed_string_ref(key.hash(), value.meta_->name) : key;
  };

  async_utils::read_write_mutex::write_mutex mutex(mutex_);
  SCOPED_LOCK(mutex);
  auto itr = map_utils::try_emplace_update_key(
    fields_,
    generator,
    name, // key
    field_meta_pool_, name, features // value
  );
  auto& field = itr.first->second;

  // new field was inserted
  if (itr.second) {
    // new field was inserted which failed to initialize its meta
    if (!field.meta_) {
      fields_.erase(itr.first);
      IR_FRMT_ERROR(
        "failed to allocate buffer for field meta while indexing new field: %s",
        std::string(name.c_str(), name.size()).c_str()
      );

      return field_ref_t();
    }

    field.meta_->features |= features;

    // if 'norm' required then create the corresponding 'norm' column
    if (field.meta_->features.check<irs::norm>()) {
      auto norm_col_id = get_column_id();

      if (!type_limits<type_t::field_id_t>::valid(norm_col_id)) {
        fields_.erase(itr.first);

        return field_ref_t();
      }

      field.norm_col_ref_ = ref_t<column_t>(columns_unnamed_[norm_col_id]);
    }

    // increment ref counter under write lock to coordinate with flush(...)
    return field_ref_t(field);
  }

  // increment ref counter under write lock to coordinate with flush(...)
  return features.is_subset_of(field.meta_->features)
    ? field_ref_t(field)
    : field_ref_t() // new field features are not a subset of existing field features
    ;
}

store_reader transaction_store::reader() const {
  REGISTER_TIMER_DETAILED();
  store_reader_impl::columns_named_t columns_named;
  store_reader_impl::columns_unnamed_t columns_unnamed;
  bitvector documents;
  store_reader_impl::fields_t fields;
  size_t generation;

  {
    async_utils::read_write_mutex::read_mutex mutex(mutex_);
    SCOPED_LOCK(mutex);
    documents = visible_docs_;
    generation = transaction_store::store_reader_helper::get_reader_state_unsafe(
      fields, columns_named, columns_unnamed, *this, documents
    );
  }

  documents.shrink_to_fit();

  PTR_NAMED(
    store_reader_impl,
    reader,
    *this,
    std::move(documents),
    std::move(fields),
    std::move(columns_named),
    std::move(columns_unnamed),
    generation
  );

  return store_reader(std::move(reader));
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
