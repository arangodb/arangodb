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
#include "index/iterators.hpp"
#include "utils/std.hpp"
#include "utils/type_limits.hpp"

namespace iresearch {
namespace detail {

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
  if (!score->is_default()) {
    *pVal++ = score->evaluate();
  }
};

template<size_t Size>
class min_match_buffer {
 public:
  explicit min_match_buffer(size_t min_match_count) noexcept
    : min_match_count_(std::max(size_t(1), min_match_count)) {
  }

  uint32_t match_count(size_t i) const noexcept {
    assert(i < Size);
    return match_count_[i];
  }

  bool inc(size_t i) noexcept {
    return ++match_count_[i] < min_match_count_;
  }

  void clear() noexcept {
    std::memset(match_count_, 0, sizeof match_count_);
  }

  size_t min_match_count() const noexcept {
    return min_match_count_;
  }

 private:
  const size_t min_match_count_;
  uint32_t match_count_[Size];
}; // min_match_buffer

template<>
class min_match_buffer<0> {
 public:
  explicit min_match_buffer(size_t) noexcept {}
  bool inc(size_t) noexcept {
    assert(false);
    return true;
  }
  void clear() noexcept { assert(false); }
  uint32_t match_count(size_t) const noexcept {
    assert(false);
    return 1;
  }
  uint32_t min_match_count() const noexcept {
    return 1;
   }
}; // min_match_buffer

class score_buffer {
 public:
  score_buffer(const order::prepared& ord, size_t size)
    : bucket_size_(ord.score_size()),
      buf_size_(bucket_size_*size),
      buf_(ord.empty() ? nullptr : new byte_type[buf_size_]) {
    if (buf_) {
      std::memset(data(), 0, this->size());
    }
  }

  byte_type* get(size_t i) noexcept {
    assert(!buf_ || bucket_size_*i < buf_size_);
    return buf_.get() + bucket_size_*i;
  }

  byte_type* data() noexcept {
    return buf_.get();
  }

  size_t size() const noexcept {
    return buf_size_;
  }

  size_t bucket_size() const noexcept {
    return bucket_size_;
  }

 private:
  size_t bucket_size_;
  size_t buf_size_;
  std::unique_ptr<byte_type[]> buf_;
}; // score_buffer

struct empty_score_buffer {
  explicit empty_score_buffer(const order::prepared&, size_t) noexcept { }

  byte_type* get(size_t) noexcept {
    assert(false);
    return nullptr;
  }

  byte_type* data() noexcept {
    return nullptr;
  }

  size_t size() const noexcept {
    return 0;
  }

