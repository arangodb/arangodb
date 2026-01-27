////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "segment_reader_impl.hpp"

#include <vector>

#include "analysis/token_attributes.hpp"
#include "index/index_meta.hpp"
#include "utils/index_utils.hpp"
#include "utils/type_limits.hpp"

#include <absl/strings/str_cat.h>

namespace irs {
namespace {

class AllIterator : public doc_iterator {
 public:
  explicit AllIterator(doc_id_t docs_count) noexcept
    : max_doc_{doc_limits::min() + docs_count - 1} {}

  bool next() noexcept final {
    if (doc_.value < max_doc_) {
      ++doc_.value;
      return true;
    } else {
      doc_.value = doc_limits::eof();
      return false;
    }
  }

  doc_id_t seek(doc_id_t target) noexcept final {
    doc_.value = target <= max_doc_ ? target : doc_limits::eof();

    return doc_.value;
  }

  doc_id_t value() const noexcept final { return doc_.value; }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::type<document>::id() == type ? &doc_ : nullptr;
  }

 private:
  document doc_;
  doc_id_t max_doc_;  // largest valid doc_id
};

class MaskDocIterator : public doc_iterator {
 public:
  MaskDocIterator(doc_iterator::ptr&& it, const DocumentMask& mask) noexcept
    : mask_{mask}, it_{std::move(it)} {}

  bool next() final {
    while (it_->next()) {
      if (!mask_.contains(value())) {
        return true;
      }
    }

    return false;
  }

  doc_id_t seek(doc_id_t target) final {
    const auto doc = it_->seek(target);

    if (!mask_.contains(doc)) {
      return doc;
    }

    next();

    return value();
  }

  doc_id_t value() const final { return it_->value(); }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return it_->get_mutable(type);
  }

 private:
  const DocumentMask& mask_;  // excluded document ids
  doc_iterator::ptr it_;
};

class MaskedDocIterator : public doc_iterator {
 public:
  MaskedDocIterator(doc_id_t begin, doc_id_t end,
                    const DocumentMask& docs_mask) noexcept
    : docs_mask_{docs_mask}, end_{end}, next_{begin} {}

  bool next() final {
    while (next_ < end_) {
      current_.value = next_++;

      if (!docs_mask_.contains(current_.value)) {
        return true;
      }
    }

    current_.value = doc_limits::eof();

    return false;
  }

  doc_id_t seek(doc_id_t target) final {
    next_ = target;
    next();

    return value();
  }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::type<document>::id() == type ? &current_ : nullptr;
  }

  doc_id_t value() const final { return current_.value; }

 private:
  const DocumentMask& docs_mask_;
  document current_;
  const doc_id_t end_;  // past last valid doc_id
  doc_id_t next_;
};

FileRefs GetRefs(const directory& dir, const SegmentMeta& meta) {
  FileRefs file_refs;
  file_refs.reserve(meta.files.size());

  auto& refs = dir.attributes().refs();
  for (auto& file : meta.files) {
    // cppcheck-suppress useStlAlgorithm
    file_refs.emplace_back(refs.add(file));
  }

  return file_refs;
}

}  // namespace

std::shared_ptr<const SegmentReaderImpl> SegmentReaderImpl::Open(
  const directory& dir, const SegmentMeta& meta,
  const IndexReaderOptions& options) {
  auto reader = std::make_shared<SegmentReaderImpl>(
    PrivateTag{}, *options.resource_manager.readers);
  // read optional docs_mask
  DocumentMask docs_mask{{*options.resource_manager.readers}};
  if (options.doc_mask) {
    index_utils::ReadDocumentMask(docs_mask, dir, meta);
  }
  reader->Update(dir, meta, std::move(docs_mask));
  // open index data
  IRS_ASSERT(meta.codec != nullptr);
  // always instantiate to avoid unnecessary checks
  reader->field_reader_ =
    meta.codec->get_field_reader(*options.resource_manager.readers);
  if (options.index) {
    reader->field_reader_->prepare(
      ReaderState{.dir = &dir, .meta = &meta, .scorers = options.scorers});
  }
  // open column store
  reader->data_ = std::make_shared<ColumnData>();
  reader->sort_ =
    reader->data_->Open(dir, meta, options, *reader->field_reader_);
  return reader;
}

std::shared_ptr<const SegmentReaderImpl> SegmentReaderImpl::ReopenColumnStore(
  const directory& dir, const SegmentMeta& meta,
  const IndexReaderOptions& options) const {
  IRS_ASSERT(meta == info_);
  auto reader = std::make_shared<SegmentReaderImpl>(
    PrivateTag{}, docs_mask_.get_allocator().ResourceManager());
  // clone removals
  reader->refs_ = refs_;
  reader->info_ = info_;
  reader->docs_mask_ = docs_mask_;
  // clone index data
  reader->field_reader_ = field_reader_;
  // open column store
  reader->data_ = std::make_shared<ColumnData>();
  reader->sort_ = reader->data_->Open(dir, meta, options, *field_reader_);
  return reader;
}

std::shared_ptr<const SegmentReaderImpl> SegmentReaderImpl::ReopenDocsMask(
  const directory& dir, const SegmentMeta& meta,
  DocumentMask&& docs_mask) const {
  auto reader = std::make_shared<SegmentReaderImpl>(
    PrivateTag{}, docs_mask_.get_allocator().ResourceManager());
  // clone field reader
  reader->field_reader_ = field_reader_;
  // clone column store
  reader->data_ = data_;
  reader->sort_ = sort_;
  // update removals
  reader->Update(dir, meta, std::move(docs_mask));
  return reader;
}

