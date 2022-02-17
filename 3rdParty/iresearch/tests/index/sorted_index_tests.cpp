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

#include "index/comparer.hpp"
#include "index/norm.hpp"
#include "iql/query_builder.hpp"
#include "utils/index_utils.hpp"

#include "tests_shared.hpp"
#include "index_tests.hpp"

namespace {

class sorted_europarl_doc_template : public tests::europarl_doc_template {
 public:
  explicit sorted_europarl_doc_template(
      std::string field,
      std::vector<irs::type_info::type_id> field_features)
    : field_{std::move(field)},
      field_features_{std::move(field_features)} {
  }

  virtual void init() override {
    indexed.push_back(
      std::make_shared<tests::string_field>(
        "title", irs::IndexFeatures::ALL, field_features_));
    indexed.push_back(
      std::make_shared<text_ref_field>(
        "title_anl", false, field_features_));
    indexed.push_back(
      std::make_shared<text_ref_field>(
        "title_anl_pay", true, field_features_));
    indexed.push_back(
      std::make_shared<text_ref_field>(
        "body_anl", false, field_features_));
    indexed.push_back(
      std::make_shared<text_ref_field>(
        "body_anl_pay", true, field_features_));
    {
      insert(std::make_shared<tests::long_field>());
      auto& field = static_cast<tests::long_field&>(indexed.back());
      field.name("date");
    }
    insert(std::make_shared<tests::string_field>(
      "datestr", irs::IndexFeatures::ALL, field_features_));
    insert(std::make_shared<tests::string_field>(
      "body", irs::IndexFeatures::ALL, field_features_));
    {
      insert(std::make_shared<tests::int_field>());
      auto& field = static_cast<tests::int_field&>(indexed.back());
      field.name("id");
    }
    insert(std::make_shared<tests::string_field>(
      "idstr", irs::IndexFeatures::ALL, field_features_));

    auto fields = indexed.find(field_);

    if (!fields.empty()) {
      sorted = fields[0];
    }
  }

 private:
  std::string field_; // sorting field
  std::vector<irs::type_info::type_id> field_features_;
}; // sorted_europal_doc_template

struct string_comparer : irs::comparer {
  virtual bool less(const irs::bytes_ref& lhs, const irs::bytes_ref& rhs) const {
    if (lhs.empty() && rhs.empty()) {
      return false;
    } else if (rhs.empty()) {
      return true;
    } else if (lhs.empty()) {
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

    return irs::zig_zag_decode64(irs::vread<uint64_t>(plhs)) <
           irs::zig_zag_decode64(irs::vread<uint64_t>(prhs));
  }
};

struct custom_feature {
  struct header {
    explicit header(irs::range<const irs::bytes_ref> headers) noexcept {
      for (const auto header : headers) {
        update(header);
      }
    }

    void write(irs::bstring& out) const {
      EXPECT_TRUE(out.empty());
      out.resize(sizeof(size_t));

      auto* p = out.data();
      irs::write<size_t>(p, count);
    }

    void update(irs::bytes_ref in) {
      EXPECT_EQ(sizeof(count), in.size());
      auto* p = in.c_str();
      count += irs::read<decltype(count)>(p);
    }

    size_t count{0};
  };

  struct writer : irs::feature_writer {
    explicit writer(irs::range<const irs::bytes_ref> headers) noexcept
      : hdr{{}} {
      if (!headers.empty()) {
        init_header.emplace(headers);
      }
    }

    virtual void write(
        const irs::field_stats& stats,
        irs::doc_id_t doc,
        // cppcheck-suppress constParameter
        irs::columnstore_writer::values_writer_f& writer) final {
      ++hdr.count;

      // We intentionally call `writer(doc)` multiple
      // times to check concatenation logic.
      writer(doc).write_int(stats.len);
      writer(doc).write_int(stats.max_term_freq);
      writer(doc).write_int(stats.num_overlap);
      writer(doc).write_int(stats.num_unique);
    }

    virtual void write(
        data_output& out,
        irs::bytes_ref payload) {
      if (!payload.empty()) {
        ++hdr.count;
        out.write_bytes(payload.c_str(), payload.size());
      }
    }

