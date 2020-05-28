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

#ifndef IRESEARCH_DISJUNCTION_H
#define IRESEARCH_DISJUNCTION_H

#include <queue>

#include "conjunction.hpp"
#include "utils/std.hpp"
#include "utils/type_limits.hpp"
#include "index/iterators.hpp"

NS_ROOT
NS_BEGIN(detail)

// Need this proxy since Microsoft has heap validity check in std::pop_heap.
// Our approach is to refresh top iterator (next or seek) and then remove it
// or move to lead. So we don't need this check.
// It is quite difficult to disable check since it managed by _ITERATOR_DEBUG_LEVEL
// macros which affects ABI (it must be the same for all libs and objs).
template<typename Iterator, typename Pred>
FORCE_INLINE void pop_heap(Iterator first, Iterator last, Pred comp) {
  assert(first != last); // pop requires non-empty range

  #ifndef _MSC_VER
    std::pop_heap(first, last, comp);
  #elif _MSC_FULL_VER < 190024000 // < MSVC2015.3
    std::_Pop_heap(std::_Unchecked(first), std::_Unchecked(last), comp);
  #elif _MSC_FULL_VER < 191526726 // < MSVC2017.8
    std::_Pop_heap_unchecked(std::_Unchecked(first), std::_Unchecked(last), comp);
  #else
    std::_Pop_heap_unchecked(first._Unwrapped(), last._Unwrapped(), comp);
  #endif
}

template<typename DocIterator>
FORCE_INLINE void evaluate_score_iter(const irs::byte_type**& pVal, DocIterator& src) {
  const auto* score = src.score;
  assert(score); // must be ensure by the adapter
  if (!score->empty()) {
    score->evaluate();
    *pVal++ = score->c_str();
  }
};

NS_END // detail

template<typename Adapter>
struct compound_doc_iterator : doc_iterator {
  virtual void visit(void* ctx, bool (*visitor)(void*, Adapter&)) = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @class unary_disjunction
////////////////////////////////////////////////////////////////////////////////
template<typename DocIterator, typename Adapter = score_iterator_adapter<DocIterator>>
class unary_disjunction final : public compound_doc_iterator<Adapter> {
 public:
  typedef Adapter doc_iterator_t;

  unary_disjunction(doc_iterator_t&& it)
    : it_(std::move(it)) {
  }

  virtual attribute* get_mutable(type_info::type_id type) noexcept override {
    return it_->get_mutable(type);
  }

  virtual doc_id_t value() const noexcept override {
    return it_.doc->value;
  }

  virtual bool next() override {
    return it_->next();
  }

  virtual doc_id_t seek(doc_id_t target) override {
    return it_->seek(target);
  }

  virtual void visit(void* ctx, bool (*visitor)(void*, Adapter&)) override {
    assert(ctx);
    assert(visitor);
    visitor(ctx, it_);
  }

