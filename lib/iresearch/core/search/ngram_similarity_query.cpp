////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Abramov
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "ngram_similarity_query.hpp"

#include "index/field_meta.hpp"
#include "search/min_match_disjunction.hpp"
#include "search/ngram_similarity_filter.hpp"

namespace irs {
namespace {

struct Position {
  template<typename Iterator>
  explicit Position(Iterator& itr) noexcept
    : pos{&position::get_mutable(itr)},
      doc{irs::get<document>(itr)},
      scr{&irs::score::get(itr)} {
    IRS_ASSERT(pos);
    IRS_ASSERT(doc);
    IRS_ASSERT(scr);
  }

  position* pos;
  const document* doc;
  const score* scr;
};

struct PositionWithOffset : Position {
  template<typename Iterator>
  explicit PositionWithOffset(Iterator& itr) noexcept
    : Position{itr}, offs{irs::get<offset>(*this->pos)} {
    IRS_ASSERT(offs);
  }

  const offset* offs;
};

template<bool IsStart, typename T>
uint32_t GetOffset(const T& pos) noexcept {
  if constexpr (std::is_same_v<PositionWithOffset, T>) {
    if constexpr (IsStart) {
      return pos.offs->start;
    } else {
      return pos.offs->end;
    }
  } else {
    return 0;
  }
}

struct SearchState {
  template<typename T>
  SearchState(uint32_t p, const T& attrs)
    : scr{attrs.scr}, len{1}, pos{p}, offs{GetOffset<true>(attrs)} {}

  // appending constructor
  template<typename T>
  SearchState(std::shared_ptr<SearchState> other, uint32_t p, const T& attrs)
    : parent{std::move(other)},
      scr{attrs.scr},
      len{parent->len + 1},
      pos{p},
      offs{GetOffset<false>(attrs)} {}

  std::shared_ptr<SearchState> parent;
  const score* scr;
  uint32_t len;
  uint32_t pos;
  uint32_t offs;
};

using SearchStates =
  std::map<uint32_t, std::shared_ptr<SearchState>, std::greater<>>;

template<bool FullMatch>
class NGramApprox : public MinMatchDisjunction<NoopAggregator> {
  using Base = MinMatchDisjunction<NoopAggregator>;

 public:
  using Base::Base;
};

template<>
class NGramApprox<true> : public Conjunction<CostAdapter<>, NoopAggregator> {
  using Base = Conjunction<CostAdapter<>, NoopAggregator>;

 public:
  NGramApprox(CostAdapters&& itrs, size_t min_match_count)
    : Base{NoopAggregator{},
           [](auto&& itrs) {
             std::sort(itrs.begin(), itrs.end(),
                       [](const auto& lhs, const auto& rhs) noexcept {
                         return lhs.est < rhs.est;
                       });
             return std::move(itrs);
           }(std::move(itrs))},
      match_count_{min_match_count} {}

  size_t match_count() const noexcept { return match_count_; }

 private:
  size_t match_count_;
};

struct Dummy {};

class NGramPosition : public position {
 public:
  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return type == irs::type<offset>::id() ? &offset_ : nullptr;
  }

  bool next() final {
    if (begin_ == std::end(offsets_)) {
      return false;
    }

    offset_ = *begin_;
    ++begin_;
    return true;
  }

  void reset() final {
    begin_ = std::begin(offsets_);
    value_ = irs::pos_limits::invalid();
  }

  void ClearOffsets() noexcept {
    offsets_.clear();
    begin_ = std::end(offsets_);
  }

  void PushOffset(const SearchState& state) {
    offsets_.emplace_back(OffsetFromState(state));
  }

 private:
  static offset OffsetFromState(const SearchState& state) noexcept {
    auto* cur = &state;
    for (auto* next = state.parent.get(); next;) {
      cur = next;
      next = next->parent.get();
    }

    IRS_ASSERT(cur->offs <= state.offs);
    return {{}, cur->offs, state.offs};
  }

