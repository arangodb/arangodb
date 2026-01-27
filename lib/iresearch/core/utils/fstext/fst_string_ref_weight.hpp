////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#if defined(_MSC_VER)
#pragma warning(disable : 4267)  // conversion from 'size_t' to 'uint32_t',
                                 // possible loss of data
#endif

#include <fst/string-weight.h>

#if defined(_MSC_VER)
#pragma warning(default : 4267)
#endif

#include "utils/string.hpp"

namespace fst {
namespace fstext {

template<typename Label>
class StringRefWeight;

template<typename Label>
struct StringRefWeightTraits {
  static const StringRefWeight<Label> Zero();

  static const StringRefWeight<Label> One();

  static const StringRefWeight<Label> NoWeight();

  static bool Member(const StringRefWeight<Label>& weight);
};

template<typename Label>
class StringRefWeight : public StringRefWeightTraits<Label> {
 public:
  using str_t = irs::basic_string_view<Label>;

  static const std::string& Type() {
    static const std::string type = "left_string";
    return type;
  }

  friend bool operator==(StringRefWeight lhs, StringRefWeight rhs) noexcept {
    return lhs.str_ == rhs.str_;
  }

  StringRefWeight() = default;

  template<typename Iterator>
  StringRefWeight(Iterator begin, Iterator end) noexcept : str_(begin, end) {}

  StringRefWeight(const StringRefWeight&) = default;
  StringRefWeight(StringRefWeight&&) = default;

  explicit StringRefWeight(irs::basic_string_view<Label> rhs) noexcept
    : str_{rhs} {}

  StringRefWeight& operator=(StringRefWeight&&) = default;
  StringRefWeight& operator=(const StringRefWeight&) = default;

  StringRefWeight& operator=(irs::basic_string_view<Label> rhs) noexcept {
    str_ = rhs;
    return *this;
  }

  bool Member() const noexcept {
    return StringRefWeightTraits<Label>::Member(*this);
  }

  size_t Hash() const noexcept { return std::hash<str_t>()(str_); }

  StringRefWeight Quantize(float /*delta*/ = kDelta) const noexcept {
    return *this;
  }

  static uint64_t Properties() noexcept {
    static constexpr auto props = kLeftSemiring | kIdempotent;
    return props;
  }

  Label& operator[](size_t i) noexcept { return str_[i]; }

  const Label& operator[](size_t i) const noexcept { return str_[i]; }

  const Label* c_str() const noexcept { return str_.c_str(); }

  bool Empty() const noexcept { return str_.empty(); }

  void Clear() noexcept { str_.clear(); }

  size_t Size() const noexcept { return str_.size(); }

  const Label* begin() const noexcept { return str_.data(); }
  const Label* end() const noexcept { return str_.data() + str_.size(); }

  // intentionally implicit
  operator irs::basic_string_view<Label>() const noexcept { return str_; }

 private:
  str_t str_;
};

template<typename Label>
inline bool operator!=(StringRefWeight<Label> w1, StringRefWeight<Label> w2) {
  return !(w1 == w2);
}

template<typename Label>
inline std::ostream& operator<<(std::ostream& strm,
                                StringRefWeight<Label> weight) {
  if (weight.Empty()) {
    return strm << "Epsilon";
  }

  auto begin = weight.begin();
  const auto& first = *begin;

  if (first == kStringInfinity) {
    return strm << "Infinity";
  } else if (first == kStringBad) {
    return strm << "BadString";
  }

  const auto end = weight.end();
  if (begin != end) {
    strm << *begin;

    for (++begin; begin != end; ++begin) {
      strm << kStringSeparator << *begin;
    }
  }

  return strm;
}

// -----------------------------------------------------------------------------
// --SECTION--                               StringRefWeight<irs::byte_type>
// -----------------------------------------------------------------------------

template<>
struct StringRefWeightTraits<irs::byte_type> {
  static constexpr StringRefWeight<irs::byte_type> Zero() noexcept {
    return {};
  }

  static constexpr StringRefWeight<irs::byte_type> One() noexcept {
    return Zero();
  }

  static constexpr StringRefWeight<irs::byte_type> NoWeight() noexcept {
    return Zero();
  }

  static constexpr bool Member(StringRefWeight<irs::byte_type>) noexcept {
    // always a member
    return true;
  }
};

inline std::ostream& operator<<(std::ostream& strm,
                                StringRefWeight<irs::byte_type> weight) {
  if (weight.Empty()) {
    return strm << "Epsilon";
  }

  auto begin = weight.begin();

  const auto end = weight.end();
  if (begin != end) {
    strm << *begin;

    for (++begin; begin != end; ++begin) {
      strm << kStringSeparator << *begin;
    }
  }

  return strm;
}

}  // namespace fstext
}  // namespace fst
