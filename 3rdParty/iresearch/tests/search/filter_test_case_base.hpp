////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_FILTER_TEST_CASE_BASE
#define IRESEARCH_FILTER_TEST_CASE_BASE

#include "tests_shared.hpp"
#include "analysis/token_attributes.hpp"
#include "search/cost.hpp"
#include "search/score.hpp"
#include "search/filter.hpp"
#include "search/tfidf.hpp"
#include "utils/singleton.hpp"
#include "utils/type_limits.hpp"
#include "index/index_tests.hpp"

NS_BEGIN(tests)
NS_BEGIN(sort)

////////////////////////////////////////////////////////////////////////////////
/// @class boost
/// @brief boost scorer assign boost value to the particular document score
////////////////////////////////////////////////////////////////////////////////
struct boost : public irs::sort {
  class scorer: public irs::sort::scorer_base<irs::boost_t> {
   public:
    DEFINE_FACTORY_INLINE(scorer)

    scorer(irs::boost_t boost): boost_(boost) { }

    virtual void score(irs::byte_type* score) {
      score_cast(score) = boost_;
    }

   private:
    irs::boost_t boost_;
  }; // sort::boost::scorer

  class prepared: public irs::sort::prepared_basic<irs::boost_t, void> {
   public:
    DEFINE_FACTORY_INLINE(prepared)
    prepared() { }

    virtual const irs::flags& features() const override {
      return irs::flags::empty_instance();
    }

    virtual scorer::ptr prepare_scorer(
        const irs::sub_reader&,
        const irs::term_reader&,
        const irs::byte_type* query_attrs,
        const irs::attribute_view& doc_attrs,
        irs::boost_t boost
    ) const override {
      return boost::scorer::make<boost::scorer>(boost);
    }

   private:
    const std::function<bool(score_t, score_t)>* less_;
  }; // sort::boost::prepared

  DECLARE_SORT_TYPE();
  DECLARE_FACTORY();
  typedef irs::boost_t score_t;
  boost() : sort(boost::type()) {}
  virtual sort::prepared::ptr prepare() const {
    return boost::prepared::make<boost::prepared>();
  }
}; // sort::boost

//////////////////////////////////////////////////////////////////////////////
/// @brief expose sort functionality through overidable lambdas
//////////////////////////////////////////////////////////////////////////////
struct custom_sort: public irs::sort {
  DECLARE_SORT_TYPE();

  class prepared: public irs::sort::prepared_base<irs::doc_id_t, void> {
   public:
    class collector
      : public irs::sort::field_collector, public irs::sort::term_collector {
     public:
      collector(const custom_sort& sort): sort_(sort) {}

      virtual void collect(
        const irs::sub_reader& segment,
        const irs::term_reader& field
      ) override {
        if (sort_.collector_collect_field) {
          sort_.collector_collect_field(segment, field);
        }
      }

      virtual void collect(
        const irs::sub_reader& segment,
        const irs::term_reader& field,
        const irs::attribute_view& term_attrs
      ) override {
        if (sort_.collector_collect_term) {
          sort_.collector_collect_term(segment, field, term_attrs);
        }
      }

      virtual void collect(const irs::bytes_ref& in) override {
        // NOOP
      }

      virtual void write(irs::data_output& out) const override {
        // NOOP
      }

     private:
      const custom_sort& sort_;
    };

    class scorer: public irs::sort::scorer_base<irs::doc_id_t> {
     public:
      virtual void score(irs::byte_type* score_buf) override {
        ASSERT_TRUE(score_buf);
        auto& doc_id = *reinterpret_cast<irs::doc_id_t*>(score_buf);

        doc_id = document_attrs_.get<irs::document>()->value;

        if (sort_.scorer_score) {
          sort_.scorer_score(doc_id);
        }
      }

      scorer(
        const custom_sort& sort,
        const irs::sub_reader& segment_reader,
        const irs::term_reader& term_reader,
        const irs::byte_type* filter_node_attrs,
        const irs::attribute_view& document_attrs
      ): document_attrs_(document_attrs),
         filter_node_attrs_(filter_node_attrs),
         segment_reader_(segment_reader),
         sort_(sort),
         term_reader_(term_reader) {
      }