    virtual void finish(irs::bstring& out) final {
      if (init_header.has_value()) {
        // <= due to removals
        EXPECT_LE(hdr.count, init_header.value().count);
      }
      hdr.write(out);
    }

    header hdr;
    std::optional<header> init_header;
    std::optional<size_t> expected_count;
  };

  static irs::feature_writer::ptr make_writer(
      irs::range<const irs::bytes_ref> payload) {
    return irs::memory::make_managed<writer>(payload);
  }
};

REGISTER_ATTRIBUTE(custom_feature);

class sorted_index_test_case : public tests::index_test_base {
 protected:
  bool supports_pluggable_features() const noexcept {
    // old formats don't support pluggable features
    constexpr irs::string_ref kOldFormats[] {
        "1_0", "1_1", "1_2", "1_3", "1_3simd" };

     return std::end(kOldFormats) == std::find(std::begin(kOldFormats),
                                               std::end(kOldFormats),
                                               codec()->type().name());
  }

  irs::feature_info_provider_t features() {
    return [this](irs::type_info::type_id id) {
      if (id == irs::type<irs::Norm>::id()) {
        return std::make_pair(
          irs::column_info{irs::type<irs::compression::lz4>::get(), {}, false},
          &irs::Norm::MakeWriter);
      }

      if (supports_pluggable_features()) {
          if (irs::type<irs::Norm2>::id() == id) {
            return std::make_pair(
                irs::column_info{irs::type<irs::compression::none>::get(), {}, false},
                &irs::Norm2::MakeWriter);
          } else if (irs::type<custom_feature>::id() == id) {
            return std::make_pair(
                irs::column_info{irs::type<irs::compression::none>::get(), {}, false},
                &custom_feature::make_writer);
          }
      }

      return std::make_pair(
          irs::column_info{irs::type<irs::compression::none>::get(), {}, false},
          irs::feature_writer_factory_t{});
    };
  }

  std::vector<irs::type_info::type_id> field_features() {
    return supports_pluggable_features()
      ? std::vector<irs::type_info::type_id>{ irs::type<irs::Norm>::id(),
                                              irs::type<irs::Norm2>::id(),
                                              irs::type<custom_feature>::id() }
      : std::vector<irs::type_info::type_id>{ irs::type<irs::Norm>::id() };
  }

  void assert_index(size_t skip = 0, irs::automaton_table_matcher* matcher = nullptr) const {
    index_test_base::assert_index(irs::IndexFeatures::NONE, skip, matcher);
    index_test_base::assert_index(
      irs::IndexFeatures::NONE | irs::IndexFeatures::FREQ, skip, matcher);
    index_test_base::assert_index(
      irs::IndexFeatures::NONE | irs::IndexFeatures::FREQ
        | irs::IndexFeatures::POS,
      skip, matcher);
    index_test_base::assert_index(
      irs::IndexFeatures::NONE | irs::IndexFeatures::FREQ
        | irs::IndexFeatures::POS | irs::IndexFeatures::OFFS,
      skip, matcher);
    index_test_base::assert_index(
      irs::IndexFeatures::NONE | irs::IndexFeatures::FREQ
        | irs::IndexFeatures::POS | irs::IndexFeatures::PAY,
      skip, matcher);
    index_test_base::assert_index(
      irs::IndexFeatures::NONE | irs::IndexFeatures::FREQ
        | irs::IndexFeatures::POS | irs::IndexFeatures::OFFS
        | irs::IndexFeatures::PAY,
      skip, matcher);
    index_test_base::assert_columnstore();
  }

  void check_feature_header(const irs::sub_reader& segment,
                            const irs::field_meta& field,
                            irs::type_info::type_id type,
                            irs::bytes_ref header) {
    ASSERT_TRUE(supports_pluggable_features());
    auto feature = field.features.find(type);
    ASSERT_NE(feature, field.features.end());
    ASSERT_TRUE(irs::field_limits::valid(feature->second));
    auto* column = segment.column(feature->second);
    ASSERT_NE(nullptr, column);
    ASSERT_FALSE(column->payload().null());
    ASSERT_EQ(header, column->payload());
  }