void SegmentReaderImpl::Update(const directory& dir, const SegmentMeta& meta,
                               DocumentMask&& docs_mask) noexcept {
  IRS_ASSERT(meta.live_docs_count <= meta.docs_count);
  IRS_ASSERT(docs_mask.size() <= meta.docs_count);
  // TODO(MBkkt) on practice only mask file changed, so it can be optimized
  refs_ = GetRefs(dir, meta);
  info_ = meta;
  docs_mask_ = std::move(docs_mask);
  info_.live_docs_count = info_.docs_count - docs_mask_.size();
}

uint64_t SegmentReaderImpl::CountMappedMemory() const {
  uint64_t mapped{0};
  if (field_reader_ != nullptr) {
    mapped += field_reader_->CountMappedMemory();
  }
  if (data_ != nullptr && data_->columnstore_reader_ != nullptr) {
    mapped += data_->columnstore_reader_->CountMappedMemory();
  }
  return mapped;
}

const irs::column_reader* SegmentReaderImpl::column(
  std::string_view name) const {
  const auto& named_columns = data_->named_columns_;
  const auto it = named_columns.find(name);
  return it == named_columns.end() ? nullptr : it->second;
}

const irs::column_reader* SegmentReaderImpl::column(field_id field) const {
  IRS_ASSERT(data_->columnstore_reader_);
  return data_->columnstore_reader_->column(field);
}

column_iterator::ptr SegmentReaderImpl::columns() const {
  struct less {
    bool operator()(const irs::column_reader& lhs,
                    std::string_view rhs) const noexcept {
      return lhs.name() < rhs;
    }
  };

  using iterator_t =
    iterator_adaptor<std::string_view, irs::column_reader,
                     decltype(data_->sorted_named_columns_.begin()),
                     column_iterator, less>;

  return memory::make_managed<iterator_t>(
    std::begin(data_->sorted_named_columns_),
    std::end(data_->sorted_named_columns_));
}

doc_iterator::ptr SegmentReaderImpl::docs_iterator() const {
  if (docs_mask_.empty()) {
    return memory::make_managed<AllIterator>(
      static_cast<doc_id_t>(info_.docs_count));
  }

  // the implementation generates doc_ids sequentially
  return memory::make_managed<MaskedDocIterator>(
    doc_limits::min(),
    doc_limits::min() + static_cast<doc_id_t>(info_.docs_count), docs_mask_);
}

doc_iterator::ptr SegmentReaderImpl::mask(doc_iterator::ptr&& it) const {
  if (!it || docs_mask_.empty()) {
    return std::move(it);
  }

  return memory::make_managed<MaskDocIterator>(std::move(it), docs_mask_);
}

const irs::column_reader* SegmentReaderImpl::ColumnData::Open(
  const directory& dir, const SegmentMeta& meta,
  const IndexReaderOptions& options, const field_reader& field_reader) {
  IRS_ASSERT(meta.codec != nullptr);
  auto& codec = *meta.codec;
  // always instantiate to avoid unnecessary checks
  columnstore_reader_ = codec.get_columnstore_reader();

  if (!options.columnstore || !meta.column_store) {
    return {};
  }

  // initialize optional columnstore
  columnstore_reader::options columnstore_opts{.resource_manager =
                                                 options.resource_manager};
  if (options.warmup_columns) {
    columnstore_opts.warmup_column = [warmup = options.warmup_columns,
                                      &field_reader,
                                      &meta](const column_reader& column) {
      return warmup(meta, field_reader, column);
    };
  }

  if (!columnstore_reader_->prepare(dir, meta, columnstore_opts)) {
    throw index_error{
      absl::StrCat("Failed to find existing (according to meta) "
                   "columnstore in segment '",
                   meta.name, "'")};
  }

  const irs::column_reader* sort{};
  if (field_limits::valid(meta.sort)) {
    sort = columnstore_reader_->column(meta.sort);

    if (!sort) {
      throw index_error{absl::StrCat(
        "Failed to find sort column '", meta.sort,
        "' (according to meta) in columnstore in segment '", meta.name, "'")};
    }
  }

  // FIXME(gnusi): too rough, we must exclude unnamed columns
  const auto num_columns = columnstore_reader_->size();
  named_columns_.reserve(num_columns);
  sorted_named_columns_.reserve(num_columns);

  columnstore_reader_->visit([this, &meta](const irs::column_reader& column) {
    const auto name = column.name();

    if (!IsNull(name)) {
      const auto [it, is_new] = named_columns_.emplace(name, &column);
      IRS_IGNORE(it);

      if (IRS_UNLIKELY(!is_new)) {
        throw index_error{absl::StrCat("Duplicate named column '", name,
                                       "' in segment '", meta.name, "'")};
      }

      if (!sorted_named_columns_.empty() &&
          sorted_named_columns_.back().get().name() >= name) {
        throw index_error{absl::StrCat(
          "Named columns are out of order in segment '", meta.name, "'")};
      }

      sorted_named_columns_.emplace_back(column);
    }

    return true;
  });
  return sort;
}

}  // namespace irs