     private:
      const irs::attribute_view& document_attrs_;
      const irs::byte_type* filter_node_attrs_;
      const irs::sub_reader& segment_reader_;
      const custom_sort& sort_;
      const irs::term_reader& term_reader_;
    };

    DEFINE_FACTORY_INLINE(prepared)

    prepared(const custom_sort& sort): sort_(sort) {
    }

    virtual void collect(
      irs::byte_type* filter_attrs,
      const irs::index_reader& index,
      const irs::sort::field_collector* field,
      const irs::sort::term_collector* term
    ) const override {
      if (sort_.collectors_collect_) {
        sort_.collectors_collect_(filter_attrs, index, field, term);
      }
    }

    virtual const irs::flags& features() const override {
      return irs::flags::empty_instance();
    }

    virtual irs::sort::field_collector::ptr prepare_field_collector() const override {
      if (sort_.prepare_field_collector_) {
        return sort_.prepare_field_collector_();
      }

      return irs::memory::make_unique<collector>(sort_);
    }

    virtual scorer::ptr prepare_scorer(
        const irs::sub_reader& segment_reader,
        const irs::term_reader& term_reader,
        const irs::byte_type* filter_node_attrs,
        const irs::attribute_view& document_attrs,
        irs::boost_t boost
    ) const override {
      if (sort_.prepare_scorer) {
        return sort_.prepare_scorer(
          segment_reader, term_reader, filter_node_attrs, document_attrs
        );
      }

      return sort::scorer::make<custom_sort::prepared::scorer>(
        sort_, segment_reader, term_reader, filter_node_attrs, document_attrs
      );
    }

    virtual void prepare_score(irs::byte_type* score) const override {
      score_cast(score) = irs::type_limits<irs::type_t::doc_id_t>::invalid();
    }

    virtual irs::sort::term_collector::ptr prepare_term_collector() const override {
      if (sort_.prepare_term_collector_) {
        return sort_.prepare_term_collector_();
      }

      return irs::memory::make_unique<collector>(sort_);
    }

    virtual void add(irs::byte_type* dst, const irs::byte_type* src) const override {
      if (sort_.scorer_add) {
        sort_.scorer_add(score_cast(dst), score_cast(src));
      }
    }

    virtual bool less(const irs::byte_type* lhs, const irs::byte_type* rhs) const override {
      return sort_.scorer_less ? sort_.scorer_less(score_cast(lhs), score_cast(rhs)) : false;
    }

   private:
    const custom_sort& sort_;
  };

  std::function<void(const irs::sub_reader&, const irs::term_reader&)> collector_collect_field;
  std::function<void(const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)> collector_collect_term;
  std::function<void(irs::byte_type*, const irs::index_reader&, const irs::sort::field_collector*, const irs::sort::term_collector*)> collectors_collect_;
  std::function<irs::sort::field_collector::ptr()> prepare_field_collector_;
  std::function<scorer::ptr(const irs::sub_reader&, const irs::term_reader&, const irs::byte_type*, const irs::attribute_view&)> prepare_scorer;
  std::function<irs::sort::term_collector::ptr()> prepare_term_collector_;
  std::function<void(irs::doc_id_t&, const irs::doc_id_t&)> scorer_add;
  std::function<bool(const irs::doc_id_t&, const irs::doc_id_t&)> scorer_less;
  std::function<void(irs::doc_id_t&)> scorer_score;

  DECLARE_FACTORY();
  custom_sort(): sort(custom_sort::type()) {}
  virtual prepared::ptr prepare() const {
    return custom_sort::prepared::make<custom_sort::prepared>(*this);
  }
};

//////////////////////////////////////////////////////////////////////////////
/// @brief order by frequency, then if equal order by doc_id_t
//////////////////////////////////////////////////////////////////////////////
struct frequency_sort: public irs::sort {
  DECLARE_SORT_TYPE();

  struct score_t {
    irs::doc_id_t id;
    double value;
    bool prepared{}; // initialized to false
  };

  struct stats_t {
    size_t count{};
  };

  class prepared: public irs::sort::prepared_base<score_t, stats_t> {
   public:
    struct term_collector: public irs::sort::term_collector {
      size_t docs_count{};
      irs::attribute_view::ref<irs::term_meta>::type meta_attr;