  void check_empty_feature(const irs::sub_reader& segment,
                           const irs::field_meta& field,
                           irs::type_info::type_id type) {
    ASSERT_TRUE(supports_pluggable_features());
    auto feature = field.features.find(type);
    ASSERT_NE(feature, field.features.end());
    ASSERT_FALSE(irs::field_limits::valid(feature->second));
    auto* column = segment.column(feature->second);
    ASSERT_EQ(nullptr, column);
  }

  void check_features(const irs::sub_reader& segment,
                      irs::string_ref field_name,
                      size_t count, bool after_consolidation) {
     auto* field_reader = segment.field(field_name);
     ASSERT_NE(nullptr, field_reader);
     auto& field = field_reader->meta();
     ASSERT_EQ(3, field.features.size());

     // irs::norm, nothing is written since all values are equal to 1
     check_empty_feature(segment, field, irs::type<irs::Norm>::id());

     // custom_feature
     {
       irs::byte_type buf[sizeof(count)];
       auto* p = buf;
       irs::write<size_t>(p, count);

       check_feature_header(segment, field,
                            irs::type<custom_feature>::id(),
                            { buf, sizeof buf });
     }

     // irs::Norm2
     {
       irs::Norm2Header hdr{after_consolidation ? irs::Norm2Encoding::Byte
                                                : irs::Norm2Encoding::Int};
       hdr.Reset(1);

       irs::bstring buf;
       irs::Norm2Header::Write(hdr, buf);

       check_feature_header(segment, field,
                            irs::type<irs::Norm2>::id(),
                            buf);
     }
  }
};

TEST_P(sorted_index_test_case, simple_sequential) {
  constexpr irs::string_ref sorted_column = "name";

  // Build index
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [&sorted_column, this](tests::document& doc,
                           const std::string& name,
                           const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        auto field = std::make_shared<tests::string_field>(
          name, data.str, irs::IndexFeatures::ALL,
          field_features());

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
  opts.features = features();

  add_segment(gen, irs::OM_CREATE, opts); // add segment

  // Check index
  assert_index();

  // Check columns
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

      auto sorted_column_it = sorted_column.iterator(false);
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

    // Check regular columns
    constexpr irs::string_ref column_names[] {
        "seq", "value", "duplicated", "prefix" };

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
      auto* column = segment.column(column_meta->id());
      ASSERT_NE(nullptr, column);

      ASSERT_EQ(id-1, column->size());

      auto column_it = column->iterator(false);
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

    // Check pluggable features
    if (supports_pluggable_features()) {
      check_features(segment, "name", 32, false);
      check_features(segment, "same", 32, false);
      check_features(segment, "duplicated", 13, false);
      check_features(segment, "prefix", 10, false);
    }
  }
}

TEST_P(sorted_index_test_case, simple_sequential_consolidate) {
  constexpr irs::string_ref sorted_column = "name";

  // Build index
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [&sorted_column, this](tests::document& doc,
                           const std::string& name,
                           const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        auto field = std::make_shared<tests::string_field>(
          name, data.str, irs::IndexFeatures::ALL,
          field_features());

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

  constexpr std::pair<size_t, size_t> segment_offsets[] {
      { 0, 15 }, { 15, 17 } };

  string_comparer less;

  irs::index_writer::init_options opts;
  opts.comparator = &less;
  opts.features = features();

  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_EQ(&less, writer->comparator());

  // Add segment 0
  {
    auto& offset = segment_offsets[0];
    tests::limiting_doc_generator segment_gen(gen, offset.first, offset.second);
    add_segment(*writer, segment_gen);
  }

  // Add segment 1
  add_segment(*writer, gen);

  // Check index
  assert_index();

  // Check columns
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());

    // Check segments
    size_t i = 0;
    for (auto& segment : *reader) {
      auto& offset = segment_offsets[i++];
      tests::limiting_doc_generator segment_gen(gen, offset.first, offset.second);

      ASSERT_EQ(offset.second, segment.docs_count());
      ASSERT_EQ(segment.docs_count(), segment.live_docs_count());
      ASSERT_NE(nullptr, segment.sort());

      // Check sorted column
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
        ASSERT_TRUE(sorted_column.name().null());
        ASSERT_TRUE(sorted_column.payload().empty());

        auto sorted_column_it = sorted_column.iterator(false);
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

      // Check stored columns
      constexpr irs::string_ref column_names[] {
          "seq", "value", "duplicated", "prefix" };

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
        auto* column = segment.column(column_meta->id());
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column_meta, column);
        ASSERT_TRUE(column->payload().empty());

        ASSERT_EQ(id-1, column->size());

        auto column_it = column->iterator(false);
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

      // Check pluggable features
      if (supports_pluggable_features()) {
        check_features(segment, "name", offset.second, false);
        check_features(segment, "same", offset.second, false);

        {
          constexpr irs::string_ref kColumnName = "duplicated";
          auto* column = segment.column(kColumnName);
          ASSERT_NE(nullptr, column);
          check_features(segment, kColumnName, column->size(), false);
        }

        {
          constexpr irs::string_ref kColumnName = "prefix";
          auto* column = segment.column(kColumnName);
          ASSERT_NE(nullptr, column);
          check_features(segment, kColumnName, column->size(), false);
        }
      }
    }
  }

