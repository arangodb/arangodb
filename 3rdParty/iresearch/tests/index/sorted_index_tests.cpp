////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
#include "iql/query_builder.hpp"
#include "utils/index_utils.hpp"

#include "index_tests.hpp"

namespace {

class sorted_europarl_doc_template : public tests::templates::europarl_doc_template {
 public:
  explicit sorted_europarl_doc_template(const std::string& field)
    : field_(field) {
  }

  virtual void init() override {
    tests::templates::europarl_doc_template::init();
    auto fields = indexed.find(field_);

    if (!fields.empty()) {
      sorted = fields[0];
    }
  }

 private:
  std::string field_; // sorting field
}; // sorted_europal_doc_template

struct string_comparer : irs::comparer {
  virtual bool less(const irs::bytes_ref& lhs, const irs::bytes_ref& rhs) const {
    if (lhs.null() && rhs.null()) {
      return false;
    } else if (rhs.null()) {
      return true;
    } else if (lhs.null()) {
      return false;
    }

    const auto lhs_value = irs::to_string<irs::bytes_ref>(lhs.c_str());
    const auto rhs_value = irs::to_string<irs::bytes_ref>(rhs.c_str());

    return lhs_value > rhs_value;
  }
};

struct long_comparer : irs::comparer {
  virtual bool less(const irs::bytes_ref& lhs, const irs::bytes_ref& rhs) const {
    if (lhs.null() && rhs.null()) {
      return false;
    } else if (rhs.null()) {
      return false;
    } else if (lhs.null()) {
      return true;
    }

    auto* plhs = lhs.c_str();
    auto* prhs = rhs.c_str();

    return irs::zig_zag_decode64(irs::vread<uint64_t>(plhs)) < irs::zig_zag_decode64(irs::vread<uint64_t>(prhs));
  }
};

class sorted_index_test_case : public tests::index_test_base {
 protected:
  void assert_index(size_t skip = 0, irs::automaton_table_matcher* matcher = nullptr) const {
    index_test_base::assert_index(irs::flags(), skip, matcher);
    index_test_base::assert_index(irs::flags{ irs::type<irs::document>::get() }, skip, matcher);
    index_test_base::assert_index(irs::flags{ irs::type<irs::document>::get(), irs::type<irs::frequency>::get() }, skip, matcher);
    index_test_base::assert_index(irs::flags{ irs::type<irs::document>::get(), irs::type<irs::frequency>::get(), irs::type<irs::position>::get() }, skip, matcher);
    index_test_base::assert_index(irs::flags{ irs::type<irs::document>::get(), irs::type<irs::frequency>::get(), irs::type<irs::position>::get(), irs::type<irs::offset>::get() }, skip, matcher);
    index_test_base::assert_index(irs::flags{ irs::type<irs::document>::get(), irs::type<irs::frequency>::get(), irs::type<irs::position>::get(), irs::type<irs::payload>::get() }, skip, matcher);
    index_test_base::assert_index(irs::flags{ irs::type<irs::document>::get(), irs::type<irs::frequency>::get(), irs::type<irs::position>::get(), irs::type<irs::payload>::get(), irs::type<irs::offset>::get() }, skip, matcher);
  }
};

TEST_P(sorted_index_test_case, simple_sequential) {
  const irs::string_ref sorted_column = "name";

  // build index
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [&sorted_column] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        auto field = std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        );

        doc.insert(field);