  using Offsets = SmallVector<offset, 16>;

  offset offset_;
  Offsets offsets_;
  Offsets::const_iterator begin_{std::begin(offsets_)};
};

template<typename Base>
class SerialPositionsChecker : public Base {
 public:
  static constexpr bool kHasPosition = std::is_same_v<NGramPosition, Base>;

  template<typename Iterator>
  SerialPositionsChecker(Iterator begin, Iterator end, size_t total_terms_count,
                         size_t min_match_count = 1,
                         bool collect_all_states = false)
    : pos_(begin, end),
      min_match_count_{min_match_count},
      // avoid runtime conversion
      total_terms_count_{static_cast<score_t>(total_terms_count)},
      collect_all_states_{collect_all_states} {}

  bool Check(size_t potential, irs::doc_id_t doc);

  attribute* GetMutable(irs::type_info::type_id type) noexcept {
    if (type == irs::type<frequency>::id()) {
      return &seq_freq_;
    }

    if (type == irs::type<filter_boost>::id()) {
      return &filter_boost_;
    }

    if constexpr (kHasPosition) {
      if (type == irs::type<irs::position>::id()) {
        return static_cast<Base*>(this);
      }
    }

    return nullptr;
  }

 private:
  friend class NGramPosition;

  using SearchStates =
    std::map<uint32_t, std::shared_ptr<SearchState>, std::greater<>>;
  using PosTemp =
    std::vector<std::pair<uint32_t, std::shared_ptr<SearchState>>>;

  using PositionType =
    std::conditional_t<kHasPosition, PositionWithOffset, Position>;