  // Consolidate segments
  {
    irs::index_utils::consolidate_count consolidate_all;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(consolidate_all)));
    writer->commit();

    // simulate consolidation
    index().clear();
    index().emplace_back(writer->feature_info());
    auto& segment = index().back();

    gen.reset();
    while (auto* doc = gen.next()) {
      segment.insert(*doc);
    }

    for (auto& column : segment.columns()) {
      column.rewrite();
    }

    ASSERT_NE(nullptr, writer->comparator());
    segment.sort(*writer->comparator());
  }

  assert_index();

  // Check columns in consolidated segment
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());

    auto& segment = reader[0];
    ASSERT_EQ(segment_offsets[0].second + segment_offsets[1].second, segment.docs_count());
    ASSERT_EQ(segment.docs_count(), segment.live_docs_count());
    ASSERT_NE(nullptr, segment.sort());

    // Check sorted column
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
      ASSERT_TRUE(sorted_column.payload().empty());
      ASSERT_TRUE(sorted_column.name().null());

      auto sorted_column_it = sorted_column.iterator(false);
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

    // Check stored columns
    constexpr irs::string_ref column_names[] {
        "seq", "value", "duplicated", "prefix" };

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
      auto* column = segment.column(column_meta->id());
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column_meta, column);
      ASSERT_TRUE(column->payload().empty());

      ASSERT_EQ(id-1, column->size());

      auto column_it = column->iterator(false);
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

    // Check pluggable features in consolidated segment
    if (supports_pluggable_features()) {
      check_features(segment, "name", 32, true);
      check_features(segment, "same", 32, true);
      check_features(segment, "duplicated", 13, true);
      check_features(segment, "prefix", 10, true);
    }
  }
}

TEST_P(sorted_index_test_case, simple_sequential_already_sorted) {
  constexpr irs::string_ref sorted_column = "seq";

  // Build index
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [&sorted_column, this](tests::document& doc,
                           const std::string& name,
                           const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        auto field = std::make_shared<tests::string_field>(
          name, data.str, irs::IndexFeatures::ALL,
          field_features());

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
  opts.features = features();

  add_segment(gen, irs::OM_CREATE, opts); // add segment

  assert_index();

  // Check columns
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());

    auto& segment = reader[0];
    ASSERT_EQ(segment.docs_count(), segment.live_docs_count());
    ASSERT_NE(nullptr, segment.sort());

    // Check sorted column
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
      ASSERT_TRUE(sorted_column.name().null());
      ASSERT_TRUE(sorted_column.payload().empty());

      auto sorted_column_it = sorted_column.iterator(false);
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

    // Check stored columns
    constexpr irs::string_ref column_names[] {
        "name", "value", "duplicated", "prefix" };

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
      auto* column = segment.column(column_meta->id());
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column_meta, column);
      ASSERT_EQ(0, column->payload().size());

      ASSERT_EQ(id-1, column->size());

      auto column_it = column->iterator(false);
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

    // Check pluggable features
    if (supports_pluggable_features()) {
      check_features(segment, "name", 32, false);
      check_features(segment, "same", 32, false);
      check_features(segment, "duplicated", 13, false);
      check_features(segment, "prefix", 10, false);
    }
  }
}