      virtual void collect(
          const irs::sub_reader& segment,
          const irs::term_reader& field,
          const irs::attribute_view& term_attrs
      ) override {
        meta_attr = term_attrs.get<irs::term_meta>();
        docs_count += meta_attr->docs_count;
      }

      virtual void collect(const irs::bytes_ref& in) override {
        // NOOP
      }

      virtual void write(irs::data_output& out) const override {
        // NOOP
      }
    };

    class scorer: public irs::sort::scorer_base<score_t> {
     public:
      scorer(const size_t* v_docs_count, const irs::attribute_view::ref<irs::document>::type& doc_id_t):
        doc_id_t_attr(doc_id_t), docs_count(v_docs_count) {
      }

      virtual void score(irs::byte_type* score_buf) override {
        auto& buf = score_cast(score_buf);
        buf.id = doc_id_t_attr->value;

        // docs_count may be nullptr if no collector called, e.g. by range_query for bitset_doc_iterator
        if (docs_count) {
          buf.value = 1. / *docs_count;
        }
      }

     private:
      const irs::attribute_view::ref<irs::document>::type& doc_id_t_attr;
      const size_t* docs_count;
    };

    DEFINE_FACTORY_INLINE(prepared)

    prepared() { }

    virtual void collect(
      irs::byte_type* stats_buf,
      const irs::index_reader& index,
      const irs::sort::field_collector* field,
      const irs::sort::term_collector* term
    ) const override {
      auto* term_ptr = dynamic_cast<const term_collector*>(term);
      if (term_ptr) { // may be null e.g. 'all' filter
        stats_cast(stats_buf).count = term_ptr->docs_count;
        const_cast<term_collector*>(term_ptr)->docs_count = 0;
      }
    }

    virtual const irs::flags& features() const override {
      return irs::flags::empty_instance();
    }

    virtual irs::sort::field_collector::ptr prepare_field_collector() const override {
      return nullptr; // do not need to collect stats
    }

    virtual scorer::ptr prepare_scorer(
        const irs::sub_reader&,
        const irs::term_reader&,
        const irs::byte_type* stats_buf,
        const irs::attribute_view& doc_attrs,
        irs::boost_t boost
    ) const override {
      auto& doc_id_t = doc_attrs.get<irs::document>();
      auto& stats = stats_cast(stats_buf);
      const size_t* docs_count = &stats.count;
      return sort::scorer::make<frequency_sort::prepared::scorer>(docs_count, doc_id_t);
    }

    virtual void prepare_score(irs::byte_type* score_buf) const override {
      auto& score = score_cast(score_buf);
      score.id = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      score.value = std::numeric_limits<double>::infinity();
      score.prepared = true;
    }

    virtual irs::sort::term_collector::ptr prepare_term_collector() const override {
      return irs::memory::make_unique<term_collector>();
    }

    virtual void add(irs::byte_type* dst_buf, const irs::byte_type* src_buf) const override {
      auto& dst = score_cast(dst_buf);
      auto& src = score_cast(src_buf);

      ASSERT_TRUE(src.prepared);
      ASSERT_TRUE(dst.prepared);

      if (std::numeric_limits<double>::infinity()) {
        dst.id = src.id;
        dst.value = 0;
      }

      dst.value += src.value;
    }

    virtual bool less(const irs::byte_type* lhs_buf, const irs::byte_type* rhs_buf) const override {
      auto& lhs = score_cast(lhs_buf);
      auto& rhs = score_cast(rhs_buf);

      return lhs.value == rhs.value
        ? std::less<irs::doc_id_t>()(lhs.id, rhs.id)
        : std::less<double>()(lhs.value, rhs.value);
    }
  };

  DECLARE_FACTORY();
  frequency_sort(): sort(frequency_sort::type()) {}
  virtual prepared::ptr prepare() const {
    return frequency_sort::prepared::make<frequency_sort::prepared>();
  }
}; // sort::frequency_sort

NS_END // sort

class filter_test_case_base : public index_test_base {
 protected:
  typedef std::vector<irs::doc_id_t> docs_t;
  typedef std::vector<irs::cost::cost_t> costs_t;