  std::vector<PositionType> pos_;
  std::set<size_t> used_pos_;  // longest sequence positions overlaping detector
  std::vector<const score*> longest_sequence_;
  std::vector<size_t> pos_sequence_;
  size_t min_match_count_;
  SearchStates search_buf_;
  score_t total_terms_count_;
  filter_boost filter_boost_;
  frequency seq_freq_;
  bool collect_all_states_;
};

template<typename Base>
bool SerialPositionsChecker<Base>::Check(size_t potential, doc_id_t doc) {
  // how long max sequence could be in the best case
  search_buf_.clear();
  uint32_t longest_sequence_len = 0;

  seq_freq_.value = 0;
  for (const auto& pos_iterator : pos_) {
    if (pos_iterator.doc->value == doc) {
      position& pos = *(pos_iterator.pos);
      if (potential <= longest_sequence_len || potential < min_match_count_) {
        // this term could not start largest (or long enough) sequence.
        // skip it to first position to append to any existing candidates
        IRS_ASSERT(!search_buf_.empty());
        pos.seek(std::rbegin(search_buf_)->first + 1);
      } else {
        pos.next();
      }
      if (!pos_limits::eof(pos.value())) {
        PosTemp swap_cache;
        auto last_found_pos = pos_limits::invalid();
        do {
          const auto current_pos = pos.value();
          if (auto found = search_buf_.lower_bound(current_pos);
              found != std::end(search_buf_)) {
            if (last_found_pos != found->first) {
              last_found_pos = found->first;
              const auto* found_state = found->second.get();
              IRS_ASSERT(found_state);
              auto current_sequence = found;
              // if we hit same position - set length to 0 to force checking
              // candidates to the left
              uint32_t current_found_len{(found->first == current_pos ||
                                          found_state->scr == pos_iterator.scr)
                                           ? 0
                                           : found_state->len + 1};
              auto initial_found = found;
              if (current_found_len > longest_sequence_len) {
                longest_sequence_len = current_found_len;
              } else {
                // maybe some previous candidates could produce better
                // results. lets go leftward and check if there are any
                // candidates which could became longer if we stick this ngram
                // to them rather than the closest one found
                for (++found; found != std::end(search_buf_); ++found) {
                  found_state = found->second.get();
                  IRS_ASSERT(found_state);
                  if (found_state->scr != pos_iterator.scr &&
                      found_state->len + 1 > current_found_len) {
                    // we have better option. Replace this match!
                    current_sequence = found;
                    current_found_len = found_state->len + 1;
                    if (current_found_len > longest_sequence_len) {
                      longest_sequence_len = current_found_len;
                      break;  // this match is the best - nothing to search
                              // further
                    }
                  }
                }
              }
              if (current_found_len) {
                auto new_candidate = std::make_shared<SearchState>(
                  current_sequence->second, current_pos, pos_iterator);
                const auto res = search_buf_.try_emplace(
                  current_pos, std::move(new_candidate));
                if (!res.second) {
                  // pos already used. This could be if same ngram used several
                  // times. Replace with new length through swap cache - to not
                  // spoil candidate for following positions of same ngram
                  swap_cache.emplace_back(current_pos,
                                          // cppcheck-suppress accessMoved
                                          std::move(new_candidate));
                }
              } else if (initial_found->second->scr == pos_iterator.scr &&
                         potential > longest_sequence_len &&
                         potential >= min_match_count_) {
                // we just hit same iterator and found no better place to
                // join, so it will produce new candidate
                search_buf_.emplace(
                  std::piecewise_construct, std::forward_as_tuple(current_pos),
                  std::forward_as_tuple(
                    std::make_shared<SearchState>(current_pos, pos_iterator)));
              }
            }
          } else if (potential > longest_sequence_len &&
                     potential >= min_match_count_) {
            // this ngram at this position  could potentially start a long
            // enough sequence so add it to candidate list
            search_buf_.emplace(
              std::piecewise_construct, std::forward_as_tuple(current_pos),
              std::forward_as_tuple(
                std::make_shared<SearchState>(current_pos, pos_iterator)));
            if (!longest_sequence_len) {
              longest_sequence_len = 1;
            }
          }
        } while (pos.next());
        for (auto& p : swap_cache) {
          auto res = search_buf_.find(p.first);
          IRS_ASSERT(res != std::end(search_buf_));
          std::swap(res->second, p.second);
        }
      }
      --potential;  // we are done with this term.
                    // next will have potential one less as less matches left

      if (!potential) {
        break;  // all further terms will not add anything
      }

      if (longest_sequence_len + potential < min_match_count_) {
        break;  // all further terms will not let us build long enough
                // sequence
      }

      // if we have no scoring - we could stop searh once we got enough
      // matches
      if (longest_sequence_len >= min_match_count_ && !collect_all_states_) {
        break;
      }
    }
  }

  if (longest_sequence_len >= min_match_count_ && collect_all_states_) {
    if constexpr (kHasPosition) {
      static_cast<NGramPosition&>(*this).ClearOffsets();
    }

    uint32_t freq{0};
    size_t count_longest{0};
    [[maybe_unused]] SearchState* last_state{};

    // try to optimize case with one longest candidate
    // performance profiling shows it is majority of cases
    for ([[maybe_unused]] auto& [_, state] : search_buf_) {
      if (state->len == longest_sequence_len) {
        ++count_longest;
        if constexpr (kHasPosition) {
          last_state = state.get();
        }
        if (count_longest > 1) {
          break;
        }
      }
    }

    if (count_longest > 1) {
      longest_sequence_.clear();
      used_pos_.clear();
      longest_sequence_.reserve(longest_sequence_len);
      pos_sequence_.reserve(longest_sequence_len);
      for (auto i = std::begin(search_buf_); i != std::end(search_buf_);) {
        pos_sequence_.clear();
        const auto* state = i->second.get();
        IRS_ASSERT(state && state->len <= longest_sequence_len);
        if (state->len == longest_sequence_len) {
          bool delete_candidate = false;
          // only first longest sequence will contribute to frequency
          if (longest_sequence_.empty()) {
            longest_sequence_.push_back(state->scr);
            pos_sequence_.push_back(state->pos);
            auto cur_parent = state->parent;
            while (cur_parent) {
              longest_sequence_.push_back(cur_parent->scr);
              pos_sequence_.push_back(cur_parent->pos);
              cur_parent = cur_parent->parent;
            }
          } else {
            if (used_pos_.find(state->pos) != std::end(used_pos_) ||
                state->scr != longest_sequence_[0]) {
              delete_candidate = true;
            } else {
              pos_sequence_.push_back(state->pos);
              auto cur_parent = state->parent;
              size_t j = 1;
              while (cur_parent) {
                IRS_ASSERT(j < longest_sequence_.size());
                if (longest_sequence_[j] != cur_parent->scr ||
                    used_pos_.find(cur_parent->pos) != std::end(used_pos_)) {
                  delete_candidate = true;
                  break;
                }
                pos_sequence_.push_back(cur_parent->pos);
                cur_parent = cur_parent->parent;
                ++j;
              }
            }
          }
          if (!delete_candidate) {
            ++freq;
            used_pos_.insert(std::begin(pos_sequence_),
                             std::end(pos_sequence_));

            if constexpr (kHasPosition) {
              static_cast<NGramPosition&>(*this).PushOffset(*state);
            }
          }
        }
        ++i;
      }
    } else {
      freq = 1;
      if constexpr (kHasPosition) {
        IRS_ASSERT(last_state);
        static_cast<NGramPosition&>(*this).PushOffset(*last_state);
      }
    }
    seq_freq_.value = freq;
    IRS_ASSERT(!pos_.empty());
    filter_boost_.value =
      static_cast<score_t>(longest_sequence_len) / total_terms_count_;

    if constexpr (kHasPosition) {
      static_cast<NGramPosition&>(*this).reset();
    }
  }
  return longest_sequence_len >= min_match_count_;
}

template<typename Approx, typename Checker>
class NGramSimilarityDocIterator : public doc_iterator, private score_ctx {
 public:
  NGramSimilarityDocIterator(CostAdapters&& itrs, size_t total_terms_count,
                             size_t min_match_count, bool collect_all_states)
    : checker_{std::begin(itrs), std::end(itrs), total_terms_count,
               min_match_count, collect_all_states},
      // we are not interested in disjunction`s // scoring
      approx_{std::move(itrs), min_match_count} {
    // avoid runtime conversion
    std::get<attribute_ptr<document>>(attrs_) =
      irs::get_mutable<document>(&approx_);

    // FIXME find a better estimation
    std::get<attribute_ptr<cost>>(attrs_) = irs::get_mutable<cost>(&approx_);
  }

