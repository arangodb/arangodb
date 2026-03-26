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

#pragma once

#include "index_reader.hpp"

namespace irs {

class SegmentReaderImpl;

// Pimpl facade for segment reader
class SegmentReader final : public SubReader {
 public:
  SegmentReader() noexcept = default;
  SegmentReader(SegmentReader&&) noexcept = default;
  SegmentReader& operator=(SegmentReader&&) noexcept = default;
  SegmentReader(const SegmentReader& other) noexcept = default;
  SegmentReader& operator=(const SegmentReader& other) noexcept = default;

  explicit SegmentReader(const directory& dir, const SegmentMeta& meta,
                         const IndexReaderOptions& opts);
  explicit SegmentReader(std::shared_ptr<const SegmentReaderImpl> impl) noexcept
    : impl_{std::move(impl)} {}

  bool operator==(const SegmentReader& rhs) const noexcept {
    return impl_ == rhs.impl_;
  }

  bool operator==(std::nullptr_t) const noexcept { return !impl_; }

  SegmentReader& operator*() noexcept { return *this; }
  const SegmentReader& operator*() const noexcept { return *this; }
  SegmentReader* operator->() noexcept { return this; }
  const SegmentReader* operator->() const noexcept { return this; }

  uint64_t CountMappedMemory() const final;

  const SegmentInfo& Meta() const final;

  column_iterator::ptr columns() const final;

  doc_iterator::ptr docs_iterator() const final;

  const DocumentMask* docs_mask() const final;

  // FIXME find a better way to mask documents
  doc_iterator::ptr mask(doc_iterator::ptr&& it) const final;

  const term_reader* field(std::string_view name) const final;

  field_iterator::ptr fields() const final;

  const irs::column_reader* sort() const final;

  const irs::column_reader* column(std::string_view name) const final;

  const irs::column_reader* column(field_id field) const final;

  const std::shared_ptr<const SegmentReaderImpl>& GetImpl() const noexcept {
    return impl_;
  }

  explicit operator bool() const noexcept { return nullptr != impl_; }

 private:
  std::shared_ptr<const SegmentReaderImpl> impl_;
};

}  // namespace irs
