//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

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

namespace ir = iresearch;

namespace tests {
namespace sort {

////////////////////////////////////////////////////////////////////////////////
/// @class boost
/// @brief boost scorer assign boost value to the particular document score
////////////////////////////////////////////////////////////////////////////////
struct boost : public iresearch::sort {
  class scorer: public iresearch::sort::scorer_base<iresearch::boost::boost_t> {
   public:
    DECLARE_FACTORY(scorer);

    scorer(
        const irs::attribute_store::ref<irs::boost>& boost,
        const irs::attribute_store& attrs
    ): attrs_(attrs), boost_(boost) {
    }

    virtual void score(irs::byte_type* score) {
      score_cast(score) = boost_ ? boost_->value : 0;
    }

   private:
    const irs::attribute_store& attrs_;
    const irs::attribute_store::ref<irs::boost>& boost_;
  }; // sort::boost::scorer

  class prepared: public iresearch::sort::prepared_base<iresearch::boost::boost_t> {
   public:
    DECLARE_FACTORY(prepared);
    prepared(bool reverse) {
      static const std::function<bool(score_t, score_t)> greater = std::greater<score_t>();
      static const std::function<bool(score_t, score_t)> less = std::less<score_t>();
      less_ = reverse ? &greater : &less;
    }

    virtual const iresearch::flags& features() const override {
      return iresearch::flags::empty_instance();
    }

    virtual collector::ptr prepare_collector() const override {
      return nullptr; // do not need to collect stats
    }

    virtual scorer::ptr prepare_scorer(
        const iresearch::sub_reader&,
        const iresearch::term_reader&,
        const irs::attribute_store& query_attrs, 
        const irs::attribute_view& doc_attrs
    ) const override {
      return boost::scorer::make<boost::scorer>(
        query_attrs.get<iresearch::boost>(), query_attrs
       );
    }

    virtual void add(score_t& dst, const score_t& src) const override {
      dst += src;
    }

    virtual bool less(const score_t& lhs, const score_t& rhs) const override {
      return (*less_)(lhs, rhs);
    }

   private:
    const iresearch::boost* boost;
    const std::function<bool(score_t, score_t)>* less_;
  }; // sort::boost::prepared

  DECLARE_SORT_TYPE();
  DECLARE_FACTORY_DEFAULT();
  typedef iresearch::boost::boost_t score_t;
  boost() : sort(boost::type()) {}
  virtual sort::prepared::ptr prepare() const {
    return boost::prepared::make<boost::prepared>(reverse());
  }
}; // sort::boost

//////////////////////////////////////////////////////////////////////////////
/// @brief expose sort functionality through overidable lambdas
//////////////////////////////////////////////////////////////////////////////
struct custom_sort: public irs::sort {
  DECLARE_SORT_TYPE();

  class prepared: public irs::sort::prepared_base<irs::doc_id_t> {
   public:
    class collector: public irs::sort::collector {
     public:
      virtual void collect(
        const irs::sub_reader& segment,
        const irs::term_reader& field,
        const irs::attribute_view& term_attrs
      ) override {
        if (sort_.collector_collect) {
          sort_.collector_collect(segment, field, term_attrs);
        }
      }

      virtual void finish(
          irs::attribute_store& filter_attrs,
          const irs::index_reader& index
      ) override {
        if (sort_.collector_finish) {
          sort_.collector_finish(filter_attrs, index);
        }
      }

      collector(const custom_sort& sort): sort_(sort) {
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
        const irs::attribute_store& filter_node_attrs,
        const irs::attribute_view& document_attrs
      ): document_attrs_(document_attrs),
         filter_node_attrs_(filter_node_attrs),
         segment_reader_(segment_reader),
         sort_(sort),
         term_reader_(term_reader) {
      }

     private:
      const irs::attribute_view& document_attrs_;
      const irs::attribute_store& filter_node_attrs_;
      const irs::sub_reader& segment_reader_;
      const custom_sort& sort_;
      const irs::term_reader& term_reader_;
    };

