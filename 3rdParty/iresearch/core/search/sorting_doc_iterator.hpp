////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_SORTING_DOC_ITERATOR_H
#define IRESEARCH_SORTING_DOC_ITERATOR_H

#include "index/iterators.hpp"
#include "index/sorted_column.hpp"
#include "analysis/token_attributes.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @class sorting_doc_iterator
///-----------------------------------------------------------------------------
///      [0] <-- begin
///      [1]      |
///      [2]      | head (payload heap)
///      [3]      |
///      [4] <-- lead_
///      [5]      |
///      [6]      | lead (list of accepted iterators)
///      ...      |
///      [n] <-- end
///-----------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
class sorting_doc_iterator final : public doc_iterator {
 public:
  class segment {
   public:
    segment(
        doc_iterator::ptr&& doc,
        doc_iterator::ptr&& sort,
        irs::payload& payload
    ) NOEXCEPT
      : doc_(std::move(doc)),
        sort_(std::move(sort)),
        payload_(&payload) {
      assert(doc_ && sort_);
    }

    doc_id_t value() const {
      return doc_->value();
    }

    const bytes_ref& payload() const {
      return payload_->value;
    }

    const attribute_view& attributes() const {
      return doc_->attributes();
    }

    bool next() {
      if (!doc_->next()) {
        return false;
      }

      const auto target = doc_->value();
      return target == sort_->seek(target);
    }

   private:
    doc_iterator::ptr doc_;
    doc_iterator::ptr sort_;
    irs::payload* payload_; // FIXME use irs::payload instead?
  };

  sorting_doc_iterator(
      const comparer& less
  ) : less_(less) {
  }

  bool emplace(doc_iterator::ptr&& docs, const columnstore_reader::column_reader& sort) {
    auto it = sort.iterator();

    if (!it) {
      return false;
    }

    irs::payload* payload = it->attributes().get<irs::payload>().get();

    if (!payload) {
      return false;
    }

    itrs_.emplace_back(std::move(docs), std::move(it), *payload);
    heap_.push_back(itrs_.back());
    ++lead_;
    return true;
  }

  virtual bool next() override;

  virtual doc_id_t seek(doc_id_t target) override {
    irs::seek(*this, target);
    return value();
  }

  virtual doc_id_t value() const NOEXCEPT override { return doc_; }

  virtual const attribute_view& attributes() const NOEXCEPT override {
    return *attrs_;
  }

 private:
  struct heap_comparer {
    explicit heap_comparer(const comparer& less) NOEXCEPT
      : less(less) {
    }

    bool operator()(const segment& lhs, const segment& rhs) const {
      return static_cast<const comparer&>(less)(rhs.payload(), lhs.payload());
    }

    std::reference_wrapper<const comparer> less;
  }; // heap_comparer

  template<typename Iterator>
  bool remove_lead(Iterator it) {
    if (&*it != &heap_.back()) {
      std::swap(*it, heap_.back());
    }
    heap_.pop_back();
    return !heap_.empty();
  }

  heap_comparer less_;
  const attribute_view* attrs_{ &attribute_view::empty_instance() };
  std::deque<segment> itrs_; // FIXME use vector instead
  std::vector<std::reference_wrapper<segment>> heap_;
  size_t lead_{0}; // number of segments in lead group
  doc_id_t doc_;
}; // sorting_doc_iterator

NS_END // ROOT

#endif // IRESEARCH_SORTING_DOC_ITERATOR_H