 private:
  doc_iterator_t it_;
}; // unary_disjunction

////////////////////////////////////////////////////////////////////////////////
/// @class basic_disjunction
/// @brief use for special adapters only
////////////////////////////////////////////////////////////////////////////////
template<typename DocIterator,
         typename Adapter = score_iterator_adapter<DocIterator>>
class basic_disjunction final
    : public frozen_attributes<3, compound_doc_iterator<Adapter>>,
      private score_ctx {
 public:
  typedef Adapter doc_iterator_t;

  basic_disjunction(
      doc_iterator_t&& lhs,
      doc_iterator_t&& rhs,
      const order::prepared& ord = order::prepared::unordered(),
      sort::MergeType merge_type = sort::MergeType::AGGREGATE)
    : basic_disjunction(std::move(lhs), std::move(rhs),
                        ord, merge_type, resolve_overload_tag()) {
    cost_.rule([this](){
      return cost::extract(lhs_, 0) + cost::extract(rhs_, 0);
    });
  }

  basic_disjunction(
      doc_iterator_t&& lhs,
      doc_iterator_t&& rhs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      cost::cost_t est)
    : basic_disjunction(std::move(lhs), std::move(rhs),
                        ord, merge_type, resolve_overload_tag()) {
    cost_.value(est);
  }

  virtual doc_id_t value() const noexcept override {
    return doc_.value;
  }

  virtual bool next() override {
    next_iterator_impl(lhs_);
    next_iterator_impl(rhs_);
    return !doc_limits::eof(doc_.value = std::min(lhs_.value(), rhs_.value()));
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (target <= doc_.value) {
      return doc_.value;
    }

    if (seek_iterator_impl(lhs_, target) || seek_iterator_impl(rhs_, target)) {
      return doc_.value = target;
    }

    return (doc_.value = std::min(lhs_.value(), rhs_.value()));
  }

  virtual void visit(void* ctx, bool (*visitor)(void*, Adapter&)) override {
    assert(ctx);
    assert(visitor);
    assert(lhs_.doc->value >= doc_.value); // assume that seek or next has been called
    if (lhs_.value() == doc_.value && !visitor(ctx, lhs_)) {
      return;
    }
    seek_iterator_impl(rhs_, doc_.value);
    if (rhs_.value() == doc_.value) {
      visitor(ctx, rhs_);
    }
  }

 private:
  struct resolve_overload_tag { };

  basic_disjunction(
      doc_iterator_t&& lhs,
      doc_iterator_t&& rhs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      resolve_overload_tag)
    : frozen_attributes<3, compound_doc_iterator<Adapter>>{{
        { type<document>::id(), &doc_   },
        { type<cost>::id(),     &cost_  },
        { type<score>::id(),    &score_ },
      }},
      lhs_(std::move(lhs)),
      rhs_(std::move(rhs)),
      doc_(doc_limits::invalid()),
      merger_(ord.prepare_merger(merge_type)) {
    prepare_score(ord);
  }

  void prepare_score(const order::prepared& ord) {
    if (ord.empty()) {
      return;
    }

    assert(lhs_.score && rhs_.score); // must be ensure by the adapter

    const bool lhs_score_empty = lhs_.score->empty();
    const bool rhs_score_empty = rhs_.score->empty();

    if (!lhs_score_empty && !rhs_score_empty) {
      // both sub-iterators has score
      scores_vals_[0] = lhs_.score->c_str();
      scores_vals_[1] = rhs_.score->c_str();
      score_.prepare(ord, this, [](const score_ctx* ctx, byte_type* score) {
        auto& self = *static_cast<const basic_disjunction*>(ctx);

        const irs::byte_type** pVal = self.scores_vals_;
        size_t matched_iterators = (size_t)self.score_iterator_impl(self.lhs_);
        pVal += !matched_iterators;
        matched_iterators += (size_t)self.score_iterator_impl(self.rhs_);
        // always call merge. even if zero matched - we need to reset last accumulated score at least.
        self.merger_(score, pVal, matched_iterators);
      });
    } else if (!lhs_score_empty) {
      // only left sub-iterator has score
      scores_vals_[0] = lhs_.score->c_str();
      score_.prepare(ord, this, [](const score_ctx* ctx, byte_type* score) {
        auto& self = *static_cast<const basic_disjunction*>(ctx);
        self.merger_(score, self.scores_vals_, (size_t)self.score_iterator_impl(self.lhs_));
      });
    } else if (!rhs_score_empty) {
      // only right sub-iterator has score
      scores_vals_[0] = rhs_.score->c_str();
      score_.prepare(ord, this, [](const score_ctx* ctx, byte_type* score) {
        auto& self = *static_cast<const basic_disjunction*>(ctx);
        self.merger_(score, self.scores_vals_, (size_t)self.score_iterator_impl(self.rhs_));
      });
    } else {
      score_.prepare(ord, nullptr, [](const score_ctx*, byte_type*) {/*NOOP*/});
    }
  }

  bool seek_iterator_impl(doc_iterator_t& it, doc_id_t target) {
    return it.value() < target && target == it->seek(target);
  }

  void next_iterator_impl(doc_iterator_t& it) {
    const auto doc = it.value();

    if (doc_.value == doc) {
      it->next();
    } else if (doc < doc_.value) {
      it->seek(doc_.value + doc_id_t(!doc_limits::eof(doc_.value)));
    }
  }

  bool score_iterator_impl(doc_iterator_t& it) const {
    auto doc = it.value();

    if (doc < doc_.value) {
      doc = it->seek(doc_.value);
    }

    if (doc == doc_.value) {
      const auto* rhs = it.score;
      rhs->evaluate();
      return true;
    }
    return false;
  }

  mutable doc_iterator_t lhs_;
  mutable doc_iterator_t rhs_;
  mutable const irs::byte_type* scores_vals_[2];
  document doc_;
  score score_;
  cost cost_;
  order::prepared::merger merger_;
}; // basic_disjunction

////////////////////////////////////////////////////////////////////////////////
/// @class small_disjunction
/// @brief linear search based disjunction
////////////////////////////////////////////////////////////////////////////////
template<typename DocIterator, typename Adapter = score_iterator_adapter<DocIterator>>
class small_disjunction final
    : public frozen_attributes<3, compound_doc_iterator<Adapter>>,
      private score_ctx {
 public:
  typedef Adapter doc_iterator_t;
  typedef std::vector<doc_iterator_t> doc_iterators_t;

  small_disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      cost::cost_t est)
    : small_disjunction(std::move(itrs), ord, merge_type, resolve_overload_tag()) {
    cost_.value(est);
  }

