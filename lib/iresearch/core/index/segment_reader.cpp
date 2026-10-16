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

#include "segment_reader.hpp"

#include "index/segment_reader_impl.hpp"

namespace irs {

SegmentReader::SegmentReader(const directory& dir, const SegmentMeta& meta,
                             const IndexReaderOptions& opts)
  : impl_{SegmentReaderImpl::Open(dir, meta, opts)} {}

uint64_t SegmentReader::CountMappedMemory() const {
  return impl_->CountMappedMemory();
}

field_iterator::ptr SegmentReader::fields() const { return impl_->fields(); }

const irs::column_reader* SegmentReader::sort() const { return impl_->sort(); }

const irs::column_reader* SegmentReader::column(std::string_view name) const {
  return impl_->column(name);
}

const irs::column_reader* SegmentReader::column(field_id field) const {
  return impl_->column(field);
}

// FIXME find a better way to mask documents
doc_iterator::ptr SegmentReader::mask(doc_iterator::ptr&& it) const {
  return impl_->mask(std::move(it));
}

const term_reader* SegmentReader::field(std::string_view name) const {
  return impl_->field(name);
}

doc_iterator::ptr SegmentReader::docs_iterator() const {
  return impl_->docs_iterator();
}

column_iterator::ptr SegmentReader::columns() const { return impl_->columns(); }

const SegmentInfo& SegmentReader::Meta() const { return impl_->Meta(); }

const DocumentMask* SegmentReader::docs_mask() const {
  return impl_->docs_mask();
}

}  // namespace irs
