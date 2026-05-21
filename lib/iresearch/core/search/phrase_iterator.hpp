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

#pragma once

#include "analysis/token_attributes.hpp"
#include "disjunction.hpp"

namespace irs {

template<typename Frequency>
class PhrasePosition final : public position, public Frequency {
 public:
  explicit PhrasePosition(
    std::vector<typename Frequency::TermPosition>&& pos) noexcept
    : Frequency{std::move(pos)} {
    std::tie(start_, end_) = this->GetOffsets();
  }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return type == irs::type<offset>::id() ? &offset_ : nullptr;
  }

  bool next() final {
    if (!left_) {
      // At least 1 position is always approved by the phrase,
      // and calling next() on exhausted iterator is UB.
      left_ = 1;
      value_ = irs::pos_limits::invalid();
      return false;
    }
    ++value_;
    offset_.start = *start_;
    offset_.end = *end_;
    left_ += this->NextPosition() - 1;
    return true;
  }

 private:
  offset offset_;
  const uint32_t* start_{};
  const uint32_t* end_{};
  uint32_t left_{1};
};

template<typename T>
struct HasPosition : std::false_type {};

template<typename T>
struct HasPosition<PhrasePosition<T>> : std::true_type {};

// position attribute + desired offset in the phrase
using FixedTermPosition = std::pair<position::ref, position::value_t>;

template<bool OneShot, bool HasFreq>
class FixedPhraseFrequency {
 public:
  using TermPosition = FixedTermPosition;

  explicit FixedPhraseFrequency(std::vector<TermPosition>&& pos) noexcept
    : pos_{std::move(pos)} {
    IRS_ASSERT(!pos_.empty());             // must not be empty
    IRS_ASSERT(0 == pos_.front().second);  // lead offset is always 0
  }

  attribute* GetMutable(irs::type_info::type_id id) noexcept {
    if constexpr (HasFreq) {
      if (id == irs::type<frequency>::id()) {
        return &phrase_freq_;
      }
    }

    return nullptr;
  }

  // returns frequency of the phrase
  uint32_t EvaluateFreq() { return phrase_freq_.value = NextPosition(); }

 private:
  friend class PhrasePosition<FixedPhraseFrequency>;

  std::pair<const uint32_t*, const uint32_t*> GetOffsets() const noexcept {
    auto start = irs::get<irs::offset>(pos_.front().first.get());
    IRS_ASSERT(start);
    auto end = irs::get<irs::offset>(pos_.back().first.get());
    IRS_ASSERT(end);
    return {&start->start, &end->end};
  }

  uint32_t NextPosition() {
    uint32_t phrase_freq = 0;
    position& lead = pos_.front().first;
    lead.next();

    for (auto end = std::end(pos_); !pos_limits::eof(lead.value());) {
      const position::value_t base_position = lead.value();

      bool match = true;

      for (auto it = std::begin(pos_) + 1; it != end; ++it) {
        position& pos = it->first;
        const auto term_position = base_position + it->second;
        if (!pos_limits::valid(term_position)) {
          return phrase_freq;
        }
        const auto sought = pos.seek(term_position);

        if (pos_limits::eof(sought)) {
          // exhausted
          return phrase_freq;
        } else if (sought != term_position) {
          // sought too far from the lead
          match = false;

          lead.seek(sought - it->second);
          break;
        }
      }

      if (match) {
        ++phrase_freq;

        if constexpr (OneShot) {
          return phrase_freq;
        }

        lead.next();
      }
    }

    return phrase_freq;
  }

  // list of desired positions along with corresponding attributes
  std::vector<TermPosition> pos_;
  // freqency of the phrase in a document
  frequency phrase_freq_;
};

// Adapter to use doc_iterator with positions for disjunction
struct VariadicPhraseAdapter : ScoreAdapter<> {
  VariadicPhraseAdapter() = default;
  VariadicPhraseAdapter(doc_iterator::ptr&& it, score_t boost) noexcept
    : ScoreAdapter<>(std::move(it)),
      position(irs::get_mutable<irs::position>(this->it.get())),
      boost(boost) {}

  irs::position* position{};
  score_t boost{kNoBoost};
};

