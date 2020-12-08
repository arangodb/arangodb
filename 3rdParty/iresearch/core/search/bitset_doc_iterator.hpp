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
#include "utils/bitset.hpp"

namespace iresearch {

class bitset_doc_iterator final
  : public frozen_attributes<3, doc_iterator>,
    private util::noncopyable {
 public:
  explicit bitset_doc_iterator(const bitset& set)
    : bitset_doc_iterator(set, order::prepared::unordered()) {
  }

  bitset_doc_iterator(
    const sub_reader& reader,
    const byte_type* stats,
    const bitset& set,
    const order::prepared& order,
    boost_t boost);

  virtual bool next() noexcept override;
  virtual doc_id_t seek(doc_id_t target) noexcept override;
  virtual doc_id_t value() const noexcept override { return doc_.value; }

 private:
  using word_t = bitset::word_t;

  bitset_doc_iterator(const bitset& set, const order::prepared& ord);

  cost cost_;
  document doc_;
  score score_;
  const word_t* begin_;
  const word_t* end_;
  const word_t* next_;
  word_t word_{};
  doc_id_t base_{doc_limits::invalid() - bits_required<word_t>()}; // before the first word
}; // bitset_doc_iterator

} // ROOT

#endif // IRESEARCH_BITSET_DOC_ITERATOR_H