  explicit small_disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord = order::prepared::unordered(),
      sort::MergeType merge_type = sort::MergeType::AGGREGATE)
    : small_disjunction(std::move(itrs), ord, merge_type, resolve_overload_tag()) {
    cost_.rule([this](){
      return std::accumulate(
        itrs_.begin(), itrs_.end(), cost::cost_t(0),
        [](cost::cost_t lhs, const doc_iterator_t& rhs) {
          return lhs + cost::extract(rhs, 0);
      });
    });
  }

  virtual doc_id_t value() const noexcept override {
    return doc_.value;
  }

  bool next_iterator_impl(doc_iterator_t& it) {
    const auto doc = it.value();

    if (doc == doc_.value) {
      return it->next();
    } else if (doc < doc_.value) {
      return !doc_limits::eof(it->seek(doc_.value+1));
    }

    return true;
  }

  virtual bool next() override {
    if (doc_limits::eof(doc_.value)) {
      return false;
    }

    doc_id_t min = doc_limits::eof();

    for (auto begin = itrs_.begin(); begin != itrs_.end(); ) {
      auto& it = *begin;
      if (!next_iterator_impl(it)) {
        if (!remove_iterator(it)) {
          doc_.value = doc_limits::eof();
          return false;
        }
#if defined(_MSC_VER) && defined(IRESEARCH_DEBUG)
        // workaround for Microsoft checked iterators
        begin = itrs_.begin() + std::distance(itrs_.data(), &it);
#endif
      } else {
        min = std::min(min, it.value());
        ++begin;
      }
    }

    doc_.value = min;
    return true;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (doc_limits::eof(doc_.value)) {
      return doc_.value;
    }

    doc_id_t min = doc_limits::eof();

    for (auto begin = itrs_.begin(); begin != itrs_.end(); ) {
      auto& it = *begin;

      if (it.value() < target) {
        const auto doc = it->seek(target);

        if (doc == target) {
          return doc_.value = doc;
        } else if (doc_limits::eof(doc)) {
          if (!remove_iterator(it)) {
            // exhausted
            return doc_.value = doc_limits::eof();
          }
#if defined(_MSC_VER) && defined(IRESEARCH_DEBUG)
          // workaround for Microsoft checked iterators
          begin = itrs_.begin() + std::distance(itrs_.data(), &it);
#endif
          continue; // don't need to increment 'begin' here
        }
      }

      min = std::min(min, it.value());
      ++begin;
    }

    return (doc_.value = min);
  }

  virtual void visit(void* ctx, bool (*visitor)(void*, Adapter&)) override {
    assert(ctx);
    assert(visitor);
    hitch_all_iterators();
    for (auto& it : itrs_) {
      if (it->value() == doc_.value && !visitor(ctx, it)) {
        return;
      }
    }
  }

 private:
  struct resolve_overload_tag{};

  small_disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      resolve_overload_tag)
    : frozen_attributes<3, compound_doc_iterator<Adapter>>{{
        { type<document>::id(), &doc_   },
        { type<cost>::id(),     &cost_  },
        { type<score>::id(),    &score_ },
      }},
      itrs_(std::move(itrs)),
      doc_(itrs_.empty()
        ? doc_limits::eof()
        : doc_limits::invalid()),
      merger_(ord.prepare_merger(merge_type)) {
    prepare_score(ord);
  }

  void prepare_score(const order::prepared& ord) {
    if (ord.empty()) {
      return;
    }

    // copy iterators with scores into separate container
    // to avoid extra checks
    scored_itrs_.reserve(itrs_.size());
    for (auto& it : itrs_) {
      if (!it.score->empty()) {
        scored_itrs_.emplace_back(it);
      }
    }

    // prepare score
    if (scored_itrs_.empty()) {
      score_.prepare(ord, nullptr, [](const irs::score_ctx*, byte_type*){ /*NOOP*/ });
    } else {
      scores_vals_.resize(scored_itrs_.size());
      score_.prepare(ord, this, [](const irs::score_ctx* ctx, byte_type* score) {
        auto& self = *static_cast<const small_disjunction*>(ctx);
        const irs::byte_type** pVal = self.scores_vals_.data();
        for (auto& it : self.scored_itrs_) {
          auto doc = it.value();

          if (doc < self.doc_.value) {
            doc = it->seek(self.doc_.value);
          }

          if (doc == self.doc_.value) {
            it.score->evaluate();
            *pVal++ = it.score->c_str();
          }
        }
        self.merger_(score, self.scores_vals_.data(),
                     std::distance(self.scores_vals_.data(), pVal));
      });
    }
  }

  bool remove_iterator(doc_iterator_t& it) {
    std::swap(it, itrs_.back());
    itrs_.pop_back();
    return !itrs_.empty();
  }

  void hitch_all_iterators() {
    if (last_hitched_doc_ == doc_.value) {
      return; // nothing to do
    }
    for (auto rbegin = itrs_.rbegin(); rbegin != itrs_.rend();) {
      auto& it = *rbegin;
      ++rbegin;
      if (it.value() < doc_.value && doc_limits::eof(it->seek(doc_.value))) {
        #ifdef IRESEARCH_DEBUG
          assert(remove_iterator(it));
        #else
          remove_iterator(it);
        #endif
      }
    }
    last_hitched_doc_ = doc_.value;
  }

  doc_id_t last_hitched_doc_{ doc_limits::invalid() };
  doc_iterators_t itrs_;
  doc_iterators_t scored_itrs_; // iterators with scores
  document doc_;
  score score_;
  cost cost_;
  mutable std::vector<const irs::byte_type*> scores_vals_;
  order::prepared::merger merger_;
}; // small_disjunction

