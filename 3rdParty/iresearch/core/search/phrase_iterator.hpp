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

// implementation is optimized for frequency based similarity measures
// for generic implementation see a03025accd8b84a5f8ecaaba7412fc92a1636be3
class phrase_iterator final : public basic_doc_iterator_base {
 public:
  typedef std::pair<
    position::ref, // position attribute
    position::value_t // desired offset in the phrase
  > position_t;
  typedef std::vector<position_t> positions_t;

  phrase_iterator(
      conjunction::doc_iterators_t&& itrs,
      positions_t&& pos,
      const sub_reader& segment,
      const term_reader& field,
      const byte_type* stats,
      const order::prepared& ord,
      boost_t boost
  ) : approx_(std::move(itrs)),
      pos_(std::move(pos)),
      order_(&ord) {
    assert(!pos_.empty()); // must not be empty
    assert(0 == pos_.front().second); // lead offset is always 0

    // FIXME find a better estimation
    // estimate iterator
    estimate([this](){ return irs::cost::extract(approx_.attributes()); });

    // set attributes
    attrs_.emplace(phrase_freq_); // phrase frequency
    doc_ = (attrs_.emplace<irs::document>()
             = approx_.attributes().get<irs::document>()).get(); // document (required by scorers)
    assert(doc_);

    // set scorers
    prepare_score(
      ord,
      ord.prepare_scorers(segment, field, stats, attributes(), boost)
    );
  }

  virtual doc_id_t value() const override final {
    return doc_->value;
  }

  virtual bool next() override {
    bool next = false;
    while ((next = approx_.next()) && !(phrase_freq_.value = phrase_freq())) {}

    return next;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    approx_.seek(target);

    if (doc_limits::eof(value()) || (phrase_freq_.value = phrase_freq())) {
      return value();
    }

    next();

    return value();
  }

 private:
  // returns frequency of the phrase
  frequency::value_t phrase_freq() {
    frequency::value_t freq = 0;
    bool match;

    position& lead = pos_.front().first;
    lead.next();

    for (auto end = pos_.end(); !pos_limits::eof(lead.value());) {
      const position::value_t base_offset = lead.value();

      match = true;

      for (auto it = pos_.begin() + 1; it != end; ++it) {
        position& pos = it->first;
        const auto term_offset = base_offset + it->second;
        const auto seeked = pos.seek(term_offset);

        if (pos_limits::eof(seeked)) {
          // exhausted
          return freq;
        } else if (seeked != term_offset) {
          // seeked too far from the lead
          match = false;

          lead.seek(seeked - it->second);
          break;
        }
      }

      if (match) {
        if (order_->empty()) {
          return 1;
        }

        ++freq;
        lead.next();
      }
    }

    return freq;
  }

  conjunction approx_; // first approximation (conjunction over all words in a phrase)
  const document* doc_{}; // document itself
  frequency phrase_freq_; // freqency of the phrase in a document
  positions_t pos_; // list of desired positions along with corresponding attributes
  const order::prepared* order_;
}; // phrase_iterator

NS_END // ROOT

#endif
