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

#ifndef IRESEARCH_PHRASE_ITERATOR_H
#define IRESEARCH_PHRASE_ITERATOR_H

#include "disjunction.hpp"

namespace iresearch {

class fixed_phrase_frequency {
 public:
  using term_position_t = std::pair<
    position::ref, // position attribute
    position::value_t>; // desired offset in the phrase

  fixed_phrase_frequency(
      std::vector<term_position_t>&& pos,
      const order::prepared& ord)
    : pos_(std::move(pos)), order_empty_(ord.empty()) {
    assert(!pos_.empty()); // must not be empty
    assert(0 == pos_.front().second); // lead offset is always 0
  }

  frequency* freq() noexcept {
    return order_empty_ ? nullptr : &phrase_freq_;
  }

  filter_boost* boost() noexcept {
    return nullptr;
  }

  // returns frequency of the phrase
  uint32_t operator()() {
    phrase_freq_.value = 0;
    bool match;

    position& lead = pos_.front().first;
    lead.next();

    for (auto end = pos_.end(); !pos_limits::eof(lead.value());) {
      const position::value_t base_position = lead.value();

      match = true;

      for (auto it = pos_.begin() + 1; it != end; ++it) {
        position& pos = it->first;
        const auto term_position = base_position + it->second;
        if (!pos_limits::valid(term_position)) {
          return phrase_freq_.value;
        }
        const auto sought = pos.seek(term_position);

        if (pos_limits::eof(sought)) {
          // exhausted
          return phrase_freq_.value;
        } else if (sought!= term_position) {
          // sought too far from the lead
          match = false;

          lead.seek(sought- it->second);
          break;
        }
      }

      if (match) {
        if (order_empty_) {
          return (phrase_freq_.value = 1);
        }

        ++phrase_freq_.value;
        lead.next();
      }
    }

    return phrase_freq_.value;
  }

 private:
  std::vector<term_position_t> pos_; // list of desired positions along with corresponding attributes
  frequency phrase_freq_; // freqency of the phrase in a document
  bool order_empty_;
}; // fixed_phrase_frequency

////////////////////////////////////////////////////////////////////////////////
/// @class doc_iterator_adapter
/// @brief adapter to use doc_iterator with positions for disjunction
////////////////////////////////////////////////////////////////////////////////
struct variadic_phrase_adapter final : score_iterator_adapter<doc_iterator::ptr> {
  variadic_phrase_adapter() = default;
  variadic_phrase_adapter(doc_iterator::ptr&& it, boost_t boost) noexcept
    : score_iterator_adapter<doc_iterator::ptr>(std::move(it)),
      position(irs::get_mutable<irs::position>(this->it.get())),
      boost(boost) {
  }

  irs::position* position{};
  boost_t boost{no_boost()};
}; // variadic_phrase_adapter

static_assert(std::is_nothrow_move_constructible_v<variadic_phrase_adapter>);
static_assert(std::is_nothrow_move_assignable_v<variadic_phrase_adapter>);

using variadic_term_position = std::pair<
  compound_doc_iterator<variadic_phrase_adapter>*,
  position::value_t>; // desired offset in the phrase

////////////////////////////////////////////////////////////////////////////////
/// @class variadic_phrase_frequency
/// @tparam VolatileBoost boost is not a const
/// @brief helper for variadic phrase frequency evaluation for cases when
///        only one term may be at a single position in a phrase (e.g. synonyms)
////////////////////////////////////////////////////////////////////////////////
template<bool VolatileBoost>
class variadic_phrase_frequency {
 public:
  using term_position_t = variadic_term_position;

  variadic_phrase_frequency(
      std::vector<term_position_t>&& pos,
      const order::prepared& ord)
    : pos_(std::move(pos)),
      phrase_size_(pos_.size()),
      order_empty_(ord.empty()) {
    assert(!pos_.empty() && phrase_size_); // must not be empty
    assert(0 == pos_.front().second); // lead offset is always 0
  }

  frequency* freq() noexcept {
    return order_empty_ ? nullptr : &phrase_freq_;
  }

  filter_boost* boost() noexcept {
    return !VolatileBoost || order_empty_ ? nullptr : &phrase_boost_;
  }

  // returns frequency of the phrase
  uint32_t operator()() {
    if constexpr (VolatileBoost) {
      phrase_boost_.value = {};
    }
    phrase_freq_.value = 0;
    pos_.front().first->visit(this, visit_lead);

    if constexpr (VolatileBoost) {
      if(phrase_freq_.value) {
        phrase_boost_.value = phrase_boost_.value / (phrase_size_ * phrase_freq_.value);
      }
    }

    return phrase_freq_.value;
  }

 private:
  struct sub_match_context {
    position::value_t term_position{ pos_limits::eof() };
    position::value_t min_sought{ pos_limits::eof() };
    boost_t boost{};
    bool match{false};
  };