TEST_P(sorted_index_test_case, europarl) {
  sorted_europarl_doc_template doc("date", field_features());
  tests::delim_doc_generator gen(resource("europarl.subset.txt"), doc);

  long_comparer less;

  irs::index_writer::init_options opts;
  opts.comparator = &less;
  opts.features = features();

  add_segment(gen, irs::OM_CREATE, opts);

  assert_index();
}

TEST_P(sorted_index_test_case, multi_valued_sorting_field) {
  struct {
    bool write(irs::data_output& out) {
      out.write_bytes(reinterpret_cast<const irs::byte_type*>(value.c_str()),
                      value.size());
      return true;
    }

    irs::string_ref value;
  } field;

  tests::string_ref_field same("same");
  same.value("A");

  // Open writer
  string_comparer less;
  irs::index_writer::init_options opts;
  opts.comparator = &less;
  opts.features = features();

  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_EQ(&less, writer->comparator());

  // Write documents
  {
    auto docs = writer->documents();

    {
      auto doc = docs.insert();

      // Compound sorted field
      field.value = "A"; doc.insert<irs::Action::STORE_SORTED>(field);
      field.value = "B"; doc.insert<irs::Action::STORE_SORTED>(field);

      // Indexed field
      doc.insert<irs::Action::INDEX>(same);
    }

    {
      auto doc = docs.insert();

      // Compound sorted field
      field.value = "C"; doc.insert<irs::Action::STORE_SORTED>(field);
      field.value = "D"; doc.insert<irs::Action::STORE_SORTED>(field);

      // Indexed field
      doc.insert<irs::Action::INDEX>(same);
    }
  }

  writer->commit();

  // Read documents
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());

    // Check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->name().null());
      ASSERT_TRUE(column->payload().empty());
      auto values = column->iterator(false);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);

      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("CD", irs::ref_cast<char>(actual_value->value));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("AB", irs::ref_cast<char>(actual_value->value));
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST_P(sorted_index_test_case, check_document_order_after_consolidation_dense) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [this](tests::document& doc,
           const std::string& name,
           const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        auto field = std::make_shared<tests::string_field>(
          name, data.str, irs::IndexFeatures::ALL,
          field_features());

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
  opts.features = features();

  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_EQ(&less, writer->comparator());

  // Segment 0
  ASSERT_TRUE(insert(*writer,
    doc0->indexed.begin(), doc0->indexed.end(),
    doc0->stored.begin(), doc0->stored.end(),
    doc0->sorted));
  ASSERT_TRUE(insert(*writer,
    doc2->indexed.begin(), doc2->indexed.end(),
    doc2->stored.begin(), doc2->stored.end(),
   doc2->sorted));
  writer->commit();

  // Segment 1
  ASSERT_TRUE(insert(*writer,
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end(),
    doc1->sorted));
  ASSERT_TRUE(insert(*writer,
    doc3->indexed.begin(), doc3->indexed.end(),
    doc3->stored.begin(), doc3->stored.end(),
    doc3->sorted));
  writer->commit();

  // Read documents
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());

    // Check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->name().null());
      ASSERT_TRUE(column->payload().empty());
      auto values = column->iterator(false);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_FALSE(docsItr->next());

      // Check pluggable features
      if (supports_pluggable_features()) {
        check_features(segment, "name", 2, false);
        check_features(segment, "same", 2, false);
        check_features(segment, "duplicated", 2, false);
        check_features(segment, "prefix", 1, false);
      }
    }

    // Check segment 1
    {
      auto& segment = reader[1];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(false);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_FALSE(docsItr->next());

      // Check pluggable features
      if (supports_pluggable_features()) {
        check_features(segment, "name", 2, false);
        check_features(segment, "same", 2, false);
        check_features(segment, "duplicated", 1, false);
        check_features(segment, "prefix", 1, false);
      }
    }
  }

  // Consolidate segments
  {
    irs::index_utils::consolidate_count consolidate_all;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(consolidate_all)));
    writer->commit();
  }

  // Check consolidated segment
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(reader->live_docs_count(), reader->docs_count());

    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->name().null());
      ASSERT_TRUE(column->payload().empty());
      auto values = column->iterator(false);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_FALSE(docsItr->next());

      // Check pluggable features in consolidated segment
      if (supports_pluggable_features()) {
        check_features(segment, "name", 4, true);
        check_features(segment, "same", 4, true);
        check_features(segment, "duplicated", 3, true);
        check_features(segment, "prefix", 2, true);
      }
    }
  }

  // Create expected index
  auto& expected_index = index();
  expected_index.emplace_back(writer->feature_info());
  expected_index.back().insert(
    doc0->indexed.begin(), doc0->indexed.end(),
    doc0->stored.begin(), doc0->stored.end(),
    doc0->sorted.get());
  expected_index.back().insert(
    doc2->indexed.begin(), doc2->indexed.end(),
    doc2->stored.begin(), doc2->stored.end(),
   doc2->sorted.get());
  expected_index.back().insert(
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end(),
    doc1->sorted.get());
  expected_index.back().insert(
    doc3->indexed.begin(), doc3->indexed.end(),
    doc3->stored.begin(), doc3->stored.end(),
    doc3->sorted.get());
  expected_index.back().sort(*writer->comparator());
  for (auto& column : expected_index.back().columns()) {
    column.rewrite();
  }
  assert_index();
}

