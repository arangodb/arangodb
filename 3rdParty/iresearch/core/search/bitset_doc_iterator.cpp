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
#include "formats/empty_term_reader.hpp"
#include "utils/bitset.hpp"
#include "utils/math_utils.hpp"

NS_ROOT

bitset_doc_iterator::bitset_doc_iterator(
    const bitset& set,
    const order::prepared& order /*= order::prepared::unordered()*/
) : basic_doc_iterator_base(order),
    begin_(set.begin()),
    end_(set.end()),
    size_(set.size()) {
  auto docs_count = set.count();

  // make doc_id accessible via attribute
  attrs_.emplace(doc_);
  doc_.value = docs_count
    ? doc_limits::invalid()
    : doc_limits::eof(); // seal iterator

  // set estimation value
  estimate(docs_count);
}

bitset_doc_iterator::bitset_doc_iterator(
    const sub_reader& reader,
    const attribute_store& prepared_filter_attrs,
    const bitset& set,
    const order::prepared& order
): bitset_doc_iterator(set, order) {
  prepare_score(ord_->prepare_scorers(
    reader,
    empty_term_reader(cost_.estimate()),
    prepared_filter_attrs,
    attributes() // doc_iterator attributes
  ));
}

bool bitset_doc_iterator::next() NOEXCEPT {
  return !doc_limits::eof(
    seek(doc_.value + irs::doc_id_t(doc_.value < size_))
  );
}

doc_id_t bitset_doc_iterator::seek(doc_id_t target) NOEXCEPT {
  const auto* pword = begin_ + bitset::word(target);

  if (pword >= end_) {
    doc_.value = doc_limits::eof();

    return doc_.value;
  }

  auto word = ((*pword) >> bitset::bit(target));

  typedef decltype(word) word_t;

  if (word) {
    // current word contains the specified 'target'
    doc_.value = target + math::math_traits<word_t>::ctz(word);

    return doc_.value;
  }

  while (!word && ++pword < end_) {
    word = *pword;
  }

  assert(pword >= begin_);

  doc_.value = word
    ? bitset::bit_offset(std::distance(begin_, pword)) + math::math_traits<word_t>::ctz(word)
    : doc_limits::eof();

  return doc_.value;
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