  static bool visit(void* ctx, variadic_phrase_adapter& it_adapter) {
    assert(ctx);
    auto& match = *reinterpret_cast<sub_match_context*>(ctx);
    auto* p = it_adapter.position;
    p->reset();
    const auto sought = p->seek(match.term_position);
    if (pos_limits::eof(sought)) {
      return true;
    } else if (sought != match.term_position) {
      if (sought < match.min_sought) {
        match.min_sought = sought;
      }
      return true;
    }

    if constexpr (VolatileBoost) {
      match.boost += it_adapter.boost;
    }

    match.match = true;
    return false;
  }

  static bool visit_lead(void* ctx, variadic_phrase_adapter& lead_adapter) {
    assert(ctx);
    auto& self = *reinterpret_cast<variadic_phrase_frequency*>(ctx);
    const auto end = self.pos_.end();
    auto* lead = lead_adapter.position;
    lead->next();

    sub_match_context match;

    for (position::value_t base_position; !pos_limits::eof(base_position = lead->value()); ) {
      match.match = true;
      if constexpr (VolatileBoost) {
        match.boost = lead_adapter.boost;
      }

      for (auto it = self.pos_.begin() + 1; it != end; ++it) {
        match.term_position = base_position + it->second;
        if (!pos_limits::valid(match.term_position)) {
          return false; // invalid for all
        }

        match.match = false;
        match.min_sought = pos_limits::eof();

        it->first->visit(&match, visit);

        if (!match.match) {
          if (!pos_limits::eof(match.min_sought)) {
            lead->seek(match.min_sought - it->second);
            break;
          }

          return true; // eof for all
        }
      }

      if (match.match) {
        ++self.phrase_freq_.value;
        if (self.order_empty_) {
          return false;
        }
        if constexpr (VolatileBoost) {
          self.phrase_boost_.value += match.boost;
        }
        lead->next();
      }
    }

    return true;
  }

  std::vector<term_position_t> pos_; // list of desired positions along with corresponding attributes
  const size_t phrase_size_;         // size of the phrase (speedup phrase boost evaluation)
  frequency phrase_freq_;            // freqency of the phrase in a document
  filter_boost phrase_boost_;        // boost of the phrase in a document
  const bool order_empty_;
}; // variadic_phrase_frequency

////////////////////////////////////////////////////////////////////////////////
/// @class variadic_phrase_frequency_overlapped
/// @tparam VolatileBoost boost is not a const
/// @brief helper for variadic phrase frequency evaluation for cases when
///        different terms may be at the same position in a phrase (e.g. synonyms)
////////////////////////////////////////////////////////////////////////////////
template<bool VolatileBoost>
class variadic_phrase_frequency_overlapped {
 public:
  using term_position_t = variadic_term_position;

  variadic_phrase_frequency_overlapped(
      std::vector<term_position_t>&& pos,
      const order::prepared& ord)
    : pos_(std::move(pos)),
      phrase_size_(pos_.size()),
      order_empty_(ord.empty()) {
    assert(!pos_.empty() && phrase_size_); // must not be empty
    assert(0 == pos_.front().second); // lead offset is always 0
  }

  frequency* freq() noexcept {
    return order_empty_ ? nullptr : &phrase_freq_;
  }

  filter_boost* boost() noexcept {
    return !VolatileBoost || order_empty_ ? nullptr : &phrase_boost_;
  }

  // returns frequency of the phrase
  uint32_t operator()() {
    if constexpr (VolatileBoost) {
      lead_freq_ = 0;
      lead_boost_ = {};
      phrase_boost_.value = {};
    }

    phrase_freq_.value = 0;
    pos_.front().first->visit(this, visit_lead);

    if constexpr (VolatileBoost) {
      if (lead_freq_) {
        phrase_boost_.value = (phrase_boost_.value + (lead_boost_ / lead_freq_)) / phrase_size_;
      }
    }

    return phrase_freq_.value;
  }

 private:
  struct sub_match_context {
    position::value_t term_position{ pos_limits::eof() };
    position::value_t min_sought{ pos_limits::eof() };
    boost_t boost{};
    uint32_t freq{};
  };

  static bool visit(void* ctx, variadic_phrase_adapter& it_adapter) {
    assert(ctx);
    auto& match = *reinterpret_cast<sub_match_context*>(ctx);
    auto* p = it_adapter.position;
    p->reset();
    const auto sought = p->seek(match.term_position);
    if (pos_limits::eof(sought)) {
      return true;
    } else if (sought != match.term_position) {
      if (sought < match.min_sought) {
        match.min_sought = sought;
      }
      return true;
    }

    ++match.freq;
    if constexpr (VolatileBoost) {
      match.boost += it_adapter.boost;
    }

    return true; // continue iteration in overlapped case
  }