TEST_P(sorted_index_test_case, check_document_order_after_consolidation_dense_with_removals) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [this](tests::document& doc,
           const std::string& name,
           const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        auto field = std::make_shared<tests::string_field>(
          name, data.str, irs::IndexFeatures::ALL,
          field_features());

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

  tests::string_field empty_field{"", irs::IndexFeatures::NONE};
  ASSERT_FALSE(empty_field.value().null());
  ASSERT_TRUE(empty_field.value().empty());

  string_comparer less;

  // open writer
  irs::index_writer::init_options opts;
  opts.comparator = &less;
  opts.features = features();
  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_EQ(&less, writer->comparator());

  // segment 0
  ASSERT_TRUE(insert(*writer,
    doc0->indexed.begin(), doc0->indexed.end(),
    doc0->stored.begin(), doc0->stored.end(),
    doc0->sorted));
  ASSERT_TRUE(insert(*writer,
    doc2->indexed.begin(), doc2->indexed.end(),
    doc2->stored.begin(), doc2->stored.end(),
    doc2->sorted));
  writer->commit();

  // segment 1
  ASSERT_TRUE(insert(*writer,
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end(),
    doc1->sorted));
  ASSERT_TRUE(insert(*writer,
    doc3->indexed.begin(), doc3->indexed.end(),
    doc3->stored.begin(), doc3->stored.end(),
    doc3->sorted));
  writer->commit();

  // Read documents
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());

    // Check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->name().null());
      ASSERT_TRUE(column->payload().empty());
      auto values = column->iterator(false);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_FALSE(docsItr->next());

      // Check pluggable features
      if (supports_pluggable_features()) {
        check_features(segment, "name", 2, false);
        check_features(segment, "same", 2, false);
        check_features(segment, "duplicated", 2, false);
        check_features(segment, "prefix", 1, false);
      }
    }

    // Check segment 1
    {
      auto& segment = reader[1];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->name().null());
      ASSERT_EQ(0, column->payload().size());
      auto values = column->iterator(false);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_FALSE(docsItr->next());

      // Check pluggable features
      if (supports_pluggable_features()) {
        check_features(segment, "name", 2, false);
        check_features(segment, "same", 2, false);
        check_features(segment, "duplicated", 1, false);
        check_features(segment, "prefix", 1, false);
      }
    }
  }

  // Remove document
  {
    auto query_doc1 = irs::iql::query_builder().build("name==C", "C");
    writer->documents().remove(*query_doc1.filter);
    writer->commit();
  }

  // Read documents
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());

    // Check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->name().null());
      ASSERT_EQ(0, column->payload().size());
      auto values = column->iterator(false);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(irs::IndexFeatures::NONE));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_FALSE(docsItr->next());

      // Check pluggable features
      if (supports_pluggable_features()) {
        check_features(segment, "name", 2, false);
        check_features(segment, "same", 2, false);
        check_features(segment, "duplicated", 2, false);
        check_features(segment, "prefix", 1, false);
      }
    }

    // Check segment 1
    {
      auto& segment = reader[1];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->name().null());
      ASSERT_EQ(0, column->payload().size());
      auto values = column->iterator(false);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(irs::IndexFeatures::NONE));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_FALSE(docsItr->next());

      // Check pluggable features
      if (supports_pluggable_features()) {
        check_features(segment, "name", 2, false);
        check_features(segment, "same", 2, false);
        check_features(segment, "duplicated", 1, false);
        check_features(segment, "prefix", 1, false);
      }
    }
  }

  // Consolidate segments
  {
    irs::index_utils::consolidate_count consolidate_all;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(consolidate_all)));
    writer->commit();
  }

  // Check consolidated segment
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(reader->live_docs_count(), reader->docs_count());

    // Check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->name().null());
      ASSERT_EQ(0, column->payload().size());
      auto values = column->iterator(false);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_FALSE(docsItr->next());

      // Check pluggable features in consolidated segment
      if (supports_pluggable_features()) {
        check_features(segment, "name", 3, true);
        check_features(segment, "same", 3, true);
        check_features(segment, "duplicated", 2, true);
        check_features(segment, "prefix", 2, true);
      }
    }
  }

  // Create expected index
  auto& expected_index = index();
  expected_index.emplace_back(writer->feature_info());
  expected_index.back().insert(
    doc0->indexed.begin(), doc0->indexed.end(),
    doc0->stored.begin(), doc0->stored.end(),
    doc0->sorted.get());
  expected_index.back().insert(
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end(),
    doc1->sorted.get());
  expected_index.back().insert(
    doc3->indexed.begin(), doc3->indexed.end(),
    doc3->stored.begin(), doc3->stored.end(),
    doc3->sorted.get());
  for (auto& column : expected_index.back().columns()) {
    column.rewrite();
  }
  expected_index.back().sort(*writer->comparator());

  assert_index();
}