////////////////////////////////////////////////////////////////////////////////
/// @class disjunction
/// @brief heap sort based disjunction
/// ----------------------------------------------------------------------------
///   [0]   <-- begin
///   [1]      |
///   [2]      | head (min doc_id heap)
///   ...      |
///   [n-1] <-- end
///   [n]   <-- lead (accepted iterator)
/// ----------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
template<typename DocIterator, typename Adapter = score_iterator_adapter<DocIterator>, bool EnableUnary = false>
class disjunction final
    : public frozen_attributes<3, compound_doc_iterator<Adapter>>,
      private score_ctx {
 public:
  typedef small_disjunction<DocIterator, Adapter> small_disjunction_t;
  typedef basic_disjunction<DocIterator, Adapter> basic_disjunction_t;
  typedef unary_disjunction<DocIterator, Adapter> unary_disjunction_t;
  typedef Adapter doc_iterator_t;
  typedef std::vector<doc_iterator_t> doc_iterators_t;

  typedef std::vector<size_t> heap_container;
  typedef heap_container::iterator heap_iterator;

  static constexpr bool ENABLE_UNARY = EnableUnary;

  disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      cost::cost_t est)
    : disjunction(std::move(itrs), ord, merge_type, resolve_overload_tag()) {
    cost_.value(est);
  }

  explicit disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord = order::prepared::unordered(),
      sort::MergeType merge_type = sort::MergeType::AGGREGATE)
    : disjunction(std::move(itrs), ord, merge_type, resolve_overload_tag()) {
    cost_.rule([this](){
      return std::accumulate(
        itrs_.begin(), itrs_.end(), cost::cost_t(0),
        [](cost::cost_t lhs, const doc_iterator_t& rhs) {
          return lhs + cost::extract(rhs, 0);
      });
    });
  }

  virtual doc_id_t value() const noexcept override {
    return doc_.value;
  }

  virtual bool next() override {
    if (doc_limits::eof(doc_.value)) {
      return false;
    }

    while (lead().value() <= doc_.value) {
      bool const exhausted = lead().value() == doc_.value
        ? !lead()->next()
        : doc_limits::eof(lead()->seek(doc_.value + 1));

      if (exhausted && !remove_lead()) {
        doc_.value = doc_limits::eof();
        return false;
      } else {
        refresh_lead();
      }
    }

    doc_.value = lead().value();

    return true;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (doc_limits::eof(doc_.value)) {
      return doc_.value;
    }

    while (lead().value() < target) {
      const auto doc = lead()->seek(target);

      if (doc_limits::eof(doc) && !remove_lead()) {
        return doc_.value = doc_limits::eof();
      } else if (doc != target) {
        refresh_lead();
      }
    }

    return doc_.value = lead().value();
  }

  virtual void visit(void* ctx, bool (*visitor)(void*, Adapter&)) override {
    assert(ctx);
    assert(visitor);
    hitch_all_iterators();
    auto& lead = itrs_[heap_.back()];
    auto cont = visitor(ctx, lead);
    if (cont && heap_.size() > 1) {
      auto value = lead.value();
      irstd::heap::for_each_if(
        heap_.cbegin(),
        heap_.cend()-1,
        [this, value, &cont](const size_t it) {
          assert(it < itrs_.size());
          return cont && itrs_[it].value() == value;
        },
        [this, ctx, visitor, &cont](const size_t it) {
          assert(it < itrs_.size());
          cont = visitor(ctx, itrs_[it]);
        });
    }
  }

 private:
  struct resolve_overload_tag{};

  disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      resolve_overload_tag)
    : frozen_attributes<3, compound_doc_iterator<Adapter>>{{
        { type<document>::id(), &doc_   },
        { type<cost>::id(),     &cost_  },
        { type<score>::id(),    &score_ },
      }},
      itrs_(std::move(itrs)),
      doc_(itrs_.empty()
        ? doc_limits::eof()
        : doc_limits::invalid()),
      merger_(ord.prepare_merger(merge_type)) {
    // since we are using heap in order to determine next document,
    // in order to avoid useless make_heap call we expect that all
    // iterators are equal here */
    //assert(irstd::all_equal(itrs_.begin(), itrs_.end()));

    // prepare external heap
    heap_.resize(itrs_.size());
    std::iota(heap_.begin(), heap_.end(), size_t(0));

    prepare_score(ord);
  }

  void prepare_score(const order::prepared& ord) {
    if (ord.empty()) {
      return;
    }

    scores_vals_.resize(itrs_.size(), nullptr);
    score_.prepare(ord, this, [](const score_ctx* ctx, byte_type* score) {
      auto& self = const_cast<disjunction&>(*static_cast<const disjunction*>(ctx));
      assert(!self.heap_.empty());

      const auto its = self.hitch_all_iterators();
      const irs::byte_type** pVal = self.scores_vals_.data();
      detail::evaluate_score_iter(pVal, self.lead());
      if (self.top().value() == self.doc_.value) {
        irstd::heap::for_each_if(
          its.first, its.second,
          [&self](const size_t it) {
            assert(it < self.itrs_.size());
            return self.itrs_[it].value() == self.doc_.value;
          },
          [&self, &pVal](size_t it) {
            assert(it < self.itrs_.size());
            detail::evaluate_score_iter(pVal, self.itrs_[it]);
        });
      }
      self.merger_(score, self.scores_vals_.data(),
                   std::distance(self.scores_vals_.data(), pVal));
    });
  }

  template<typename Iterator>
  inline void push(Iterator begin, Iterator end) {
    // lambda here gives ~20% speedup on GCC
    std::push_heap(begin, end, [this](const size_t lhs, const size_t rhs) noexcept {
      assert(lhs < itrs_.size());
      assert(rhs < itrs_.size());
      return itrs_[lhs].value() > itrs_[rhs].value();
    });
  }

  template<typename Iterator>
  inline void pop(Iterator begin, Iterator end) {
    // lambda here gives ~20% speedup on GCC
    detail::pop_heap(begin, end, [this](const size_t lhs, const size_t rhs) noexcept {
      assert(lhs < itrs_.size());
      assert(rhs < itrs_.size());
      return itrs_[lhs].value() > itrs_[rhs].value();
    });
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes lead iterator
  /// @returns true - if the disjunction condition still can be satisfied,
  ///          false - otherwise
  //////////////////////////////////////////////////////////////////////////////
  inline bool remove_lead() {
    heap_.pop_back();

    if (!heap_.empty()) {
      pop(heap_.begin(), heap_.end());
      return true;
    }

    return false;
  }

  inline void refresh_lead() {
    auto begin = heap_.begin(), end = heap_.end();
    push(begin, end);
    pop(begin, end);
  }

  inline doc_iterator_t& lead() noexcept {
    assert(!heap_.empty());
    assert(heap_.back() < itrs_.size());
    return itrs_[heap_.back()];
  }

  inline doc_iterator_t& top() noexcept {
    assert(!heap_.empty());
    assert(heap_.front() < itrs_.size());
    return itrs_[heap_.front()];
  }

  std::pair<heap_iterator, heap_iterator> hitch_all_iterators() {
    // hitch all iterators in head to the lead (current doc_)
    auto begin = heap_.begin(), end = heap_.end()-1;

    while (begin != end && top().value() < doc_.value) {
      const auto doc = top()->seek(doc_.value);

      if (doc_limits::eof(doc)) {
        // remove top
        pop(begin,end);
        std::swap(*--end, heap_.back());
        heap_.pop_back();
      } else {
        // refresh top
        pop(begin,end);
        push(begin,end);
      }
    }
    return {begin, end};
  }

  doc_iterators_t itrs_;
  heap_container heap_;
  mutable std::vector<const irs::byte_type*> scores_vals_;
  document doc_;
  score score_;
  cost cost_;
  order::prepared::merger merger_;
}; // disjunction

