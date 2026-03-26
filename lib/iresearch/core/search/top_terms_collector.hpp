////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <unordered_map>
#include <vector>

#include "analysis/token_attributes.hpp"
#include "index/iterators.hpp"
#include "search/scorer.hpp"
#include "shared.hpp"
#include "utils/hash_utils.hpp"
#include "utils/noncopyable.hpp"

namespace irs {

template<typename T>
struct top_term {
  using key_type = T;

  template<typename U = key_type>
  top_term(const bytes_view& term, U&& key)
    : term(term.data(), term.size()), key(std::forward<U>(key)) {}

  template<typename CollectorState>
  void emplace(const CollectorState& /*state*/) {
    // NOOP
  }

  template<typename Visitor>
  void visit(const Visitor& /*visitor*/) const {
    // NOOP
  }

  bstring term;
  key_type key;
};

template<typename T>
struct top_term_comparer {
  bool operator()(const top_term<T>& lhs,
                  const top_term<T>& rhs) const noexcept {
    return operator()(lhs, rhs.key, rhs.term);
  }

  bool operator()(const top_term<T>& lhs, const T& rhs_key,
                  const bytes_view& rhs_term) const noexcept {
    return lhs.key < rhs_key || (!(rhs_key < lhs.key) && lhs.term < rhs_term);
  }
};

template<typename T>
struct top_term_state : top_term<T> {
  struct segment_state {
    segment_state(const SubReader& segment, const term_reader& field,
                  uint32_t docs_count) noexcept
      : segment(&segment), field(&field), docs_count(docs_count) {}

    const SubReader* segment;
    const term_reader* field;
    size_t terms_count{1};  // number of terms in a segment
    uint32_t docs_count;
  };

  template<typename U = T>
  top_term_state(const bytes_view& term, U&& key)
    : top_term<T>(term, std::forward<U>(key)) {}

  template<typename CollectorState>
  void emplace(const CollectorState& state) {
    IRS_ASSERT(state.segment && state.docs_count && state.field);

    const auto* segment = state.segment;
    const auto docs_count = *state.docs_count;

    if (segments.empty() || segments.back().segment != segment) {
      segments.emplace_back(*segment, *state.field, docs_count);
    } else {
      auto& segment = segments.back();
      ++segment.terms_count;
      segment.docs_count += docs_count;
    }
    terms.emplace_back(state.terms->cookie());
  }

  template<typename Visitor>
  void visit(const Visitor& visitor) {
    auto cookie = terms.begin();
    for (auto& segment : segments) {
      visitor(*segment.segment, *segment.field, segment.docs_count);
      for (size_t i = 0, size = segment.terms_count; i < size; ++i, ++cookie) {
        visitor(*cookie);
      }
    }
  }

  std::vector<segment_state> segments;
  std::vector<seek_cookie::ptr> terms;
};

template<typename State,
         typename Comparer = top_term_comparer<typename State::key_type>>
class top_terms_collector : private util::noncopyable {
 public:
  using state_type = State;
  static_assert(std::is_nothrow_move_assignable_v<state_type>);
  using key_type = typename state_type::key_type;
  using comparer_type = Comparer;

  // We disallow 0 size collectors for consistency since we're not
  // interested in this use case and don't want to burden "collect(...)"
  explicit top_terms_collector(size_t size, const Comparer& comp = {})
    : comparer_{comp}, size_{std::max(size_t(1), size)} {
    heap_.reserve(size);
    terms_.reserve(size);  // ensure all iterators remain valid
  }

  // Prepare scorer for terms collecting
  // `segment` segment reader for the current term
  // `state` state containing this scored term
  // `terms` segment term-iterator positioned at the current term
  void prepare(const SubReader& segment, const term_reader& field,
               const seek_term_iterator& terms) noexcept {
    state_.segment = &segment;
    state_.field = &field;
    state_.terms = &terms;

    if (auto* term = irs::get<term_attribute>(terms); IRS_LIKELY(term)) {
      state_.term = &term->value;
    } else {
      IRS_ASSERT(false);
      static const bytes_view kNoTerm;
      state_.term = &kNoTerm;
    }

    if (auto* meta = irs::get<term_meta>(terms); IRS_LIKELY(meta)) {
      state_.docs_count = &meta->docs_count;
    } else {
      static const doc_id_t kNoDocs{0};
      state_.docs_count = &kNoDocs;
    }
  }

  // Collect current term
  void visit(const key_type& key) {
    const auto term = *state_.term;

    if (left_) {
      hashed_bytes_view hashedTerm{term};
      const auto res = terms_.try_emplace(hashedTerm, hashedTerm, key);

      if (res.second) {
        auto& old_key = const_cast<hashed_bytes_view&>(res.first->first);
        auto& value = res.first->second;
        hashed_bytes_view new_key{value.term, old_key.hash()};
        IRS_ASSERT(old_key == new_key);
        old_key = new_key;
        heap_.emplace_back(res.first);

        if (!--left_) {
          make_heap();
        }
      }

      res.first->second.emplace(state_);

      return;
    }

    const auto min = heap_.front();
    const auto& min_state = min->second;

    if (!comparer_(min_state, key, term)) {
      // nothing to do
      return;
    }

    const auto hashed_term = hashed_bytes_view{term};
    const auto it = terms_.find(hashed_term);

    if (it == terms_.end()) {
      // update min entry
      pop();

      auto node = terms_.extract(min);
      IRS_ASSERT(!node.empty());
      node.mapped() = state_type{term, key};
      node.key() = hashed_bytes_view{node.mapped().term, hashed_term.hash()};
      auto res = terms_.insert(std::move(node));
      IRS_ASSERT(res.inserted);
      IRS_ASSERT(res.node.empty());

      heap_.back() = res.position;
      push();

      res.position->second.emplace(state_);
    } else {
      IRS_ASSERT(it->second.key == key);
      // update existing entry
      it->second.emplace(state_);
    }
  }

  // FIXME rename
  template<typename Visitor>
  void visit(const Visitor& visitor) noexcept {
    for (auto& entry : terms_) {
      visitor(entry.second);
    }
  }

 private:
  // We don't use absl hash table implementation as it doesn't guarantee
  // rehashing won't happen even if enough buckets are reserve()ed
  using states_map_t = std::unordered_map<hashed_bytes_view, state_type>;

  // Collector state
  struct collector_state {
    const SubReader* segment{};
    const term_reader* field{};
    const seek_term_iterator* terms{};
    const bytes_view* term{};
    const uint32_t* docs_count{};
  };

  void make_heap() noexcept {
    std::make_heap(heap_.begin(), heap_.end(),
                   [this](const auto lhs, const auto rhs) noexcept {
                     return comparer_(rhs->second, lhs->second);
                   });
  }

  void push() noexcept {
    std::push_heap(heap_.begin(), heap_.end(),
                   [this](const auto lhs, const auto rhs) noexcept {
                     return comparer_(rhs->second, lhs->second);
                   });
  }

  void pop() noexcept {
    std::pop_heap(heap_.begin(), heap_.end(),
                  [this](const auto lhs, const auto rhs) noexcept {
                    return comparer_(rhs->second, lhs->second);
                  });
  }

  IRS_NO_UNIQUE_ADDRESS comparer_type comparer_;
  collector_state state_;
  std::vector<typename states_map_t::iterator> heap_;
  states_map_t terms_;
  size_t size_;
  size_t left_{size_};
};

}  // namespace irs
