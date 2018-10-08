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

#include "tests_shared.hpp"
#include "filter_test_case_base.hpp"
#include "store/memory_directory.hpp"
#include "formats/formats_10.hpp"
#include "store/fs_directory.hpp"
#include "search/column_existence_filter.hpp"
#include "search/sort.hpp"

NS_BEGIN(tests)

class column_existence_filter_test_case
    : public filter_test_case_base {
 protected:
  void simple_sequential_exact_match() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &tests::generic_json_field_factory);
      add_segment(gen);
    }

    auto rdr = open_reader();

    // 'prefix' column
    {
      const std::string column_name = "prefix";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(filter_it->attributes()));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
      }
      ASSERT_FALSE(column_it->next());
    }

    // 'name' column
    {
      const std::string column_name = "name";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(filter_it->attributes()));

      size_t docs_count = 0;
      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
        ++docs_count;
      }
      ASSERT_FALSE(column_it->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);
    }

    // 'seq' column
    {
      const std::string column_name = "seq";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(filter_it->attributes()));

      size_t docs_count = 0;
      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
        ++docs_count;
      }
      ASSERT_FALSE(column_it->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);
    }

    // 'same' column
    {
      const std::string column_name = "same";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(filter_it->attributes()));

      size_t docs_count = 0;
      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
        ++docs_count;
      }
      ASSERT_FALSE(column_it->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);
    }

    // 'value' column
    {
      const std::string column_name = "value";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(filter_it->attributes()));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
      }
      ASSERT_FALSE(column_it->next());
    }

    // 'duplicated' column
    {
      const std::string column_name = "duplicated";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(filter_it->attributes()));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
      }
      ASSERT_FALSE(column_it->next());
    }

    // invalid column
    {
      const std::string column_name = "invalid_column";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(0, irs::cost::extract(filter_it->attributes()));

      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), filter_it->value());
      ASSERT_FALSE(filter_it->next());
    }
  }

  void simple_sequential_prefix_match() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential_common_prefix.json"),
        &tests::generic_json_field_factory);
      add_segment(gen);
    }

    auto rdr = open_reader();

    // looking for 'foo*' columns
    {
      const std::string column_prefix = "foo";

      irs::by_column_existence filter;
      filter.prefix_match(true);
      filter.field(column_prefix);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];
      auto column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      irs::bytes_ref value;
      auto it = prepared->execute(segment);

      // #(foo) + #(foobar) + #(foobaz) + #(fookar)
      ASSERT_EQ(8+9+1+10, irs::cost::extract(it->attributes()));

      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("J", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("K", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("L", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("R", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("S", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("T", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("U", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("V", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("!", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("%", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_FALSE(it->next());
    }

    // looking for 'koob*' columns
    {
      const std::string column_prefix = "koob";

      irs::by_column_existence filter;
      filter.prefix_match(true);
      filter.field(column_prefix);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];
      auto column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      irs::bytes_ref value;
      auto it = prepared->execute(segment);

      // #(koobar) + #(koobaz)
      ASSERT_EQ(4+2, irs::cost::extract(it->attributes()));

      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("U", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("V", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("X", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("Z", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_FALSE(it->next());
    }

    // looking for 'oob*' columns
    {
      const std::string column_prefix = "oob";

      irs::by_column_existence filter;
      filter.prefix_match(true);
      filter.field(column_prefix);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];
      auto column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      irs::bytes_ref value;
      auto it = prepared->execute(segment);

      // #(oobar) + #(oobaz)
      ASSERT_EQ(5+3, irs::cost::extract(it->attributes()));

      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("Z", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("~", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("@", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("#", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("$", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_FALSE(it->next());
    }

    // looking for 'collection*' columns
    {
      const std::string column_prefix = "collection";

      irs::by_column_existence filter;
      filter.prefix_match(true);
      filter.field(column_prefix);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];
      auto column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      irs::bytes_ref value;
      auto it = prepared->execute(segment);

      // #(collection)
      ASSERT_EQ(4, irs::cost::extract(it->attributes()));

      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("J", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("L", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_TRUE(it->next());
      ASSERT_TRUE(values(it->value(), value));
      ASSERT_EQ("N", irs::to_string<irs::string_ref>(value.c_str()));
      ASSERT_FALSE(it->next());
    }

    // invalid prefix
    {
      const std::string column_prefix = "invalid_prefix";

      irs::by_column_existence filter;
      filter.prefix_match(true);
      filter.field(column_prefix);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(0, irs::cost::extract(filter_it->attributes()));

      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), filter_it->value());
      ASSERT_FALSE(filter_it->next());
    }
  }

  void simple_sequential_order() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &tests::generic_json_field_factory);
      add_segment(gen);
    }

    auto rdr = open_reader();

    // 'seq' column
    {
      const std::string column_name = "seq";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      irs::order order;
      size_t collector_collect_count = 0;
      size_t collector_finish_count = 0;
      size_t scorer_score_count = 0;
      auto& sort = order.add<sort::custom_sort>(false);

      sort.collector_collect = [&collector_collect_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void {
        ++collector_collect_count;
      };
      sort.collector_finish = [&collector_finish_count](irs::attribute_store&, const irs::index_reader&)->void {
        ++collector_finish_count;
      };
      sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void { ASSERT_TRUE(&dst); ASSERT_TRUE(&src); dst = src; };
      sort.scorer_less = [](const irs::doc_id_t& lhs, const irs::doc_id_t& rhs)->bool { return (lhs & 0xAAAAAAAAAAAAAAAA) < (rhs & 0xAAAAAAAAAAAAAAAA); };
      sort.scorer_score = [&scorer_score_count](irs::doc_id_t& score)->void { ASSERT_TRUE(&score); ++scorer_score_count; };

      auto prepared_order = order.prepare();
      auto prepared_filter = filter.prepare(*rdr, prepared_order);
      auto score_less = [&prepared_order](
        const iresearch::bytes_ref& lhs, const iresearch::bytes_ref& rhs
      )->bool {
        return prepared_order.less(lhs.c_str(), rhs.c_str());
      };
      std::multimap<iresearch::bstring, iresearch::doc_id_t, decltype(score_less)> scored_result(score_less);

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_itr = column->iterator();
      auto filter_itr = prepared_filter->execute(segment, prepared_order);
      ASSERT_EQ(column->size(), irs::cost::extract(filter_itr->attributes()));

      size_t docs_count = 0;
      auto& score = filter_itr->attributes().get<irs::score>();
      ASSERT_TRUE(bool(score));

      // ensure that we avoid COW for pre c++11 std::basic_string
      const irs::bytes_ref score_value = score->value();

      while (filter_itr->next()) {
        score->evaluate();
        ASSERT_FALSE(!score);
        scored_result.emplace(score_value, filter_itr->value());
        ASSERT_TRUE(column_itr->next());
        ASSERT_EQ(filter_itr->value(), column_itr->value());
        ++docs_count;
      }

      ASSERT_FALSE(column_itr->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);

      ASSERT_EQ(0, collector_collect_count); // should not be executed
      ASSERT_EQ(1, collector_finish_count);
      ASSERT_EQ(32, scorer_score_count);

      std::vector<irs::doc_id_t> expected = { 1, 4, 5, 16, 17, 20, 21, 2, 3, 6, 7, 18, 19, 22, 23, 8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31, 32 };
      std::vector<irs::doc_id_t> actual;

      for (auto& entry: scored_result) {
        actual.emplace_back(entry.second);
      }

      ASSERT_EQ(expected, actual);
    }

    // 'seq*' column (prefix single)
    {
      const std::string column_name = "seq";

      irs::by_column_existence filter;
      filter.prefix_match(true);
      filter.field(column_name);

      irs::order order;
      size_t collector_collect_count = 0;
      size_t collector_finish_count = 0;
      size_t scorer_score_count = 0;
      auto& sort = order.add<sort::custom_sort>(false);

      sort.collector_collect = [&collector_collect_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void {
        ++collector_collect_count;
      };
      sort.collector_finish = [&collector_finish_count](irs::attribute_store&, const irs::index_reader&)->void {
        ++collector_finish_count;
      };
      sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void { ASSERT_TRUE(&dst); ASSERT_TRUE(&src); dst = src; };
      sort.scorer_less = [](const irs::doc_id_t& lhs, const irs::doc_id_t& rhs)->bool { return (lhs & 0xAAAAAAAAAAAAAAAA) < (rhs & 0xAAAAAAAAAAAAAAAA); };
      sort.scorer_score = [&scorer_score_count](irs::doc_id_t& score)->void { ASSERT_TRUE(&score); ++scorer_score_count; };

      auto prepared_order = order.prepare();
      auto prepared_filter = filter.prepare(*rdr, prepared_order);
      auto score_less = [&prepared_order](
        const iresearch::bytes_ref& lhs, const iresearch::bytes_ref& rhs
      )->bool {
        return prepared_order.less(lhs.c_str(), rhs.c_str());
      };
      std::multimap<iresearch::bstring, iresearch::doc_id_t, decltype(score_less)> scored_result(score_less);

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_itr = column->iterator();
      auto filter_itr = prepared_filter->execute(segment, prepared_order);
      ASSERT_EQ(column->size(), irs::cost::extract(filter_itr->attributes()));

      size_t docs_count = 0;
      auto& score = filter_itr->attributes().get<irs::score>();
      ASSERT_TRUE(bool(score));

      // ensure that we avoid COW for pre c++11 std::basic_string
      const irs::bytes_ref score_value = score->value();

      while (filter_itr->next()) {
        score->evaluate();
        ASSERT_FALSE(!score);
        scored_result.emplace(score_value, filter_itr->value());
        ASSERT_TRUE(column_itr->next());
        ASSERT_EQ(filter_itr->value(), column_itr->value());
        ++docs_count;
      }

      ASSERT_FALSE(column_itr->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);

      ASSERT_EQ(0, collector_collect_count); // should not be executed
      ASSERT_EQ(1, collector_finish_count);
      ASSERT_EQ(32, scorer_score_count);

      std::vector<irs::doc_id_t> expected = { 1, 4, 5, 16, 17, 20, 21, 2, 3, 6, 7, 18, 19, 22, 23, 8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31, 32 };
      std::vector<irs::doc_id_t> actual;

      for (auto& entry: scored_result) {
        actual.emplace_back(entry.second);
      }

      ASSERT_EQ(expected, actual);
    }

    // 's*' column (prefix multiple)
    {
      const std::string column_name = "s";
      const std::string column_name_full = "seq";

      irs::by_column_existence filter;
      filter.prefix_match(true);
      filter.field(column_name);

      irs::order order;
      size_t collector_collect_count = 0;
      size_t collector_finish_count = 0;
      size_t scorer_score_count = 0;
      auto& sort = order.add<sort::custom_sort>(false);

      sort.collector_collect = [&collector_collect_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void {
        ++collector_collect_count;
      };
      sort.collector_finish = [&collector_finish_count](irs::attribute_store&, const irs::index_reader&)->void {
        ++collector_finish_count;
      };
      sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void { ASSERT_TRUE(&dst); ASSERT_TRUE(&src); dst = src; };
      sort.scorer_less = [](const irs::doc_id_t& lhs, const irs::doc_id_t& rhs)->bool { return (lhs & 0xAAAAAAAAAAAAAAAA) < (rhs & 0xAAAAAAAAAAAAAAAA); };
      sort.scorer_score = [&scorer_score_count](irs::doc_id_t& score)->void { ASSERT_TRUE(&score); ++scorer_score_count; };

      auto prepared_order = order.prepare();
      auto prepared_filter = filter.prepare(*rdr, prepared_order);
      auto score_less = [&prepared_order](
        const iresearch::bytes_ref& lhs, const iresearch::bytes_ref& rhs
      )->bool {
        return prepared_order.less(lhs.c_str(), rhs.c_str());
      };
      std::multimap<iresearch::bstring, iresearch::doc_id_t, decltype(score_less)> scored_result(score_less);

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name_full);
      ASSERT_NE(nullptr, column);
      auto column_itr = column->iterator();
      auto filter_itr = prepared_filter->execute(segment, prepared_order);
      ASSERT_EQ(column->size() * 2, irs::cost::extract(filter_itr->attributes())); // 2 columns matched

      size_t docs_count = 0;
      auto& score = filter_itr->attributes().get<irs::score>();
      ASSERT_TRUE(bool(score));

      // ensure that we avoid COW for pre c++11 std::basic_string
      const irs::bytes_ref score_value = score->value();

      while (filter_itr->next()) {
        score->evaluate();
        ASSERT_FALSE(!score);
        scored_result.emplace(score_value, filter_itr->value());
        ASSERT_TRUE(column_itr->next());
        ASSERT_EQ(filter_itr->value(), column_itr->value());
        ++docs_count;
      }

      ASSERT_FALSE(column_itr->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);

      ASSERT_EQ(0, collector_collect_count); // should not be executed
      ASSERT_EQ(1, collector_finish_count);
      ASSERT_EQ(32 * 2, scorer_score_count); // 2 columns matched

      std::vector<irs::doc_id_t> expected = { 1, 4, 5, 16, 17, 20, 21, 2, 3, 6, 7, 18, 19, 22, 23, 8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31, 32 };
      std::vector<irs::doc_id_t> actual;

      for (auto& entry: scored_result) {
        actual.emplace_back(entry.second);
      }

      ASSERT_EQ(expected, actual);
    }
  }
}; // column_existence_filter_test_case

NS_END

TEST(by_column_existence, ctor) {
  irs::by_column_existence filter;
  ASSERT_EQ(irs::by_column_existence::type(), filter.type());
  ASSERT_FALSE(filter.prefix_match());
  ASSERT_TRUE(filter.field().empty());
  ASSERT_EQ(irs::boost::no_boost(), filter.boost());
}

TEST(by_column_existence, boost) {
  // FIXME
}

TEST(by_column_existence, equal) {
  ASSERT_EQ(irs::by_column_existence(), irs::by_column_existence());

  {
    irs::by_column_existence q0;
    q0.field("name");

    irs::by_column_existence q1;
    q1.field("name");
    ASSERT_EQ(q0, q1);
    ASSERT_EQ(q0.hash(), q1.hash());
  }

  {
    irs::by_column_existence q0;
    q0.field("name");
    q0.prefix_match(true);

    irs::by_column_existence q1;
    q1.field("name");
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_column_existence q0;
    q0.field("name");
    q0.prefix_match(true);

    irs::by_column_existence q1;
    q1.field("name1");
    q1.prefix_match(true);
    ASSERT_NE(q0, q1);
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_column_existence_filter_test_case
    : public tests::column_existence_filter_test_case {
protected:
  virtual irs::directory* get_directory() override {
    return new irs::memory_directory();
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
};

TEST_F(memory_column_existence_filter_test_case, exact_prefix_match) {
  simple_sequential_exact_match();
  simple_sequential_prefix_match();
}

// ----------------------------------------------------------------------------
// --SECTION--                               fs_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class fs_column_existence_test_case
    : public tests::column_existence_filter_test_case {
protected:
  virtual irs::directory* get_directory() override {
    auto dir = test_dir();

    dir /= "index";

    return new irs::fs_directory(dir.utf8());
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
};

TEST_F(fs_column_existence_test_case, exact_prefix_match) {
  simple_sequential_exact_match();
  simple_sequential_prefix_match();
  simple_sequential_order();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
