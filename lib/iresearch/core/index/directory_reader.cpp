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

#include "directory_reader.hpp"

#include "index/directory_reader_impl.hpp"
#include "index/segment_reader.hpp"
#include "utils/hash_utils.hpp"
#include "utils/singleton.hpp"
#include "utils/type_limits.hpp"

#include <absl/container/flat_hash_map.h>

namespace irs {

DirectoryReader::DirectoryReader(
  const directory& dir, format::ptr codec /*= nullptr*/,
  const IndexReaderOptions& opts /*= directory_reader_options()*/)
  : impl_{DirectoryReaderImpl::Open(dir, opts, std::move(codec), nullptr)} {}

DirectoryReader::DirectoryReader(
  std::shared_ptr<const DirectoryReaderImpl>&& impl) noexcept
  : impl_{std::move(impl)} {}

DirectoryReader::DirectoryReader(const DirectoryReader& other) noexcept
  : impl_{std::atomic_load_explicit(&other.impl_, std::memory_order_acquire)} {}

DirectoryReader& DirectoryReader::operator=(
  const DirectoryReader& other) noexcept {
  if (this != &other) {
    // make a copy
    auto impl =
      std::atomic_load_explicit(&other.impl_, std::memory_order_acquire);

    std::atomic_store_explicit(&impl_, impl, std::memory_order_release);
  }

  return *this;
}

const SubReader& DirectoryReader::operator[](size_t i) const {
  return (*impl_)[i];
}

uint64_t DirectoryReader::CountMappedMemory() const {
  return impl_->CountMappedMemory();
}

uint64_t DirectoryReader::docs_count() const { return impl_->docs_count(); }

uint64_t DirectoryReader::live_docs_count() const {
  return impl_->live_docs_count();
}

size_t DirectoryReader::size() const { return impl_->size(); }

const DirectoryMeta& DirectoryReader::Meta() const { return impl_->Meta(); }

DirectoryReader DirectoryReader::Reopen() const {
  // make a copy
  auto impl = std::atomic_load_explicit(&impl_, std::memory_order_acquire);

  return DirectoryReader{DirectoryReaderImpl::Open(
    impl->Dir(), impl->Options(), impl->Codec(), std::move(impl))};
}

}  // namespace irs
