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

#include "shared.hpp"
#include "segment_reader.hpp"

#include "analysis/token_attributes.hpp"

#include "index/index_meta.hpp"

#include "formats/format_utils.hpp"
#include "utils/index_utils.hpp"
#include "utils/singleton.hpp"
#include "utils/type_limits.hpp"

#include <unordered_map>

namespace {

class all_iterator final : public irs::doc_iterator {
 public:
  explicit all_iterator(irs::doc_id_t docs_count) noexcept
    : max_doc_(irs::doc_id_t(irs::doc_limits::min() + docs_count - 1)) {
  }

  virtual bool next() noexcept override {
    if (doc_.value >= max_doc_) {
      doc_.value = irs::doc_limits::eof();
      return false;
    } else {
      doc_.value++;
      return true;
    }
  }

  virtual irs::doc_id_t seek(irs::doc_id_t target) noexcept override {
    doc_.value = target <= max_doc_
      ? target
      : irs::doc_limits::eof();

    return doc_.value;
  }

  virtual irs::doc_id_t value() const noexcept override {
    return doc_.value;
  }

  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::type<irs::document>::id() == type ? &doc_ : nullptr;
  }

 private:
  irs::document doc_;
  irs::doc_id_t max_doc_; // largest valid doc_id
}; // all_iterator

class mask_doc_iterator final : public irs::doc_iterator {
 public:
  explicit mask_doc_iterator(
      irs::doc_iterator::ptr&& it,
      const irs::document_mask& mask) noexcept
    : mask_(mask), it_(std::move(it))  {
  }

  virtual bool next() override {
    while (it_->next()) {
      if (mask_.find(value()) == mask_.end()) {
        return true;
      }
    }

    return false;
  }

  virtual irs::doc_id_t seek(irs::doc_id_t target) override {
    const auto doc = it_->seek(target);

    if (mask_.find(doc) == mask_.end()) {
      return doc;
    }

    next();

    return value();
  }

  virtual irs::doc_id_t value() const override {
    return it_->value();
  }

  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return it_->get_mutable(type);
  }

 private:
  const irs::document_mask& mask_; // excluded document ids
  irs::doc_iterator::ptr it_;
}; // mask_doc_iterator

class masked_docs_iterator 
    : public irs::doc_iterator,
      private irs::util::noncopyable {
 public:
  masked_docs_iterator(
    irs::doc_id_t begin,
    irs::doc_id_t end,
    const irs::document_mask& docs_mask)
  : docs_mask_(docs_mask),
    end_(end),
    next_(begin) {
  }

  virtual bool next() override {
    while (next_ < end_) {
      current_.value = next_++;

      if (docs_mask_.find(current_.value) == docs_mask_.end()) {
        return true;
      }
    }

    current_.value = irs::doc_limits::eof();

    return false;
  }

  virtual irs::doc_id_t seek(irs::doc_id_t target) override {
    next_ = target;
    next();

    return value();
  }

  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::type<irs::document>::id() == type ? &current_ : nullptr;
  }

  virtual irs::doc_id_t value() const override {
    return current_.value;
  }

 private:
  irs::document current_;
  const irs::document_mask& docs_mask_;
  const irs::doc_id_t end_; // past last valid doc_id
  irs::doc_id_t next_;
};