  NGramSimilarityDocIterator(CostAdapters&& itrs, const SubReader& segment,
                             const term_reader& field, score_t boost,
                             const byte_type* stats, size_t total_terms_count,
                             size_t min_match_count = 1,
                             const Scorers& ord = Scorers::kUnordered)
    : NGramSimilarityDocIterator{std::move(itrs), total_terms_count,
                                 min_match_count, !ord.empty()} {
    if (!ord.empty()) {
      auto& score = std::get<irs::score>(attrs_);
      CompileScore(score, ord.buckets(), segment, field, stats, *this, boost);
    }
  }

  attribute* get_mutable(type_info::type_id type) noexcept final {
    auto* attr = irs::get_mutable(attrs_, type);

    return attr != nullptr ? attr : checker_.GetMutable(type);
  }

  bool next() final {
    while (approx_.next()) {
      if (checker_.Check(approx_.match_count(), value())) {
        return true;
      }
    }
    return false;
  }

  doc_id_t value() const final {
    return std::get<attribute_ptr<document>>(attrs_).ptr->value;
  }

  doc_id_t seek(doc_id_t target) final {
    auto* doc = std::get<attribute_ptr<document>>(attrs_).ptr;

    if (doc->value >= target) {
      return doc->value;
    }
    const auto doc_id = approx_.seek(target);

    if (doc_limits::eof(doc_id) ||
        checker_.Check(approx_.match_count(), doc->value)) {
      return doc_id;
    }

    next();
    return doc->value;
  }

