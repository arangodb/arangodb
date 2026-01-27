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

#pragma once

#include <memory>

#include "index/index_reader.hpp"
#include "utils/directory_utils.hpp"
#include "utils/hash_utils.hpp"

#include <absl/container/flat_hash_map.h>

namespace irs {

// Reader holds file refs to files from the segment.
class SegmentReaderImpl final : public SubReader {
  struct PrivateTag final {
    explicit PrivateTag() = default;
  };

 public:
  SegmentReaderImpl(PrivateTag, IResourceManager& rm) noexcept
    : docs_mask_{{rm}} {}

  static std::shared_ptr<const SegmentReaderImpl> Open(
    const directory& dir, const SegmentMeta& meta,
    const IndexReaderOptions& options);

  std::shared_ptr<const SegmentReaderImpl> ReopenColumnStore(
    const directory& dir, const SegmentMeta& meta,
    const IndexReaderOptions& options) const;
  std::shared_ptr<const SegmentReaderImpl> ReopenDocsMask(
    const directory& dir, const SegmentMeta& meta,
    DocumentMask&& docs_mask) const;

  uint64_t CountMappedMemory() const final;

  const SegmentInfo& Meta() const final { return info_; }

  column_iterator::ptr columns() const final;

  const DocumentMask* docs_mask() const final { return &docs_mask_; }

  doc_iterator::ptr docs_iterator() const final;

  doc_iterator::ptr mask(doc_iterator::ptr&& it) const final;

  const term_reader* field(std::string_view name) const final {
    return field_reader_->field(name);
  }

  field_iterator::ptr fields() const final { return field_reader_->iterator(); }

  const irs::column_reader* sort() const noexcept final { return sort_; }

  const irs::column_reader* column(field_id field) const final;

  const irs::column_reader* column(std::string_view name) const final;

 private:
  void Update(const directory& dir, const SegmentMeta& meta,
              DocumentMask&& docs_mask) noexcept;

  using NamedColumns =
    absl::flat_hash_map<std::string_view, const irs::column_reader*>;
  using SortedNamedColumns =
    std::vector<std::reference_wrapper<const irs::column_reader>>;

  struct ColumnData {
    columnstore_reader::ptr columnstore_reader_;
    NamedColumns named_columns_;
    SortedNamedColumns sorted_named_columns_;

    const irs::column_reader* Open(const directory& dir,
                                   const SegmentMeta& meta,
                                   const IndexReaderOptions& options,
                                   const field_reader& field_reader);
  };

  FileRefs refs_;
  SegmentInfo info_;
  DocumentMask docs_mask_;
  field_reader::ptr field_reader_;
  std::shared_ptr<ColumnData> data_;
  // logically part of data_, stored separate to avoid unnecessary indirection
  const irs::column_reader* sort_{};
};

}  // namespace irs
