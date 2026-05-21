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
////////////////////////////////////////////////////////////////////////////////

#include "index_reader.hpp"

#include "resource_manager.hpp"

namespace irs {
namespace {

const SegmentInfo kEmptyInfo;

struct EmptySubReader final : SubReader {
  uint64_t CountMappedMemory() const final { return 0; }

  column_iterator::ptr columns() const final {
    return irs::column_iterator::empty();
  }
  const column_reader* column(field_id) const final { return nullptr; }
  const column_reader* column(std::string_view) const final { return nullptr; }
  const SegmentInfo& Meta() const final { return kEmptyInfo; }
  const irs::DocumentMask* docs_mask() const final { return nullptr; }
  irs::doc_iterator::ptr docs_iterator() const final {
    return irs::doc_iterator::empty();
  }
  const irs::term_reader* field(std::string_view) const final {
    return nullptr;
  }
  irs::field_iterator::ptr fields() const final {
    return irs::field_iterator::empty();
  }
  const irs::column_reader* sort() const final { return nullptr; }
};

const EmptySubReader kEmpty;

}  // namespace

const SubReader& SubReader::empty() noexcept { return kEmpty; }

#ifdef IRESEARCH_DEBUG
IResourceManager IResourceManager::kForbidden;
#endif
IResourceManager IResourceManager::kNoop;
ResourceManagementOptions ResourceManagementOptions::kDefault;

}  // namespace irs