        if (name == sorted_column) {
          doc.sorted = field;
        }
      } else if (data.is_number()) {
        auto field = std::make_shared<tests::long_field>();
        field->name(name);
        field->value(data.ui);

        doc.insert(field);
      }
  });

  string_comparer less;

  irs::index_writer::init_options opts;
  opts.comparator = &less;
  add_segment(gen, irs::OM_CREATE, opts); // add segment

  // check inverted index
  assert_index();

  // check columns
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());

    auto& segment = reader[0];
    ASSERT_EQ(segment.docs_count(), segment.live_docs_count());
    ASSERT_NE(nullptr, segment.sort());

    // check sorted column
    {
      std::vector<irs::bstring> column_payload;
      gen.reset();

      while (auto* doc = gen.next()) {
        auto* field = doc->stored.get(sorted_column);
        ASSERT_NE(nullptr, field);

        column_payload.emplace_back();
        irs::bytes_output out(column_payload.back());
        field->write(out);
      }

      ASSERT_EQ(column_payload.size(), segment.docs_count());

      std::sort(
        column_payload.begin(), column_payload.end(),
        [&less](const irs::bstring& lhs, const irs::bstring& rhs) {
          return less(lhs, rhs);
      });

      auto& sorted_column = *segment.sort();
      ASSERT_EQ(segment.docs_count(), sorted_column.size());

      auto sorted_column_it = sorted_column.iterator();
      ASSERT_NE(nullptr, sorted_column_it);

      auto* payload = irs::get<irs::payload>(*sorted_column_it);
      ASSERT_TRUE(payload);

      auto expected_doc = irs::doc_limits::min();
      for (auto& expected_payload : column_payload) {
        ASSERT_TRUE(sorted_column_it->next());
        ASSERT_EQ(expected_doc, sorted_column_it->value());
        ASSERT_EQ(expected_payload, payload->value);
        ++expected_doc;
      }
      ASSERT_FALSE(sorted_column_it->next());
    }

    // check regular columns
    std::vector<irs::string_ref> column_names {
      "seq", "value", "duplicated", "prefix"
    };

    for (auto& column_name : column_names) {
      struct doc {
        irs::doc_id_t id{ irs::doc_limits::eof() };
        irs::bstring order;
        irs::bstring value;
      };

      std::vector<doc> column_docs;
      column_docs.reserve(segment.docs_count());

      gen.reset();
      irs::doc_id_t id{ irs::doc_limits::min() };
      while (auto* doc = gen.next()) {
        auto* sorted = doc->stored.get(sorted_column);
        ASSERT_NE(nullptr, sorted);

        column_docs.emplace_back();

        auto* column = doc->stored.get(column_name);

        auto& value = column_docs.back();
        irs::bytes_output order_out(value.order);
        sorted->write(order_out);

        if (column) {
          value.id = id++;
        irs::bytes_output value_out(value.value);
          column->write(value_out);
        }
      }

      std::sort(
        column_docs.begin(), column_docs.end(),
        [&less](const doc& lhs, const doc& rhs) {
          return less(lhs.order, rhs.order);
      });

      auto* column_meta = segment.column(column_name);
      ASSERT_NE(nullptr, column_meta);
      auto* column = segment.column_reader(column_meta->id);
      ASSERT_NE(nullptr, column);

      ASSERT_EQ(id-1, column->size());

      auto column_it = column->iterator();
      ASSERT_NE(nullptr, column_it);

      auto* payload = irs::get<irs::payload>(*column_it);
      ASSERT_TRUE(payload);

      irs::doc_id_t doc = 0;
      for (auto& expected_value: column_docs) {
        ++doc;

        if (irs::doc_limits::eof(expected_value.id)) {
          // skip empty values
          continue;
        }

        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(doc, column_it->value());
        EXPECT_EQ(expected_value.value, payload->value);
      }
      ASSERT_FALSE(column_it->next());
    }
  }
}

//FIXME check norms