  static bool visit_lead(void* ctx, variadic_phrase_adapter& lead_adapter) {
    assert(ctx);
    auto& self = *reinterpret_cast<variadic_phrase_frequency_overlapped*>(ctx);
    const auto end = self.pos_.end();
    auto* lead = lead_adapter.position;
    lead->next();

    sub_match_context match;   // sub-match
    uint32_t phrase_freq = 0;  // phrase frequency for current lead_iterator
    uint32_t match_freq;       // accumulated match frequency for current lead_iterator
    boost_t phrase_boost = {}; // phrase boost for current lead_iterator
    boost_t match_boost;       // accumulated match boost for current lead_iterator
    for (position::value_t base_position; !pos_limits::eof(base_position = lead->value()); ) {
      match_freq = 1;
      if constexpr (VolatileBoost) {
        match_boost = 0.f;
      }

      for (auto it = self.pos_.begin() + 1; it != end; ++it) {
        match.term_position = base_position + it->second;
        if (!pos_limits::valid(match.term_position)) {
          return false; // invalid for all
        }

        match.freq = 0;
        if constexpr (VolatileBoost) {
          match.boost = 0.f;
        }
        match.min_sought = pos_limits::eof();

        it->first->visit(&match, visit);

        if (!match.freq) {
          match_freq = 0;

          if (!pos_limits::eof(match.min_sought)) {
            lead->seek(match.min_sought - it->second);
            break;
          }

          if constexpr (VolatileBoost) {
            if (phrase_freq) {
              ++self.lead_freq_;
              self.lead_boost_ += lead_adapter.boost;
              self.phrase_boost_.value += phrase_boost / phrase_freq;
            }
          }

          return true; // eof for all
        }

        match_freq *= match.freq;
        if constexpr (VolatileBoost) {
          match_boost += match.boost / match.freq;
        }
      }

      if (match_freq) {
        self.phrase_freq_.value += match_freq;
        if (self.order_empty_) {
          return false;
        }
        ++phrase_freq;
        if constexpr (VolatileBoost) {
          phrase_boost += match_boost;
        }
        lead->next();
      }
    }

    if constexpr (VolatileBoost) {
      if (phrase_freq) {
        ++self.lead_freq_;
        self.lead_boost_ += lead_adapter.boost;
        self.phrase_boost_.value += phrase_boost / phrase_freq;
      }
    }

    return true;
  }

  std::vector<term_position_t> pos_; // list of desired positions along with corresponding attributes
  const size_t phrase_size_;         // size of the phrase (speedup phrase boost evaluation)
  frequency phrase_freq_;            // freqency of the phrase in a document
  filter_boost phrase_boost_;        // boost of the phrase in a document
  boost_t lead_boost_{0.f}; 				 // boost from all matched lead iterators
  uint32_t lead_freq_{0};            // number of matched lead iterators
  const bool order_empty_;
}; // variadic_phrase_frequency_overlapped

// implementation is optimized for frequency based similarity measures
// for generic implementation see a03025accd8b84a5f8ecaaba7412fc92a1636be3
template<typename Conjunction, typename Frequency>
class phrase_iterator final : public doc_iterator {
 public:
  phrase_iterator(
      typename Conjunction::doc_iterators_t&& itrs,
      std::vector<typename Frequency::term_position_t>&& pos,
      const sub_reader& segment,
      const term_reader& field,
      const byte_type* stats,
      const order::prepared& ord,
      boost_t boost)
    : approx_(std::move(itrs)),
      freq_(std::move(pos), ord),
      doc_(irs::get_mutable<document>(&approx_)),
      attrs_{{
        { type<document>::id(),     doc_          },
        { type<cost>::id(),         &cost_        },
        { type<score>::id(),        &score_       },
        { type<frequency>::id(),    freq_.freq()  },
        { type<filter_boost>::id(), freq_.boost() },
      }},
      score_(ord),
      cost_([this](){ return cost::extract(approx_); }) { // FIXME find a better estimation
    assert(doc_);

    if (!ord.empty()) {
      order::prepared::scorers scorers(
        ord, segment, field, stats,
        score_.data(), *this, boost);

      irs::reset(score_, std::move(scorers));
    }
  }

  virtual attribute* get_mutable(type_info::type_id type) noexcept {
    return attrs_.get_mutable(type);
  }

  virtual doc_id_t value() const override final {
    return doc_->value;
  }

  virtual bool next() override {
    bool next = false;
    while ((next = approx_.next()) && !freq_()) {}

    return next;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    // important to call freq_() in order
    // to set attribute values
    const auto prev = doc_->value;
    const auto doc = approx_.seek(target);

    if (prev == doc || freq_()) {
      return doc;
    }

    next();

    return doc_->value;
  }

 private:
  Conjunction approx_; // first approximation (conjunction over all words in a phrase)
  Frequency freq_;
  document* doc_{}; // document itself
  frozen_attributes<5, attribute_provider> attrs_; // FIXME can store only 4 attrbiutes for non-volatile boost case
  score score_;
  cost cost_;
}; // phrase_iterator

} // ROOT

#endif
