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

#pragma once

#include "index_reader.hpp"
#include "shared.hpp"

namespace irs {

// Common implementation for readers composied of multiple other readers
// for use/inclusion into cpp files
template<typename Readers>
class CompositeReaderImpl : public IndexReader {
 public:
  using ReadersType = Readers;

  using ReaderType = typename std::enable_if_t<
    std::is_base_of_v<IndexReader, typename Readers::value_type>,
    typename Readers::value_type>;

  CompositeReaderImpl(ReadersType&& readers, uint64_t live_docs_count,
                      uint64_t docs_count) noexcept
    : readers_{std::move(readers)},
      live_docs_count_{live_docs_count},
      docs_count_{docs_count} {}

  // Returns corresponding sub-reader
  const ReaderType& operator[](size_t i) const noexcept final {
    IRS_ASSERT(i < readers_.size());
    return *(readers_[i]);
  }

  std::span<const ReaderType> GetReaders() const noexcept { return readers_; }
  std::span<ReaderType> GetMutReaders() noexcept { return readers_; }

  uint64_t CountMappedMemory() const final {
    uint64_t mapped{0};
    for (const auto& segment : readers_) {
      mapped += segment.CountMappedMemory();
    }
    return mapped;
  }

  // maximum number of documents
  uint64_t docs_count() const noexcept final { return docs_count_; }

  // number of live documents
  uint64_t live_docs_count() const noexcept final { return live_docs_count_; }

  // returns total number of opened writers
  size_t size() const noexcept final { return readers_.size(); }

 private:
  ReadersType readers_;
  uint64_t live_docs_count_;
  uint64_t docs_count_;
};

}  // namespace irs