static_assert(std::is_nothrow_move_constructible_v<VariadicPhraseAdapter>);
static_assert(std::is_nothrow_move_assignable_v<VariadicPhraseAdapter>);

struct VariadicPhraseOffsetAdapter final : VariadicPhraseAdapter {
  VariadicPhraseOffsetAdapter() = default;
  VariadicPhraseOffsetAdapter(doc_iterator::ptr&& it, score_t boost) noexcept
    : VariadicPhraseAdapter{std::move(it), boost},
      offset{this->position ? irs::get<irs::offset>(*this->position)
                            // FIXME(gnusi): use constant
                            : nullptr} {}

  const irs::offset* offset{};
};

static_assert(
  std::is_nothrow_move_constructible_v<VariadicPhraseOffsetAdapter>);
static_assert(std::is_nothrow_move_assignable_v<VariadicPhraseOffsetAdapter>);

template<typename Adapter>
using VariadicTermPosition =
  std::pair<compound_doc_iterator<Adapter>*,
            position::value_t>;  // desired offset in the phrase

// Helper for variadic phrase frequency evaluation for cases when
// only one term may be at a single position in a phrase (e.g. synonyms)
template<typename Adapter, bool VolatileBoost, bool OneShot, bool HasFreq>
class VariadicPhraseFrequency {
 public:
  using TermPosition = VariadicTermPosition<Adapter>;

  explicit VariadicPhraseFrequency(std::vector<TermPosition>&& pos) noexcept
    : pos_{std::move(pos)}, phrase_size_{pos_.size()} {
    IRS_ASSERT(!pos_.empty() && phrase_size_);  // must not be empty
    IRS_ASSERT(0 == pos_.front().second);       // lead offset is always 0
  }

  attribute* GetMutable(irs::type_info::type_id id) noexcept {
    if constexpr (VolatileBoost) {
      if (id == irs::type<filter_boost>::id()) {
        return &phrase_boost_;
      }
    }

    if constexpr (HasFreq) {
      if (id == irs::type<frequency>::id()) {
        return &phrase_freq_;
      }
    }

    return nullptr;
  }

  // Evaluate and return frequency of the phrase
  uint32_t EvaluateFreq() {
    if constexpr (VolatileBoost) {
      phrase_boost_.value = {};
    }
    phrase_freq_.value = 0;
    pos_.front().first->visit(this, VisitLead);

    if constexpr (VolatileBoost) {
      if (phrase_freq_.value) {
        phrase_boost_.value /=
          static_cast<score_t>(phrase_size_ * phrase_freq_.value);
      }
    }

    return phrase_freq_.value;
  }

 private:
  friend class PhrasePosition<VariadicPhraseFrequency>;

  struct SubMatchContext {
    position::value_t term_position{pos_limits::eof()};
    position::value_t min_sought{pos_limits::eof()};
    const uint32_t* end{};  // end match offset
    score_t boost{};
    bool match{false};
  };

  std::pair<const uint32_t*, const uint32_t*> GetOffsets() const noexcept {
    return {&start, &end};
  }

  uint32_t NextPosition() {
    // FIXME(gnusi): don't change iterator state
    phrase_freq_.value = 0;
    pos_.front().first->visit(this, VisitLead);
    return phrase_freq_.value;
  }

  static bool VisitFollower(void* ctx, Adapter& it_adapter) {
    IRS_ASSERT(ctx);
    auto& match = *reinterpret_cast<SubMatchContext*>(ctx);
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

    if constexpr (std::is_same_v<Adapter, VariadicPhraseOffsetAdapter>) {
      if (it_adapter.offset) {  // FIXME(gnusi): remove condition
        match.end = &it_adapter.offset->end;
      }
    }

    match.match = true;
    return false;
  }

