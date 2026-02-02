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

#include "formats/seek_cookie.hpp"

namespace irs {

struct term_reader;

// Cached per reader fixed phrase state
struct FixedPhraseState {
  explicit FixedPhraseState(IResourceManager& memory) noexcept
    : terms{{memory}} {}

  // Mimic std::pair interface
  struct TermState {
    TermState(seek_cookie::ptr&& first, score_t /*second*/) noexcept
      : first{std::move(first)} {}

    seek_cookie::ptr first;
  };

  using Terms = ManagedVector<TermState>;
  Terms terms;
  const term_reader* reader{};
};

static_assert(std::is_nothrow_move_constructible_v<FixedPhraseState>);
static_assert(std::is_nothrow_move_assignable_v<FixedPhraseState>);

// Cached per reader variadic phrase state
struct VariadicPhraseState {
  explicit VariadicPhraseState(IResourceManager& memory) noexcept
    : num_terms{{memory}}, terms{{memory}} {}

  using TermState = std::pair<seek_cookie::ptr, score_t>;

  ManagedVector<size_t> num_terms;  // number of terms per phrase part
  using Terms = ManagedVector<TermState>;
  Terms terms;
  const term_reader* reader{};
  bool volatile_boost{};
};

static_assert(std::is_nothrow_move_constructible_v<VariadicPhraseState>);
static_assert(std::is_nothrow_move_assignable_v<VariadicPhraseState>);

}  // namespace irs
