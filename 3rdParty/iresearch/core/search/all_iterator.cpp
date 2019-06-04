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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "all_iterator.hpp"
#include "search/score_doc_iterators.hpp"
#include "formats/empty_term_reader.hpp"

NS_ROOT

all_iterator::all_iterator(
    const irs::sub_reader& reader,
    const irs::attribute_store& prepared_filter_attrs,
    const irs::order::prepared& order,
    uint64_t docs_count)
  : basic_doc_iterator_base(order),
    max_doc_(irs::doc_id_t(irs::type_limits<irs::type_t::doc_id_t>::min() + docs_count - 1)) {
  // make doc_id accessible via attribute
  attrs_.emplace(doc_);

  // set estimation value
  estimate(max_doc_);

  // set scorers
  prepare_score(ord_->prepare_scorers(
    reader,
    irs::empty_term_reader(docs_count),
    prepared_filter_attrs,
    attributes() // doc_iterator attributes
  ));
}

NS_END // ROOT
