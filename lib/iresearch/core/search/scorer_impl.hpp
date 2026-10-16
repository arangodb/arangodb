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

#include <cstdint>
#include <iterator>
#include <type_traits>

#include "analysis/token_attributes.hpp"
#include "error/error.hpp"
#include "search/scorer.hpp"
#include "utils/string.hpp"

namespace irs {

struct ByteRefIterator final {
  using iterator_category = std::input_iterator_tag;
  using value_type = byte_type;
  using pointer = value_type*;
  using reference = value_type&;
  using difference_type = void;

  const byte_type* end_;
  const byte_type* pos_;

  explicit ByteRefIterator(bytes_view in) noexcept
    : end_{in.data() + in.size()}, pos_{in.data()} {}

  byte_type operator*() {
    if (pos_ >= end_) {
      throw io_error{"invalid read past end of input"};
    }

    return *pos_;
  }

  void operator++() noexcept { ++pos_; }
};

struct TermCollectorImpl final : TermCollector {
  // number of documents containing the matched term
  uint64_t docs_with_term = 0;

  void collect(const SubReader& /*segment*/, const term_reader& /*field*/,
               const attribute_provider& term_attrs) final {
    auto* meta = get<term_meta>(term_attrs);

    if (meta) {
      docs_with_term += meta->docs_count;
    }
  }

  void reset() noexcept final { docs_with_term = 0; }

  void collect(bytes_view in) final {
    ByteRefIterator itr{in};
    auto docs_with_term_value = vread<uint64_t>(itr);

    if (itr.pos_ != itr.end_) {
      throw io_error{"input not read fully"};
    }

    docs_with_term += docs_with_term_value;
  }

  void write(data_output& out) const final { out.write_vlong(docs_with_term); }
};

inline constexpr frequency kEmptyFreq;

template<typename Ctx>
struct MakeScoreFunctionImpl {
  template<bool HasFilterBoost, typename... Args>
  static ScoreFunction Make(Args&&... args);
};

template<typename Ctx, typename... Args>
ScoreFunction MakeScoreFunction(const filter_boost* filter_boost,
                                Args&&... args) noexcept {
  if (filter_boost) {
    return MakeScoreFunctionImpl<Ctx>::template Make<true>(
      std::forward<Args>(args)..., filter_boost);
  }
  return MakeScoreFunctionImpl<Ctx>::template Make<false>(
    std::forward<Args>(args)...);
}

enum class NormType {
  // Norm2 values
  kNorm2 = 0,
  // Norm2 values fit 1-byte
  kNorm2Tiny,
  // Old norms, 1/sqrt(|doc|)
  kNorm
};

}  // namespace irs
