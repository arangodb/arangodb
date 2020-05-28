////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_PYRESEARCH_H
#define IRESEARCH_PYRESEARCH_H

#include "index/index_reader.hpp"
#include "index/field_meta.hpp"

#ifdef SWIG
#define SWIG_noexcept
#else
#define SWIG_noexcept noexcept
#endif

struct invalid_feature { };

#ifdef PYRESEARCH_DLL
#define PYRESEARCH_API __declspec(dllexport)
#else
#define PYRESEARCH_API
#endif

///////////////////////////////////////////////////////////////////////////////
/// @class doc_iterator
/// @brief python proxy for irs::doc_iterator
///////////////////////////////////////////////////////////////////////////////
class PYRESEARCH_API doc_iterator {
 public:
  ~doc_iterator() SWIG_noexcept { }

  bool next() { return it_->next(); }

  uint32_t seek(uint32_t target) {
    return it_->seek(target);
  }

  uint32_t value() const {
    return it_->value();
  }

 private:
  friend class column_reader;
  friend class term_iterator;
  friend class segment_reader;

  doc_iterator(irs::doc_iterator::ptr it) SWIG_noexcept 
    : it_(it) {
  }

  irs::doc_iterator::ptr it_;
}; // doc_iterator

///////////////////////////////////////////////////////////////////////////////
/// @class term_iterator
/// @brief python proxy for irs::term_iterator
///////////////////////////////////////////////////////////////////////////////
class PYRESEARCH_API  term_iterator {
 public:
  ~term_iterator() SWIG_noexcept { }

  bool next() { return it_->next(); }
  doc_iterator postings(
    const std::vector<std::string>& features = std::vector<std::string>()
  ) const;
  bool seek(irs::string_ref term) {
    return it_->seek(irs::ref_cast<irs::byte_type>(term));
  }
  uint32_t seek_ge(irs::string_ref term) {
    typedef std::underlying_type<irs::SeekResult>::type type;

    static_assert(
      std::is_convertible<type, uint32_t>::value,
      "types are not equal"
    );

    return static_cast<type>(
      it_->seek_ge(irs::ref_cast<irs::byte_type>(term))
    );
  }
  irs::bytes_ref value() const { return it_->value(); }

 private:
  friend class field_reader;

  term_iterator(irs::seek_term_iterator::ptr&& it) SWIG_noexcept 
    :it_(irs::memory::make_shared(std::move(it))) {
  }

  std::shared_ptr<irs::seek_term_iterator> it_;
}; // term_iterator

///////////////////////////////////////////////////////////////////////////////
/// @class column_meta
/// @brief python proxy for irs::column_meta
///////////////////////////////////////////////////////////////////////////////
class PYRESEARCH_API  column_meta {
 public:
  ~column_meta() SWIG_noexcept { }

  irs::string_ref name() const SWIG_noexcept { return meta_->name; }
  uint64_t id() const SWIG_noexcept { return meta_->id; }

 private:
  friend class column_iterator;

  column_meta(const irs::column_meta* meta) SWIG_noexcept 
    : meta_(meta) {
  }

  const irs::column_meta* meta_;
}; // column_meta

///////////////////////////////////////////////////////////////////////////////
/// @class column_iterator
/// @brief python proxy for irs::column_iterator
///////////////////////////////////////////////////////////////////////////////
class PYRESEARCH_API  column_iterator {
 public:
  ~column_iterator() SWIG_noexcept { }

  bool next() { return it_->next(); }
  bool seek(irs::string_ref column) {
    return it_->seek(column);
  }
  column_meta value() const { return &it_->value(); }

 private:
  friend class segment_reader;

  column_iterator(irs::column_iterator::ptr&& it) SWIG_noexcept 
    : it_(it.release(), std::move(it.get_deleter())) {
  }

  std::shared_ptr<irs::column_iterator> it_;
}; // column_iterator

///////////////////////////////////////////////////////////////////////////////
/// @class column_values_reader
/// @brief python proxy for irs::columnstore_reader::values_reader_f
///////////////////////////////////////////////////////////////////////////////
class PYRESEARCH_API  column_values_reader {
 public:
  ~column_values_reader() SWIG_noexcept { }

  std::pair<bool, irs::bytes_ref> get(uint32_t key) {
    irs::bytes_ref value;
    const bool found = reader_(key, value);
    return std::make_pair(found, value);
  }

  bool has(uint32_t key) const {
    irs::bytes_ref value;
    return reader_(key, value);
  }

 private:
  friend class column_reader;

  column_values_reader(irs::columnstore_reader::values_reader_f&& reader) SWIG_noexcept 
    : reader_(std::move(reader)) {
  }

  irs::columnstore_reader::values_reader_f reader_;
};

///////////////////////////////////////////////////////////////////////////////
/// @class column_reader
/// @brief python proxy for irs::columnstore_reader::column_reader
///////////////////////////////////////////////////////////////////////////////
class PYRESEARCH_API  column_reader {
 public:
  ~column_reader() SWIG_noexcept { }

  doc_iterator iterator() const {
    assert(reader_);
    return reader_->iterator();
  }

  column_values_reader values() const {
    assert(reader_);
    return reader_->values();
  }

  operator bool() const SWIG_noexcept {
    return nullptr != reader_;
  }

