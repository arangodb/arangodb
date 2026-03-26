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

#include "search/column_existence_filter.hpp"

#include "filter_test_case_base.hpp"
#include "index/doc_generator.hpp"
#include "search/scorer.hpp"
#include "tests_shared.hpp"
#include "utils/lz4compression.hpp"

namespace {

irs::by_column_existence make_filter(const std::string_view& field,
                                     bool prefix_match) {
  irs::by_column_existence filter;
  *filter.mutable_field() = field;
  if (prefix_match) {
    filter.mutable_options()->acceptor = [](std::string_view,
                                            std::string_view) { return true; };
  }
  return filter;
}

class column_existence_filter_test_case : public tests::FilterTestCaseBase {
 protected:
  void simple_sequential_mask() {
    // add segment
    {
      class mask_field : public tests::ifield {
       public:
        explicit mask_field(const std::string& name) : name_(name) {}

        bool write(irs::data_output&) const { return true; }
        std::string_view name() const { return name_; }
        irs::IndexFeatures index_features() const noexcept {
          return irs::IndexFeatures::NONE;
        }
        irs::features_t features() const { return {}; }
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
        [](tests::document& doc, const std::string& name,
           const tests::json_doc_generator::json_value& /*data*/) {
          doc.insert(std::make_shared<mask_field>(name));
        });
      add_segment(gen);
    }

    auto rdr = open_reader();

    MaxMemoryCounter counter;

    // 'prefix' column
    {
      const std::string column_name = "prefix";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      auto filter_it = prepared->execute({.segment = segment});

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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // 'name' column
    {
      const std::string column_name = "name";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      auto filter_it = prepared->execute({.segment = segment});
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // 'seq' column
    {
      const std::string column_name = "seq";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      auto filter_it = prepared->execute({.segment = segment});
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // 'same' column
    {
      const std::string column_name = "same";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      auto filter_it = prepared->execute({.segment = segment});
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // 'value' column
    {
      const std::string column_name = "value";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      auto filter_it = prepared->execute({.segment = segment});
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
      }
      ASSERT_FALSE(column_it->next());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // 'duplicated' column
    {
      const std::string column_name = "duplicated";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      auto filter_it = prepared->execute({.segment = segment});
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
      }
      ASSERT_FALSE(column_it->next());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // invalid column
    {
      const std::string column_name = "invalid_column";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto filter_it = prepared->execute({.segment = segment});
      ASSERT_EQ(0, irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      ASSERT_EQ(irs::doc_limits::eof(), filter_it->value());
      ASSERT_FALSE(filter_it->next());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();
  }

  void simple_sequential_exact_match() {
    // add segment
    {
      tests::json_doc_generator gen(resource("simple_sequential.json"),
                                    &tests::generic_json_field_factory);
      add_segment(gen);
    }

    auto rdr = open_reader();

    MaxMemoryCounter counter;

    // 'prefix' column
    {
      const std::string column_name = "prefix";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      auto filter_it = prepared->execute({.segment = segment});

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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // 'name' column
    {
      const std::string column_name = "name";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      auto filter_it = prepared->execute({.segment = segment});
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // 'seq' column
    {
      const std::string column_name = "seq";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      auto filter_it = prepared->execute({.segment = segment});
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // 'same' column
    {
      const std::string column_name = "same";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      auto filter_it = prepared->execute({.segment = segment});
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // 'value' column
    {
      const std::string column_name = "value";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      auto filter_it = prepared->execute({.segment = segment});
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
      }
      ASSERT_FALSE(column_it->next());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // 'duplicated' column
    {
      const std::string column_name = "duplicated";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      auto filter_it = prepared->execute({.segment = segment});
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value());
      }
      ASSERT_FALSE(column_it->next());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // invalid column
    {
      const std::string column_name = "invalid_column";

      irs::by_column_existence filter = make_filter(column_name, false);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto filter_it = prepared->execute({.segment = segment});
      ASSERT_EQ(0, irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      ASSERT_EQ(irs::doc_limits::eof(), filter_it->value());
      ASSERT_FALSE(filter_it->next());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();
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

    MaxMemoryCounter counter;

    // looking for 'foo*' columns
    {
      const std::string column_prefix = "foo";

      irs::by_column_existence filter = make_filter(column_prefix, true);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];
      auto column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, value);

      auto it = prepared->execute({.segment = segment});

      auto* doc = irs::get<irs::document>(*it);
      ASSERT_TRUE(bool(doc));

      // #(foo) + #(foobar) + #(foobaz) + #(fookar)
      ASSERT_EQ(8 + 9 + 1 + 10, irs::cost::extract(*it));

      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("A", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("C", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("D", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("J", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("K", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("L", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("R", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("S", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("T", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("U", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("V", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("!", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("%", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_FALSE(it->next());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // looking for 'koob*' columns
    {
      const std::string column_prefix = "koob";

      irs::by_column_existence filter = make_filter(column_prefix, true);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];
      auto column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, value);

      auto it = prepared->execute({.segment = segment});

      auto* doc = irs::get<irs::document>(*it);
      ASSERT_TRUE(bool(doc));

      // #(koobar) + #(koobaz)
      ASSERT_EQ(4 + 2, irs::cost::extract(*it));

      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("B", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("U", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("V", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("X", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("Z", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_FALSE(it->next());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // looking for 'oob*' columns
    {
      const std::string column_prefix = "oob";

      irs::by_column_existence filter = make_filter(column_prefix, true);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];
      auto column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, value);

      auto it = prepared->execute({.segment = segment});

      auto* doc = irs::get<irs::document>(*it);
      ASSERT_TRUE(bool(doc));

      // #(oobar) + #(oobaz)
      ASSERT_EQ(5 + 3, irs::cost::extract(*it));

      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("Z", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("~", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("@", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("#", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("$", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_FALSE(it->next());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // looking for 'collection*' columns
    {
      const std::string column_prefix = "collection";

      irs::by_column_existence filter = make_filter(column_prefix, true);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];
      auto column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, value);

      auto it = prepared->execute({.segment = segment});

      auto* doc = irs::get<irs::document>(*it);
      ASSERT_TRUE(bool(doc));

      // #(collection)
      ASSERT_EQ(4, irs::cost::extract(*it));

      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("A", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("J", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("L", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_TRUE(it->next());
      ASSERT_EQ(it->value(), values->seek(it->value()));
      ASSERT_EQ("N", irs::to_string<std::string_view>(value->value.data()));
      ASSERT_FALSE(it->next());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // invalid prefix
    {
      const std::string column_prefix = "invalid_prefix";

      irs::by_column_existence filter = make_filter(column_prefix, true);

      auto prepared = filter.prepare({
        .index = *rdr,
        .memory = counter,
      });

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto filter_it = prepared->execute({.segment = segment});
      ASSERT_EQ(0, irs::cost::extract(*filter_it));

      auto* doc = irs::get<irs::document>(*filter_it);
      ASSERT_TRUE(bool(doc));

      ASSERT_EQ(irs::doc_limits::eof(), filter_it->value());
      ASSERT_FALSE(filter_it->next());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();
  }

  void simple_sequential_order() {
    // add segment
    {
      tests::json_doc_generator gen(resource("simple_sequential.json"),
                                    &tests::generic_json_field_factory);
      add_segment(gen);
    }

    auto rdr = open_reader();

    MaxMemoryCounter counter;

    // 'seq' column
    {
      const std::string column_name = "seq";

      irs::by_column_existence filter = make_filter(column_name, false);

      size_t collector_collect_field_count = 0;
      size_t collector_collect_term_count = 0;
      size_t collector_finish_count = 0;
      size_t scorer_score_count = 0;

      tests::sort::custom_sort sort;

      sort.collector_collect_field = [&collector_collect_field_count](
                                       const irs::SubReader&,
                                       const irs::term_reader&) -> void {
        ++collector_collect_field_count;
      };
      sort.collector_collect_term = [&collector_collect_term_count](
                                      const irs::SubReader&,
                                      const irs::term_reader&,
                                      const irs::attribute_provider&) -> void {
        ++collector_collect_term_count;
      };
      sort.collectors_collect_ = [&collector_finish_count](
                                   irs::byte_type*, const irs::FieldCollector*,
                                   const irs::TermCollector*) -> void {
        ++collector_finish_count;
      };
      sort.scorer_score = [&scorer_score_count](irs::doc_id_t doc,
                                                irs::score_t* score) -> void {
        ++scorer_score_count;
        *score = irs::score_t(doc & 0xAAAAAAAA);
      };

      auto prepared_order = irs::Scorers::Prepare(sort);
      auto prepared_filter = filter.prepare({
        .index = *rdr,
        .memory = counter,
        .scorers = prepared_order,
      });
      std::multimap<irs::score_t, irs::doc_id_t> scored_result;

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_itr = column->iterator(irs::ColumnHint::kNormal);
      auto filter_itr = prepared_filter->execute(
        {.segment = segment, .scorers = prepared_order});
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_itr));

      auto* doc = irs::get<irs::document>(*filter_itr);
      ASSERT_TRUE(bool(doc));

      size_t docs_count = 0;
      auto* score = irs::get<irs::score>(*filter_itr);
      ASSERT_TRUE(bool(score));

      while (filter_itr->next()) {
        ASSERT_FALSE(!score);
        irs::score_t score_value;
        (*score)(&score_value);
        scored_result.emplace(score_value, filter_itr->value());
        ASSERT_TRUE(column_itr->next());
        ASSERT_EQ(filter_itr->value(), column_itr->value());
        ASSERT_EQ(filter_itr->value(), doc->value);
        ++docs_count;
      }

      ASSERT_FALSE(column_itr->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);

      ASSERT_EQ(
        0, collector_collect_field_count);  // should not be executed (field
                                            // statistics not applicable to
                                            // columnstore) FIXME TODO discuss
      ASSERT_EQ(0, collector_collect_term_count);  // should not be executed
      ASSERT_EQ(1, collector_finish_count);
      ASSERT_EQ(32, scorer_score_count);

      std::vector<irs::doc_id_t> expected = {
        1, 4,  5,  16, 17, 20, 21, 2,  3,  6,  7,  18, 19, 22, 23, 8,
        9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31, 32};
      std::vector<irs::doc_id_t> actual;

      for (auto& entry : scored_result) {
        actual.emplace_back(entry.second);
      }

      ASSERT_EQ(expected, actual);
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // 'seq*' column (prefix single)
    {
      const std::string column_name = "seq";

      irs::by_column_existence filter = make_filter(column_name, true);

      size_t collector_collect_field_count = 0;
      size_t collector_collect_term_count = 0;
      size_t collector_finish_count = 0;
      size_t scorer_score_count = 0;

      tests::sort::custom_sort sort;

      sort.collector_collect_field = [&collector_collect_field_count](
                                       const irs::SubReader&,
                                       const irs::term_reader&) -> void {
        ++collector_collect_field_count;
      };
      sort.collector_collect_term = [&collector_collect_term_count](
                                      const irs::SubReader&,
                                      const irs::term_reader&,
                                      const irs::attribute_provider&) -> void {
        ++collector_collect_term_count;
      };
      sort.collectors_collect_ = [&collector_finish_count](
                                   irs::byte_type*, const irs::FieldCollector*,
                                   const irs::TermCollector*) -> void {
        ++collector_finish_count;
      };
      sort.scorer_score = [&scorer_score_count](irs::doc_id_t doc,
                                                irs::score_t* score) -> void {
        ++scorer_score_count;
        *score = irs::score_t(doc & 0xAAAAAAAA);
      };

      auto prepared_order = irs::Scorers::Prepare(sort);
      auto prepared_filter = filter.prepare({
        .index = *rdr,
        .memory = counter,
        .scorers = prepared_order,
      });
      std::multimap<irs::score_t, irs::doc_id_t> scored_result;

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name);
      ASSERT_NE(nullptr, column);
      auto column_itr = column->iterator(irs::ColumnHint::kNormal);
      auto filter_itr = prepared_filter->execute(
        {.segment = segment, .scorers = prepared_order});
      ASSERT_EQ(column->size(), irs::cost::extract(*filter_itr));

      size_t docs_count = 0;
      auto* score = irs::get<irs::score>(*filter_itr);
      ASSERT_TRUE(bool(score));

      while (filter_itr->next()) {
        ASSERT_FALSE(!score);

        irs::score_t score_value;
        (*score)(&score_value);

        scored_result.emplace(score_value, filter_itr->value());
        ASSERT_TRUE(column_itr->next());
        ASSERT_EQ(filter_itr->value(), column_itr->value());
        ++docs_count;
      }

      ASSERT_FALSE(column_itr->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);

      ASSERT_EQ(
        0, collector_collect_field_count);  // should not be executed (field
                                            // statistics not applicable to
                                            // columnstore) FIXME TODO discuss
      ASSERT_EQ(0, collector_collect_term_count);  // should not be executed
      ASSERT_EQ(1, collector_finish_count);
      ASSERT_EQ(32, scorer_score_count);

      std::vector<irs::doc_id_t> expected = {
        1, 4,  5,  16, 17, 20, 21, 2,  3,  6,  7,  18, 19, 22, 23, 8,
        9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31, 32};
      std::vector<irs::doc_id_t> actual;

      for (auto& entry : scored_result) {
        actual.emplace_back(entry.second);
      }

      ASSERT_EQ(expected, actual);
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // 's*' column (prefix multiple)
    {
      const std::string column_name = "s";
      const std::string column_name_full = "seq";

      irs::by_column_existence filter = make_filter(column_name, true);

      size_t collector_collect_field_count = 0;
      size_t collector_collect_term_count = 0;
      size_t collector_finish_count = 0;
      size_t scorer_score_count = 0;

      tests::sort::custom_sort sort;

      sort.collector_collect_field = [&collector_collect_field_count](
                                       const irs::SubReader&,
                                       const irs::term_reader&) -> void {
        ++collector_collect_field_count;
      };
      sort.collector_collect_term = [&collector_collect_term_count](
                                      const irs::SubReader&,
                                      const irs::term_reader&,
                                      const irs::attribute_provider&) -> void {
        ++collector_collect_term_count;
      };
      sort.collectors_collect_ = [&collector_finish_count](
                                   irs::byte_type*, const irs::FieldCollector*,
                                   const irs::TermCollector*) -> void {
        ++collector_finish_count;
      };
      sort.scorer_score = [&scorer_score_count](irs::doc_id_t doc,
                                                irs::score_t* score) -> void {
        ++scorer_score_count;
        *score = irs::score_t(doc & 0xAAAAAAAA);
      };

      auto prepared_order = irs::Scorers::Prepare(sort);
      auto prepared_filter = filter.prepare({
        .index = *rdr,
        .memory = counter,
        .scorers = prepared_order,
      });
      std::multimap<irs::score_t, irs::doc_id_t> scored_result;

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column = segment.column(column_name_full);
      ASSERT_NE(nullptr, column);
      auto column_itr = column->iterator(irs::ColumnHint::kNormal);
      auto filter_itr = prepared_filter->execute(
        {.segment = segment, .scorers = prepared_order});
      ASSERT_EQ(column->size() * 2,
                irs::cost::extract(*filter_itr));  // 2 columns matched

      size_t docs_count = 0;
      auto* score = irs::get<irs::score>(*filter_itr);
      ASSERT_TRUE(bool(score));

      while (filter_itr->next()) {
        ASSERT_FALSE(!score);
        irs::score_t score_value;
        (*score)(&score_value);
        scored_result.emplace(score_value, filter_itr->value());
        ASSERT_TRUE(column_itr->next());
        ASSERT_EQ(filter_itr->value(), column_itr->value());
        ++docs_count;
      }

      ASSERT_FALSE(column_itr->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);

      ASSERT_EQ(
        0, collector_collect_field_count);  // should not be executed (field
                                            // statistics not applicable to
                                            // columnstore) FIXME TODO discuss
      ASSERT_EQ(0, collector_collect_term_count);  // should not be executed
      ASSERT_EQ(1, collector_finish_count);
      ASSERT_EQ(32 * 2, scorer_score_count);  // 2 columns matched

      std::vector<irs::doc_id_t> expected = {
        1, 4,  5,  16, 17, 20, 21, 2,  3,  6,  7,  18, 19, 22, 23, 8,
        9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31, 32};
      std::vector<irs::doc_id_t> actual;

      for (auto& entry : scored_result) {
        actual.emplace_back(entry.second);
      }

      ASSERT_EQ(expected, actual);
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();
  }
};

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
  ASSERT_FALSE(opts.acceptor);
}

TEST(by_column_existence, ctor) {
  irs::by_column_existence filter;
  ASSERT_EQ(irs::type<irs::by_column_existence>::id(), filter.type());
  ASSERT_EQ(irs::by_column_existence_options{}, filter.options());
  ASSERT_TRUE(filter.field().empty());
  ASSERT_EQ(irs::kNoBoost, filter.boost());
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

class column_existence_long_filter_test_case
  : public tests::FilterTestCaseBase {};

TEST_P(column_existence_long_filter_test_case, mixed_seeks) {
  // need that many docs as in "some_docs" should be at least 4096 docs
  // and should be sparse
  irs::doc_id_t with_fields[] = {
    1,    3,    5,    7,    9,    11,   13,   15,   17,   19,   21,   23,
    25,   27,   29,   31,   33,   35,   37,   39,   41,   43,   45,   47,
    49,   51,   53,   55,   57,   59,   61,   63,   65,   67,   69,   71,
    73,   75,   77,   79,   81,   83,   85,   87,   89,   91,   93,   95,
    97,   99,   101,  103,  105,  107,  109,  111,  113,  115,  117,  119,
    121,  123,  125,  127,  129,  131,  133,  135,  137,  139,  141,  143,
    145,  147,  149,  151,  153,  155,  157,  159,  161,  163,  165,  167,
    169,  171,  173,  175,  177,  179,  181,  183,  185,  187,  189,  191,
    193,  195,  197,  199,  201,  203,  205,  207,  209,  211,  213,  215,
    217,  219,  221,  223,  225,  227,  229,  231,  233,  235,  237,  239,
    241,  243,  245,  247,  249,  251,  253,  255,  257,  259,  261,  263,
    265,  267,  269,  271,  273,  275,  277,  279,  281,  283,  285,  287,
    289,  291,  293,  295,  297,  299,  301,  303,  305,  307,  309,  311,
    313,  315,  317,  319,  321,  323,  325,  327,  329,  331,  333,  335,
    337,  339,  341,  343,  345,  347,  349,  351,  353,  355,  357,  359,
    361,  363,  365,  367,  369,  371,  373,  375,  377,  379,  381,  383,
    385,  387,  389,  391,  393,  395,  397,  399,  401,  403,  405,  407,
    409,  411,  413,  415,  417,  419,  421,  423,  425,  427,  429,  431,
    433,  435,  437,  439,  441,  443,  445,  447,  449,  451,  453,  455,
    457,  459,  461,  463,  465,  467,  469,  471,  473,  475,  477,  479,
    481,  483,  485,  487,  489,  491,  493,  495,  497,  499,  501,  503,
    505,  507,  509,  511,  526,  543,  545,  546,  547,  548,  549,  550,
    551,  552,  553,  554,  555,  556,  557,  558,  559,  561,  563,  565,
    567,  569,  571,  573,  575,  577,  579,  581,  583,  585,  587,  589,
    591,  593,  595,  597,  599,  601,  603,  605,  607,  609,  611,  613,
    615,  617,  619,  621,  623,  625,  627,  629,  631,  633,  635,  637,
    639,  641,  643,  645,  647,  649,  651,  653,  655,  657,  659,  661,
    663,  665,  667,  669,  671,  673,  675,  677,  679,  681,  682,  683,
    685,  687,  689,  691,  693,  695,  697,  699,  701,  703,  705,  707,
    709,  711,  713,  715,  717,  719,  721,  723,  725,  727,  729,  731,
    733,  735,  737,  739,  741,  743,  745,  747,  749,  751,  753,  755,
    756,  757,  759,  761,  763,  765,  767,  769,  771,  773,  775,  777,
    779,  781,  783,  785,  787,  789,  791,  793,  795,  797,  799,  801,
    803,  805,  807,  809,  811,  813,  815,  817,  819,  821,  823,  825,
    827,  829,  831,  833,  835,  837,  839,  841,  843,  845,  847,  849,
    851,  853,  855,  857,  858,  859,  861,  863,  865,  867,  869,  871,
    873,  875,  877,  879,  881,  883,  885,  887,  889,  891,  893,  895,
    897,  899,  901,  903,  905,  907,  909,  911,  913,  915,  917,  919,
    921,  923,  925,  927,  929,  931,  933,  935,  937,  939,  941,  943,
    945,  947,  949,  951,  953,  955,  957,  959,  961,  963,  965,  967,
    969,  970,  971,  973,  975,  976,  977,  978,  979,  981,  983,  985,
    987,  989,  991,  993,  995,  996,  997,  998,  999,  1001, 1003, 1005,
    1007, 1009, 1011, 1013, 1015, 1017, 1019, 1021, 1023, 1025, 1027, 1029,
    1031, 1033, 1035, 1037, 1039, 1041, 1043, 1045, 1047, 1049, 1051, 1053,
    1055, 1057, 1059, 1061, 1063, 1065, 1067, 1069, 1071, 1073, 1075, 1077,
    1079, 1081, 1083, 1085, 1087, 1089, 1091, 1093, 1095, 1097, 1099, 1101,
    1103, 1105, 1107, 1109, 1111, 1113, 1115, 1117, 1119, 1121, 1123, 1125,
    1127, 1129, 1131, 1133, 1135, 1137, 1139, 1141, 1143, 1145, 1147, 1149,
    1151, 1153, 1155, 1157, 1159, 1161, 1163, 1165, 1167, 1169, 1171, 1173,
    1175, 1177, 1179, 1181, 1183, 1185, 1187, 1189, 1191, 1193, 1195, 1197,
    1199, 1201, 1203, 1205, 1207, 1209, 1211, 1213, 1215, 1217, 1219, 1221,
    1223, 1225, 1227, 1229, 1231, 1233, 1235, 1237, 1239, 1241, 1243, 1245,
    1247, 1249, 1251, 1253, 1255, 1257, 1259, 1261, 1263, 1265, 1267, 1269,
    1271, 1273, 1275, 1277, 1279, 1281, 1283, 1285, 1287, 1289, 1291, 1293,
    1295, 1297, 1299, 1301, 1303, 1305, 1307, 1309, 1311, 1313, 1315, 1317,
    1319, 1321, 1323, 1325, 1327, 1329, 1331, 1333, 1335, 1337, 1339, 1341,
    1343, 1345, 1347, 1349, 1351, 1353, 1355, 1357, 1359, 1361, 1363, 1365,
    1367, 1369, 1371, 1373, 1375, 1377, 1379, 1381, 1383, 1385, 1387, 1389,
    1391, 1393, 1395, 1397, 1399, 1401, 1403, 1405, 1407, 1409, 1411, 1413,
    1415, 1417, 1419, 1421, 1423, 1425, 1427, 1429, 1431, 1433, 1435, 1437,
    1439, 1441, 1443, 1445, 1447, 1449, 1451, 1453, 1455, 1457, 1459, 1461,
    1463, 1465, 1467, 1469, 1471, 1473, 1475, 1477, 1479, 1481, 1483, 1485,
    1487, 1489, 1491, 1493, 1495, 1497, 1499, 1501, 1503, 1505, 1507, 1509,
    1511, 1513, 1515, 1517, 1519, 1521, 1523, 1525, 1527, 1529, 1531, 1533,
    1535, 1537, 1539, 1541, 1543, 1545, 1547, 1549, 1551, 1553, 1555, 1557,
    1559, 1561, 1563, 1565, 1567, 1569, 1571, 1573, 1575, 1577, 1579, 1581,
    1583, 1585, 1587, 1589, 1591, 1593, 1595, 1597, 1599, 1601, 1603, 1605,
    1607, 1609, 1611, 1613, 1615, 1617, 1619, 1621, 1623, 1625, 1627, 1629,
    1631, 1633, 1635, 1637, 1639, 1641, 1643, 1645, 1647, 1649, 1651, 1653,
    1655, 1657, 1659, 1661, 1663, 1665, 1667, 1669, 1671, 1673, 1675, 1677,
    1679, 1681, 1683, 1685, 1687, 1689, 1691, 1693, 1695, 1697, 1699, 1701,
    1703, 1705, 1707, 1709, 1711, 1713, 1715, 1717, 1719, 1721, 1723, 1725,
    1727, 1729, 1731, 1733, 1735, 1737, 1739, 1741, 1743, 1745, 1747, 1749,
    1751, 1753, 1755, 1757, 1759, 1761, 1763, 1765, 1767, 1769, 1771, 1773,
    1775, 1777, 1779, 1781, 1783, 1785, 1787, 1789, 1791, 1793, 1795, 1797,
    1799, 1801, 1803, 1805, 1807, 1809, 1811, 1813, 1815, 1817, 1819, 1821,
    1823, 1825, 1827, 1829, 1831, 1833, 1835, 1837, 1839, 1841, 1843, 1845,
    1847, 1849, 1851, 1853, 1855, 1857, 1859, 1861, 1863, 1865, 1867, 1869,
    1871, 1873, 1875, 1877, 1879, 1881, 1883, 1885, 1887, 1889, 1891, 1893,
    1895, 1897, 1899, 1901, 1903, 1905, 1907, 1909, 1911, 1913, 1915, 1917,
    1919, 1921, 1923, 1925, 1927, 1929, 1931, 1933, 1935, 1937, 1939, 1941,
    1943, 1945, 1947, 1949, 1951, 1953, 1955, 1957, 1959, 1961, 1963, 1965,
    1967, 1969, 1971, 1973, 1975, 1977, 1979, 1981, 1983, 1985, 1987, 1989,
    1991, 1993, 1995, 1997, 1999, 2001, 2003, 2005, 2007, 2009, 2011, 2013,
    2015, 2017, 2019, 2021, 2023, 2025, 2027, 2029, 2031, 2033, 2035, 2037,
    2039, 2041, 2043, 2045, 2047, 2049, 2051, 2053, 2055, 2057, 2059, 2061,
    2063, 2065, 2067, 2069, 2071, 2073, 2075, 2077, 2079, 2081, 2083, 2085,
    2087, 2089, 2091, 2093, 2095, 2097, 2099, 2101, 2103, 2105, 2107, 2109,
    2111, 2113, 2115, 2117, 2119, 2121, 2123, 2125, 2127, 2129, 2131, 2133,
    2135, 2137, 2139, 2141, 2143, 2145, 2147, 2149, 2151, 2153, 2155, 2157,
    2159, 2161, 2163, 2165, 2167, 2169, 2171, 2173, 2175, 2177, 2179, 2181,
    2183, 2185, 2187, 2189, 2191, 2193, 2195, 2197, 2199, 2201, 2203, 2205,
    2207, 2209, 2211, 2213, 2215, 2217, 2219, 2221, 2223, 2225, 2227, 2229,
    2231, 2233, 2235, 2237, 2239, 2241, 2243, 2245, 2247, 2249, 2251, 2253,
    2255, 2257, 2259, 2261, 2263, 2265, 2267, 2269, 2271, 2273, 2275, 2277,
    2279, 2281, 2283, 2285, 2287, 2289, 2291, 2293, 2295, 2297, 2299, 2301,
    2303, 2305, 2307, 2309, 2311, 2313, 2315, 2317, 2319, 2321, 2323, 2325,
    2327, 2329, 2331, 2333, 2335, 2337, 2339, 2341, 2343, 2345, 2347, 2349,
    2351, 2353, 2355, 2357, 2359, 2361, 2363, 2365, 2367, 2369, 2371, 2373,
    2375, 2377, 2379, 2381, 2383, 2385, 2387, 2389, 2391, 2393, 2395, 2397,
    2399, 2401, 2403, 2405, 2407, 2409, 2411, 2413, 2415, 2417, 2419, 2421,
    2423, 2425, 2427, 2429, 2431, 2433, 2435, 2437, 2439, 2441, 2443, 2445,
    2447, 2449, 2451, 2453, 2455, 2457, 2459, 2461, 2463, 2465, 2467, 2469,
    2471, 2473, 2475, 2477, 2479, 2481, 2483, 2485, 2487, 2489, 2491, 2493,
    2495, 2497, 2499, 2501, 2503, 2505, 2507, 2509, 2511, 2513, 2515, 2517,
    2519, 2521, 2523, 2525, 2527, 2529, 2531, 2533, 2535, 2537, 2539, 2541,
    2543, 2545, 2547, 2549, 2551, 2553, 2555, 2557, 2559, 2561, 2563, 2565,
    2567, 2569, 2571, 2573, 2575, 2577, 2579, 2581, 2583, 2585, 2587, 2589,
    2591, 2593, 2595, 2597, 2599, 2601, 2603, 2605, 2607, 2609, 2611, 2613,
    2615, 2617, 2619, 2621, 2623, 2625, 2627, 2629, 2631, 2633, 2635, 2637,
    2639, 2641, 2643, 2645, 2647, 2649, 2651, 2653, 2655, 2657, 2659, 2661,
    2663, 2665, 2667, 2669, 2671, 2673, 2675, 2677, 2679, 2681, 2683, 2685,
    2687, 2689, 2691, 2693, 2695, 2697, 2699, 2701, 2703, 2705, 2707, 2709,
    2711, 2713, 2715, 2717, 2719, 2721, 2723, 2725, 2727, 2729, 2731, 2733,
    2735, 2737, 2739, 2741, 2743, 2745, 2747, 2749, 2751, 2753, 2755, 2757,
    2759, 2761, 2763, 2765, 2767, 2769, 2771, 2773, 2775, 2777, 2779, 2781,
    2783, 2785, 2787, 2789, 2791, 2793, 2795, 2797, 2799, 2801, 2803, 2805,
    2807, 2809, 2811, 2813, 2815, 2817, 2819, 2821, 2823, 2825, 2827, 2829,
    2831, 2833, 2835, 2837, 2839, 2841, 2843, 2845, 2847, 2849, 2851, 2853,
    2855, 2857, 2859, 2861, 2863, 2865, 2867, 2869, 2871, 2873, 2875, 2877,
    2879, 2881, 2883, 2885, 2887, 2889, 2891, 2893, 2895, 2897, 2899, 2901,
    2903, 2905, 2907, 2909, 2911, 2913, 2915, 2917, 2919, 2921, 2923, 2925,
    2927, 2929, 2931, 2933, 2935, 2937, 2939, 2941, 2943, 2945, 2947, 2949,
    2951, 2953, 2955, 2957, 2959, 2961, 2963, 2965, 2967, 2969, 2971, 2973,
    2975, 2977, 2979, 2981, 2983, 2985, 2987, 2989, 2991, 2993, 2995, 2997,
    2999, 3001, 3003, 3005, 3007, 3009, 3011, 3013, 3015, 3017, 3019, 3021,
    3023, 3025, 3027, 3029, 3031, 3033, 3035, 3037, 3039, 3041, 3043, 3045,
    3047, 3049, 3051, 3053, 3055, 3057, 3059, 3061, 3063, 3065, 3067, 3069,
    3071, 3073, 3075, 3077, 3079, 3081, 3083, 3085, 3087, 3089, 3091, 3093,
    3095, 3097, 3099, 3101, 3103, 3105, 3107, 3109, 3111, 3113, 3115, 3117,
    3119, 3121, 3123, 3125, 3127, 3129, 3131, 3133, 3135, 3137, 3139, 3141,
    3143, 3145, 3147, 3149, 3151, 3153, 3155, 3157, 3159, 3161, 3163, 3165,
    3167, 3169, 3171, 3173, 3175, 3177, 3179, 3181, 3183, 3185, 3187, 3189,
    3191, 3193, 3195, 3197, 3199, 3201, 3203, 3205, 3207, 3209, 3211, 3213,
    3215, 3217, 3219, 3221, 3223, 3225, 3227, 3229, 3231, 3233, 3235, 3237,
    3239, 3241, 3243, 3245, 3247, 3249, 3251, 3253, 3255, 3257, 3259, 3261,
    3263, 3265, 3267, 3269, 3271, 3273, 3275, 3277, 3279, 3281, 3283, 3285,
    3287, 3289, 3291, 3293, 3295, 3297, 3299, 3301, 3303, 3305, 3307, 3309,
    3311, 3313, 3315, 3317, 3319, 3321, 3323, 3325, 3327, 3329, 3331, 3333,
    3335, 3337, 3339, 3341, 3343, 3345, 3347, 3349, 3351, 3353, 3355, 3357,
    3359, 3361, 3363, 3365, 3367, 3369, 3371, 3373, 3375, 3377, 3379, 3381,
    3383, 3385, 3387, 3389, 3391, 3393, 3395, 3397, 3399, 3401, 3403, 3405,
    3407, 3409, 3411, 3413, 3415, 3417, 3419, 3421, 3423, 3425, 3427, 3429,
    3431, 3433, 3435, 3437, 3439, 3441, 3443, 3445, 3447, 3449, 3451, 3453,
    3455, 3457, 3459, 3461, 3463, 3465, 3467, 3469, 3471, 3473, 3475, 3477,
    3479, 3481, 3483, 3485, 3487, 3489, 3491, 3493, 3495, 3497, 3499, 3501,
    3503, 3505, 3507, 3509, 3511, 3513, 3515, 3517, 3519, 3521, 3523, 3525,
    3527, 3529, 3531, 3533, 3535, 3537, 3539, 3541, 3543, 3545, 3547, 3549,
    3551, 3553, 3555, 3557, 3559, 3561, 3563, 3565, 3567, 3569, 3571, 3573,
    3575, 3577, 3579, 3581, 3583, 3585, 3587, 3589, 3591, 3593, 3595, 3597,
    3599, 3601, 3603, 3605, 3607, 3609, 3611, 3613, 3615, 3617, 3619, 3621,
    3623, 3625, 3627, 3629, 3631, 3633, 3635, 3637, 3639, 3641, 3643, 3645,
    3647, 3649, 3651, 3653, 3655, 3657, 3659, 3661, 3663, 3665, 3667, 3669,
    3671, 3673, 3675, 3677, 3679, 3681, 3683, 3685, 3687, 3689, 3691, 3693,
    3695, 3697, 3699, 3701, 3703, 3705, 3707, 3709, 3711, 3713, 3715, 3717,
    3719, 3721, 3723, 3725, 3727, 3729, 3731, 3733, 3735, 3737, 3739, 3741,
    3743, 3745, 3747, 3749, 3751, 3753, 3755, 3757, 3759, 3761, 3763, 3765,
    3767, 3769, 3771, 3773, 3775, 3777, 3779, 3781, 3783, 3785, 3787, 3789,
    3791, 3793, 3795, 3797, 3799, 3801, 3803, 3805, 3807, 3809, 3811, 3813,
    3815, 3817, 3819, 3821, 3823, 3825, 3827, 3829, 3831, 3833, 3835, 3837,
    3839, 3841, 3843, 3845, 3847, 3849, 3851, 3853, 3855, 3857, 3859, 3861,
    3863, 3865, 3867, 3869, 3871, 3873, 3875, 3877, 3879, 3881, 3883, 3885,
    3887, 3889, 3891, 3893, 3895, 3897, 3899, 3901, 3903, 3905, 3907, 3909,
    3911, 3913, 3915, 3917, 3919, 3921, 3923, 3925, 3927, 3929, 3931, 3933,
    3935, 3937, 3939, 3941, 3943, 3945, 3947, 3949, 3951, 3953, 3955, 3957,
    3959, 3961, 3963, 3965, 3967, 3969, 3971, 3973, 3975, 3977, 3979, 3981,
    3983, 3985, 3987, 3989, 3991, 3993, 3995, 3997, 3999, 4001, 4003, 4005,
    4007, 4009, 4011, 4013, 4015, 4017, 4019, 4021, 4023, 4025, 4027, 4029,
    4031, 4033, 4035, 4037, 4039, 4041, 4043, 4045, 4047, 4049, 4051, 4053,
    4055, 4057, 4059, 4061, 4063, 4065, 4067, 4069, 4071, 4073, 4075, 4077,
    4079, 4081, 4083, 4085, 4087, 4089, 4091, 4093, 4095, 4097, 4099, 4101,
    4103, 4105, 4107, 4109, 4111, 4113, 4115, 4117, 4119, 4121, 4123, 4125,
    4127, 4129, 4131, 4133, 4135, 4137, 4139, 4141, 4143, 4145, 4147, 4149,
    4151, 4153, 4155, 4157, 4159, 4161, 4163, 4165, 4167, 4169, 4171, 4173,
    4175, 4177, 4179, 4181, 4183, 4185, 4187, 4189, 4191, 4193, 4195, 4197,
    4199, 4201, 4203, 4205, 4207, 4209, 4211, 4213, 4215, 4217, 4219, 4221,
    4223, 4225, 4227, 4229, 4231, 4233, 4235, 4237, 4239, 4241, 4243, 4245,
    4247, 4249, 4251, 4253, 4255, 4257, 4259, 4261, 4263, 4265, 4267, 4269,
    4271, 4273, 4275, 4277, 4279, 4281, 4283, 4285, 4287, 4289, 4291, 4293,
    4295, 4297, 4299, 4301, 4303, 4305, 4307, 4309, 4311, 4313, 4315, 4317,
    4319, 4321, 4323, 4325, 4327, 4329, 4331, 4333, 4335, 4337, 4339, 4341,
    4343, 4345, 4347, 4349, 4351, 4353, 4355, 4357, 4359, 4361, 4363, 4365,
    4367, 4369, 4371, 4373, 4375, 4377, 4379, 4381, 4383, 4385, 4387, 4389,
    4391, 4393, 4395, 4397, 4399, 4401, 4403, 4405, 4407, 4409, 4411, 4413,
    4415, 4417, 4419, 4421, 4423, 4425, 4427, 4429, 4431, 4433, 4435, 4437,
    4439, 4441, 4443, 4445, 4447, 4449, 4451, 4453, 4455, 4457, 4459, 4461,
    4463, 4465, 4467, 4469, 4471, 4473, 4475, 4477, 4479, 4481, 4483, 4485,
    4487, 4489, 4491, 4493, 4495, 4497, 4499, 4501, 4503, 4505, 4507, 4509,
    4511, 4513, 4515, 4517, 4519, 4521, 4523, 4525, 4527, 4529, 4531, 4533,
    4535, 4537, 4539, 4541, 4543, 4545, 4547, 4549, 4551, 4553, 4555, 4557,
    4559, 4561, 4563, 4565, 4567, 4569, 4571, 4573, 4575, 4577, 4579, 4581,
    4583, 4585, 4587, 4589, 4591, 4593, 4595, 4597, 4599, 4601, 4603, 4605,
    4607, 4609, 4611, 4613, 4615, 4617, 4619, 4621, 4623, 4625, 4627, 4629,
    4631, 4633, 4635, 4637, 4639, 4641, 4643, 4645, 4647, 4649, 4651, 4653,
    4655, 4657, 4659, 4661, 4663, 4665, 4667, 4669, 4671, 4673, 4675, 4677,
    4679, 4681, 4683, 4685, 4687, 4689, 4691, 4693, 4695, 4697, 4699, 4701,
    4703, 4705, 4707, 4709, 4711, 4713, 4715, 4717, 4719, 4721, 4723, 4725,
    4727, 4729, 4731, 4733, 4735, 4737, 4739, 4741, 4743, 4745, 4747, 4749,
    4751, 4753, 4755, 4757, 4759, 4761, 4763, 4765, 4767, 4769, 4771, 4773,
    4775, 4777, 4779, 4781, 4783, 4785, 4787, 4789, 4791, 4793, 4795, 4797,
    4799, 4801, 4803, 4805, 4807, 4809, 4811, 4813, 4815, 4817, 4819, 4821,
    4823, 4825, 4827, 4829, 4831, 4833, 4835, 4837, 4839, 4841, 4843, 4845,
    4847, 4849, 4851, 4853, 4855, 4857, 4859, 4861, 4863, 4865, 4867, 4869,
    4871, 4873, 4875, 4877, 4879, 4881, 4883, 4885, 4887, 4889, 4891, 4893,
    4895, 4897, 4899, 4901, 4903, 4905, 4907, 4909, 4911, 4913, 4915, 4917,
    4919, 4921, 4923, 4925, 4927, 4929, 4931, 4933, 4935, 4937, 4939, 4941,
    4943, 4945, 4947, 4949, 4951, 4953, 4955, 4957, 4959, 4961, 4963, 4965,
    4967, 4969, 4971, 4973, 4975, 4977, 4979, 4981, 4983, 4985, 4987, 4989,
    4991, 4993, 4995, 4997, 4999, 5001, 5003, 5005, 5007, 5009, 5011, 5013,
    5015, 5017, 5019, 5021, 5023, 5025, 5027, 5029, 5031, 5033, 5035, 5037,
    5039, 5041, 5043, 5045, 5047, 5049, 5051, 5053, 5055, 5057, 5059, 5061,
    5063, 5065, 5067, 5069, 5071, 5073, 5075, 5077, 5079, 5081, 5083, 5085,
    5087, 5089, 5091, 5093, 5095, 5097, 5099, 5101, 5103, 5105, 5107, 5109,
    5111, 5113, 5115, 5117, 5119, 5121, 5123, 5125, 5127, 5129, 5131, 5133,
    5135, 5137, 5139, 5141, 5143, 5145, 5147, 5149, 5151, 5153, 5155, 5157,
    5159, 5161, 5163, 5165, 5167, 5169, 5171, 5173, 5175, 5177, 5179, 5181,
    5183, 5185, 5187, 5189, 5191, 5193, 5195, 5197, 5199, 5201, 5203, 5205,
    5207, 5209, 5211, 5213, 5215, 5217, 5219, 5221, 5223, 5225, 5227, 5229,
    5231, 5233, 5235, 5237, 5239, 5241, 5243, 5245, 5247, 5249, 5251, 5253,
    5255, 5257, 5259, 5261, 5263, 5265, 5267, 5269, 5271, 5273, 5275, 5277,
    5279, 5281, 5283, 5285, 5287, 5289, 5291, 5293, 5295, 5297, 5299, 5301,
    5303, 5305, 5307, 5309, 5311, 5313, 5315, 5317, 5319, 5321, 5323, 5325,
    5327, 5329, 5331, 5333, 5335, 5337, 5339, 5341, 5343, 5345, 5347, 5349,
    5351, 5353, 5355, 5357, 5359, 5361, 5363, 5365, 5367, 5369, 5371, 5373,
    5375, 5377, 5379, 5381, 5383, 5385, 5387, 5389, 5391, 5393, 5395, 5397,
    5399, 5401, 5403, 5405, 5407, 5409, 5411, 5413, 5415, 5417, 5419, 5421,
    5423, 5425, 5427, 5429, 5431, 5433, 5435, 5437, 5439, 5441, 5443, 5445,
    5447, 5449, 5451, 5453, 5455, 5457, 5459, 5461, 5463, 5465, 5467, 5469,
    5471, 5473, 5475, 5477, 5479, 5481, 5483, 5485, 5487, 5489, 5491, 5493,
    5495, 5497, 5499, 5501, 5503, 5505, 5507, 5509, 5511, 5513, 5515, 5517,
    5519, 5521, 5523, 5525, 5527, 5529, 5531, 5533, 5535, 5537, 5539, 5541,
    5543, 5545, 5547, 5549, 5551, 5553, 5555, 5557, 5559, 5561, 5563, 5565,
    5567, 5569, 5571, 5573, 5575, 5577, 5579, 5581, 5583, 5585, 5587, 5589,
    5591, 5593, 5595, 5597, 5599, 5601, 5603, 5605, 5607, 5609, 5611, 5613,
    5615, 5617, 5619, 5621, 5623, 5625, 5627, 5629, 5631, 5633, 5635, 5637,
    5639, 5641, 5643, 5645, 5647, 5649, 5651, 5653, 5655, 5657, 5659, 5661,
    5663, 5665, 5667, 5669, 5671, 5673, 5675, 5677, 5679, 5681, 5683, 5685,
    5687, 5689, 5691, 5693, 5695, 5697, 5699, 5701, 5703, 5705, 5707, 5709,
    5711, 5713, 5715, 5717, 5719, 5721, 5723, 5725, 5727, 5729, 5731, 5733,
    5735, 5737, 5739, 5741, 5743, 5745, 5747, 5749, 5751, 5753, 5755, 5757,
    5759, 5761, 5763, 5765, 5767, 5769, 5771, 5773, 5775, 5777, 5779, 5781,
    5783, 5785, 5787, 5789, 5791, 5793, 5795, 5797, 5799, 5801, 5803, 5805,
    5807, 5809, 5811, 5813, 5815, 5817, 5819, 5821, 5823, 5825, 5827, 5829,
    5831, 5833, 5835, 5837, 5839, 5841, 5843, 5845, 5847, 5849, 5851, 5853,
    5855, 5857, 5859, 5861, 5863, 5865, 5867, 5869, 5871, 5873, 5875, 5877,
    5879, 5881, 5883, 5885, 5887, 5889, 5891, 5893, 5895, 5897, 5899, 5901,
    5903, 5905, 5907, 5909, 5911, 5913, 5915, 5917, 5919, 5921, 5923, 5925,
    5927, 5929, 5931, 5933, 5935, 5937, 5939, 5941, 5943, 5945, 5947, 5949,
    5951, 5953, 5955, 5957, 5959, 5961, 5963, 5965, 5967, 5969, 5971, 5973,
    5975, 5977, 5979, 5981, 5983, 5985, 5987, 5989, 5991, 5993, 5995, 5997,
    5999, 6001, 6003, 6005, 6007, 6009, 6011, 6013, 6015, 6017, 6019, 6021,
    6023, 6025, 6027, 6029, 6031, 6033, 6035, 6037, 6039, 6041, 6043, 6045,
    6047, 6049, 6051, 6053, 6055, 6057, 6059, 6061, 6063, 6065, 6067, 6069,
    6071, 6073, 6075, 6077, 6079, 6081, 6083, 6085, 6087, 6089, 6091, 6093,
    6095, 6097, 6099, 6101, 6103, 6105, 6107, 6109, 6111, 6113, 6115, 6117,
    6119, 6121, 6123, 6125, 6127, 6129, 6131, 6133, 6135, 6137, 6139, 6141,
    6143, 6145, 6147, 6149, 6151, 6153, 6155, 6157, 6159, 6161, 6163, 6165,
    6167, 6169, 6171, 6173, 6175, 6177, 6179, 6181, 6183, 6185, 6187, 6189,
    6191, 6193, 6195, 6197, 6199, 6201, 6203, 6205, 6207, 6209, 6211, 6213,
    6215, 6217, 6219, 6221, 6223, 6225, 6227, 6229, 6231, 6233, 6235, 6237,
    6239, 6241, 6243, 6245, 6247, 6249, 6251, 6253, 6255, 6257, 6259, 6261,
    6263, 6265, 6267, 6269, 6271, 6273, 6275, 6277, 6279, 6281, 6283, 6285,
    6287, 6289, 6291, 6293, 6295, 6297, 6299, 6301, 6303, 6305, 6307, 6309,
    6311, 6313, 6315, 6317, 6319, 6321, 6323, 6325, 6327, 6329, 6331, 6333,
    6335, 6337, 6339, 6341, 6343, 6345, 6347, 6349, 6351, 6353, 6355, 6357,
    6359, 6361, 6363, 6365, 6367, 6369, 6371, 6373, 6375, 6377, 6379, 6381,
    6383, 6385, 6387, 6389, 6391, 6393, 6395, 6397, 6399, 6401, 6403, 6405,
    6407, 6409, 6411, 6413, 6415, 6417, 6419, 6421, 6423, 6425, 6427, 6429,
    6431, 6433, 6435, 6437, 6439, 6441, 6443, 6445, 6447, 6449, 6451, 6453,
    6455, 6457, 6459, 6461, 6463, 6465, 6467, 6469, 6471, 6473, 6475, 6477,
    6479, 6481, 6483, 6485, 6487, 6489, 6491, 6493, 6495, 6497, 6499, 6501,
    6503, 6505, 6507, 6509, 6511, 6513, 6515, 6517, 6519, 6521, 6523, 6525,
    6527, 6529, 6531, 6533, 6535, 6537, 6539, 6541, 6543, 6545, 6547, 6549,
    6551, 6553, 6555, 6557, 6559, 6561, 6563, 6565, 6567, 6569, 6571, 6573,
    6575, 6577, 6579, 6581, 6583, 6585, 6587, 6589, 6591, 6593, 6595, 6597,
    6599, 6601, 6603, 6605, 6607, 6609, 6611, 6613, 6615, 6617, 6619, 6621,
    6623, 6625, 6627, 6629, 6631, 6633, 6635, 6637, 6639, 6641, 6643, 6645,
    6647, 6649, 6651, 6653, 6655, 6657, 6659, 6661, 6663, 6665, 6667, 6669,
    6671, 6673, 6675, 6677, 6679, 6681, 6683, 6685, 6687, 6689, 6691, 6693,
    6695, 6697, 6699, 6701, 6703, 6705, 6707, 6709, 6711, 6713, 6715, 6717,
    6719, 6721, 6723, 6725, 6727, 6729, 6731, 6733, 6735, 6737, 6739, 6741,
    6743, 6745, 6747, 6749, 6751, 6753, 6755, 6757, 6759, 6761, 6763, 6765,
    6767, 6769, 6771, 6773, 6775, 6777, 6779, 6781, 6783, 6785, 6787, 6789,
    6791, 6793, 6795, 6797, 6799, 6801, 6803, 6805, 6807, 6809, 6811, 6813,
    6815, 6817, 6819, 6821, 6823, 6825, 6827, 6829, 6831, 6833, 6835, 6837,
    6839, 6841, 6843, 6845, 6847, 6849, 6851, 6853, 6855, 6857, 6859, 6861,
    6863, 6865, 6867, 6869, 6871, 6873, 6875, 6877, 6879, 6881, 6883, 6885,
    6887, 6889, 6891, 6893, 6895, 6897, 6899, 6901, 6903, 6905, 6907, 6909,
    6911, 6913, 6915, 6917, 6919, 6921, 6923, 6925, 6927, 6929, 6931, 6933,
    6935, 6937, 6939, 6941, 6943, 6945, 6947, 6949, 6951, 6953, 6955, 6957,
    6959, 6961, 6963, 6965, 6967, 6969, 6971, 6973, 6975, 6977, 6979, 6981,
    6983, 6985, 6987, 6989, 6991, 6993, 6995, 6997, 6999, 7001, 7003, 7005,
    7007, 7009, 7011, 7013, 7015, 7017, 7019, 7021, 7023, 7025, 7027, 7029,
    7031, 7033, 7035, 7037, 7039, 7041, 7043, 7045, 7047, 7049, 7051, 7053,
    7055, 7057, 7059, 7061, 7063, 7065, 7067, 7069, 7071, 7073, 7075, 7077,
    7079, 7081, 7083, 7085, 7087, 7089, 7091, 7093, 7095, 7097, 7099, 7101,
    7103, 7105, 7107, 7109, 7111, 7113, 7115, 7117, 7119, 7121, 7123, 7125,
    7127, 7129, 7131, 7133, 7135, 7137, 7139, 7141, 7143, 7145, 7147, 7149,
    7151, 7153, 7155, 7157, 7159, 7161, 7163, 7165, 7167, 7169, 7171, 7173,
    7175, 7177, 7179, 7181, 7183, 7185, 7187, 7189, 7191, 7193, 7195, 7197,
    7199, 7201, 7203, 7205, 7207, 7209, 7211, 7213, 7215, 7217, 7219, 7221,
    7223, 7225, 7227, 7229, 7231, 7233, 7235, 7237, 7239, 7241, 7243, 7245,
    7247, 7249, 7251, 7253, 7255, 7257, 7259, 7261, 7263, 7265, 7267, 7269,
    7271, 7273, 7275, 7277, 7279, 7281, 7283, 7285, 7287, 7289, 7291, 7293,
    7295, 7297, 7299, 7301, 7303, 7305, 7307, 7309, 7311, 7313, 7315, 7317,
    7319, 7321, 7323, 7325, 7327, 7329, 7331, 7333, 7335, 7337, 7339, 7341,
    7343, 7345, 7347, 7349, 7351, 7353, 7355, 7357, 7359, 7361, 7363, 7365,
    7367, 7369, 7371, 7373, 7375, 7377, 7379, 7381, 7383, 7385, 7387, 7389,
    7391, 7393, 7395, 7397, 7399, 7401, 7403, 7405, 7407, 7409, 7411, 7413,
    7415, 7417, 7419, 7421, 7423, 7425, 7427, 7429, 7431, 7433, 7435, 7437,
    7439, 7441, 7443, 7445, 7447, 7449, 7451, 7453, 7455, 7457, 7459, 7461,
    7463, 7465, 7467, 7469, 7471, 7473, 7475, 7477, 7479, 7481, 7483, 7485,
    7487, 7489, 7491, 7493, 7495, 7497, 7499, 7501, 7503, 7505, 7507, 7509,
    7511, 7513, 7515, 7517, 7519, 7521, 7523, 7525, 7527, 7529, 7531, 7533,
    7535, 7537, 7539, 7541, 7543, 7545, 7547, 7549, 7551, 7553, 7555, 7557,
    7559, 7561, 7563, 7565, 7567, 7569, 7571, 7573, 7575, 7577, 7579, 7581,
    7583, 7585, 7587, 7589, 7591, 7593, 7595, 7597, 7599, 7601, 7603, 7605,
    7607, 7609, 7611, 7613, 7615, 7617, 7619, 7621, 7623, 7625, 7627, 7629,
    7631, 7633, 7635, 7637, 7639, 7641, 7643, 7645, 7647, 7649, 7651, 7653,
    7655, 7657, 7659, 7661, 7663, 7665, 7667, 7669, 7671, 7673, 7675, 7677,
    7679, 7681, 7683, 7685, 7687, 7689, 7691, 7693, 7695, 7697, 7699, 7701,
    7703, 7705, 7707, 7709, 7711, 7713, 7715, 7717, 7719, 7721, 7723, 7725,
    7727, 7729, 7731, 7733, 7735, 7737, 7739, 7741, 7743, 7745, 7747, 7749,
    7751, 7753, 7755, 7757, 7759, 7761, 7763, 7765, 7767, 7769, 7771, 7773,
    7775, 7777, 7779, 7781, 7783, 7785, 7787, 7789, 7791, 7793, 7795, 7797,
    7799, 7801, 7803, 7805, 7807, 7809, 7811, 7813, 7815, 7817, 7819, 7821,
    7823, 7825, 7827, 7829, 7831, 7833, 7835, 7837, 7839, 7841, 7843, 7845,
    7847, 7849, 7851, 7853, 7855, 7857, 7859, 7861, 7863, 7865, 7867, 7869,
    7871, 7873, 7875, 7877, 7879, 7881, 7883, 7885, 7887, 7889, 7891, 7893,
    7895, 7897, 7899, 7901, 7903, 7905, 7907, 7909, 7911, 7913, 7915, 7917,
    7919, 7921, 7923, 7925, 7927, 7929, 7931, 7933, 7935, 7937, 7939, 7941,
    7943, 7945, 7947, 7949, 7951, 7953, 7955, 7957, 7959, 7961, 7963, 7965,
    7967, 7969, 7971, 7973, 7975, 7977, 7979, 7981, 7983, 7985, 7987, 7989,
    7991, 7993, 7995, 7997, 7999, 8001, 8003, 8005, 8007, 8009, 8011, 8013,
    8015, 8017, 8019, 8021, 8023, 8025, 8027, 8029, 8031, 8033, 8035, 8037,
    8039, 8041, 8043, 8045, 8047, 8049, 8051, 8053, 8055, 8057, 8059, 8061,
    8063, 8065, 8067, 8069, 8071, 8073, 8075, 8077, 8079, 8081, 8083, 8085,
    8087, 8089, 8091, 8093, 8095, 8097, 8099, 8101, 8103, 8105, 8107, 8109,
    8111, 8113, 8115, 8117, 8119, 8121, 8123, 8125, 8127, 8129, 8131, 8133,
    8135, 8137, 8139, 8141, 8143, 8145, 8147, 8149, 8151, 8153, 8155, 8157,
    8159, 8161, 8163, 8165, 8167, 8169, 8171, 8173, 8175, 8177, 8179, 8181,
    8183, 8185, 8187, 8189, 8191};

  class pattern_doc_generator : public tests::doc_generator_base {
   public:
    pattern_doc_generator(std::string_view all_docs_field,
                          std::string_view some_docs_field, size_t total_docs,
                          std::span<irs::doc_id_t> with_field)
      : all_docs_field_{all_docs_field},
        some_docs_field_{some_docs_field},
        with_field_{with_field},
        max_doc_id_(total_docs + 1) {}
    const tests::document* next() final {
      if (produced_docs_ >= max_doc_id_) {
        return nullptr;
      }
      ++produced_docs_;
      doc_.clear();
      doc_.insert(
        std::make_shared<tests::string_field>(all_docs_field_, "all"));
      if (span_index_ < with_field_.size() &&
          with_field_[span_index_] == produced_docs_) {
        doc_.insert(
          std::make_shared<tests::string_field>(some_docs_field_, "some"),
          false, true);
        span_index_++;
      }
      return &doc_;
    }
    void reset() final {
      produced_docs_ = 0;
      span_index_ = 0;
    }

    tests::document doc_;
    std::string all_docs_field_;
    std::string some_docs_field_;
    std::span<irs::doc_id_t> with_field_;
    size_t produced_docs_{0};
    size_t max_doc_id_;
    size_t span_index_{0};
  };

  constexpr std::string_view target{"some_docs"};

  auto max_doc_id = with_fields[std::size(with_fields) - 1];
  {
    pattern_doc_generator gen("all_docs", target, max_doc_id, with_fields);
    irs::IndexWriterOptions opts;
    opts.column_info = [target](std::string_view name) -> irs::ColumnInfo {
      // std::string to avoid ambigous comparison operator
      if (std::string(target) == name) {
        return {.compression = irs::type<irs::compression::lz4>::id()(),
                .options = {},
                .encryption = false,
                .track_prev_doc = true};
      } else {
        return {.compression = irs::type<irs::compression::lz4>::id()(),
                .options = {},
                .encryption = false,
                .track_prev_doc = false};
      }
    };
    add_segment(gen, irs::OM_CREATE, opts);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  using seek_type = std::tuple<irs::doc_id_t, irs::doc_id_t>;
  // doing a long (>512) jump to trigger the issue
  const seek_type seeks[] = {{527, 543}};
  // surrogate seek pattern check
  {
    // target, expected seek result
    irs::by_column_existence filter = make_filter(target, false);

    auto prepared = filter.prepare({
      .index = *rdr,
      .memory = counter,
    });

    ASSERT_EQ(1, rdr->size());
    auto& segment = (*rdr)[0];

    auto column = segment.column(target);
    ASSERT_NE(nullptr, column);
    auto column_it = column->iterator(irs::ColumnHint::kPrevDoc);
    auto filter_it = prepared->execute({.segment = segment});

    auto* doc = irs::get<irs::document>(*filter_it);
    ASSERT_TRUE(bool(doc));

    ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));
    for (auto& seek : seeks) {
      irs::seek(*filter_it, std::get<0>(seek));
      irs::seek(*column_it, std::get<0>(seek));
      ASSERT_EQ(filter_it->value(), column_it->value());
      ASSERT_EQ(filter_it->value(), doc->value);
      ASSERT_EQ(std::get<1>(seek), doc->value);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // seek pattern check
  {
    irs::by_column_existence filter = make_filter(target, false);

    auto prepared = filter.prepare({
      .index = *rdr,
      .memory = counter,
    });

    ASSERT_EQ(1, rdr->size());
    auto& segment = (*rdr)[0];

    auto column = segment.column(target);
    ASSERT_NE(nullptr, column);
    auto column_it = column->iterator(irs::ColumnHint::kPrevDoc);
    auto filter_it = prepared->execute({.segment = segment});

    auto* doc = irs::get<irs::document>(*filter_it);
    ASSERT_TRUE(bool(doc));

    ASSERT_EQ(column->size(), irs::cost::extract(*filter_it));
    for (auto& seek : seeks) {
      ASSERT_EQ(std::get<1>(seek), filter_it->seek(std::get<0>(seek)));
      ASSERT_EQ(std::get<1>(seek), column_it->seek(std::get<0>(seek)));
      ASSERT_EQ(filter_it->value(), column_it->value());
      ASSERT_EQ(filter_it->value(), doc->value);
      ASSERT_EQ(std::get<1>(seek), doc->value);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(column_existence_filter_test,
                         column_existence_filter_test_case,
                         ::testing::Combine(::testing::ValuesIn(kTestDirs),
                                            ::testing::Values("1_0")),
                         column_existence_filter_test_case::to_string);

INSTANTIATE_TEST_SUITE_P(column_existence_long_filter_test,
                         column_existence_long_filter_test_case,
                         ::testing::Combine(::testing::ValuesIn(kTestDirs),
                                            ::testing::Values("1_4",
                                                              "1_4simd")),
                         column_existence_long_filter_test_case::to_string);

}  // namespace
