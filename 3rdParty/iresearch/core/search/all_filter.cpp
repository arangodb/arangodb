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

#include "all_filter.hpp"
#include "all_iterator.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                               all
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @class all_query
/// @brief compiled all_filter that returns all documents
////////////////////////////////////////////////////////////////////////////////
class all_query: public filter::prepared {
 public:
  explicit all_query(bstring&& stats, boost_t boost)
    : filter::prepared(std::move(stats), boost) {
  }

  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& order,
      const attribute_view& /*ctx*/
  ) const override {
    return doc_iterator::make<all_iterator>(
      rdr, stats(), order, rdr.docs_count(), boost()
    );
  }
};

DEFINE_FILTER_TYPE(irs::all)
DEFINE_FACTORY_DEFAULT(irs::all)

all::all() NOEXCEPT
  : filter(all::type()) {
}

filter::prepared::ptr all::prepare(
    const index_reader& reader,
    const order::prepared& order,
    boost_t filter_boost,
    const attribute_view& /*ctx*/
) const {
  // skip field-level/term-level statistics because there are no explicit
  // fields/terms, but still collect index-level statistics
  // i.e. all fields and terms implicitly match
  bstring stats(order.stats_size(), 0);
  order.prepare_collectors(const_cast<byte_type*>(stats.data()), reader);

  return filter::prepared::make<all_query>(std::move(stats), this->boost()*filter_boost);
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
