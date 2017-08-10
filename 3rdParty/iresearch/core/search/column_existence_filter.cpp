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

#include "column_existence_filter.hpp"
#include "index/field_meta.hpp"
#include "search/score_doc_iterators.hpp"
#include "search/disjunction.hpp"

#include <boost/functional/hash.hpp>

NS_LOCAL

class column_existence_iterator final : public irs::score_doc_iterator_base {
 public:
  explicit column_existence_iterator(
      irs::columnstore_reader::column_iterator::ptr&& it,
      const irs::order::prepared& ord,
      irs::cost::cost_t estimation) NOEXCEPT
    : score_doc_iterator_base(ord),
      it_(std::move(it)) {
    assert(it_);

    // set estimation value
    attrs_.emplace<irs::cost>()->value(estimation);
  }

  virtual void score() override { }

  virtual irs::doc_id_t value() const NOEXCEPT override {
    return it_->value().first;
  }

  virtual bool next() override {
    return it_->next();
  }

  virtual irs::doc_id_t seek(irs::doc_id_t target) override {
    it_->seek(target);
    return value();
  }

 private:
  irs::columnstore_reader::column_iterator::ptr it_;
}; // column_existence_iterator

class column_existence_query final : public irs::filter::prepared {
 public:
  explicit column_existence_query(const std::string& field)
    : field_(field) {
  }

  virtual irs::score_doc_iterator::ptr execute(
      const irs::sub_reader& rdr,
      const irs::order::prepared& ord) const override {
    const auto* column = rdr.column_reader(field_);

    if (!column) {
      return irs::score_doc_iterator::empty();
    }

    return irs::score_doc_iterator::make<column_existence_iterator>(
      column->iterator(), ord, column->size()
    );
  }

 private:
  std::string field_;
}; // column_existence_query

class column_prefix_existence_query final : public irs::filter::prepared {
 public:
  explicit column_prefix_existence_query(const std::string& prefix)
    : prefix_(prefix) {
  }

  virtual irs::score_doc_iterator::ptr execute(
      const irs::sub_reader& rdr,
      const irs::order::prepared& ord) const override {
    auto it = rdr.columns();

    // FIXME seek
    bool next;
    while ((next = it->next()) && it->value().name < prefix_);

    if (!next) {
      // reached the end
      return irs::score_doc_iterator::empty();
    }

    std::vector<column_existence_iterator::ptr> itrs;
    while (irs::starts_with(it->value().name, prefix_)) {
      const auto* column = rdr.column_reader(it->value().id);

      if (!column) {
        continue;
      }

      auto column_it = irs::memory::make_unique<column_existence_iterator>(
        column->iterator(), ord, column->size()
      );

      itrs.emplace_back(std::move(column_it));

      if (!it->next()) {
        break;
      }
    }

    typedef irs::detail::disjunction<
      column_existence_iterator::ptr
    > disjunction_t;

    return irs::detail::make_disjunction<disjunction_t>(
      std::move(itrs), 1, ord
    );
  }

 private:
  std::string prefix_;
}; // column_prefix_existence_query

NS_END

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                by_column_existence implementation
// -----------------------------------------------------------------------------

DEFINE_FILTER_TYPE(by_column_existence);
DEFINE_FACTORY_DEFAULT(by_column_existence);

by_column_existence::by_column_existence() NOEXCEPT
  : filter(by_column_existence::type()) {
}

bool by_column_existence::equals(const filter& rhs) const {
  const auto& trhs = static_cast<const by_column_existence&>(rhs);

  return filter::equals(rhs)
    && field_ == trhs.field_
    && prefix_match_ == trhs.prefix_match_;
}

size_t by_column_existence::hash() const {
  size_t seed = 0;
  ::boost::hash_combine(seed, filter::hash());
  ::boost::hash_combine(seed, field_);
  ::boost::hash_combine(seed, prefix_match_);
  return seed;
}

filter::prepared::ptr by_column_existence::prepare(
    const index_reader&,
    const order::prepared&,
    boost_t) const {
  if (prefix_match_) {
    return memory::make_unique<column_prefix_existence_query>(field_);
  }

  return memory::make_unique<column_existence_query>(field_);
}

NS_END // ROOT

