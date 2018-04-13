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

#ifndef IRESEARCH_FST_STRING_WEIGHT_H
#define IRESEARCH_FST_STRING_WEIGHT_H

#if defined(_MSC_VER)
  #pragma warning(disable : 4267) // conversion from 'size_t' to 'uint32_t', possible loss of data
#endif

  #include <fst/string-weight.h>

#if defined(_MSC_VER)
  #pragma warning(default: 4267)
#endif

#include "shared.hpp"
#include "utils/string.hpp"
#include "utils/std.hpp"
#include "utils/bytes_utils.hpp"

#include <string>

NS_BEGIN(fst)

// String semiring: (longest_common_prefix/suffix, ., Infinity, Epsilon)
template <typename Label>
class StringLeftWeight {
 public:
  typedef StringLeftWeight<Label> ReverseWeight;
  typedef std::basic_string<Label> str_t;
  typedef typename str_t::const_iterator iterator;

  static const StringLeftWeight<Label>& Zero() {
    static const StringLeftWeight<Label> zero((Label)kStringInfinity); // cast same as in FST
    return zero;
  }

  static const StringLeftWeight<Label>& One() {
    static const StringLeftWeight<Label> one;
    return one;
  }

  static const StringLeftWeight<Label>& NoWeight() {
    static const StringLeftWeight<Label> no_weight((Label)kStringBad); // cast same as in FST
    return no_weight;
  }

  static const std::string& Type() {
    static const std::string type = "left_string";
    return type;
  }

  friend bool operator==(
      const StringLeftWeight& lhs, 
      const StringLeftWeight& rhs) NOEXCEPT {
    return lhs.str_ == rhs.str_;
  }

  StringLeftWeight() = default;

  explicit StringLeftWeight(Label label) 
    : str_(1, label) {
  }

  template <typename Iterator>
  StringLeftWeight(Iterator begin, Iterator end)
    : str_(begin, end) {
  }

  StringLeftWeight(const StringLeftWeight& rhs)
    : str_(rhs.str_) {
  }

  StringLeftWeight(StringLeftWeight&& rhs) NOEXCEPT
    : str_(std::move(rhs.str_)) {
  }

  StringLeftWeight& operator=(StringLeftWeight&& rhs) NOEXCEPT {
    if (this != &rhs) {
      str_ = std::move(rhs.str_);
    }
    return *this;
  }

  StringLeftWeight& operator=(const StringLeftWeight& rhs) {
    if (this != &rhs) {
      str_ = rhs.str_;
    }
    return *this;
  }

  bool Member() const NOEXCEPT {
    return NoWeight() != *this;
  }

  std::istream& Read(std::istream& strm) {
    // read size
    // use varlen encoding since weights are usually small
    uint32 size;
    {
      auto it = irs::irstd::make_istream_iterator(strm);
      size = irs::vread<uint32_t>(it);
    }

    // read content
    str_.resize(size);
    strm.read(
      reinterpret_cast<char *>(&str_[0]),
      size*sizeof(Label)
    );

    return strm;
  }

  std::ostream& Write(std::ostream &strm) const {
    // write size
    // use varlen encoding since weights are usually small
    const uint32 size =  static_cast<uint32_t>(Size());
    {
      auto it = irs::irstd::make_ostream_iterator(strm);
      irs::vwrite(it, size);
    }

    // write content
    strm.write(
      reinterpret_cast<const char*>(str_.c_str()),
      size*sizeof(Label)
    );

    return strm;
  }

  size_t Hash() const NOEXCEPT {
    return std::hash<str_t>()(str_);
  }

  StringLeftWeight Quantize(float delta = kDelta) const NOEXCEPT { 
    return *this; 
  }

  ReverseWeight Reverse() const {
    return ReverseWeight(str_.rbegin(), str_.rend());
  }

  static uint64 Properties() NOEXCEPT {
    static CONSTEXPR auto props = kLeftSemiring | kIdempotent;
    return props;
  }

  Label& operator[](size_t i) NOEXCEPT {
    return str_[i];
  }

  const Label& operator[](size_t i) const NOEXCEPT {
    return str_[i];
  }

  const Label* c_str() const NOEXCEPT {
    return str_.c_str();
  }

  bool Empty() const NOEXCEPT {
    return str_.empty();
  }

  void Clear() NOEXCEPT {
    str_.clear();
  }

  size_t Size() const NOEXCEPT { 
    return str_.size(); 
  }