TEST_P(sorted_index_test_case, simple_sequential_consolidate) {
  const irs::string_ref sorted_column = "name";

  // build index
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [&sorted_column] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        auto field = std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        );

        doc.insert(field);

        if (name == sorted_column) {
          doc.sorted = field;
        }
      } else if (data.is_number()) {
        auto field = std::make_shared<tests::long_field>();
        field->name(name);
        field->value(data.i64);

        doc.insert(field);
      }
  });

  std::vector<std::pair<size_t, size_t>> segment_offsets {
    { 0, 15 }, { 15, 17 }
  };

  string_comparer less;

  irs::index_writer::init_options opts;
  opts.comparator = &less;
  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_EQ(&less, writer->comparator());

  // add segment 0
  {
    auto& offset = segment_offsets[0];
    tests::limiting_doc_generator segment_gen(gen, offset.first, offset.second);
    add_segment(*writer, segment_gen); // add segment
  }

  // add segment 1
  add_segment(*writer, gen); // add segment

  // check inverted index
  assert_index();

  // check columns
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());

    // check segments
    size_t i = 0;
    for (auto& segment : *reader) {
      auto& offset = segment_offsets[i++];
      tests::limiting_doc_generator segment_gen(gen, offset.first, offset.second);

      ASSERT_EQ(offset.second, segment.docs_count());
      ASSERT_EQ(segment.docs_count(), segment.live_docs_count());
      ASSERT_NE(nullptr, segment.sort());

      // check sorted column
      {
        segment_gen.reset();
        std::vector<irs::bstring> column_payload;

        while (auto* doc = segment_gen.next()) {
          auto* field = doc->stored.get(sorted_column);
          ASSERT_NE(nullptr, field);

          column_payload.emplace_back();
          irs::bytes_output out(column_payload.back());
          field->write(out);
        }

        ASSERT_EQ(column_payload.size(), segment.docs_count());

        std::sort(
          column_payload.begin(), column_payload.end(),
          [&less](const irs::bstring& lhs, const irs::bstring& rhs) {
            return less(lhs, rhs);
        });

        auto& sorted_column = *segment.sort();
        ASSERT_EQ(segment.docs_count(), sorted_column.size());

        auto sorted_column_it = sorted_column.iterator();
        ASSERT_NE(nullptr, sorted_column_it);

        auto* payload = irs::get<irs::payload>(*sorted_column_it);
        ASSERT_TRUE(payload);

        auto expected_doc = irs::doc_limits::min();
        for (auto& expected_payload : column_payload) {
          ASSERT_TRUE(sorted_column_it->next());
          ASSERT_EQ(expected_doc, sorted_column_it->value());
          ASSERT_EQ(expected_payload, payload->value);
          ++expected_doc;
        }
        ASSERT_FALSE(sorted_column_it->next());
      }

      // check regular columns
      std::vector<irs::string_ref> column_names {
        "seq", "value", "duplicated", "prefix"
      };

      for (auto& column_name : column_names) {
        struct doc {
          irs::doc_id_t id{ irs::doc_limits::eof() };
          irs::bstring order;
          irs::bstring value;
        };

        std::vector<doc> column_docs;
        column_docs.reserve(segment.docs_count());

        segment_gen.reset();
        irs::doc_id_t id{ irs::doc_limits::min() };
        while (auto* doc = segment_gen.next()) {
          auto* sorted = doc->stored.get(sorted_column);
          ASSERT_NE(nullptr, sorted);

          column_docs.emplace_back();

          auto* column = doc->stored.get(column_name);

          auto& value = column_docs.back();
          irs::bytes_output order_out(value.order);
          sorted->write(order_out);

          if (column) {
            value.id = id++;
            irs::bytes_output value_out(value.value);
            column->write(value_out);
          }
        }

        std::sort(
          column_docs.begin(), column_docs.end(),
          [&less](const doc& lhs, const doc& rhs) {
            return less(lhs.order, rhs.order);
        });

        auto* column_meta = segment.column(column_name);
        ASSERT_NE(nullptr, column_meta);
        auto* column = segment.column_reader(column_meta->id);
        ASSERT_NE(nullptr, column);

        ASSERT_EQ(id-1, column->size());

        auto column_it = column->iterator();
        ASSERT_NE(nullptr, column_it);

        auto* payload = irs::get<irs::payload>(*column_it);
        ASSERT_TRUE(payload);

        irs::doc_id_t doc = 0;
        for (auto& expected_value: column_docs) {
          ++doc;

          if (irs::doc_limits::eof(expected_value.id)) {
            // skip empty values
            continue;
          }

          ASSERT_TRUE(column_it->next());
          ASSERT_EQ(doc, column_it->value());
          EXPECT_EQ(expected_value.value, payload->value);
        }
        ASSERT_FALSE(column_it->next());
      }
    }
  }

  // consolidate segments
  {
    irs::index_utils::consolidate_count consolidate_all;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(consolidate_all)));
    writer->commit();

    // simulate consolidation
    index().clear();
    index().emplace_back();
    auto& segment = index().back();

    gen.reset();
    while (auto* doc = gen.next()) {
      segment.add(
        doc->indexed.begin(),
        doc->indexed.end(),
        doc->sorted
      );
    }

    ASSERT_NE(nullptr, writer->comparator());
    segment.sort(*writer->comparator());
  }

  assert_index();

  // check columns in consolidated segment
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());

    // check segments
    auto& segment = reader[0];

    ASSERT_EQ(segment_offsets[0].second + segment_offsets[1].second, segment.docs_count());
    ASSERT_EQ(segment.docs_count(), segment.live_docs_count());
    ASSERT_NE(nullptr, segment.sort());

    // check sorted column
    {
      gen.reset();
      std::vector<irs::bstring> column_payload;

      while (auto* doc = gen.next()) {
        auto* field = doc->stored.get(sorted_column);
        ASSERT_NE(nullptr, field);

        column_payload.emplace_back();
        irs::bytes_output out(column_payload.back());
        field->write(out);
      }

      ASSERT_EQ(column_payload.size(), segment.docs_count());

      std::sort(
        column_payload.begin(), column_payload.end(),
        [&less](const irs::bstring& lhs, const irs::bstring& rhs) {
          return less(lhs, rhs);
      });

      auto& sorted_column = *segment.sort();
      ASSERT_EQ(segment.docs_count(), sorted_column.size());

      auto sorted_column_it = sorted_column.iterator();
      ASSERT_NE(nullptr, sorted_column_it);

      auto* payload = irs::get<irs::payload>(*sorted_column_it);
      ASSERT_TRUE(payload);

      auto expected_doc = irs::doc_limits::min();
      for (auto& expected_payload : column_payload) {
        ASSERT_TRUE(sorted_column_it->next());
        ASSERT_EQ(expected_doc, sorted_column_it->value());
        ASSERT_EQ(expected_payload, payload->value);
        ++expected_doc;
      }
      ASSERT_FALSE(sorted_column_it->next());
    }

    // check regular columns
    std::vector<irs::string_ref> column_names {
      "seq", "value", "duplicated", "prefix"
    };

    for (auto& column_name : column_names) {
      struct doc {
        irs::doc_id_t id{ irs::doc_limits::eof() };
        irs::bstring order;
        irs::bstring value;
      };

      std::vector<doc> column_docs;
      column_docs.reserve(segment.docs_count());

      gen.reset();
      irs::doc_id_t id{ irs::doc_limits::min() };
      while (auto* doc = gen.next()) {
        auto* sorted = doc->stored.get(sorted_column);
        ASSERT_NE(nullptr, sorted);

        column_docs.emplace_back();

        auto* column = doc->stored.get(column_name);

        auto& value = column_docs.back();
        irs::bytes_output order_out(value.order);
        sorted->write(order_out);

        if (column) {
          value.id = id++;
          irs::bytes_output value_out(value.value);
          column->write(value_out);
        }
      }

      std::sort(
        column_docs.begin(), column_docs.end(),
        [&less](const doc& lhs, const doc& rhs) {
          return less(lhs.order, rhs.order);
      });

      auto* column_meta = segment.column(column_name);
      ASSERT_NE(nullptr, column_meta);
      auto* column = segment.column_reader(column_meta->id);
      ASSERT_NE(nullptr, column);

      ASSERT_EQ(id-1, column->size());

      auto column_it = column->iterator();
      ASSERT_NE(nullptr, column_it);

      auto* payload = irs::get<irs::payload>(*column_it);
      ASSERT_TRUE(payload);

      irs::doc_id_t doc = 0;
      for (auto& expected_value: column_docs) {
        ++doc;

        if (irs::doc_limits::eof(expected_value.id)) {
          // skip empty values
          continue;
        }

        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(doc, column_it->value());
        EXPECT_EQ(expected_value.value, payload->value);
      }
      ASSERT_FALSE(column_it->next());
    }
  }
}

