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
#include "search/column_existence_filter.hpp"
#include "search/sort.hpp"

namespace {

irs::by_column_existence make_filter(const irs::string_ref& field,
                                     bool prefix_match) {
  irs::by_column_existence filter;
  *filter.mutable_field() = field;
  filter.mutable_options()->prefix_match = prefix_match;
  return filter;
}

class column_existence_filter_test_case : public tests::filter_test_case_base {
 protected:
  void simple_sequential_mask() {
    // add segment
    {
      class mask_field : public tests::ifield {
       public:
        explicit mask_field(const std::string& name)
          : name_(name) {
        }

        bool write(data_output&) const { return true; }
        irs::string_ref name() const { return name_; }
        const irs::flags& features() const { return irs::flags::empty_instance(); }
        irs::token_stream& get_tokens() const {
          // nothing to index
          stream_.next();
          return stream_;
        }

       private:
        std::string name_;
        mutable irs::null_token_stream stream_;
      };

      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
          doc.insert(std::make_shared<mask_field>(name));
      });
      add_segment(gen);
    }

    auto rdr = open_reader();

    // 'prefix' column
    {
      const std::string column_name = "prefix";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
        ASSERT_EQ(filter_it->value(), doc->value);
      }
      ASSERT_FALSE(column_it->next());
    }

    // 'name' column
    {
      const std::string column_name = "name";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      size_t docs_count = 0;
      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
        ASSERT_EQ(filter_it->value(), doc->value);
        ++docs_count;
      }
      ASSERT_FALSE(column_it->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);
    }

    // 'seq' column
    {
      const std::string column_name = "seq";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

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

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

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

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
      }
      ASSERT_FALSE(column_it->next());
    }

    // 'duplicated' column
    {
      const std::string column_name = "duplicated";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
      }
      ASSERT_FALSE(column_it->next());
    }

    // invalid column
    {
      const std::string column_name = "invalid_column";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(0, irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      ASSERT_EQ(irs::doc_limits::eof(), filter_it->value());
      ASSERT_FALSE(filter_it->next());
    }
  }

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

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
        ASSERT_EQ(filter_it->value(), doc->value);
      }
      ASSERT_FALSE(column_it->next());
    }

    // 'name' column
    {
      const std::string column_name = "name";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      size_t docs_count = 0;
      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
        ASSERT_EQ(filter_it->value(), doc->value);
        ++docs_count;
      }
      ASSERT_FALSE(column_it->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);
    }

    // 'seq' column
    {
      const std::string column_name = "seq";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

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

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

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

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
      }
      ASSERT_FALSE(column_it->next());
    }

    // 'duplicated' column
    {
      const std::string column_name = "duplicated";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator();
      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
      }
      ASSERT_FALSE(column_it->next());
    }

    // invalid column
    {
      const std::string column_name = "invalid_column";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(0, irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      ASSERT_EQ(irs::doc_limits::eof(), filter_it->value());
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

      irs::by_column_existence filter = make_filter(column_prefix, true);

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

      auto* doc = irs::get<irs::document>(*it);
      ASSERT_TRUE(bool(doc));

      // #(foo) + #(foobar) + #(foobaz) + #(fookar)
      ASSERT_EQ(8+9+1+10, irs::cost::extract(*it));

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

      irs::by_column_existence filter = make_filter(column_prefix, true);

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

      auto* doc = irs::get<irs::document>(*it);
      ASSERT_TRUE(bool(doc));

      // #(koobar) + #(koobaz)
      ASSERT_EQ(4+2, irs::cost::extract(*it));

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

      irs::by_column_existence filter = make_filter(column_prefix, true);

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

      auto* doc = irs::get<irs::document>(*it);
      ASSERT_TRUE(bool(doc));

      // #(oobar) + #(oobaz)
      ASSERT_EQ(5+3, irs::cost::extract(*it));

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

      irs::by_column_existence filter = make_filter(column_prefix, true);

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

      auto* doc = irs::get<irs::document>(*it);
      ASSERT_TRUE(bool(doc));

      // #(collection)
      ASSERT_EQ(4, irs::cost::extract(*it));

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

      irs::by_column_existence filter = make_filter(column_prefix, true);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto filter_it = prepared->execute(segment);
      ASSERT_EQ(0, irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      ASSERT_EQ(irs::doc_limits::eof(), filter_it->value());
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

      irs::by_column_existence filter = make_filter(column_name, false);

      irs::order order;
      size_t collector_collect_field_count = 0;
      size_t collector_collect_term_count = 0;
      size_t collector_finish_count = 0;
      size_t scorer_score_count = 0;
      auto& sort = order.add<tests::sort::custom_sort>(false);

      sort.collector_collect_field = [&collector_collect_field_count](
          const irs::sub_reader&, const irs::term_reader&)->void {
        ++collector_collect_field_count;
      };
      sort.collector_collect_term = [&collector_collect_term_count](
          const irs::sub_reader&,
          const irs::term_reader&,
          const irs::attribute_provider&)->void {
        ++collector_collect_term_count;
      };
      sort.collectors_collect_ = [&collector_finish_count](
          irs::byte_type*,
          const irs::index_reader&,
          const irs::sort::field_collector*,
          const irs::sort::term_collector*)->void {
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
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_itr));

      auto* doc = irs::get<irs::document>(*filter_itr);
      ASSERT_TRUE(bool(doc));

      size_t docs_count = 0;
      auto* score = irs::get<irs::score>(*filter_itr);
      ASSERT_TRUE(bool(score));

      while (filter_itr->next()) {
        ASSERT_FALSE(!score);
        scored_result.emplace(
          irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
          filter_itr->value());
        ASSERT_TRUE(column_itr->next());
        ASSERT_EQ(filter_itr->value(), column_itr->value());
        ASSERT_EQ(filter_itr->value(), doc->value);
        ++docs_count;
      }

      ASSERT_FALSE(column_itr->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);

      ASSERT_EQ(0, collector_collect_field_count); // should not be executed (field statistics not applicable to columnstore) FIXME TODO discuss
      ASSERT_EQ(0, collector_collect_term_count); // should not be executed
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

      irs::by_column_existence filter = make_filter(column_name, true);

      irs::order order;
      size_t collector_collect_field_count = 0;
      size_t collector_collect_term_count = 0;
      size_t collector_finish_count = 0;
      size_t scorer_score_count = 0;
      auto& sort = order.add<tests::sort::custom_sort>(false);

      sort.collector_collect_field = [&collector_collect_field_count](
          const irs::sub_reader&, const irs::term_reader&)->void {
        ++collector_collect_field_count;
      };
      sort.collector_collect_term = [&collector_collect_term_count](
          const irs::sub_reader&,
          const irs::term_reader&,
          const irs::attribute_provider&)->void {
        ++collector_collect_term_count;
      };
      sort.collectors_collect_ = [&collector_finish_count](
          irs::byte_type*,
          const irs::index_reader&,
          const irs::sort::field_collector*,
          const irs::sort::term_collector*)->void {
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
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_itr));

      size_t docs_count = 0;
      auto* score = irs::get<irs::score>(*filter_itr);
      ASSERT_TRUE(bool(score));

      while (filter_itr->next()) {
        ASSERT_FALSE(!score);
        scored_result.emplace(
          irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
          filter_itr->value());
        ASSERT_TRUE(column_itr->next());
        ASSERT_EQ(filter_itr->value(), column_itr->value());
        ++docs_count;
      }

      ASSERT_FALSE(column_itr->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);

      ASSERT_EQ(0, collector_collect_field_count); // should not be executed (field statistics not applicable to columnstore) FIXME TODO discuss
      ASSERT_EQ(0, collector_collect_term_count); // should not be executed
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

      irs::by_column_existence filter = make_filter(column_name, true);

      irs::order order;
      size_t collector_collect_field_count = 0;
      size_t collector_collect_term_count = 0;
      size_t collector_finish_count = 0;
      size_t scorer_score_count = 0;
      auto& sort = order.add<tests::sort::custom_sort>(false);

      sort.collector_collect_field = [&collector_collect_field_count](
          const irs::sub_reader&, const irs::term_reader&)->void {
        ++collector_collect_field_count;
      };
      sort.collector_collect_term = [&collector_collect_term_count](
          const irs::sub_reader&,
          const irs::term_reader&,
          const irs::attribute_provider&)->void {
        ++collector_collect_term_count;
      };
      sort.collectors_collect_ = [&collector_finish_count](
          irs::byte_type*,
          const irs::index_reader&,
          const irs::sort::field_collector*,
          const irs::sort::term_collector*)->void {
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
      ASSERT_EQ(column->size() * 2, irs::cost::extract(*filter_itr)); // 2 columns matched

      size_t docs_count = 0;
      auto* score = irs::get<irs::score>(*filter_itr);
      ASSERT_TRUE(bool(score));

      while (filter_itr->next()) {
        ASSERT_FALSE(!score);
        scored_result.emplace(
          irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
          filter_itr->value());
        ASSERT_TRUE(column_itr->next());
        ASSERT_EQ(filter_itr->value(), column_itr->value());
        ++docs_count;
      }

      ASSERT_FALSE(column_itr->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);

      ASSERT_EQ(0, collector_collect_field_count); // should not be executed (field statistics not applicable to columnstore) FIXME TODO discuss
      ASSERT_EQ(0, collector_collect_term_count); // should not be executed
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

TEST_P(column_existence_filter_test_case, mask_column) {
  simple_sequential_mask();
}

TEST_P(column_existence_filter_test_case, exact_prefix_match) {
  simple_sequential_exact_match();
  simple_sequential_prefix_match();
  simple_sequential_order();
}

TEST(by_column_existence, options) {
  irs::by_column_existence_options opts;
  ASSERT_FALSE(opts.prefix_match);
}

TEST(by_column_existence, ctor) {
  irs::by_column_existence filter;
  ASSERT_EQ(irs::type<irs::by_column_existence>::id(), filter.type());
  ASSERT_EQ(irs::by_column_existence_options{}, filter.options());
  ASSERT_TRUE(filter.field().empty());
  ASSERT_EQ(irs::no_boost(), filter.boost());
}

TEST(by_column_existence, boost) {
  // FIXME
}

TEST(by_column_existence, equal) {
  ASSERT_EQ(irs::by_column_existence(), irs::by_column_existence());

  {
    irs::by_column_existence q0 = make_filter("name", false);
    irs::by_column_existence q1 = make_filter("name", false);
    ASSERT_EQ(q0, q1);
    ASSERT_EQ(q0.hash(), q1.hash());
  }

  {
    irs::by_column_existence q0 = make_filter("name", true);
    irs::by_column_existence q1 = make_filter("name", false);
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_column_existence q0 = make_filter("name", true);
    irs::by_column_existence q1 = make_filter("name1", true);
    ASSERT_NE(q0, q1);
  }
}

INSTANTIATE_TEST_CASE_P(
  column_existence_filter_test,
  column_existence_filter_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::memory_directory,
      &tests::fs_directory,
      &tests::mmap_directory
    ),
    ::testing::Values("1_0")
  ),
  tests::to_string
);

}
