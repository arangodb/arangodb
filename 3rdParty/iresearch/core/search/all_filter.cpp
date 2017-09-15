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

#include "formats/empty_term_reader.hpp"
#include "search/score_doc_iterators.hpp"

#include "all_filter.hpp"

NS_LOCAL

class all_iterator final : public irs::doc_iterator_base {
 public:
  all_iterator(
      const irs::sub_reader& reader,
      const irs::attribute_store& prepared_filter_attrs,
      const irs::order::prepared& order,
      uint64_t docs_count)
    : doc_iterator_base(order),
      max_doc_(irs::doc_id_t(irs::type_limits<irs::type_t::doc_id_t>::min() + docs_count - 1)) {
    // make doc_id accessible via attribute
    attrs_.emplace(doc_);

    // set estimation value
    estimate(max_doc_);

    // set scorers
    scorers_ = ord_->prepare_scorers(
      reader,
      irs::empty_term_reader(docs_count),
      prepared_filter_attrs,
      attributes() // doc_iterator attributes
    );

    prepare_score([this](irs::byte_type* score) {
      scorers_.score(*ord_, score);
    });
  }

  virtual bool next() override {
    return !irs::type_limits<irs::type_t::doc_id_t>::eof(seek(doc_.value + 1));
  }

  virtual irs::doc_id_t seek(irs::doc_id_t target) override {
    doc_.value = target <= max_doc_
      ? target
      : irs::type_limits<irs::type_t::doc_id_t>::eof();

    return doc_.value;
  }

  virtual irs::doc_id_t value() const NOEXCEPT override {
    return doc_.value;
  }

 private:
  irs::document doc_;
  irs::doc_id_t max_doc_; // largest valid doc_id
  irs::order::prepared::scorers scorers_;
};

NS_END

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
  explicit all_query(attribute_store&& attrs)
    : filter::prepared(std::move(attrs)) {
  }

  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& order
  ) const override {
    return doc_iterator::make<all_iterator>(
      rdr,
      attributes(), // prepared_filter attributes
      order,
      rdr.docs_count()
    );
  }
};

DEFINE_FILTER_TYPE(irs::all);
DEFINE_FACTORY_DEFAULT(irs::all);

all::all() NOEXCEPT
  : filter(all::type()) {
}

filter::prepared::ptr all::prepare(
    const index_reader& reader,
    const order::prepared& order,
    boost_t filter_boost
) const {
  attribute_store attrs;

  // skip filed-level/term-level statistics because there are no fields/terms
  order.prepare_stats().finish(attrs, reader);

  irs::boost::apply(attrs, boost() * filter_boost); // apply boost

  return filter::prepared::make<all_query>(std::move(attrs));
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------