bool read_columns_meta(
    const irs::format& codec,
    const irs::directory& dir,
    const irs::segment_meta& meta,
    std::vector<irs::column_meta>& columns,
    std::vector<irs::column_meta*>& id_to_column,
    std::unordered_map<irs::hashed_string_ref, irs::column_meta*>& name_to_column) {
  size_t count = 0;
  irs::field_id max_id;
  auto reader = codec.get_column_meta_reader();

  if (!reader->prepare(dir, meta, count, max_id)) {
    // no column meta found in a segment
    return false;
  }

  columns.reserve(count);
  id_to_column.resize(max_id + 1); // +1 for count
  name_to_column.reserve(count);

  for (irs::column_meta col_meta; reader->read(col_meta);) {
    columns.emplace_back(std::move(col_meta));

    auto& column = columns.back();
    const auto res = name_to_column.emplace(
      irs::make_hashed_ref(irs::string_ref(column.name)),
      &column);

    assert(column.id < id_to_column.size());

    if (!res.second || id_to_column[column.id]) {
      throw irs::index_error(irs::string_utils::to_string(
        "duplicate column '%s' in segment '%s'",
        column.name.c_str(),
        meta.name.c_str()
      ));
    }

    id_to_column[column.id] = &column;
  }

  // check column meta order

  auto less = [] (
      const irs::column_meta& lhs,
      const irs::column_meta& rhs
  ) noexcept {
    return lhs.name < rhs.name;
  };

  if (!std::is_sorted(columns.begin(), columns.end(), less)) {
    columns.clear();
    id_to_column.clear();
    name_to_column.clear();

    throw irs::index_error(irs::string_utils::to_string(
      "invalid column order in segment '%s'",
      meta.name.c_str()
    ));
  }

  // column meta exists
  return true;
}

} // namespace {

namespace iresearch {

// -------------------------------------------------------------------
// segment_reader
// -------------------------------------------------------------------

class segment_reader_impl : public sub_reader {
 public:
  static sub_reader::ptr open(
    const directory& dir, 
    const segment_meta& meta
  );

  const directory& dir() const noexcept { 
    return dir_;
  }

  virtual const column_meta* column(const string_ref& name) const override;

  virtual column_iterator::ptr columns() const override;

  using sub_reader::docs_count;
  virtual uint64_t docs_count() const override {
    return docs_count_;
  }

  virtual doc_iterator::ptr docs_iterator() const override;

  virtual doc_iterator::ptr mask(doc_iterator::ptr&& it) const override {
    if (!it) {
      return nullptr;
    }

    if (docs_mask_.empty()) {
      return std::move(it);
    }

    return memory::make_managed<mask_doc_iterator>(std::move(it), docs_mask_);
  }

  virtual const term_reader* field(const string_ref& name) const override {
    return field_reader_->field(name);
  }

  virtual field_iterator::ptr fields() const override {
    return field_reader_->iterator();
  }

  virtual uint64_t live_docs_count() const noexcept override {
    return docs_count_ - docs_mask_.size();
  }

  uint64_t meta_version() const noexcept {
    return meta_version_;
  }

  virtual const sub_reader& operator[](size_t i) const noexcept override {
    assert(!i);
    return *this;
  }

  virtual size_t size() const noexcept override {
    return 1; // only 1 segment
  }

  virtual const columnstore_reader::column_reader* sort() const noexcept override {
    return sort_;
  }

  virtual const columnstore_reader::column_reader* column_reader(
    field_id field
  ) const override;

 private:
  DECLARE_SHARED_PTR(segment_reader_impl); // required for NAMED_PTR(...)
  std::vector<column_meta> columns_;
  columnstore_reader::ptr columnstore_reader_;
  const columnstore_reader::column_reader* sort_{};
  const directory& dir_;
  uint64_t docs_count_;
  document_mask docs_mask_;
  field_reader::ptr field_reader_;
  std::vector<column_meta*> id_to_column_;
  uint64_t meta_version_;
  std::unordered_map<hashed_string_ref, column_meta*> name_to_column_;

