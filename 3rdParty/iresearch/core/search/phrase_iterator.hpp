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

#ifndef IRESEARCH_PHRASE_ITERATOR_H
#define IRESEARCH_PHRASE_ITERATOR_H

#include "shared.hpp"
#include "conjunction.hpp"

NS_ROOT

class phrase_iterator final : public conjunction {
 public:
  typedef std::pair<
    position::cref, // position attribute
    position::value_t // desired offset in the phrase
  > position_t;
  typedef std::vector<position_t> positions_t;

  phrase_iterator(
      conjunction::doc_iterators_t&& itrs,
      const order::prepared& ord,
      positions_t&& pos)
    : conjunction(std::move(itrs), ord),
     pos_(std::move(pos)) {
    assert(!pos_.empty());

    // add phrase frequency
    conjunction::attrs_.emplace(phrase_freq_);
  }

  virtual bool next() override {
    bool next = false;
    while((next = conjunction::next()) && !(phrase_freq_.value = phrase_freq())) {}
    return next;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    const auto doc = conjunction::seek(target);

    if (type_limits<type_t::doc_id_t>::eof(doc) || (phrase_freq_.value = phrase_freq())) {
      return doc;
    }

    next();
    return this->value();
  }

 private:
  // returns frequency of the phrase
  frequency::value_t phrase_freq() const {
    assert(!pos_.empty());

    if (1 == conjunction::size()) {
      // equal to term, do not to count it with postings
      return 1;
    }

    frequency::value_t freq = 0;
    for (const position& lead = pos_.front().first; lead.next();) {
      position::value_t base_offset = lead.value();

      frequency::value_t match = 1;
      for (auto it = pos_.begin()+1, end = pos_.end(); it != end; ++it) {
        const position &pos = it->first;
        const auto term_offset = base_offset + it->second;
        const auto seeked = pos.seek(term_offset);
        if (position::NO_MORE == seeked) {
          // finished
          return freq;
        } else if (seeked != term_offset) {
          match = 0;
          break;
        }
      }
      freq += match;
    }

    return freq;
  }

  // a value representing the freqency of the phrase in the document
  // FIXME TODO: should be used to modify scoring by supporting scorers
  frequency phrase_freq_;

  positions_t pos_;
}; // phrase_iterator

NS_END // ROOT

#endif
