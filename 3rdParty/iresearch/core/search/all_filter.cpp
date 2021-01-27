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

#include "all_filter.hpp"
#include "all_iterator.hpp"

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                                               all
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @class all_query
/// @brief compiled all_filter that returns all documents
////////////////////////////////////////////////////////////////////////////////
class all_query final : public filter::prepared {
 public:
  explicit all_query(bstring&& stats, boost_t boost)
    : filter::prepared(boost),
      stats_(std::move(stats)) {
  }

  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& order,
      const attribute_provider* /*ctx*/) const override {
    return memory::make_managed<all_iterator>(
      rdr, stats_.c_str(), order,
      rdr.docs_count(), boost());
  }

 private:
  bstring stats_;
};

DEFINE_FACTORY_DEFAULT(irs::all)

all::all() noexcept
  : filter(irs::type<all>::get()) {
}

filter::prepared::ptr all::prepare(
    const index_reader& reader,
    const order::prepared& order,
    boost_t filter_boost,
    const attribute_provider* /*ctx*/) const {
  // skip field-level/term-level statistics because there are no explicit
  // fields/terms, but still collect index-level statistics
  // i.e. all fields and terms implicitly match
  bstring stats(order.stats_size(), 0);
  auto* stats_buf = const_cast<byte_type*>(stats.data());

  order.prepare_collectors(stats_buf, reader);

  return memory::make_managed<all_query>(std::move(stats), this->boost()*filter_boost);
}

} // ROOT