  size_t bucket_size() const noexcept {
    return 0;
  }
}; // empty_score_buffer

} // detail

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
  using doc_iterator_t = Adapter;

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
  using adapter = Adapter;

  basic_disjunction(
      adapter&& lhs,
      adapter&& rhs,
      const order::prepared& ord = order::prepared::unordered(),
      sort::MergeType merge_type = sort::MergeType::AGGREGATE)
    : basic_disjunction(
        std::move(lhs), std::move(rhs), ord, merge_type,
        [this](){ return cost::extract(lhs_, 0) + cost::extract(rhs_, 0); },
        resolve_overload_tag{}) {
  }

  basic_disjunction(
      adapter&& lhs,
      adapter&& rhs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      cost::cost_t est)
    : basic_disjunction(
        std::move(lhs), std::move(rhs),
        ord, merge_type, est,
        resolve_overload_tag{}) {
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
  struct resolve_overload_tag{};

  template<typename Estimation>
  basic_disjunction(
      adapter&& lhs,
      adapter&& rhs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      Estimation&& estimation,
      resolve_overload_tag)
    : frozen_attributes<3, compound_doc_iterator<Adapter>>{{
        { type<document>::id(), &doc_   },
        { type<cost>::id(),     &cost_  },
        { type<score>::id(),    &score_ },
      }},
      lhs_(std::move(lhs)),
      rhs_(std::move(rhs)),
      score_(ord),
      no_score_value_(ord.score_size(), 0),
      cost_(std::forward<Estimation>(estimation)),
      merger_(ord.prepare_merger(merge_type)) {
    prepare_score(ord);
  }

  void prepare_score(const order::prepared& ord) {
    if (ord.empty()) {
      return;
    }

    assert(lhs_.score && rhs_.score); // must be ensure by the adapter

    const bool lhs_score_empty = lhs_.score->is_default();
    const bool rhs_score_empty = rhs_.score->is_default();

    if (!lhs_score_empty && !rhs_score_empty) {
      // both sub-iterators have score
      score_.reset(this, [](score_ctx* ctx) -> const byte_type* {
        auto& self = *static_cast<basic_disjunction*>(ctx);

        const byte_type* score_values[2] {
          self.score_iterator_impl(self.lhs_),
          self.score_iterator_impl(self.rhs_) };

        auto* score_buf = self.score_.data();
        self.merger_(score_buf, score_values, 2);

        return score_buf;
      });
    } else if (!lhs_score_empty) {
      // only left sub-iterator has score
      score_.reset(this, [](score_ctx* ctx) -> const byte_type* {
        auto& self = *static_cast<basic_disjunction*>(ctx);
        return self.score_iterator_impl(self.lhs_);
      });
    } else if (!rhs_score_empty) {
      // only right sub-iterator has score
      score_.reset(this, [](score_ctx* ctx) -> const byte_type* {
        auto& self = *static_cast<basic_disjunction*>(ctx);
        return self.score_iterator_impl(self.rhs_);
      });
    } else {
      assert(score_.is_default());
    }
  }

  bool seek_iterator_impl(adapter& it, doc_id_t target) {
    return it.value() < target && target == it->seek(target);
  }

  void next_iterator_impl(adapter& it) {
    const auto doc = it.value();

    if (doc_.value == doc) {
      it->next();
    } else if (doc < doc_.value) {
      it->seek(doc_.value + doc_id_t(!doc_limits::eof(doc_.value)));
    }
  }

  const byte_type* score_iterator_impl(adapter& it) const {
    auto doc = it.value();

    if (doc < doc_.value) {
      doc = it->seek(doc_.value);
    }

    if (doc == doc_.value) {
      return it.score->evaluate();
    }

    return no_score_value_.c_str();
  }

  mutable adapter lhs_;
  mutable adapter rhs_;
  document doc_;
  score score_;
  bstring no_score_value_; // empty score value
  cost cost_;
  order::prepared::merger merger_;
}; // basic_disjunction

