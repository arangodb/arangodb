//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

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
  auto reader = codec.get_column_meta_reader();
  iresearch::field_id count = 0;

  if (!reader->prepare(dir, meta, count)) {
    return false;
  }

  columns.reserve(count);
  id_to_column.resize(count);
  name_to_column.reserve(count);

  for (iresearch::column_meta meta; reader->read(meta);) {
    columns.emplace_back(std::move(meta));

    auto& column = columns.back();
    id_to_column[column.id] = &column;

    const auto res = name_to_column.emplace(
      irs::make_hashed_ref(iresearch::string_ref(column.name), std::hash<irs::string_ref>()),
      &column
    );

    if (!res.second) {
      // duplicate field
      return false;
    }
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