    DECLARE_FACTORY(prepared);

    prepared(const custom_sort& sort): sort_(sort) {
    }

    virtual const iresearch::flags& features() const override {
      return iresearch::flags::empty_instance();
    }

    virtual collector::ptr prepare_collector() const override {
      if (sort_.prepare_collector) {
        return sort_.prepare_collector();
      }

      return irs::sort::collector::make<custom_sort::prepared::collector>(sort_);
    }

    virtual scorer::ptr prepare_scorer(
        const irs::sub_reader& segment_reader,
        const irs::term_reader& term_reader,
        const irs::attribute_store& filter_node_attrs,
        const irs::attribute_view& document_attrs
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

    virtual void prepare_score(irs::doc_id_t& score) const {
      score = irs::type_limits<irs::type_t::doc_id_t>::invalid();
    }

    virtual void add(irs::doc_id_t& dst, const irs::doc_id_t& src) const override {
      if (sort_.scorer_add) {
        sort_.scorer_add(dst, src);
      }
    }

    virtual bool less(const irs::doc_id_t& lhs, const irs::doc_id_t& rhs) const override {
      return sort_.scorer_less ? sort_.scorer_less(lhs, rhs) : false;
    }

   private:
    const custom_sort& sort_;
  };

  std::function<void(const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)> collector_collect;
  std::function<void(irs::attribute_store&, const irs::index_reader&)> collector_finish;
  std::function<collector::ptr()> prepare_collector;
  std::function<scorer::ptr(const irs::sub_reader&, const irs::term_reader&, const irs::attribute_store&, const irs::attribute_view&)> prepare_scorer;
  std::function<void(irs::doc_id_t&, const irs::doc_id_t&)> scorer_add;
  std::function<bool(const irs::doc_id_t&, const irs::doc_id_t&)> scorer_less;
  std::function<void(irs::doc_id_t&)> scorer_score;

  DECLARE_FACTORY_DEFAULT();
  custom_sort(): sort(custom_sort::type()) {}
  virtual prepared::ptr prepare() const {
    return custom_sort::prepared::make<custom_sort::prepared>(*this);
  }
};

//////////////////////////////////////////////////////////////////////////////
/// @brief order by frequency, then if equal order by doc_id_t
//////////////////////////////////////////////////////////////////////////////
struct frequency_sort: public iresearch::sort {
  DECLARE_SORT_TYPE();

  struct score_t {
    iresearch::doc_id_t id;
    double value;
    bool prepared{}; // initialized to false
  };

  class prepared: public iresearch::sort::prepared_base<score_t> {
   public:
    struct count: public iresearch::basic_stored_attribute<size_t> {
      DECLARE_ATTRIBUTE_TYPE();
      DECLARE_FACTORY_DEFAULT();
      size_t value{};
    };

    class collector: public iresearch::sort::collector {
     public:
      virtual void collect(
          const iresearch::sub_reader& segment,
          const iresearch::term_reader& field,
          const irs::attribute_view& term_attrs
      ) {
        meta_attr = term_attrs.get<iresearch::term_meta>();
        docs_count += meta_attr->docs_count;
      }

      virtual void finish(
          irs::attribute_store& filter_attrs,
          const irs::index_reader& index
      ) override {
        filter_attrs.emplace<count>()->value = docs_count;
        docs_count = 0;
      }

     private:
      size_t docs_count{};
      irs::attribute_view::ref<irs::term_meta> meta_attr;
    };

    class scorer: public iresearch::sort::scorer_base<score_t> {
     public:
      scorer(const size_t* v_docs_count, const irs::attribute_view::ref<irs::document>& doc_id_t):
        doc_id_t_attr(doc_id_t), docs_count(v_docs_count) {
      }

      virtual void score(irs::byte_type* score_buf) override {
        auto& buf = score_cast(score_buf);
        buf.id = doc_id_t_attr->value;
        buf.value = 1. / *docs_count;
      }