TEST_P(sorted_index_test_case, simple_sequential_already_sorted) {
  const irs::string_ref sorted_column = "seq";

  // build index
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [&sorted_column] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        auto field = std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        );

        doc.insert(field);

      } else if (data.is_number()) {
        auto field = std::make_shared<tests::long_field>();
        field->name(name);
        field->value(data.i64);

        doc.insert(field);

        if (name == sorted_column) {
          doc.sorted = field;
        }
      }
  });

  long_comparer less;
  irs::index_writer::init_options opts;
  opts.comparator = &less;
  add_segment(gen, irs::OM_CREATE, opts); // add segment

  assert_index();

  // check columns
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());

    auto& segment = reader[0];
    ASSERT_EQ(segment.docs_count(), segment.live_docs_count());
    ASSERT_NE(nullptr, segment.sort());

    // check sorted column
    {
      std::vector<irs::bstring> column_payload;
      gen.reset();

      while (auto* doc = gen.next()) {
        auto* field = doc->stored.get(sorted_column);
        ASSERT_NE(nullptr, field);

        column_payload.emplace_back();
        irs::bytes_output out(column_payload.back());
        field->write(out);
      }

      ASSERT_EQ(column_payload.size(), segment.docs_count());

      std::sort(
        column_payload.begin(), column_payload.end(),
        [&less](const irs::bstring& lhs, const irs::bstring& rhs) {
          return less(lhs, rhs);
      });

      auto& sorted_column = *segment.sort();
      ASSERT_EQ(segment.docs_count(), sorted_column.size());

      auto sorted_column_it = sorted_column.iterator();
      ASSERT_NE(nullptr, sorted_column_it);

      auto* payload = irs::get<irs::payload>(*sorted_column_it);
      ASSERT_TRUE(payload);

      auto expected_doc = irs::doc_limits::min();
      for (auto& expected_payload : column_payload) {
        ASSERT_TRUE(sorted_column_it->next());
        ASSERT_EQ(expected_doc, sorted_column_it->value());
        ASSERT_EQ(expected_payload, payload->value);
        ++expected_doc;
      }
      ASSERT_FALSE(sorted_column_it->next());
    }

    // check regular columns
    std::vector<irs::string_ref> column_names {
      "name", "value", "duplicated", "prefix"
    };

    for (auto& column_name : column_names) {
      struct doc {
        irs::doc_id_t id{ irs::doc_limits::eof() };
        irs::bstring order;
        irs::bstring value;
      };

      std::vector<doc> column_docs;
      column_docs.reserve(segment.docs_count());

      gen.reset();
      irs::doc_id_t id{ irs::doc_limits::min() };
      while (auto* doc = gen.next()) {
        auto* sorted = doc->stored.get(sorted_column);
        ASSERT_NE(nullptr, sorted);

        column_docs.emplace_back();

        auto* column = doc->stored.get(column_name);

        auto& value = column_docs.back();
        irs::bytes_output order_out(value.order);
        sorted->write(order_out);

        if (column) {
          value.id = id++;
          irs::bytes_output value_out(value.value);
          column->write(value_out);
        }
      }

      std::sort(
        column_docs.begin(), column_docs.end(),
        [&less](const doc& lhs, const doc& rhs) {
          return less(lhs.order, rhs.order);
      });

      auto* column_meta = segment.column(column_name);
      ASSERT_NE(nullptr, column_meta);
      auto* column = segment.column_reader(column_meta->id);
      ASSERT_NE(nullptr, column);

      ASSERT_EQ(id-1, column->size());

      auto column_it = column->iterator();
      ASSERT_NE(nullptr, column_it);

      auto* payload = irs::get<irs::payload>(*column_it);
      ASSERT_TRUE(payload);

      irs::doc_id_t doc = 0;
      for (auto& expected_value: column_docs) {
        ++doc;

        if (irs::doc_limits::eof(expected_value.id)) {
          // skip empty values
          continue;
        }

        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(doc, column_it->value());
        EXPECT_EQ(expected_value.value, payload->value);
      }
      ASSERT_FALSE(column_it->next());
    }
  }
}