  void PushBack(Label label) {
    str_.push_back(label);
  }

  template<typename Iterator>
  void PushBack(Iterator begin, Iterator end) {
    str_.append(begin, end);
  }

  void PushBack(const StringLeftWeight& w) {
    PushBack(w.begin(), w.end());
  }

  void Reserve(size_t capacity) {
    str_.reserve(capacity);
  }

  iterator begin() const NOEXCEPT { return str_.begin(); }
  iterator end() const NOEXCEPT { return str_.end(); }

 private:
  str_t str_;
}; // StringLeftWeight 

template <typename Label>
inline bool operator!=(
    const StringLeftWeight<Label>& w1,
    const StringLeftWeight<Label>& w2) {
  return !(w1 == w2);
}

template <typename Label>
inline std::ostream& operator<<(
    std::ostream& strm,
    const StringLeftWeight<Label>& weight) {
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

template <typename Label>
inline std::istream& operator>>(
    std::istream& strm,
    StringLeftWeight<Label>& weight) {
  std::string str;
  strm >> str;
  if (str == "Infinity") {
    weight = StringLeftWeight<Label>::Zero();
  } else if (str == "Epsilon") {
    weight = StringLeftWeight<Label>::One();
  } else {
    weight.Clear();
    char *p = nullptr;
    for (const char *cs = str.c_str(); !p || *p != '\0'; cs = p + 1) {
      const Label label = strtoll(cs, &p, 10);
      if (p == cs || (*p != 0 && *p != kStringSeparator)) {
        strm.clear(std::ios::badbit);
        break;
      }
      weight.PushBack(label);
    }
  }
  return strm;
}

// Longest common prefix for left string semiring.
template <typename Label>
inline StringLeftWeight<Label> Plus(
    const StringLeftWeight<Label>& lhs,
    const StringLeftWeight<Label>& rhs) {
  typedef StringLeftWeight<Label> Weight;

  if (!lhs.Member() || !rhs.Member()) {
    return Weight::NoWeight();
  }

  if (lhs == Weight::Zero()) {
    return rhs;
  }

  if (rhs == Weight::Zero()) {
    return lhs;
  }

  const auto* plhs = &lhs;
  const auto* prhs = &rhs;

  if (rhs.Size() > lhs.Size()) {
    // enusre that 'prhs' is shorter than 'plhs'
    // The behavior is undefined if the second range is shorter than the first range.
    // (http://en.cppreference.com/w/cpp/algorithm/mismatch)
    std::swap(plhs, prhs);
  }

  assert(prhs->Size() <= plhs->Size());

  return Weight(
    prhs->begin(),
    std::mismatch(prhs->begin(), prhs->end(), plhs->begin()).first
  );
}

template <typename Label>
inline StringLeftWeight<Label> Times(
    const StringLeftWeight<Label>& lhs,
    const StringLeftWeight<Label>& rhs) {
  typedef StringLeftWeight<Label> Weight;

  if (!lhs.Member() || !rhs.Member()) {
    return Weight::NoWeight();
  }

  if (lhs == Weight::Zero() || rhs == Weight::Zero()) {
    return Weight::Zero();
  }

  Weight product;
  product.Reserve(lhs.Size() + rhs.Size());
  product.PushBack(lhs.begin(), lhs.end());
  product.PushBack(rhs.begin(), rhs.end());
  return product;
}

// Left division in a left string semiring.
template <typename Label>
inline StringLeftWeight<Label> DivideLeft(
    const StringLeftWeight<Label>& lhs,
    const StringLeftWeight<Label>& rhs) {
  typedef StringLeftWeight<Label> Weight;
  
  if (!lhs.Member() || !rhs.Member()) {
    return Weight::NoWeight();
  }

  if (rhs == Weight::Zero()) {
    return Weight::NoWeight();
  } if (lhs == Weight::Zero()) {
    return Weight::Zero();
  }

  if (rhs.Size() > lhs.Size()) {
    return Weight();
  }

  return Weight(lhs.begin() + rhs.Size(), lhs.end());
}

template <typename Label>
inline StringLeftWeight<Label> Divide(
    const StringLeftWeight<Label>& lhs,
    const StringLeftWeight<Label>& rhs,
    DivideType typ) {
  assert(DIVIDE_LEFT == typ);
  return DivideLeft(lhs, rhs);
}

NS_END // fst

#endif  // IRESEARCH_FST_STRING_WEIGHT_H