     private:
      const irs::attribute_view::ref<irs::document>& doc_id_t_attr;
      const size_t* docs_count;
    };

    DECLARE_FACTORY(prepared);

    prepared() { }

    virtual const iresearch::flags& features() const override {
      return iresearch::flags::empty_instance();
    }

    virtual collector::ptr prepare_collector() const override {
      return iresearch::sort::collector::make<frequency_sort::prepared::collector>();
    }

    virtual scorer::ptr prepare_scorer(
        const iresearch::sub_reader&,
        const iresearch::term_reader&,
        const irs::attribute_store& query_attrs,
        const irs::attribute_view& doc_attrs
    ) const override {
      auto& doc_id_t = doc_attrs.get<iresearch::document>();
      auto& count = query_attrs.get<prepared::count>();
      const size_t* docs_count = count ? &(count->value) : nullptr;
      return sort::scorer::make<frequency_sort::prepared::scorer>(docs_count, doc_id_t);
    }

    virtual void prepare_score(score_t& score) const {
      score.id = ir::type_limits<ir::type_t::doc_id_t>::invalid();
      score.value = std::numeric_limits<double>::infinity();
      score.prepared = true;
    }

    virtual void add(score_t& dst, const score_t& src) const override {
      ASSERT_TRUE(src.prepared);
      ASSERT_TRUE(dst.prepared);

      if (std::numeric_limits<double>::infinity()) {
        dst.id = src.id;
        dst.value = 0;
      }

      dst.value += src.value;
    }

    virtual bool less(const score_t& lhs, const score_t& rhs) const override {
      return lhs.value == rhs.value
        ? std::less<iresearch::doc_id_t>()(lhs.id, rhs.id)
        : std::less<double>()(lhs.value, rhs.value);
    }
  };

  DECLARE_FACTORY_DEFAULT();
  frequency_sort(): sort(frequency_sort::type()) {}
  virtual prepared::ptr prepare() const {
    return frequency_sort::prepared::make<frequency_sort::prepared>();
  }
}; // sort::frequency_sort

} // sort

class filter_test_case_base : public index_test_base {
 protected:
  typedef std::vector<iresearch::doc_id_t> docs_t;
  typedef std::vector<ir::cost::cost_t> costs_t;

  void check_query(
      const ir::filter& filter,
      const std::vector<iresearch::doc_id_t>& expected,
      const std::vector<ir::cost::cost_t>& expected_costs, 
      const ir::index_reader& rdr) {
    std::vector<ir::doc_id_t> result;
    std::vector<ir::cost::cost_t> result_costs;
    get_query_result(
      filter.prepare(rdr, iresearch::order::prepared::unordered()),
      expected, rdr, 
      result, result_costs);
    ASSERT_EQ(expected, result);
    ASSERT_EQ(expected_costs, result_costs);
  }

  void check_query(
      const ir::filter& filter,
      const std::vector<iresearch::doc_id_t>& expected,
      const ir::index_reader& rdr) {
    std::vector<ir::doc_id_t> result;
    std::vector<ir::cost::cost_t> result_costs;
    get_query_result(
      filter.prepare(rdr, iresearch::order::prepared::unordered()),
      expected, rdr, 
      result, result_costs);
    ASSERT_EQ(expected, result);
  }

  void check_query(
      const ir::filter& filter,
      const iresearch::order& order,
      const std::vector<iresearch::doc_id_t>& expected,
      const ir::index_reader& rdr) {
    typedef std::pair<iresearch::string_ref, iresearch::doc_id_t> result_item_t;
    auto prepared_order = order.prepare();
    auto prepared_filter = filter.prepare(rdr, prepared_order);
    auto score_less = [&prepared_order](
      const iresearch::bytes_ref& lhs, const iresearch::bytes_ref& rhs
    )->bool {
      return prepared_order.less(lhs.c_str(), rhs.c_str());
    };
    std::multimap<iresearch::bstring, iresearch::doc_id_t, decltype(score_less)> scored_result(score_less);

    for (const auto& sub: rdr) {
      auto docs = prepared_filter->execute(sub, prepared_order);
      auto& score = docs->attributes().get<ir::score>();
      ASSERT_TRUE(score);

      // ensure that we avoid COW for pre c++11 std::basic_string
      const irs::bytes_ref score_value = score->value();

      while(docs->next()) {
        score->evaluate();
        scored_result.emplace(score_value, docs->value());
      }
    }

    std::vector<iresearch::doc_id_t> result;

    for (auto& entry: scored_result) {
      result.emplace_back(entry.second);
    }

    ASSERT_EQ(expected, result);
  }