TEST_P(sorted_index_test_case, check_document_order_after_consolidation_sparse) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [this] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        auto field = std::make_shared<tests::string_field>(
          name, data.str, irs::IndexFeatures::ALL,
          field_features());

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
  opts.features = features();

  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_NE(nullptr, writer->comparator());

  // Create segment 0
  ASSERT_TRUE(insert(*writer,
    doc2->indexed.begin(), doc2->indexed.end(),
    doc2->stored.begin(), doc2->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc0->indexed.begin(), doc0->indexed.end(),
    doc0->stored.begin(), doc0->stored.end(),
    doc0->sorted));
  writer->commit();

  // Create segment 1
  ASSERT_TRUE(insert(*writer,
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end(),
    doc1->sorted));
  ASSERT_TRUE(insert(*writer,
    doc3->indexed.begin(), doc3->indexed.end(),
    doc3->stored.begin(), doc3->stored.end()));
  writer->commit();

  // Read documents
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());

    // Check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->name().null());
      ASSERT_EQ(0, column->payload().size());
      ASSERT_EQ(2, column->size());
      auto values = column->iterator(false);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_TRUE(actual_value->value.empty());
      ASSERT_FALSE(docsItr->next());

      // Check pluggable features
      if (supports_pluggable_features()) {
        check_features(segment, "name", 2, false);
        check_features(segment, "same", 2, false);
        check_features(segment, "duplicated", 2, false);
        check_features(segment, "prefix", 1, false);
      }
    }

    // Check segment 1
    {
      auto& segment = reader[1];
      const auto* column = segment.sort();
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->name().null());
      ASSERT_EQ(0, column->payload().size());
      ASSERT_EQ(2, column->size());
      auto values = column->iterator(false);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_TRUE(actual_value->value.empty());
      ASSERT_FALSE(docsItr->next());

      // Check pluggable features
      if (supports_pluggable_features()) {
        check_features(segment, "name", 2, false);
        check_features(segment, "same", 2, false);
        check_features(segment, "duplicated", 1, false);
        check_features(segment, "prefix", 1, false);
      }
    }
  }

  // Consolidate segments
  {
    irs::index_utils::consolidate_count consolidate_all;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(consolidate_all)));
    writer->commit();
  }

  // Check consolidated segment
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(reader->live_docs_count(), reader->docs_count());

    // Check segment 0
    {
      auto& segment = reader[0];
      const auto* column = segment.sort();
      ASSERT_EQ(4, column->size());
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->name().null());
      ASSERT_EQ(0, column->payload().size());
      auto values = column->iterator(false);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value->value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_TRUE(actual_value->value.empty());
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_TRUE(actual_value->value.empty());
      ASSERT_FALSE(docsItr->next());

      // Check pluggable features in consolidated segment
      if (supports_pluggable_features()) {
        check_features(segment, "name", 4, true);
        check_features(segment, "same", 4, true);
        check_features(segment, "duplicated", 3, true);
        check_features(segment, "prefix", 2, true);
      }
    }
  }

  // FIXME(gnusi)
  // We can't use assert_index() to check the
  // whole index because sort is not stable
  //
  //  struct empty_field : tests::ifield {
  //    irs::string_ref name() const override {
  //      EXPECT_FALSE(true);
  //      throw irs::not_impl_error{};
  //    }
  //
  //    irs::token_stream& get_tokens() const override {
  //      EXPECT_FALSE(true);
  //      throw irs::not_impl_error{};
  //    }
  //
  //    irs::features_t features() const override {
  //      EXPECT_FALSE(true);
  //      throw irs::not_impl_error{};
  //    }
  //
  //
  //    irs::IndexFeatures index_features() const override {
  //      EXPECT_FALSE(true);
  //      throw irs::not_impl_error{};
  //    }
  //
  //    bool write(irs::data_output&) const override {
  //      return true;
  //    }
  //
  //    mutable irs::null_token_stream stream_;
  //  } empty;
  //
  //  // Create expected index
  //  auto& expected_index = index();
  //  expected_index.emplace_back(writer->feature_info());
  //  expected_index.back().insert(
  //    doc2->indexed.begin(), doc2->indexed.end(),
  //    doc2->stored.begin(), doc2->stored.end(),
  //    &empty);
  //  expected_index.back().insert(
  //    doc0->indexed.begin(), doc0->indexed.end(),
  //    doc0->stored.begin(), doc0->stored.end(),
  //    doc0->sorted.get());
  //  expected_index.back().insert(
  //    doc1->indexed.begin(), doc1->indexed.end(),
  //    doc1->stored.begin(), doc1->stored.end(),
  //    doc1->sorted.get());
  //  expected_index.back().insert(
  //    doc3->indexed.begin(), doc3->indexed.end(),
  //    doc3->stored.begin(), doc3->stored.end(),
  //    &empty);
  //  for (auto& column : expected_index.back().columns()) {
  //    column.rewrite();
  //  }
  //  expected_index.back().sort(*writer->comparator());
  //
  //  assert_index();
}

// Separate definition as MSVC parser fails to do conditional defines in macro expansion
#ifdef IRESEARCH_SSE2
const auto kSortedIndexTestCaseValues = ::testing::Values(
    tests::format_info{"1_1", "1_0"},
    tests::format_info{"1_2", "1_0"},
    tests::format_info{"1_3", "1_0"},
    tests::format_info{"1_4", "1_0"},
    tests::format_info{"1_3simd", "1_0"},
    tests::format_info{"1_4simd", "1_0"});
#else
const auto kSortedIndexTestCaseValues = ::testing::Values(
    tests::format_info{"1_1", "1_0"},
    tests::format_info{"1_2", "1_0"},
    tests::format_info{"1_3", "1_0"},
    tests::format_info{"1_4", "1_0"});
#endif

INSTANTIATE_TEST_SUITE_P(
  sorted_index_test,
  sorted_index_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::directory<&tests::memory_directory>,
      &tests::directory<&tests::fs_directory>,
      &tests::directory<&tests::mmap_directory>),
    kSortedIndexTestCaseValues),
  sorted_index_test_case::to_string);

}