  segment_reader_impl(
    const directory& dir,
    uint64_t meta_version,
    uint64_t docs_count
  );
};

segment_reader::segment_reader(impl_ptr&& impl) noexcept
  : impl_(std::move(impl)) {
}

segment_reader::segment_reader(const segment_reader& other) noexcept {
  *this = other;
}

segment_reader& segment_reader::operator=(
    const segment_reader& other) noexcept {
  if (this != &other) {
    // make a copy
    impl_ptr impl = atomic_utils::atomic_load(&other.impl_);

    atomic_utils::atomic_store(&impl_, impl);
  }

  return *this;
}

template<>
/*static*/ bool segment_reader::has<columnstore_reader>(
    const segment_meta& meta) noexcept {
  return meta.column_store; // a separate flag to track presence of column store
}

template<>
/*static*/ bool segment_reader::has<document_mask_reader>(
    const segment_meta& meta) noexcept {
//  return meta.version > 0; // all version > 0 have document mask
  return meta.live_docs_count != meta.docs_count;
}

/*static*/ segment_reader segment_reader::open(
    const directory& dir,
    const segment_meta& meta) {
  return segment_reader_impl::open(dir, meta);
}

segment_reader segment_reader::reopen(const segment_meta& meta) const {
  // make a copy
  impl_ptr impl = atomic_utils::atomic_load(&impl_);

#ifdef IRESEARCH_DEBUG
  auto& reader_impl = dynamic_cast<const segment_reader_impl&>(*impl);
#else
  auto& reader_impl = static_cast<const segment_reader_impl&>(*impl);
#endif

  // reuse self if no changes to meta
  return reader_impl.meta_version() == meta.version
    ? *this
    : segment_reader_impl::open(reader_impl.dir(), meta);
}

// -------------------------------------------------------------------
// segment_reader_impl
// -------------------------------------------------------------------

segment_reader_impl::segment_reader_impl(
    const directory& dir,
    uint64_t meta_version,
    uint64_t docs_count)
  : dir_(dir),
    docs_count_(docs_count),
    meta_version_(meta_version) {
}

const column_meta* segment_reader_impl::column(
    const string_ref& name) const {
  auto it = name_to_column_.find(make_hashed_ref(name));
  return it == name_to_column_.end() ? nullptr : it->second;
}

column_iterator::ptr segment_reader_impl::columns() const {
  struct less {
    bool operator()(
        const irs::column_meta& lhs,
        const string_ref& rhs) const noexcept {
      return lhs.name < rhs;
    }
  }; // less

  typedef iterator_adaptor<
    string_ref, column_meta, column_iterator, less
  > iterator_t;

  return memory::make_managed<iterator_t>(
    columns_.data(), columns_.data() + columns_.size());
}

doc_iterator::ptr segment_reader_impl::docs_iterator() const {
  if (docs_mask_.empty()) {
    return memory::make_managed<::all_iterator>(docs_count_);
  }

  // the implementation generates doc_ids sequentially
  return memory::make_managed<masked_docs_iterator>(
    doc_limits::min(),
    doc_id_t(doc_limits::min() + docs_count_),
    docs_mask_);
}

/*static*/ sub_reader::ptr segment_reader_impl::open(
    const directory& dir, const segment_meta& meta) {
  auto& codec = *meta.codec;

  PTR_NAMED(segment_reader_impl, reader, dir, meta.version, meta.docs_count);

  // read document mask
  index_utils::read_document_mask(reader->docs_mask_, dir, meta);

  // initialize mandatory field reader
  auto& field_reader = reader->field_reader_;
  field_reader = codec.get_field_reader();
  field_reader->prepare(dir, meta, reader->docs_mask_);

  // initialize optional columnstore
  if (segment_reader::has<irs::columnstore_reader>(meta)) {
    auto& columnstore_reader = reader->columnstore_reader_;
    columnstore_reader  = codec.get_columnstore_reader();

    if (!columnstore_reader->prepare(dir, meta)) {
      throw index_error(string_utils::to_string(
        "failed to find existing (according to meta) columnstore in segment '%s'",
        meta.name.c_str()
      ));
    }

    if (field_limits::valid(meta.sort)) {
      reader->sort_ = columnstore_reader->column(meta.sort);

      if (!reader->sort_) {
        throw index_error(string_utils::to_string(
          "failed to find sort column '" IR_UINT64_T_SPECIFIER "' (according to meta) in columnstore in segment '%s'",
          meta.sort, meta.name.c_str()
        ));
      }
    }
  }

  // initialize optional columns meta
  read_columns_meta(
    codec,
    dir,
    meta,
    reader->columns_,
    reader->id_to_column_,
    reader->name_to_column_
  );

  return reader;
}

const columnstore_reader::column_reader* segment_reader_impl::column_reader(
    field_id field) const {
  return columnstore_reader_
    ? columnstore_reader_->column(field)
    : nullptr;
}

}
