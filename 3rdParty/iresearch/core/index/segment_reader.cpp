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

#include "index/index_meta.hpp"

#include "formats/format_utils.hpp"
#include "utils/index_utils.hpp"
#include "utils/singleton.hpp"
#include "utils/type_limits.hpp"

#include <unordered_map>

NS_LOCAL

class iterator_impl: public iresearch::index_reader::reader_iterator_impl {
 public:
  explicit iterator_impl(const iresearch::sub_reader* rdr = nullptr) NOEXCEPT
    : rdr_(rdr) {
  }

  virtual void operator++() override { rdr_ = nullptr; }
  virtual reference operator*() override {
    return *const_cast<iresearch::sub_reader*>(rdr_);
  }
  virtual const_reference operator*() const override { return *rdr_; }
  virtual bool operator==(
    const iresearch::index_reader::reader_iterator_impl& rhs
  ) override {
    return rdr_ == static_cast<const iterator_impl&>(rhs).rdr_;
  }

 private:
  const iresearch::sub_reader* rdr_;
};

class mask_doc_iterator final : public irs::doc_iterator {
 public:
  explicit mask_doc_iterator(
      irs::doc_iterator::ptr&& it,
      const irs::document_mask& mask) NOEXCEPT
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

  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return it_->attributes();
  }

 private:
  const irs::document_mask& mask_; // excluded document ids
  irs::doc_iterator::ptr it_;
}; // mask_doc_iterator

class masked_docs_iterator 
    : public iresearch::segment_reader::docs_iterator_t,
      private iresearch::util::noncopyable {
 public:
  masked_docs_iterator(
    iresearch::doc_id_t begin,
    iresearch::doc_id_t end,
    const iresearch::document_mask& docs_mask
  ) :
    current_(iresearch::type_limits<iresearch::type_t::doc_id_t>::invalid()),
    docs_mask_(docs_mask),
    end_(end),
    next_(begin) {
  }

  virtual ~masked_docs_iterator() {}

  virtual bool next() override {
    while (next_ < end_) {
      current_ = next_++;

      if (docs_mask_.find(current_) == docs_mask_.end()) {
        return true;
      }
    }

    current_ = iresearch::type_limits<iresearch::type_t::doc_id_t>::eof();

    return false;
  }

  virtual iresearch::doc_id_t value() const override {
    return current_;
  }

 private:
  iresearch::doc_id_t current_;
  const iresearch::document_mask& docs_mask_;
  const iresearch::doc_id_t end_; // past last valid doc_id
  iresearch::doc_id_t next_;
};

bool read_columns_meta(
    const iresearch::format& codec,
    const iresearch::directory& dir,
    const irs::segment_meta& meta,
    std::vector<iresearch::column_meta>& columns,
    std::vector<iresearch::column_meta*>& id_to_column,
    std::unordered_map<iresearch::hashed_string_ref, iresearch::column_meta*>& name_to_column
) {
  size_t count = 0;
  irs::field_id max_id;
  auto reader = codec.get_column_meta_reader();

  if (!reader->prepare(dir, meta, count, max_id)
      || max_id >= irs::integer_traits<size_t>::const_max) {
    return false;
  }

  columns.reserve(count);
  id_to_column.resize(max_id + 1); // +1 for count
  name_to_column.reserve(count);

  for (irs::column_meta col_meta; reader->read(col_meta);) {
    columns.emplace_back(std::move(col_meta));

    auto& column = columns.back();
    const auto res = name_to_column.emplace(
      irs::make_hashed_ref(iresearch::string_ref(column.name), std::hash<irs::string_ref>()),
      &column
    );

    assert(column.id < id_to_column.size());

    if (!res.second || id_to_column[column.id]) {
      // duplicate field
      return false;
    }

    id_to_column[column.id] = &column;
  }

  if (!std::is_sorted(
    columns.begin(), columns.end(),
    [] (const iresearch::column_meta& lhs, const iresearch::column_meta& rhs) {
        return lhs.name < rhs.name; })) {
    columns.clear();
    id_to_column.clear();
    name_to_column.clear();
    // TODO: log
    return false;
  }

  return true;
}

NS_END // NS_LOCAL

NS_ROOT

// -------------------------------------------------------------------
// segment_reader
// -------------------------------------------------------------------

class segment_reader_impl : public sub_reader {
 public:
  static sub_reader::ptr open(
    const directory& dir, 
    const segment_meta& meta
  );

  const directory& dir() const NOEXCEPT { 
    return dir_;
  }

  virtual index_reader::reader_iterator begin() const override;
  virtual index_reader::reader_iterator end() const override;

  virtual const column_meta* column(const string_ref& name) const override;

  virtual column_iterator::ptr columns() const override;

