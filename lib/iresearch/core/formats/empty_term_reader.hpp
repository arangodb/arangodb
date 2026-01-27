////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "formats.hpp"
#include "index/field_meta.hpp"

namespace irs {

// term_reader implementation with docs_count but without terms
class empty_term_reader final : public irs::term_reader {
 public:
  constexpr explicit empty_term_reader(uint64_t docs_count) noexcept
    : docs_count_{docs_count} {}

  seek_term_iterator::ptr iterator(SeekMode) const noexcept final {
    return seek_term_iterator::empty();
  }

  seek_term_iterator::ptr iterator(
    automaton_table_matcher&) const noexcept final {
    return seek_term_iterator::empty();
  }

  size_t bit_union(const cookie_provider&, size_t*) const noexcept final {
    return 0;
  }

  size_t read_documents(bytes_view, std::span<doc_id_t>) const noexcept final {
    return 0;
  }

  term_meta term(bytes_view) const noexcept final { return {}; }

  doc_iterator::ptr postings(const seek_cookie&,
                             IndexFeatures) const noexcept final {
    return doc_iterator::empty();
  }

  doc_iterator::ptr wanderator(const seek_cookie&, IndexFeatures,
                               const WanderatorOptions&,
                               WandContext) const noexcept final {
    return doc_iterator::empty();
  }

  const field_meta& meta() const noexcept final { return field_meta::kEmpty; }

  attribute* get_mutable(irs::type_info::type_id) noexcept final {
    return nullptr;
  }

  // total number of terms
  size_t size() const noexcept final {
    return 0;  // no terms in reader
  }

  // total number of documents
  uint64_t docs_count() const noexcept final { return docs_count_; }

  // least significant term
  bytes_view(min)() const noexcept final { return {}; }

  // most significant term
  bytes_view(max)() const noexcept final { return {}; }

  bool has_scorer(byte_type) const noexcept final { return false; }

 private:
  uint64_t docs_count_;
};

}  // namespace irs
