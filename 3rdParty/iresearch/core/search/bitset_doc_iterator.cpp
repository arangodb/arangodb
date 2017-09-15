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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "bitset_doc_iterator.hpp"
#include "utils/bitset.hpp"
#include "utils/math_utils.hpp"

NS_ROOT

bitset_doc_iterator::bitset_doc_iterator(const bitset& set)
  : begin_(set.begin()), end_(set.end()), size_(set.size()) {
  const auto cardinality = set.count();

  // set estimation value
  est_.value(cardinality);
  attrs_.emplace(est_);

  if (!cardinality) {
    // seal iterator
    doc_ = type_limits<type_t::doc_id_t>::eof();
  }
}

bool bitset_doc_iterator::next() NOEXCEPT {
  return !type_limits<type_t::doc_id_t>::eof(
    seek(doc_ + irs::doc_id_t(doc_ < size_))
  );
}

doc_id_t bitset_doc_iterator::seek(doc_id_t target) NOEXCEPT {
  const auto* pword = begin_ + bitset::word(target);

  if (pword >= end_) {
    doc_ = type_limits<type_t::doc_id_t>::eof();
    return doc_;
  }

  auto word = ((*pword) >> bitset::bit(target));

  typedef decltype(word) word_t;

  if (word) {
    // current word contains the specified 'target'
    doc_ = target + math::math_traits<word_t>::ctz(word);
    return doc_;
  }

  while (!word && ++pword < end_) {
    word = *pword;
  }

  assert(pword >= begin_);

  doc_ = word
    ? bitset::bit_offset(std::distance(begin_, pword)) + math::math_traits<word_t>::ctz(word)
    : type_limits<type_t::doc_id_t>::eof();

  return doc_;
}

NS_END // ROOT
