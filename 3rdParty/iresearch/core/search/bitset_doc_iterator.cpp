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
#include "utils/math_utils.hpp"

namespace iresearch {

bitset_doc_iterator::bitset_doc_iterator(
    const word_t* begin,
    const word_t* end) noexcept
  : cost_(math::math_traits<word_t>::pop(begin, end)),
    doc_(cost_.estimate()
      ? doc_limits::invalid()
      : doc_limits::eof()),
    begin_(begin),
    end_(end) {
  reset();
}

attribute* bitset_doc_iterator::get_mutable(irs::type_info::type_id id) noexcept {
  if (type<document>::id() == id) {
    return &doc_;
  }

  return type<cost>::id() == id
    ? &cost_ : nullptr;
}

bool bitset_doc_iterator::next() noexcept {
  while (!word_) {
    if (next_ >= end_) {
      if (refill(&begin_, &end_)) {
        reset();
        continue;
      }

      doc_.value = doc_limits::eof();
      word_ = 0;

      return false;
    }

    word_ = *next_++;
    base_ += bits_required<word_t>();
    doc_.value = base_ - 1;
  }

  // FIXME remove conversion
  const doc_id_t delta = doc_id_t(math::math_traits<word_t>::ctz(word_));
  assert(delta < bits_required<word_t>());

  word_ = (word_ >> delta) >> 1;
  doc_.value += 1 + delta;

  return true;
}

doc_id_t bitset_doc_iterator::seek(doc_id_t target) noexcept {
  const doc_id_t word_idx = target / bits_required<word_t>();

  while (1) {
    next_ = begin_ + word_idx;

    if (next_ >= end_) {
      if (refill(&begin_, &end_)) {
        reset();
        continue;
      }

      doc_.value = doc_limits::eof();
      word_ = 0;

      return doc_.value;
    }

    break;
  }

  const doc_id_t bit_idx = target % bits_required<word_t>();
  base_ = word_idx * bits_required<word_t>();
  word_ = (*next_++) >> bit_idx;
  doc_.value = base_ - 1 + bit_idx;

  // FIXME consider inlining to speedup
  next();

  return doc_.value;
}

} // ROOT