  using sub_reader::docs_count;
  virtual uint64_t docs_count() const override {
    return docs_count_;
  }

  virtual docs_iterator_t::ptr docs_iterator() const override;

  virtual doc_iterator::ptr mask(doc_iterator::ptr&& it) const override {
    if (docs_mask_.empty()) {
      return std::move(it);
    }

    return doc_iterator::make<mask_doc_iterator>(
      std::move(it), docs_mask_
    );
  }

  virtual const term_reader* field(const string_ref& name) const override {
    return field_reader_->field(name);
  }

  virtual field_iterator::ptr fields() const override {
    return field_reader_->iterator();
  }

  virtual uint64_t live_docs_count() const NOEXCEPT override {
    return docs_count_ - docs_mask_.size();
  }

  uint64_t meta_version() const NOEXCEPT {
    return meta_version_;
  }

  virtual size_t size() const NOEXCEPT override {
    return 1; // only 1 segment
  }

  virtual const columnstore_reader::column_reader* column_reader(
    field_id field
  ) const override;

 private:
  DECLARE_SPTR(segment_reader_impl); // required for NAMED_PTR(...)
  std::vector<column_meta> columns_;
  columnstore_reader::ptr columnstore_reader_;
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

segment_reader::segment_reader(impl_ptr&& impl) NOEXCEPT
  : impl_(std::move(impl)) {
}

segment_reader::segment_reader(const segment_reader& other) NOEXCEPT {
  *this = other;
}

segment_reader& segment_reader::operator=(
    const segment_reader& other) NOEXCEPT {
  if (this != &other) {
    // make a copy
    impl_ptr impl = atomic_utils::atomic_load(&other.impl_);

    atomic_utils::atomic_store(&impl_, impl);
  }

  return *this;
}

index_reader::reader_iterator segment_reader::begin() const {
  return index_reader::reader_iterator(new iterator_impl(this));
}

template<>
/*static*/ bool segment_reader::has<columnstore_reader>(
    const segment_meta& meta) NOEXCEPT {
  return meta.column_store; // a separate flag to track presence of column store
}

template<>
/*static*/ bool segment_reader::has<document_mask_reader>(
    const segment_meta& meta) NOEXCEPT {
  return meta.version > 0; // all version > 0 have document mask
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
  auto& reader_impl = dynamic_cast<segment_reader_impl&>(*impl);
#else
  auto& reader_impl = static_cast<segment_reader_impl&>(*impl);
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

index_reader::reader_iterator segment_reader_impl::begin() const {
  return index_reader::reader_iterator(new iterator_impl(this));
}

index_reader::reader_iterator segment_reader_impl::end() const {
  return index_reader::reader_iterator(new iterator_impl());
}

const column_meta* segment_reader_impl::column(
    const string_ref& name) const {
  auto it = name_to_column_.find(make_hashed_ref(name, std::hash<irs::string_ref>()));
  return it == name_to_column_.end() ? nullptr : it->second;
}

column_iterator::ptr segment_reader_impl::columns() const {
  struct less {
    bool operator()(
        const irs::column_meta& lhs,
        const string_ref& rhs) const NOEXCEPT {
      return lhs.name < rhs;
    }
  }; // less

  typedef iterator_adaptor<
    string_ref, column_meta, column_iterator, less
  > iterator_t;

  auto it = memory::make_unique<iterator_t>(
    columns_.data(), columns_.data() + columns_.size()
  );

  return memory::make_managed<column_iterator>(std::move(it));
}

sub_reader::docs_iterator_t::ptr segment_reader_impl::docs_iterator() const {
  // the implementation generates doc_ids sequentially
  return memory::make_unique<masked_docs_iterator>(
    type_limits<type_t::doc_id_t>::min(),
    doc_id_t(type_limits<type_t::doc_id_t>::min() + docs_count_),
    docs_mask_
  );
}

/*static*/ sub_reader::ptr segment_reader_impl::open(
    const directory& dir, const segment_meta& meta) {
  PTR_NAMED(segment_reader_impl, reader, dir, meta.version, meta.docs_count);

  index_utils::read_document_mask(reader->docs_mask_, dir, meta);

  auto& codec = *meta.codec;
  auto field_reader = codec.get_field_reader();

  // initialize field reader
  if (!field_reader->prepare(dir, meta, reader->docs_mask_)) {
    return nullptr; // i.e. nullptr, field reader required
  }

  reader->field_reader_ = std::move(field_reader);

  auto columnstore_reader = codec.get_columnstore_reader();

  // initialize column reader (if available)
  if (segment_reader::has<irs::columnstore_reader>(meta)
      && columnstore_reader->prepare(dir, meta)) {
    reader->columnstore_reader_ = std::move(columnstore_reader);
  }

  // initialize columns meta
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

NS_END