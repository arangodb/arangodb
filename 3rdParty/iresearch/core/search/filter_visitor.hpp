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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_FILTER_VISITOR_H
#define IRESEARCH_FILTER_VISITOR_H

#include "multiterm_query.hpp"
#include "formats/formats.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class filter_visitor
/// @brief base filter visitor interface
//////////////////////////////////////////////////////////////////////////////
struct filter_visitor {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief makes preparations for a visitor
  //////////////////////////////////////////////////////////////////////////////
  virtual void prepare(const seek_term_iterator& terms) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief applies actions to a current term iterator
  //////////////////////////////////////////////////////////////////////////////
  virtual void visit() = 0;

  virtual ~filter_visitor() = default;
};

//////////////////////////////////////////////////////////////////////////////
/// @class multiterm_visitor
/// @brief filter visitor for multiterm queries
//////////////////////////////////////////////////////////////////////////////
class multiterm_visitor final : public filter_visitor {
 public:
  multiterm_visitor(
      const sub_reader& segment,
      const term_reader& reader,
      limited_sample_scorer& scorer,
      multiterm_query::states_t& states)
    : segment_(segment), reader_(reader),
      scorer_(scorer), states_(states) {
  }

  virtual void prepare(const seek_term_iterator& terms) override {
    // get term metadata
    auto& meta = terms.attributes().get<term_meta>();

    // NOTE: we can't use reference to 'docs_count' here, like
    // 'const auto& docs_count = meta ? meta->docs_count : NO_DOCS;'
    // since not gcc4.9 nor msvc2015-2019 can handle this correctly
    // probably due to broken optimization
    docs_count_ = meta ? &meta->docs_count : &no_docs_;

    // get state for current segment
    state_ = &states_.insert(segment_);
    state_->reader = &reader_;

    terms_ = &terms;
  }

  virtual void visit() override {
    // fill scoring candidates
    assert(state_);
    assert(docs_count_);
    assert(terms_);
    scorer_.collect(*docs_count_, state_->count++, *state_, segment_, *terms_);
    state_->estimation += *docs_count_; // collect cost
  }

 private:
  const decltype(term_meta::docs_count) no_docs_ = 0;
  const sub_reader& segment_;
  const term_reader& reader_;
  limited_sample_scorer& scorer_;
  multiterm_query::states_t& states_;
  multiterm_state* state_ = nullptr;
  const decltype(term_meta::docs_count)* docs_count_ = nullptr;
  const seek_term_iterator* terms_ = nullptr;
};

NS_END

#endif // IRESEARCH_FILTER_VISITOR_H