 private:
  void get_query_result(
      const iresearch::filter::prepared::ptr& q,
      const std::vector<iresearch::doc_id_t>& expected,
      const ir::index_reader& rdr,
      std::vector<ir::doc_id_t>& result,
      std::vector<ir::cost::cost_t>& result_costs) {
    for (const auto& sub : rdr) {
      auto docs = q->execute(sub);
      auto& score = docs->attributes().get<ir::score>();

      result_costs.push_back(ir::cost::extract(docs->attributes()));
      for (;docs->next();) {
        if (score) {
          score->evaluate();
        }
        /* put score attributes to iterator */
        result.push_back(docs->value());
      }
    }
  }
};

struct empty_index_reader : iresearch::singleton<empty_index_reader>, iresearch::index_reader {
  virtual uint64_t live_docs_count() const { return 0; }

  virtual uint64_t docs_count(const iresearch::string_ref& field) const { return 0; }

  virtual uint64_t docs_count() const { return 0; }

  virtual reader_iterator begin() const { return reader_iterator(); }

  virtual reader_iterator end() const { return reader_iterator(); }

  virtual size_t size() const { return 0; }
}; // index_reader

struct empty_sub_reader : iresearch::singleton<empty_sub_reader>, iresearch::sub_reader {
  struct empty_docs_iterator: docs_iterator_t {
    virtual bool next() { return false; }
    virtual iresearch::doc_id_t value() const { 
      return iresearch::type_limits<iresearch::type_t::doc_id_t>::invalid();
    }
  };

  virtual iresearch::column_iterator::ptr columns() const {
    return iresearch::column_iterator::empty();
  }

  virtual iresearch::column_meta* column(const iresearch::string_ref& name) const {
    return nullptr;
  }

  virtual uint64_t live_docs_count() const { return 0; }

  virtual uint64_t docs_count(const iresearch::string_ref&) const { return 0; }

  virtual docs_iterator_t::ptr docs_iterator() const override { 
    return docs_iterator_t::make<empty_docs_iterator>(); 
  }

  virtual uint64_t docs_count() const { return 0; }

  virtual reader_iterator begin() const { return reader_iterator(); }

  virtual reader_iterator end() const { return reader_iterator(); }

  virtual const iresearch::term_reader* field(const iresearch::string_ref&) const {
    return nullptr;
  }

  virtual const irs::columnstore_reader::column_reader* column_reader(irs::field_id field) const {
    return nullptr;
  }

  virtual size_t size() const { return 0; }

  virtual iresearch::field_iterator::ptr fields() const {
    return iresearch::field_iterator::empty();
  }
}; // empty_sub_reader

struct empty_term_reader : iresearch::singleton<empty_term_reader>, iresearch::term_reader {
  virtual iresearch::seek_term_iterator::ptr iterator() const { return nullptr; }
  virtual const iresearch::field_meta& meta() const { 
    static iresearch::field_meta EMPTY;
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
  virtual const iresearch::bytes_ref& (min)() const { 
    return iresearch::bytes_ref::nil; 
  }

  // most significant term
  virtual const iresearch::bytes_ref& (max)() const { 
    return iresearch::bytes_ref::nil; 
  }
}; // empty_term_reader

} // tests

#endif // IRESEARCH_FILTER_TEST_CASE_BASE
