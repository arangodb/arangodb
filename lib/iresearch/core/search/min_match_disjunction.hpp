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

#pragma once

#include "disjunction.hpp"

namespace irs {

template<typename DocIterator = doc_iterator::ptr>
struct CostAdapter : ScoreAdapter<DocIterator> {
  explicit CostAdapter(DocIterator&& it) noexcept
    : ScoreAdapter<DocIterator>{std::move(it)} {
    // TODO(MBkkt) 0 instead of kMax?
    est = cost::extract(*this->it, cost::kMax);
  }

  CostAdapter(CostAdapter&&) noexcept = default;
  CostAdapter& operator=(CostAdapter&&) noexcept = default;

  cost::cost_t est{};
};

using CostAdapters = std::vector<CostAdapter<>>;

// Heapsort-based "weak and" iterator
// -----------------------------------------------------------------------------
//      [0] <-- begin
//      [1]      |
//      [2]      | head (min doc_id, cost heap)
//      [3]      |
//      [4] <-- lead_
// c ^  [5]      |
// o |  [6]      | lead (list of accepted iterators)
// s |  ...      |
// t |  [n] <-- end
// -----------------------------------------------------------------------------
template<typename Merger>
class MinMatchDisjunction : public doc_iterator,
                            protected Merger,
                            protected score_ctx {
 public:
  MinMatchDisjunction(CostAdapters&& itrs, size_t min_match_count,
                      Merger&& merger = {})
    : Merger{std::move(merger)},
      itrs_{std::move(itrs)},
      min_match_count_{std::clamp(min_match_count, size_t{1}, itrs_.size())},
      lead_{itrs_.size()} {
    IRS_ASSERT(!itrs_.empty());
    IRS_ASSERT(min_match_count_ >= 1 && min_match_count_ <= itrs_.size());

    // sort subnodes in ascending order by their cost
    std::sort(itrs_.begin(), itrs_.end(),
              [](const auto& lhs, const auto& rhs) noexcept {
                return lhs.est < rhs.est;
              });

    std::get<cost>(attrs_).reset([this]() noexcept {
      return std::accumulate(itrs_.begin(), itrs_.end(), cost::cost_t{0},
                             [](cost::cost_t lhs, const auto& rhs) noexcept {
                               return lhs + rhs.est;
                             });
    });

    // prepare external heap
    heap_.resize(itrs_.size());
    std::iota(heap_.begin(), heap_.end(), size_t{0});

    if constexpr (HasScore_v<Merger>) {
      PrepareScore();
    }
  }

  attribute* get_mutable(type_info::type_id id) noexcept final {
    return irs::get_mutable(attrs_, id);
  }

  doc_id_t value() const final { return std::get<document>(attrs_).value; }

  bool next() final {
    auto& doc_value = std::get<document>(attrs_).value;

    if (doc_limits::eof(doc_value)) {
      return false;
    }

    while (CheckSize()) {
      // start next iteration. execute next for all lead iterators
      // and move them to head
      if (!PopLead()) {
        doc_value = doc_limits::eof();
        return false;
      }

      // make step for all head iterators less or equal current doc (doc_)
      while (Top().value() <= doc_value) {
        const bool exhausted = Top().value() == doc_value
                                 ? !Top()->next()
                                 : doc_limits::eof(Top()->seek(doc_value + 1));

        if (exhausted && !RemoveTop()) {
          doc_value = doc_limits::eof();
          return false;
        }
        RefreshTop();
      }

      // count equal iterators
      const auto top = Top().value();

      do {
        AddLead();
        if (lead_ >= min_match_count_) {
          return !doc_limits::eof(doc_value = top);
        }
      } while (top == Top().value());
    }

    doc_value = doc_limits::eof();
    return false;
  }

  doc_id_t seek(doc_id_t target) final {
    auto& doc_value = std::get<document>(attrs_).value;

    if (target <= doc_value) {
      return doc_value;
    }

    // execute seek for all lead iterators and
    // move one to head if it doesn't hit the target
    for (auto it = Lead(), end = heap_.end(); it != end;) {
      IRS_ASSERT(*it < itrs_.size());
      const auto doc = itrs_[*it]->seek(target);

      if (doc_limits::eof(doc)) {
        --lead_;

        // iterator exhausted
        if (!RemoveLead(it)) {
          return doc_value = doc_limits::eof();
        }

        it = Lead();
        end = heap_.end();
      } else {
        if (doc != target) {
          // move back to head
          PushHead(it);
          --lead_;
        }
        ++it;
      }
    }

    // check if we still satisfy search criteria
    if (lead_ >= min_match_count_) {
      return doc_value = target;
    }

    // main search loop
    for (;; target = Top().value()) {
      while (Top().value() <= target) {
        const auto doc = Top()->seek(target);

        if (doc_limits::eof(doc)) {
          // iterator exhausted
          if (!RemoveTop()) {
            return doc_value = doc_limits::eof();
          }
        } else if (doc == target) {
          // valid iterator, doc == target
          AddLead();
          if (lead_ >= min_match_count_) {
            return doc_value = target;
          }
        } else {
          // invalid iterator, doc != target
          RefreshTop();
        }
      }

      // can't find enough iterators equal to target here.
      // start next iteration. execute next for all lead iterators
      // and move them to head
      if (!PopLead()) {
        return doc_value = doc_limits::eof();
      }
    }
  }

  // Calculates total count of matched iterators. This value could be
  // greater than required min_match. All matched iterators points
  // to current matched document after this call.
  // Returns total matched iterators count.
  size_t match_count() {
    PushValidToLead();
    return lead_;
  }

 private:
  using Attributes = std::tuple<document, cost, score>;

  void PrepareScore() {
    IRS_ASSERT(Merger::size());

    auto& score = std::get<irs::score>(attrs_);

    score.Reset(*this, [](score_ctx* ctx, score_t* res) noexcept {
      auto& self = static_cast<MinMatchDisjunction&>(*ctx);
      IRS_ASSERT(!self.heap_.empty());

      self.PushValidToLead();

      // score lead iterators
      std::memset(res, 0, static_cast<Merger&>(self).byte_size());
      std::for_each(self.Lead(), self.heap_.end(), [&self, res](size_t it) {
        IRS_ASSERT(it < self.itrs_.size());
        if (auto& score = *self.itrs_[it].score; !score.IsDefault()) {
          auto& merger = static_cast<Merger&>(self);
          score(merger.temp());
          merger(res, merger.temp());
        }
      });
    });
  }

  // Push all valid iterators to lead.
  void PushValidToLead() {
    auto& doc_value = std::get<document>(attrs_).value;

    for (auto lead = Lead(), begin = heap_.begin();
         lead != begin && Top().value() <= doc_value;) {
      // hitch head
      if (Top().value() == doc_value) {
        // got hit here
        AddLead();
        --lead;
      } else {
        if (doc_limits::eof(Top()->seek(doc_value))) {
          // iterator exhausted
          RemoveTop();
          lead = Lead();
        } else {
          RefreshTop();
        }
      }
    }
  }

  template<typename Iterator>
  void Push(Iterator begin, Iterator end) noexcept {
    // lambda here gives ~20% speedup on GCC
    std::push_heap(begin, end,
                   [this](const size_t lhs, const size_t rhs) noexcept {
                     IRS_ASSERT(lhs < itrs_.size());
                     IRS_ASSERT(rhs < itrs_.size());
                     const auto& lhs_it = itrs_[lhs];
                     const auto& rhs_it = itrs_[rhs];
                     const auto lhs_doc = lhs_it.value();
                     const auto rhs_doc = rhs_it.value();
                     return (lhs_doc > rhs_doc ||
                             (lhs_doc == rhs_doc && lhs_it.est > rhs_it.est));
                   });
  }

  template<typename Iterator>
  void Pop(Iterator begin, Iterator end) noexcept {
    // lambda here gives ~20% speedup on GCC
    detail::pop_heap(begin, end,
                     [this](const size_t lhs, const size_t rhs) noexcept {
                       IRS_ASSERT(lhs < itrs_.size());
                       IRS_ASSERT(rhs < itrs_.size());
                       const auto& lhs_it = itrs_[lhs];
                       const auto& rhs_it = itrs_[rhs];
                       const auto lhs_doc = lhs_it.value();
                       const auto rhs_doc = rhs_it.value();
                       return (lhs_doc > rhs_doc ||
                               (lhs_doc == rhs_doc && lhs_it.est > rhs_it.est));
                     });
  }

  // Performs a step for each iterator in lead group and pushes it to the head.
  // Returns true - if the min_match_count_ condition still can be satisfied,
  // false - otherwise
  bool PopLead() {
    for (auto it = Lead(), end = heap_.end(); it != end;) {
      IRS_ASSERT(*it < itrs_.size());
      if (!itrs_[*it]->next()) {
        --lead_;

        // remove iterator
        if (!RemoveLead(it)) {
          return false;
        }

        it = Lead();
        end = heap_.end();
      } else {
        // push back to head
        Push(heap_.begin(), ++it);
        --lead_;
      }
    }

    return true;
  }

  // Removes an iterator from the specified position in lead group
  // without moving iterators after the specified iterator.
  // Returns true - if the min_match_count_ condition still can be satisfied,
  // false - otherwise.
  template<typename Iterator>
  bool RemoveLead(Iterator it) noexcept {
    if (&*it != &heap_.back()) {
      std::swap(*it, heap_.back());
    }
    heap_.pop_back();
    return CheckSize();
  }

  // Removes iterator from the top of the head without moving
  // iterators after the specified iterator.
  // Returns true - if the min_match_count_ condition still can be satisfied,
  // false - otherwise.
  bool RemoveTop() noexcept {
    auto lead = Lead();
    Pop(heap_.begin(), lead);
    return RemoveLead(--lead);
  }

  // Refresh the value of the top of the head.
  void RefreshTop() noexcept {
    auto lead = Lead();
    Pop(heap_.begin(), lead);
    Push(heap_.begin(), lead);
  }

  // Push the specified iterator from lead group to the head
  // without movinh iterators after the specified iterator.
  template<typename Iterator>
  void PushHead(Iterator it) noexcept {
    Iterator lead = Lead();
    if (it != lead) {
      std::swap(*it, *lead);
    }
    ++lead;
    Push(heap_.begin(), lead);
  }

  // Returns true - if the min_match_count_ condition still can be satisfied,
  // false - otherwise.
  bool CheckSize() const noexcept { return heap_.size() >= min_match_count_; }

  // Returns reference to the top of the head
  auto& Top() noexcept {
    IRS_ASSERT(!heap_.empty());
    IRS_ASSERT(heap_.front() < itrs_.size());
    return itrs_[heap_.front()];
  }

  // Returns the first iterator in the lead group
  auto Lead() noexcept {
    IRS_ASSERT(lead_ <= heap_.size());
    return heap_.end() - lead_;
  }

  // Adds iterator to the lead group
  void AddLead() {
    Pop(heap_.begin(), Lead());
    ++lead_;
  }

  CostAdapters itrs_;  // sub iterators
  std::vector<size_t> heap_;
  size_t min_match_count_;  // minimum number of hits
  size_t lead_;             // number of iterators in lead group
  Attributes attrs_;
};

}  // namespace irs