  void check_query(
      const irs::filter& filter,
      const std::vector<irs::doc_id_t>& expected,
      const std::vector<irs::cost::cost_t>& expected_costs,
      const irs::index_reader& rdr
  ) {
    std::vector<irs::doc_id_t> result;
    std::vector<irs::cost::cost_t> result_costs;
    get_query_result(
      filter.prepare(rdr, irs::order::prepared::unordered()),
      expected, rdr, 
      result, result_costs);
    ASSERT_EQ(expected, result);
    ASSERT_EQ(expected_costs, result_costs);
  }

  void check_query(
      const irs::filter& filter,
      const std::vector<irs::doc_id_t>& expected,
      const irs::index_reader& rdr
  ) {
    std::vector<irs::doc_id_t> result;
    std::vector<irs::cost::cost_t> result_costs;
    get_query_result(
      filter.prepare(rdr, irs::order::prepared::unordered()),
      expected, rdr, 
      result, result_costs);
    ASSERT_EQ(expected, result);
  }

  void check_query(
      const irs::filter& filter,
      const irs::order& order,
      const std::vector<irs::doc_id_t>& expected,
      const irs::index_reader& rdr,
      bool score_must_be_present = true
  ) {
    typedef std::pair<irs::string_ref, irs::doc_id_t> result_item_t;
    auto prepared_order = order.prepare();
    auto prepared_filter = filter.prepare(rdr, prepared_order);
    auto score_less = [&prepared_order](
      const irs::bytes_ref& lhs, const irs::bytes_ref& rhs
    )->bool {
      return prepared_order.less(lhs.c_str(), rhs.c_str());
    };
    std::multimap<irs::bstring, irs::doc_id_t, decltype(score_less)> scored_result(score_less);

    for (const auto& sub: rdr) {
      auto docs = prepared_filter->execute(sub, prepared_order);

      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute

      const auto* score = docs->attributes().get<irs::score>().get();

      // ensure that we avoid COW for pre c++11 std::basic_string
      irs::bytes_ref score_value;

      if (score) {
        score_value = score->value();
      } else {
        ASSERT_FALSE(score_must_be_present);
        score = &irs::score::no_score();
      }

      while (docs->next()) {
        ASSERT_EQ(docs->value(), doc->value);
        score->evaluate();
        scored_result.emplace(score_value, docs->value());
      }
    }

    std::vector<irs::doc_id_t> result;

    for (auto& entry: scored_result) {
      result.emplace_back(entry.second);
    }

    ASSERT_EQ(expected, result);
  }

 private:
  void get_query_result(
      const irs::filter::prepared::ptr& q,
      const std::vector<irs::doc_id_t>& expected,
      const irs::index_reader& rdr,
      std::vector<irs::doc_id_t>& result,
      std::vector<irs::cost::cost_t>& result_costs
  ) {
    for (const auto& sub : rdr) {
      auto docs = q->execute(sub);

      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute

      auto& score = docs->attributes().get<irs::score>();

      result_costs.push_back(irs::cost::extract(docs->attributes()));

      while (docs->next()) {
        ASSERT_EQ(docs->value(), doc->value);

        if (score) {
          score->evaluate();
        }
        // put score attributes to iterator
        result.push_back(docs->value());
      }
    }
  }
};

struct empty_term_reader : irs::singleton<empty_term_reader>, irs::term_reader {
  virtual irs::seek_term_iterator::ptr iterator() const { return nullptr; }
  virtual const irs::field_meta& meta() const {
    static irs::field_meta EMPTY;
    return EMPTY;
  }

  virtual const irs::attribute_view& attributes() const NOEXCEPT {
    return irs::attribute_view::empty_instance();
  }

  // total number of terms
  virtual size_t size() const { return 0; }

  // total number of documents
  virtual uint64_t docs_count() const { return 0; }

  // less significant term
  virtual const irs::bytes_ref& (min)() const {
    return irs::bytes_ref::NIL;
  }

  // most significant term
  virtual const irs::bytes_ref& (max)() const {
    return irs::bytes_ref::NIL;
  }
}; // empty_term_reader

NS_END // tests

#endif // IRESEARCH_FILTER_TEST_CASE_BASE