//////////////////////////////////////////////////////////////////////////////
/// @returns disjunction iterator created from the specified sub iterators
//////////////////////////////////////////////////////////////////////////////
template<typename Disjunction, typename... Args>
doc_iterator::ptr make_disjunction(
    typename Disjunction::doc_iterators_t&& itrs,
    Args&&... args) {
  const auto size = itrs.size();

  switch (size) {
    case 0:
      // empty or unreachable search criteria
      return doc_iterator::empty();
    case 1:
      if constexpr (Disjunction::ENABLE_UNARY) {
        typedef typename Disjunction::unary_disjunction_t unary_disjunction_t;
        return doc_iterator::make<unary_disjunction_t>(
          std::move(itrs.front())
        );
      }
      // single sub-query
      return std::move(itrs.front());
    case 2: {
      typedef typename Disjunction::basic_disjunction_t basic_disjunction_t;

      // simple disjunction
      auto first = itrs.begin();
      auto second = first;
      std::advance(second, 1);

      return doc_iterator::make<basic_disjunction_t>(
        std::move(*first),
        std::move(*second),
        std::forward<Args>(args)...);
    }
  }

  constexpr size_t LINEAR_MERGE_UPPER_BOUND = 5;
  if (size <= LINEAR_MERGE_UPPER_BOUND) {
    typedef typename Disjunction::small_disjunction_t small_disjunction_t;

    // small disjunction
    return doc_iterator::make<small_disjunction_t>(
      std::move(itrs),
      std::forward<Args>(args)...);
  }

  // disjunction
  return doc_iterator::make<Disjunction>(
    std::move(itrs),
    std::forward<Args>(args)...);
}

NS_END // ROOT

#endif // IRESEARCH_DISJUNCTION_H