 private:
  friend class segment_reader;

  column_reader(const irs::columnstore_reader::column_reader* reader) SWIG_noexcept 
    : reader_(reader) {
  }

  const irs::columnstore_reader::column_reader* reader_;
}; // column_reader

///////////////////////////////////////////////////////////////////////////////
/// @class field_reader
/// @brief python proxy for irs::term_reader
///////////////////////////////////////////////////////////////////////////////
class PYRESEARCH_API  field_reader {
 public:
  ~field_reader() SWIG_noexcept { }

  size_t docs_count() const {
    assert(field_);
    return field_->docs_count(); }
  std::vector<std::string> features() const;
  term_iterator iterator() const {
    assert(field_);
    return field_->iterator();
  }
  irs::string_ref name() const {
    assert(field_);
    return field_->meta().name;
  }
  uint64_t norm() const {
    assert(field_);
    return field_->meta().norm;
  }
  size_t size() const {
    assert(field_);
    return field_->size();
  }
  irs::bytes_ref min() const {
    assert(field_);
    return field_->min();
  }
  irs::bytes_ref max() const { 
    assert(field_);
    return field_->max();
  }
  operator bool() const SWIG_noexcept {
    return nullptr != field_; 
  }

 private:
  friend class segment_reader;
  friend class field_iterator;

  field_reader(const irs::term_reader* field) SWIG_noexcept 
    : field_(field) {
  }

  const irs::term_reader* field_;
}; // field_reader

///////////////////////////////////////////////////////////////////////////////
/// @class field_iterator
/// @brief python proxy for irs::field_iterator
///////////////////////////////////////////////////////////////////////////////
class PYRESEARCH_API  field_iterator {
 public:
  ~field_iterator() SWIG_noexcept { }

  bool seek(irs::string_ref field) {
    return it_->seek(field);
  }
  bool next() { return it_->next(); }
  field_reader value() const { return &it_->value(); }

 private:
  friend class segment_reader;

  field_iterator(irs::field_iterator::ptr&& it) SWIG_noexcept
    : it_(it.release(), std::move(it.get_deleter())) {
  }

  std::shared_ptr<irs::field_iterator> it_;
}; // field_iterator

///////////////////////////////////////////////////////////////////////////////
/// @class segment_reader
/// @brief python proxy for irs::segment_reader
///////////////////////////////////////////////////////////////////////////////
class PYRESEARCH_API  segment_reader {
 public:
  ~segment_reader() SWIG_noexcept { }

  column_iterator columns() const { return reader_->columns(); }
  column_reader column(uint64_t id) const { return reader_->column_reader(id); }
  column_reader column(irs::string_ref column) const {
    return reader_->column_reader(column);
  }
  size_t docs_count() const { return reader_->docs_count(); }
  doc_iterator docs_iterator() const { 
    return reader_->mask(reader_->docs_iterator()); 
  }
  field_reader field(irs::string_ref field) const {
    return reader_->field(field);
  }
  field_iterator fields() const { return reader_->fields(); }
  size_t live_docs_count() const { return reader_->live_docs_count(); }
  doc_iterator live_docs_iterator() const { return reader_->docs_iterator(); }
  bool valid() const SWIG_noexcept { return nullptr != reader_; }

 private:
  friend class index_reader;
  friend class segment_iterator;

  segment_reader(irs::sub_reader::ptr reader) SWIG_noexcept
    : reader_(std::move(reader)) {
  }

  irs::sub_reader::ptr reader_;
}; // segment_reader

///////////////////////////////////////////////////////////////////////////////
/// @class segment_iterator
/// @brief python iterator for index_reader proxy
///////////////////////////////////////////////////////////////////////////////
class PYRESEARCH_API   segment_iterator {
 public:
  segment_reader next() {
    if (begin_ == end_) {
      return segment_reader(nullptr);
    }

    auto* segment = *begin_;
    ++begin_;

    return irs::sub_reader::ptr(
      irs::sub_reader::ptr(),
      segment
    );
  }

 private:
  friend class index_reader;

  typedef std::vector<const irs::sub_reader*>::const_iterator iterator_t;

  segment_iterator(iterator_t begin, iterator_t end) SWIG_noexcept
    : begin_(begin), end_(end) {
  }

  iterator_t begin_;
  iterator_t end_;
}; // segment_iterator

///////////////////////////////////////////////////////////////////////////////
/// @class index_reader
/// @brief python proxy for irs::index_reader
///////////////////////////////////////////////////////////////////////////////
class PYRESEARCH_API  index_reader {
 public:
  static index_reader open(const char* path);

  ~index_reader() SWIG_noexcept { }

  segment_reader segment(size_t i) const;
  size_t docs_count() const { return reader_->docs_count(); }
  size_t live_docs_count() const { return reader_->live_docs_count(); }
  size_t size() const { return reader_->size(); }

  segment_iterator iterator() const SWIG_noexcept {
    return segment_iterator(segments_.begin(), segments_.end());
  }

 private:
  index_reader(irs::index_reader::ptr reader);

  irs::index_reader::ptr reader_;
  std::vector<const irs::sub_reader*> segments_;
}; // index_reader

#endif // IRESEARCH_PYRESEARCH_H