////////////////////////////////////////////////////////////////////////////////
/// @class small_disjunction
/// @brief linear search based disjunction
/// ----------------------------------------------------------------------------
///  Unscored iterators   Scored iterators
///   [0]   [1]   [2]   |   [3]    [4]     [5]
///    ^                |    ^                    ^
///    |                |    |                    |
///   begin             |   scored               end
///                     |   begin
/// ----------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
template<typename DocIterator, typename Adapter = score_iterator_adapter<DocIterator>>
class small_disjunction final
    : public frozen_attributes<3, compound_doc_iterator<Adapter>>,
      private score_ctx {
 public:
  using adapter = Adapter;
  using doc_iterators_t = std::vector<adapter>;

  small_disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      cost::cost_t est)
    : small_disjunction(std::move(itrs), ord, merge_type, est, resolve_overload_tag()) {
  }

  explicit small_disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord = order::prepared::unordered(),
      sort::MergeType merge_type = sort::MergeType::AGGREGATE)
    : small_disjunction(
        std::move(itrs), ord, merge_type,
        [this](){
          return std::accumulate(
            begin_, end_, cost::cost_t(0),
            [](cost::cost_t lhs, const adapter& rhs) {
              return lhs + cost::extract(rhs, 0);
          });
        },
        resolve_overload_tag()) {
  }

  virtual doc_id_t value() const noexcept override {
    return doc_.value;
  }

  bool next_iterator_impl(adapter& it) {
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

    for (auto begin = begin_; begin != end_; ) {
      auto& it = *begin;
      if (!next_iterator_impl(it)) {
        if (!remove_iterator(begin)) {
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

    for (auto begin = begin_; begin != end_; ) {
      auto& it = *begin;

      if (it.value() < target) {
        const auto doc = it->seek(target);

        if (doc == target) {
          return doc_.value = doc;
        } else if (doc_limits::eof(doc)) {
          if (!remove_iterator(begin)) {
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
    for (auto begin = begin_; begin != end_; ++begin) {
      auto& it = *begin;
      if (it->value() == doc_.value && !visitor(ctx, it)) {
        return;
      }
    }
  }

 private:
  struct resolve_overload_tag{};

  template<typename Estimation>
  small_disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      Estimation&& estimation,
      resolve_overload_tag)
    : frozen_attributes<3, compound_doc_iterator<Adapter>>{{
        { type<document>::id(), &doc_   },
        { type<cost>::id(),     &cost_  },
        { type<score>::id(),    &score_ },
      }},
      itrs_(itrs.size()),
      scored_begin_(itrs_.begin()),
      begin_(scored_begin_),
      end_(itrs_.end()),
      doc_(itrs_.empty()
        ? doc_limits::eof()
        : doc_limits::invalid()),
      score_(ord),
      cost_(std::forward<Estimation>(estimation)),
      merger_(ord.prepare_merger(merge_type)) {
    auto rbegin = itrs_.rbegin();
    for (auto& it : itrs) {
      if (it.score->is_default()) {
        *scored_begin_ = std::move(it);
        ++scored_begin_;
      } else {
        *rbegin = std::move(it);
        ++rbegin;
      }
    }

    prepare_score(ord);
  }

  void prepare_score(const order::prepared& ord) {
    if (ord.empty()) {
      return;
    }

    // prepare score
    if (scored_begin_ != end_) {
      scores_vals_.resize(size_t(std::distance(scored_begin_, end_)));

      score_.reset(this, [](irs::score_ctx* ctx) -> const byte_type* {
        auto& self = *static_cast<small_disjunction*>(ctx);
        auto* score_buf = self.score_.data();
        const irs::byte_type** pVal = self.scores_vals_.data();
        for (auto begin = self.scored_begin_, end = self.end_; begin != end; ++begin) {
          auto doc = begin->value();

          if (doc < self.doc_.value) {
            doc = (*begin)->seek(self.doc_.value);
          }

          if (doc == self.doc_.value) {
            *pVal++ = begin->score->evaluate();
          }
        }

        self.merger_(score_buf,
                     self.scores_vals_.data(),
                     std::distance(self.scores_vals_.data(), pVal));

        return score_buf;
      });
    } else {
      assert(score_.is_default());
    }
  }

  bool remove_iterator(typename doc_iterators_t::iterator it) {
    if (it->score->is_default()) {
      std::swap(*it, *begin_);
      ++begin_;
    } else {
      std::swap(*it, *(--end_));
    }

    return begin_ != end_;
  }

  void hitch_all_iterators() {
    if (last_hitched_doc_ == doc_.value) {
      return; // nothing to do
    }
    for (auto begin = begin_; begin != end_;++begin) {
      auto& it = *begin;
      if (it.value() < doc_.value && doc_limits::eof(it->seek(doc_.value))) {
        #ifdef IRESEARCH_DEBUG
          assert(remove_iterator(begin));
        #else
          remove_iterator(begin);
        #endif
      }
    }
    last_hitched_doc_ = doc_.value;
  }

  using iterator = typename doc_iterators_t::iterator;

  doc_id_t last_hitched_doc_{ doc_limits::invalid() };
  doc_iterators_t itrs_;
  iterator scored_begin_; // beginning of scored doc iterator range
  iterator begin_; // beginning of unscored doc iterators range
  iterator end_; // end of scored doc iterator range
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
  using unary_disjunction_t = unary_disjunction<DocIterator, Adapter>;
  using basic_disjunction_t = basic_disjunction<DocIterator, Adapter>;
  using small_disjunction_t = small_disjunction<DocIterator, Adapter>;

  using adapter = Adapter;
  using doc_iterators_t = std::vector<adapter>;
  using heap_container  = std::vector<size_t>;
  using heap_iterator   = heap_container::iterator;

  static constexpr bool enable_unary() noexcept { return EnableUnary; };
  static constexpr size_t small_disjunction_upper_bound() noexcept { return 5; }

  disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      cost::cost_t est)
    : disjunction(std::move(itrs), ord, merge_type, est, resolve_overload_tag()) {
  }

  explicit disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord = order::prepared::unordered(),
      sort::MergeType merge_type = sort::MergeType::AGGREGATE)
    : disjunction(
        std::move(itrs), ord, merge_type,
        [this](){
          return std::accumulate(
            itrs_.begin(), itrs_.end(), cost::cost_t(0),
            [](cost::cost_t lhs, const adapter& rhs) {
              return lhs + cost::extract(rhs, 0);
          });
        },
        resolve_overload_tag()) {
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

  template<typename Estimation>
  disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      Estimation&& estimation,
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
      score_(ord),
      cost_(std::forward<Estimation>(estimation)),
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
    score_.reset(this, [](score_ctx* ctx) -> const byte_type* {
      auto& self = *static_cast<disjunction*>(ctx);
      assert(!self.heap_.empty());
      auto* score_buf = self.score_.data();

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
      self.merger_(score_buf, self.scores_vals_.data(),
                   std::distance(self.scores_vals_.data(), pVal));

      return score_buf;
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

  inline adapter& lead() noexcept {
    assert(!heap_.empty());
    assert(heap_.back() < itrs_.size());
    return itrs_[heap_.back()];
  }

  inline adapter& top() noexcept {
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

enum class MatchType {
  MATCH,
  MIN_MATCH_FAST,
  MIN_MATCH
};

////////////////////////////////////////////////////////////////////////////////
/// @struct block_disjunction_traits
////////////////////////////////////////////////////////////////////////////////
template<bool Score, MatchType MinMatch, bool SeekReadahead, size_t NumBlocks = 32>
struct block_disjunction_traits {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief "false" - iterator is used for filtering only,
  ///        "true" - otherwise
  //////////////////////////////////////////////////////////////////////////////
  static constexpr bool score() noexcept { return Score; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief "false" - iterator is used for min match filtering,
  ///        "true" - otherwise
  //////////////////////////////////////////////////////////////////////////////
  static constexpr bool min_match() noexcept {
    return MatchType::MATCH != MinMatch;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief "false" - iterator is used for min match filtering,
  ///        "true" - otherwise
  //////////////////////////////////////////////////////////////////////////////
  static constexpr bool min_match_early_pruning() noexcept {
    return MatchType::MIN_MATCH_FAST == MinMatch;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief use readahead buffer for random access
  //////////////////////////////////////////////////////////////////////////////
  static constexpr bool seek_readahead() noexcept { return SeekReadahead; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief size of the readhead buffer in blocks
  //////////////////////////////////////////////////////////////////////////////
  static constexpr size_t num_blocks() noexcept { return NumBlocks; };
}; // block_disjunction_traits

////////////////////////////////////////////////////////////////////////////////
/// @class block_disjunction
/// @brief the implementation reads ahead 64*NumBlocks documents
/// @note the implementation isn't optimized for conjunction case
///       when the requected min match count equals to a number of input
///       iterators. It's better to to use a dedicated "conjunction" iterator.
////////////////////////////////////////////////////////////////////////////////
template<typename DocIterator,
         typename Traits,
         typename Adapter = score_iterator_adapter<DocIterator>>
class block_disjunction final
    : public frozen_attributes<3, doc_iterator>,
      private score_ctx {
 public:
  using traits_type = Traits;
  using adapter  = Adapter;
  using doc_iterators_t = std::vector<adapter>;

  using unary_disjunction_t = unary_disjunction<DocIterator, Adapter>;
  using basic_disjunction_t = basic_disjunction<DocIterator, Adapter>;
  using small_disjunction_t = block_disjunction;

  static constexpr bool enable_unary() { return false; } // FIXME

  // Block disjunction is faster than small_disjunction
  static constexpr size_t small_disjunction_upper_bound() noexcept { return 0; }

  block_disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord,
      sort::MergeType merge_type,
      cost::cost_t est)
    : block_disjunction(std::move(itrs), 1, ord, merge_type, est) {
  }

  block_disjunction(
      doc_iterators_t&& itrs,
      size_t min_match_count,
      const order::prepared& ord,
      sort::MergeType merge_type,
      cost::cost_t est)
    : block_disjunction(std::move(itrs), min_match_count, ord,
                        merge_type, est, resolve_overload_tag()) {
  }

  explicit block_disjunction(
      doc_iterators_t&& itrs,
      const order::prepared& ord = order::prepared::unordered(),
      sort::MergeType merge_type = sort::MergeType::AGGREGATE)
    : block_disjunction(std::move(itrs), 1, ord, merge_type) {
  }

  block_disjunction(
      doc_iterators_t&& itrs,
      size_t min_match_count,
      const order::prepared& ord = order::prepared::unordered(),
      sort::MergeType merge_type = sort::MergeType::AGGREGATE)
    : block_disjunction(
        std::move(itrs), min_match_count, ord, merge_type,
        [this](){
          return std::accumulate(
            itrs_.begin(), itrs_.end(), cost::cost_t(0),
            [](cost::cost_t lhs, const adapter& rhs) {
              return lhs + cost::extract(rhs, 0);
          });
        },
        resolve_overload_tag()) {
  }

  size_t match_count() const noexcept {
    return match_count_;
  }

  virtual doc_id_t value() const noexcept override {
    return doc_.value;
  }

  virtual bool next() override {
    do {
      while (!cur_) {
        if (begin_ >= std::end(mask_)) {
          if (refill()) {
            assert(cur_);
            break;
          }

          doc_.value = doc_limits::eof();
          match_count_ = 0;

          return false;
        }

        cur_ = *begin_++;
        doc_base_ += bits_required<uint64_t>();
        if constexpr (traits_type::min_match() || traits_type::score()) {
          buf_offset_ += bits_required<uint64_t>();
        }
      }

      const size_t offset = math::math_traits<uint64_t>::ctz(cur_);
      irs::unset_bit(cur_, offset);

      [[maybe_unused]] const size_t buf_offset = buf_offset_ + offset;

      if constexpr (traits_type::min_match()) {
        match_count_ = match_buf_.match_count(buf_offset);

        if (match_count_ < match_buf_.min_match_count()) {
          continue;
        }
      }

      doc_.value = doc_base_ + doc_id_t(offset);
      if constexpr (traits_type::score()) {
        score_value_ = score_buf_.get(buf_offset);
      }

      return true;
    } while (traits_type::min_match());

    assert(false);
    return true;
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (target <= doc_.value) {
      return doc_.value;
    } else if (target < max_) {
      const doc_id_t block_base = (max_ - window());

      target -= block_base;
      const doc_id_t block_offset = target / block_size();

      doc_base_ = block_base + block_offset * block_size();
      begin_ = mask_ + block_offset + 1;

      assert(begin_ > std::begin(mask_) && begin_ <= std::end(mask_));
      cur_ = begin_[-1] & ((~UINT64_C(0)) << target % block_size());

      next();
    } else {
      doc_.value = doc_limits::eof();

      if constexpr (traits_type::min_match()) {
        match_count_ = 0;
      }

      visit_and_purge([this, target](auto& it) mutable {
        const auto doc = it->seek(target);

        if (doc_limits::eof(doc)) {
          // exhausted
          return false;
        }

        if (doc < doc_.value) {
          doc_.value = doc;
          if constexpr (traits_type::min_match()) {
            match_count_ = 1;
          }
        } else if constexpr (traits_type::min_match()) {
          if (target == doc) {
            ++match_count_;
          }
        }

        return true;
      });

      if (itrs_.empty()) {
        doc_.value = doc_limits::eof();
        match_count_ = 0;

        return doc_limits::eof();
      }

      assert(!doc_limits::eof(doc_.value));
      cur_ = 0;
      begin_ = std::end(mask_); // enforce "refill()" for upcoming "next()"
      max_ = doc_.value;

      if constexpr (traits_type::seek_readahead()) {
        min_ = doc_.value;
        next();
      } else {
        min_ = doc_.value + 1;
        buf_offset_ = 0;

        if constexpr (traits_type::min_match()) {
          if (match_count_ < match_buf_.min_match_count()) {
            next();
            return doc_.value;
          }
        }

        if constexpr (traits_type::score()) {
          std::memset(score_buf_.data(), 0, score_buf_.bucket_size());
          for (auto& it : itrs_) {
            if (!it.score->is_default() && doc_.value == it->value()) {
              assert(it.score);
              merger_(score_buf_.data(), it.score->evaluate());
            }
          }

          score_value_ = score_buf_.data();
        }
      }
    }

    return doc_.value;
  }

 private:
  static constexpr doc_id_t block_size() noexcept {
    return bits_required<uint64_t>();
  }

  static constexpr doc_id_t num_blocks() noexcept {
    return std::max(size_t(1), traits_type::num_blocks());
  }

  static constexpr doc_id_t window() noexcept {
    return block_size()*num_blocks();
  }

  static_assert(block_size()*size_t(num_blocks()) < std::numeric_limits<doc_id_t>::max());

  using score_buffer_type = std::conditional_t<traits_type::score(),
    detail::score_buffer,
    detail::empty_score_buffer>;

  using min_match_buffer_type = detail::min_match_buffer<
    traits_type::min_match() ? window() : 0>;

  struct resolve_overload_tag{};

  template<typename Estimation>
  block_disjunction(
      doc_iterators_t&& itrs,
      size_t min_match_count,
      const order::prepared& ord,
      sort::MergeType merge_type,
      Estimation&& estimation,
      resolve_overload_tag)
    : frozen_attributes<3, doc_iterator>{{
        { type<document>::id(), &doc_   },
        { type<cost>::id(),     &cost_  },
        { type<score>::id(),    &score_ },
      }},
      itrs_(std::move(itrs)),
      doc_(itrs_.empty()
        ? doc_limits::eof()
        : doc_limits::invalid()),
      match_count_(itrs_.empty()
        ? size_t(0)
        : static_cast<size_t>(!traits_type::min_match())),
      cost_(std::forward<Estimation>(estimation)),
      score_buf_(ord, window()),
      match_buf_(min_match_count),
      merger_(ord.prepare_merger(merge_type)) {
    if (traits_type::score() && !ord.empty()) {
      score_.reset(this, [](score_ctx* ctx) noexcept -> const byte_type* {
        return static_cast<block_disjunction*>(ctx)->score_value_;
      });
    }

    if (traits_type::min_match() && min_match_count > 1) {
      // sort subnodes in ascending order by their cost
      // FIXME don't use extract
      std::sort(
        itrs_.begin(), itrs_.end(),
        [](const adapter& lhs, const adapter& rhs) {
          return cost::extract(lhs, 0) < cost::extract(rhs, 0);
      });
    }
  }

  template<typename Visitor>
  void visit_and_purge(Visitor visitor) {
    auto* begin = itrs_.data();
    auto* end = itrs_.data() + itrs_.size();

    while (begin != end) {
      if (!visitor(*begin)) {
        irstd::swap_remove(itrs_, begin);
        --end;

        if constexpr (traits_type::min_match_early_pruning()) {
          // we don't need precise match count
          if (itrs_.size() < match_buf_.min_match_count()) {
            // can't fulfill min match requirement anymore
            itrs_.clear();
            return;
          }
        }
      } else {
        ++begin;
      }
    }

    if constexpr (traits_type::min_match() && !traits_type::min_match_early_pruning()) {
      // we need precise match count, so can't break earlier
      if (itrs_.size() < match_buf_.min_match_count()) {
        // can't fulfill min match requirement anymore
        itrs_.clear();
        return;
      }
    }
  }

  void reset() noexcept {
    std::memset(mask_, 0, sizeof mask_);
    if constexpr (traits_type::score()) {
      score_value_ = score_buf_.data();
      std::memset(score_buf_.data(), 0, score_buf_.size());
    }
    if constexpr (traits_type::min_match()) {
      match_buf_.clear();
    }
  }

  bool refill() {
    if (itrs_.empty()) {
      return false;
    }

    if constexpr (!traits_type::min_match()) {
      reset();
    }

    bool empty = true;

    do {
      if constexpr (traits_type::min_match()) {
        // in min match case we need to clear
        // internal buffers on every iteration
        reset();
      }

      doc_base_ = min_;
      max_ = min_ + window();
      min_ = doc_limits::eof();

      visit_and_purge([this, &empty](auto& it) mutable {
        // FIXME
        // for min match case we can skip the whole block if
        // we can't satisfy match_buf_.min_match_count() conditions, namely
        //if constexpr (traits_type::min_match()) {
        //  if (empty && (&it + (match_buf_.min_match_count() - match_buf_.max_match_count()) < (itrs_.data() + itrs_.size()))) {
        //    // skip current block
        //    return true;
        //  }
        //}

        if constexpr (traits_type::score()) {
          if (!it.score->is_default()) {
            return this->refill<true>(it, empty);
          }
        }

        return this->refill<false>(it, empty);
      });
    } while (empty && !itrs_.empty());

    if (empty) {
      // exhausted
      assert(itrs_.empty());
      return false;
    }

    cur_ = *mask_;
    begin_ = mask_ + 1;
    while (!cur_) {
      cur_ = *begin_++;
      doc_base_ += bits_required<uint64_t>();
    }
    assert(cur_);

    if constexpr (traits_type::min_match() || traits_type::score()) {
      buf_offset_ = 0;
    }

    return true;
  }

  template<bool Score>
  bool refill(adapter& it, bool& empty) {
    assert(it.doc);
    const auto* doc = &it.doc->value;
    assert(!doc_limits::eof(*doc));

    // disjunction is 1 step next behind, that may happen:
    // - before the very first next()
    // - after seek() in case of 'seek_readahead() == false'
    if (*doc < doc_base_ && !it->next()) {
      // exhausted
      return false;
    }

    for (;;) {
      if (*doc >= max_) {
        min_ = std::min(*doc, min_);
        return true;
      }

      const size_t offset = *doc - doc_base_;

      irs::set_bit(mask_[offset / block_size()], offset % block_size());

      if constexpr (Score) {
        assert(it.score);
        merger_(score_buf_.get(offset), it.score->evaluate());
      }

      if constexpr (traits_type::min_match()) {
        empty &= match_buf_.inc(offset);
      } else {
        empty = false;
      }

      if (!it->next()) {
        // exhausted
        return false;
      }
    }
  }

  doc_iterators_t itrs_;
  uint64_t mask_[num_blocks()]{};
  uint64_t* begin_{std::end(mask_)};
  uint64_t cur_{};
  document doc_;
  doc_id_t doc_base_{doc_limits::invalid()};
  doc_id_t min_{doc_limits::min()}; // base doc id for the next mask
  doc_id_t max_{doc_limits::invalid()}; // max doc id in the current mask
  score score_;
  size_t match_count_;
  cost cost_;
  size_t buf_offset_{}; // offset within a buffer
  score_buffer_type score_buf_;
  min_match_buffer_type match_buf_;
  const byte_type* score_value_{score_buf_.data()};
  order::prepared::merger merger_;
}; // block_disjunction

template<
  typename DocIterator,
  typename Adapter = score_iterator_adapter<DocIterator>>
using scored_disjunction_iterator = block_disjunction<
  DocIterator,
  block_disjunction_traits<true, MatchType::MATCH, false>,
  Adapter>;

template<
  typename DocIterator,
  typename Adapter = score_iterator_adapter<DocIterator>>
using disjunction_iterator = block_disjunction<
  DocIterator,
  block_disjunction_traits<false, MatchType::MATCH, false>,
  Adapter>;

template<
  typename DocIterator,
  typename Adapter = score_iterator_adapter<DocIterator>>
using scored_min_match_iterator = block_disjunction<
  DocIterator,
  block_disjunction_traits<true, MatchType::MIN_MATCH, false>,
  Adapter>;

template<
  typename DocIterator,
  typename Adapter = score_iterator_adapter<DocIterator>>
using min_match_iterator = block_disjunction<
  DocIterator,
  block_disjunction_traits<false, MatchType::MIN_MATCH, false>,
  Adapter>;

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
      if constexpr (Disjunction::enable_unary()) {
        using unary_disjunction_t = typename Disjunction::unary_disjunction_t;
        return memory::make_managed<unary_disjunction_t>(std::move(itrs.front()));
      }

      // single sub-query
      return std::move(itrs.front());
    case 2: {
      using basic_disjunction_t = typename Disjunction::basic_disjunction_t;

      // simple disjunction
      return memory::make_managed<basic_disjunction_t>(
         std::move(itrs.front()),
         std::move(itrs.back()),
         std::forward<Args>(args)...);
    }
  }

  if (Disjunction::small_disjunction_upper_bound() &&
        size <= Disjunction::small_disjunction_upper_bound()) {
    using small_disjunction_t = typename Disjunction::small_disjunction_t;

    // small disjunction
    return memory::make_managed<small_disjunction_t>(
      std::move(itrs),
      std::forward<Args>(args)...);
  }

  // disjunction
  return memory::make_managed<Disjunction>(
    std::move(itrs),
    std::forward<Args>(args)...);
}

} // ROOT

#endif // IRESEARCH_DISJUNCTION_H
