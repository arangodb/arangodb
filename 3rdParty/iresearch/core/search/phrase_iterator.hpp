//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_PHRASE_ITERATOR_H
#define IRESEARCH_PHRASE_ITERATOR_H

#include "shared.hpp"

NS_ROOT

template<typename Conjunction>
class phrase_iterator final : public Conjunction {
 public:
  typedef typename Conjunction::doc_iterator doc_iterator_t;
  typedef typename Conjunction::traits_t traits_t;
  typedef typename std::enable_if<
    std::is_base_of<
      detail::conjunction<doc_iterator_t, traits_t>, 
      Conjunction
    >::value, Conjunction
  >::type conjunction_t;

  typedef std::pair<
    position::cref, // position attribute
    position::value_t // desired offset in the phrase
  > position_t;
  typedef std::vector<position_t> positions_t;

  phrase_iterator(
      typename conjunction_t::doc_iterators_t&& itrs,
      const order::prepared& ord, 
      positions_t&& pos, 
      boost::boost_t phrase_boost = boost::no_boost())
    : conjunction_t(std::move(itrs), ord), 
      pos_(std::move(pos)),
      phrase_boost_(phrase_boost) {
    assert(!pos_.empty());

    // add phrase frequency
    conjunction_t::attrs_.template emplace(phrase_freq_);
  }

  virtual void score_impl(byte_type* lhs) override {
    conjunction_t::score_impl(lhs);

    // TODO: it's dangeorus to pass byte_type* here because scorers have no
    // assumptions about the the of the boost. Perhaps we should introduce
    // operation with fixed type or cast operation for scorer.
    // Refactor this piece of code when score API will be defined
    const boost::boost_t phrase_boost = phrase_boost_*phrase_freq_.value;
    conjunction_t::ord_->add(lhs, reinterpret_cast<const byte_type*>(&phrase_boost));
  }

  virtual bool next() override {
    bool next = false;
    while((next = conjunction_t::next()) && !(phrase_freq_.value = phrase_freq())) {}
    return next;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    const auto doc = conjunction_t::seek(target);

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

    if (1 == conjunction_t::size()) {
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

  positions_t pos_;
  frequency phrase_freq_;
  boost::boost_t phrase_boost_;
}; // phrase_iterator

NS_END // ROOT

#endif
