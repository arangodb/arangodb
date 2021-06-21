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

#ifndef IRESEARCH_BITSET_DOC_ITERATOR_H
#define IRESEARCH_BITSET_DOC_ITERATOR_H

#include "analysis/token_attributes.hpp"
#include "search/cost.hpp"
#include "search/score.hpp"
#include "utils/frozen_attributes.hpp"
#include "utils/type_limits.hpp"

namespace iresearch {

class bitset_doc_iterator
  : public doc_iterator,
    private util::noncopyable {
 public:
  using word_t = size_t;

  bitset_doc_iterator(const word_t* begin, const word_t* end) noexcept;

  virtual bool next() noexcept override final;
  virtual doc_id_t seek(doc_id_t target) noexcept override final;
  virtual doc_id_t value() const noexcept override final { return doc_.value; }
  virtual attribute* get_mutable(irs::type_info::type_id id) noexcept override;

 protected:
  explicit bitset_doc_iterator(cost::cost_t cost) noexcept
    : cost_(cost),
      doc_(doc_limits::invalid()),
      begin_(nullptr),
      end_(nullptr) {
    reset();
  }

  virtual bool refill(const word_t** /*begin*/,
                      const word_t** /*end*/) {
    return false;
  }

 private:
  // assume begin_, end_ are set
  void reset() noexcept {
    next_ = begin_;
    word_ = 0;
    base_ = doc_limits::invalid() - bits_required<word_t>(); // before the first word
    assert(begin_ <= end_);
  }

  cost cost_;
  document doc_;
  const word_t* begin_;
  const word_t* end_;
  const word_t* next_;
  word_t word_;
  doc_id_t base_;
}; // bitset_doc_iterator

} // ROOT

#endif // IRESEARCH_BITSET_DOC_ITERATOR_H