  static bool VisitLead(void* ctx, Adapter& lead_adapter) {
    IRS_ASSERT(ctx);
    auto& self = *reinterpret_cast<VariadicPhraseFrequency*>(ctx);
    const auto end = std::end(self.pos_);
    auto* lead = lead_adapter.position;
    lead->next();

    SubMatchContext match;

    for (position::value_t base_position;
         !pos_limits::eof(base_position = lead->value());) {
      match.match = true;
      if constexpr (VolatileBoost) {
        match.boost = lead_adapter.boost;
      }

      for (auto it = std::begin(self.pos_) + 1; it != end; ++it) {
        match.term_position = base_position + it->second;
        if (!pos_limits::valid(match.term_position)) {
          return false;  // invalid for all
        }

        match.match = false;
        match.min_sought = pos_limits::eof();

        it->first->visit(&match, VisitFollower);

        if (!match.match) {
          if (!pos_limits::eof(match.min_sought)) {
            lead->seek(match.min_sought - it->second);
            break;
          }

          return true;  // eof for all
        }
      }

      if (match.match) {
        ++self.phrase_freq_.value;
        if constexpr (std::is_same_v<Adapter, VariadicPhraseOffsetAdapter>) {
          IRS_ASSERT(lead_adapter.offset);
          self.start = lead_adapter.offset->start;
          IRS_ASSERT(match.end);
          self.end = *match.end;
        }
        if constexpr (OneShot) {
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

  // list of desired positions along with corresponding attributes
  std::vector<TermPosition> pos_;
  // size of the phrase (speedup phrase boost evaluation)
  const size_t phrase_size_;
  frequency phrase_freq_;      // freqency of the phrase in a document
  filter_boost phrase_boost_;  // boost of the phrase in a document

  // FIXME(gnusi): refactor
  uint32_t start{};
  uint32_t end{};
};

// Helper for variadic phrase frequency evaluation for cases when
// different terms may be at the same position in a phrase (e.g.
// synonyms)
template<typename Adapter, bool VolatileBoost, bool OneShot, bool HasFreq>
class VariadicPhraseFrequencyOverlapped {
 public:
  using TermPosition = VariadicTermPosition<Adapter>;

  explicit VariadicPhraseFrequencyOverlapped(
    std::vector<TermPosition>&& pos) noexcept
    : pos_(std::move(pos)), phrase_size_(pos_.size()) {
    IRS_ASSERT(!pos_.empty() && phrase_size_);  // must not be empty
    IRS_ASSERT(0 == pos_.front().second);       // lead offset is always 0
  }

  attribute* GetMutable(irs::type_info::type_id id) noexcept {
    if constexpr (VolatileBoost) {
      if (id == irs::type<filter_boost>::id()) {
        return &phrase_boost_;
      }
    }

    if constexpr (HasFreq) {
      if (id == irs::type<frequency>::id()) {
        return &phrase_freq_;
      }
    }

    return nullptr;
  }

  // returns frequency of the phrase
  uint32_t EvaluateFreq() {
    if constexpr (VolatileBoost) {
      lead_freq_ = 0;
      lead_boost_ = {};
      phrase_boost_.value = {};
    }

    phrase_freq_.value = 0;
    pos_.front().first->visit(this, VisitLead);

    if constexpr (VolatileBoost) {
      if (lead_freq_) {
        phrase_boost_.value =
          (phrase_boost_.value + (lead_boost_ / lead_freq_)) / phrase_size_;
      }
    }

    return phrase_freq_.value;
  }

 private:
  struct SubMatchContext {
    position::value_t term_position{pos_limits::eof()};
    position::value_t min_sought{pos_limits::eof()};
    score_t boost{};
    uint32_t freq{};
  };

  static bool VisitFollower(void* ctx, Adapter& it_adapter) {
    IRS_ASSERT(ctx);
    auto& match = *reinterpret_cast<SubMatchContext*>(ctx);
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

    return true;  // continue iteration in overlapped case
  }

  static bool VisitLead(void* ctx, Adapter& lead_adapter) {
    IRS_ASSERT(ctx);
    auto& self = *reinterpret_cast<VariadicPhraseFrequencyOverlapped*>(ctx);
    const auto end = std::end(self.pos_);
    auto* lead = lead_adapter.position;
    lead->next();

    SubMatchContext match;     // sub-match
    uint32_t phrase_freq = 0;  // phrase frequency for current lead_iterator
    // accumulated match frequency for current lead_iterator
    uint32_t match_freq;
    score_t phrase_boost = {};  // phrase boost for current lead_iterator
    score_t match_boost;  // accumulated match boost for current lead_iterator
    for (position::value_t base_position;
         !pos_limits::eof(base_position = lead->value());) {
      match_freq = 1;
      if constexpr (VolatileBoost) {
        match_boost = 0.f;
      }

      for (auto it = std::begin(self.pos_) + 1; it != end; ++it) {
        match.term_position = base_position + it->second;
        if (!pos_limits::valid(match.term_position)) {
          return false;  // invalid for all
        }

        match.freq = 0;
        if constexpr (VolatileBoost) {
          match.boost = 0.f;
        }
        match.min_sought = pos_limits::eof();

        it->first->visit(&match, VisitFollower);

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

          return true;  // eof for all
        }

        match_freq *= match.freq;
        if constexpr (VolatileBoost) {
          match_boost += match.boost / match.freq;
        }
      }

      if (match_freq) {
        self.phrase_freq_.value += match_freq;
        if constexpr (OneShot) {
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
  // list of desired positions along with corresponding attributes
  std::vector<TermPosition> pos_;
  // size of the phrase (speedup phrase boost evaluation)
  const size_t phrase_size_;
  frequency phrase_freq_;      // freqency of the phrase in a document
  filter_boost phrase_boost_;  // boost of the phrase in a document
  score_t lead_boost_{0.f};    // boost from all matched lead iterators
  uint32_t lead_freq_{0};      // number of matched lead iterators
};

// implementation is optimized for frequency based similarity measures
// for generic implementation see a03025accd8b84a5f8ecaaba7412fc92a1636be3
template<typename Conjunction, typename Frequency>
class PhraseIterator : public doc_iterator {
 public:
  using TermPosition = typename Frequency::TermPosition;

  PhraseIterator(ScoreAdapters&& itrs, std::vector<TermPosition>&& pos)
    : approx_{NoopAggregator{},
              [](auto&& itrs) {
                std::sort(itrs.begin(), itrs.end(),
                          [](const auto& lhs, const auto& rhs) noexcept {
                            return cost::extract(lhs, cost::kMax) <
                                   cost::extract(rhs, cost::kMax);
                          });
                return std::move(itrs);
              }(std::move(itrs))},
      freq_{std::move(pos)} {
    std::get<attribute_ptr<document>>(attrs_) =
      irs::get_mutable<document>(&approx_);

    // FIXME find a better estimation
    std::get<attribute_ptr<irs::cost>>(attrs_) =
      irs::get_mutable<irs::cost>(&approx_);
  }

  PhraseIterator(ScoreAdapters&& itrs,
                 std::vector<typename Frequency::TermPosition>&& pos,
                 const SubReader& segment, const term_reader& field,
                 const byte_type* stats, const Scorers& ord, score_t boost)
    : PhraseIterator{std::move(itrs), std::move(pos)} {
    if (!ord.empty()) {
      auto& score = std::get<irs::score>(attrs_);
      CompileScore(score, ord.buckets(), segment, field, stats, *this, boost);
    }
  }

  attribute* get_mutable(type_info::type_id type) noexcept final {
    if (type == irs::type<irs::position>::id()) {
      if constexpr (HasPosition<Frequency>::value) {
        return &freq_;
      } else {
        return nullptr;
      }
    }

    auto* attr = freq_.GetMutable(type);
    return attr ? attr : irs::get_mutable(attrs_, type);
  }

  doc_id_t value() const final {
    return std::get<attribute_ptr<document>>(attrs_).ptr->value;
  }

  bool next() final {
    bool next = false;
    while ((next = approx_.next()) && !freq_.EvaluateFreq()) {
    }

    return next;
  }

  doc_id_t seek(doc_id_t target) final {
    auto* pdoc = std::get<attribute_ptr<document>>(attrs_).ptr;

    // important to call freq_.EvaluateFreq() in order
    // to set attribute values
    const auto prev = pdoc->value;
    const auto doc = approx_.seek(target);

    if (prev == doc || freq_.EvaluateFreq()) {
      return doc;
    }

    next();

    return pdoc->value;
  }

 private:
  using attributes =
    std::tuple<attribute_ptr<document>, attribute_ptr<cost>, score>;

  // first approximation (conjunction over all words in a phrase)
  Conjunction approx_;
  Frequency freq_;
  attributes attrs_;
};

}  // namespace irs