 private:
  using Attributes =
    std::tuple<attribute_ptr<document>, attribute_ptr<cost>, score>;

  Checker checker_;
  Approx approx_;
  Attributes attrs_;
};

CostAdapters Execute(const NGramState& query_state,
                     IndexFeatures required_features,
                     IndexFeatures extra_features) {
  const auto* field = query_state.field;

  if (field == nullptr ||
      required_features != (field->meta().index_features & required_features)) {
    return {};
  }

  required_features |= extra_features;

  CostAdapters itrs;
  itrs.reserve(query_state.terms.size());

  for (const auto& term_state : query_state.terms) {
    if (IRS_UNLIKELY(term_state == nullptr)) {
      continue;
    }

    if (auto docs = field->postings(*term_state, required_features); docs) {
      auto& it = itrs.emplace_back(std::move(docs));

      if (IRS_UNLIKELY(!it)) {
        itrs.pop_back();
      }
    }
  }

  return itrs;
}

}  // namespace

doc_iterator::ptr NGramSimilarityQuery::execute(
  const ExecutionContext& ctx) const {
  const auto& ord = ctx.scorers;
  IRS_ASSERT(1 != min_match_count_ || !ord.empty());

  const auto& segment = ctx.segment;
  const auto* query_state = states_.find(segment);

  if (query_state == nullptr) {
    return doc_iterator::empty();
  }

  auto itrs = Execute(*query_state, kRequiredFeatures, ord.features());

  if (itrs.size() < min_match_count_) {
    return doc_iterator::empty();
  }
  // TODO(MBkkt) itrs.size() == 1: return itrs_[0], but needs to add score
  // optimization for single ngram case
  if (itrs.size() == min_match_count_) {
    return memory::make_managed<NGramSimilarityDocIterator<
      NGramApprox<true>, SerialPositionsChecker<Dummy>>>(
      std::move(itrs), segment, *query_state->field, boost_, stats_.c_str(),
      query_state->terms.size(), min_match_count_, ord);
  }
  // TODO(MBkkt) min_match_count_ == 1: disjunction for approx,
  // optimization for low threshold case
  return memory::make_managed<NGramSimilarityDocIterator<
    NGramApprox<false>, SerialPositionsChecker<Dummy>>>(
    std::move(itrs), segment, *query_state->field, boost_, stats_.c_str(),
    query_state->terms.size(), min_match_count_, ord);
}

doc_iterator::ptr NGramSimilarityQuery::ExecuteWithOffsets(
  const SubReader& rdr) const {
  const auto* query_state = states_.find(rdr);

  if (query_state == nullptr) {
    return doc_iterator::empty();
  }

  auto itrs = Execute(*query_state, kRequiredFeatures | IndexFeatures::OFFS,
                      IndexFeatures::NONE);

  if (itrs.size() < min_match_count_) {
    return doc_iterator::empty();
  }
  // TODO(MBkkt) itrs.size() == 1: return itrs_[0], but needs to add score
  // optimization for single ngram case
  if (itrs.size() == min_match_count_) {
    return memory::make_managed<NGramSimilarityDocIterator<
      NGramApprox<true>, SerialPositionsChecker<NGramPosition>>>(
      std::move(itrs), query_state->terms.size(), min_match_count_, true);
  }
  // TODO(MBkkt) min_match_count_ == 1: disjunction for approx,
  // optimization for low threshold case
  return memory::make_managed<NGramSimilarityDocIterator<
    NGramApprox<false>, SerialPositionsChecker<NGramPosition>>>(
    std::move(itrs), query_state->terms.size(), min_match_count_, true);
}

}  // namespace irs
