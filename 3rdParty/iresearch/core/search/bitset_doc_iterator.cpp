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

#include "bitset_doc_iterator.hpp"
#include "formats/empty_term_reader.hpp"
#include "utils/bitset.hpp"
#include "utils/math_utils.hpp"

namespace iresearch {

bitset_doc_iterator::bitset_doc_iterator(const bitset& set,
                                         const order::prepared& ord)
  : attributes{{
      { type<document>::id(), &doc_   },
      { type<cost>::id(),     &cost_  },
      { type<score>::id(),    &score_ },
    }},
    cost_(set.count()),
    doc_(cost_.estimate()
      ? doc_limits::invalid()
      : doc_limits::eof()),
    score_(ord),
    begin_(set.begin()),
    end_(set.end()),
    next_(begin_) {
}

bitset_doc_iterator::bitset_doc_iterator(
      const sub_reader& reader,
      const byte_type* stats,
      const bitset& set,
      const order::prepared& order,
      boost_t boost)
  : bitset_doc_iterator(set, order) {
  // prepare score
  if (!order.empty()) {
    order::prepared::scorers scorers(
      order, reader, empty_term_reader(cost_.estimate()),
      stats, score_.data(),
      *this, // doc_iterator attributes
      boost);

    irs::reset(score_, std::move(scorers));
  }
}

bool bitset_doc_iterator::next() noexcept {
  while (!word_) {
    if (next_ >= end_) {
      doc_.value = doc_limits::eof();
      word_ = 0;

      return false;
    }

    word_ = *next_++;
    base_ += bits_required<word_t>();
  }

  const doc_id_t delta = math::math_traits<word_t>::ctz(word_);
  irs::unset_bit(word_, delta);
  doc_.value = base_ + delta;

  return true;
}

doc_id_t bitset_doc_iterator::seek(doc_id_t target) noexcept {
  next_ = begin_ + bitset::word(target);

  if (next_ >= end_) {
    doc_.value = doc_limits::eof();
    word_ = 0;

    return doc_.value;
  }

  base_ = doc_id_t(std::distance(begin_, next_) * bits_required<word_t>());
  word_ = (*next_++) & ((~word_t(0)) << bitset::bit(target));

  next();

  return doc_.value;
}

} // ROOT