TEST_P(sorted_index_test_case, europarl) {
  sorted_europarl_doc_template doc("date");
  tests::delim_doc_generator gen(resource("europarl.subset.txt"), doc);

  long_comparer less;

  irs::index_writer::init_options opts;
  opts.comparator = &less;
  add_segment(gen, irs::OM_CREATE, opts); // add segment

  assert_index();
}

TEST_P(sorted_index_test_case, multi_valued_sorting_field) {
  struct {
    bool write(irs::data_output& out) {
      out.write_bytes(reinterpret_cast<const irs::byte_type*>(value.c_str()), value.size());
      return true;
    }

    irs::string_ref value;
  } field;

  tests::templates::string_ref_field same("same");
  same.value("A");

  // open writer
  string_comparer less;
  irs::index_writer::init_options opts;
  opts.comparator = &less;
  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_EQ(&less, writer->comparator());

  // write documents
  {
    auto docs = writer->documents();

    {
      auto doc = docs.insert();

      // compund sorted field
      field.value = "A"; doc.insert<irs::Action::STORE_SORTED>(field);
      field.value = "B"; doc.insert<irs::Action::STORE_SORTED>(field);

      // indexed field
      doc.insert<irs::Action::INDEX>(same);
    }

    {
      auto doc = docs.insert();

      // compund sorted field
      field.value = "C"; doc.insert<irs::Action::STORE_SORTED>(field);
      field.value = "D"; doc.insert<irs::Action::STORE_SORTED>(field);

      // indexed field
      doc.insert<irs::Action::INDEX>(same);
    }
  }

  writer->commit();

  // read documents
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());
    irs::bytes_ref actual_value;

    // check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("CD", irs::ref_cast<char>(actual_value));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("AB", irs::ref_cast<char>(actual_value));
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST_P(sorted_index_test_case, check_document_order_after_consolidation_dense) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        auto field = std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        );

        doc.insert(field);

        if (name == "name") {
          doc.sorted = field;
        }
      }
  });

  auto* doc0 = gen.next(); // name == 'A'
  auto* doc1 = gen.next(); // name == 'B'
  auto* doc2 = gen.next(); // name == 'C'
  auto* doc3 = gen.next(); // name == 'D'

  string_comparer less;

  // open writer
  irs::index_writer::init_options opts;
  opts.comparator = &less;
  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_EQ(&less, writer->comparator());

  // create index segment
  {
    // segment 0
    {
      ASSERT_TRUE(insert(*writer,
        doc0->indexed.begin(), doc0->indexed.end(),
        doc0->stored.begin(), doc0->stored.end(),
        doc0->sorted
      ));
      ASSERT_TRUE(insert(*writer,
        doc2->indexed.begin(), doc2->indexed.end(),
        doc2->stored.begin(), doc2->stored.end(),
        doc2->sorted
      ));
      writer->commit();
    }

    // segment 1
    {
      ASSERT_TRUE(insert(*writer,
        doc1->indexed.begin(), doc1->indexed.end(),
        doc1->stored.begin(), doc1->stored.end(),
        doc1->sorted
      ));
      ASSERT_TRUE(insert(*writer,
        doc3->indexed.begin(), doc3->indexed.end(),
        doc3->stored.begin(), doc3->stored.end(),
        doc3->sorted
      ));
      writer->commit();
    }
  }

  // read documents
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());
    irs::bytes_ref actual_value;

    // check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_FALSE(docsItr->next());
    }

    // check segment 1
    {
      auto& segment = reader[1];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_FALSE(docsItr->next());
    }
  }

  // consolidate segments
  {
    irs::index_utils::consolidate_count consolidate_all;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(consolidate_all)));
    writer->commit();
  }

  // check consolidated segment
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(reader->live_docs_count(), reader->docs_count());
    irs::bytes_ref actual_value;

    // check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST_P(sorted_index_test_case, check_document_order_after_consolidation_dense_with_removals) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        auto field = std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        );

        doc.insert(field);

        if (name == "name") {
          doc.sorted = field;
        }
      }
  });

  auto* doc0 = gen.next(); // name == 'A'
  auto* doc1 = gen.next(); // name == 'B'
  auto* doc2 = gen.next(); // name == 'C'
  auto* doc3 = gen.next(); // name == 'D'

  string_comparer less;

  // open writer
  irs::index_writer::init_options opts;
  opts.comparator = &less;
  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_EQ(&less, writer->comparator());

  // create index segment
  {
    // segment 0
    {
      ASSERT_TRUE(insert(*writer,
        doc0->indexed.begin(), doc0->indexed.end(),
        doc0->stored.begin(), doc0->stored.end(),
        doc0->sorted
      ));
      ASSERT_TRUE(insert(*writer,
        doc2->indexed.begin(), doc2->indexed.end(),
        doc2->stored.begin(), doc2->stored.end(),
        doc2->sorted
      ));
      writer->commit();
    }

    // segment 1
    {
      ASSERT_TRUE(insert(*writer,
        doc1->indexed.begin(), doc1->indexed.end(),
        doc1->stored.begin(), doc1->stored.end(),
        doc1->sorted
      ));
      ASSERT_TRUE(insert(*writer,
        doc3->indexed.begin(), doc3->indexed.end(),
        doc3->stored.begin(), doc3->stored.end(),
        doc3->sorted
      ));
      writer->commit();
    }
  }

  // read documents
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());
    irs::bytes_ref actual_value;

    // check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_FALSE(docsItr->next());
    }

    // check segment 1
    {
      auto& segment = reader[1];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_FALSE(docsItr->next());
    }
  }

  // remove document
  {
    auto query_doc1 = irs::iql::query_builder().build("name==C", std::locale::classic());
    writer->documents().remove(*query_doc1.filter);
    writer->commit();
  }

  // read documents
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());
    irs::bytes_ref actual_value;

    // check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_FALSE(docsItr->next());
    }

    // check segment 1
    {
      auto& segment = reader[1];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_FALSE(docsItr->next());
    }
  }

  // consolidate segments
  {
    irs::index_utils::consolidate_count consolidate_all;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(consolidate_all)));
    writer->commit();
  }

  // check consolidated segment
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(reader->live_docs_count(), reader->docs_count());
    irs::bytes_ref actual_value;

    // check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST_P(sorted_index_test_case, check_document_order_after_consolidation_sparse) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        auto field = std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        );

        doc.insert(field);

        if (name == "name") {
          doc.sorted = field;
        }
      }
  });

  auto* doc0 = gen.next(); // name == 'A'
  auto* doc1 = gen.next(); // name == 'B'
  auto* doc2 = gen.next(); // name == 'C'
  auto* doc3 = gen.next(); // name == 'D'

  string_comparer less;
  irs::index_writer::init_options opts;
  opts.comparator = &less;

  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_NE(nullptr, writer->comparator());

  // create index segment
  {
    // segment 0
    {
      ASSERT_TRUE(insert(*writer,
        doc2->indexed.begin(), doc2->indexed.end(),
        doc2->stored.begin(), doc2->stored.end()
      ));
      ASSERT_TRUE(insert(*writer,
        doc0->indexed.begin(), doc0->indexed.end(),
        doc0->stored.begin(), doc0->stored.end(),
        doc0->sorted
      ));
      writer->commit();
    }

    // segment 1
    {
      ASSERT_TRUE(insert(*writer,
        doc1->indexed.begin(), doc1->indexed.end(),
        doc1->stored.begin(), doc1->stored.end(),
        doc1->sorted
      ));
      ASSERT_TRUE(insert(*writer,
        doc3->indexed.begin(), doc3->indexed.end(),
        doc3->stored.begin(), doc3->stored.end()
      ));
      writer->commit();
    }
  }

  // read documents
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());
    irs::bytes_ref actual_value;

    // check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_TRUE(actual_value.empty());
      ASSERT_FALSE(docsItr->next());
    }

    // check segment 1
    {
      auto& segment = reader[1];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_TRUE(actual_value.empty());
      ASSERT_FALSE(docsItr->next());
    }
  }

  // consolidate segments
  {
    irs::index_utils::consolidate_count consolidate_all;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(consolidate_all)));
    writer->commit();
  }

  // check consolidated segment
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(reader->live_docs_count(), reader->docs_count());
    irs::bytes_ref actual_value;

    // check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_TRUE(actual_value.empty());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_TRUE(actual_value.empty());
      ASSERT_FALSE(docsItr->next());
    }
  }
}

INSTANTIATE_TEST_CASE_P(
  sorted_index_test,
  sorted_index_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::memory_directory,
      &tests::fs_directory,
      &tests::mmap_directory
    ),
    ::testing::Values(tests::format_info{"1_1", "1_0"},
                      tests::format_info{"1_2", "1_0"})
  ),
  tests::to_string
);

}
