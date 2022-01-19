////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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

#include "index_tests.hpp"

#include "index/norm.hpp"
#include "iql/query_builder.hpp"
#include "search/cost.hpp"
#include "utils/index_utils.hpp"

namespace  {

class Analyzer : public irs::analysis::analyzer {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "NormTestAnalyzer";
  }

  explicit Analyzer(size_t count)
    : irs::analysis::analyzer{irs::type<Analyzer>::get()},
      count_{count} {
  }

  virtual irs::attribute* get_mutable(
      irs::type_info::type_id id) noexcept override final {
    return irs::get_mutable(attrs_, id);
  }

  virtual bool reset(const irs::string_ref& value) noexcept override {
    value_ = value;
    i_ = 0;
    return true;
  }

  virtual bool next() override {
    if (i_ < count_) {
      std::get<irs::term_attribute>(attrs_).value = irs::ref_cast<irs::byte_type>(value_);
      auto& offset = std::get<irs::offset>(attrs_);
      offset.start = 0;
      offset.end = static_cast<uint32_t>(value_.size());
      ++i_;
      return true;
    }

    return false;
  }

 private:
  std::tuple<irs::offset, irs::increment, irs::term_attribute> attrs_;
  irs::string_ref value_;
  size_t count_;
  size_t i_{};
};

class NormField final : public tests::ifield {
 public:
  NormField(std::string name, std::string value, size_t count)
    : name_{std::move(name)}, value_{std::move(value)}, analyzer_{count} {
  }

  irs::string_ref name() const override { return name_; }

  irs::token_stream& get_tokens() const override {
    analyzer_.reset(value_);
    return analyzer_;
  }

  irs::features_t features() const noexcept override {
    return { &norm_, 1 };
  }

  irs::IndexFeatures index_features() const noexcept override {
    return irs::IndexFeatures::ALL;
  }

  bool write(data_output& out) const override {
    irs::write_string(out, value_);
    return true;
  }

 private:
  std::string name_;
  std::string value_;
  mutable Analyzer analyzer_;
  irs::type_info::type_id norm_{irs::type<irs::Norm2>::id() };
};

void AssertNorm2Header(irs::bytes_ref header,
                       uint32_t num_bytes,
                       uint32_t min,
                       uint32_t max) {
  constexpr irs::Norm2Version kVersion{irs::Norm2Version::kMin};

  ASSERT_FALSE(header.null());
  ASSERT_EQ(10, header.size());

  auto* p = header.c_str();
  const auto actual_verson = *p++;
  const auto actual_num_bytes = *p++;
  const auto actual_min = irs::read<uint32_t>(p);
  const auto actual_max = irs::read<uint32_t>(p);
  ASSERT_EQ(p, header.end());

  ASSERT_EQ(static_cast<uint32_t>(kVersion), actual_verson);
  ASSERT_EQ(num_bytes, actual_num_bytes);
  ASSERT_EQ(min, actual_min);
  ASSERT_EQ(max, actual_max);
}

TEST(Norm2HeaderTest, Construct) {
  irs::Norm2Header hdr{irs::Norm2Encoding::Int};
  ASSERT_EQ(1, hdr.MaxNumBytes());
  ASSERT_EQ(sizeof(uint32_t), hdr.NumBytes());

  irs::bstring buf;
  irs::Norm2Header::Write(hdr, buf);
  AssertNorm2Header(buf,
                    sizeof(uint32_t),
                    std::numeric_limits<uint32_t>::max(),
                    std::numeric_limits<uint32_t>::min());
}

TEST(Norm2HeaderTest, ResetByValue) {
  auto AssertNumBytes = [](auto value) {
    using value_type = decltype(value);

    static_assert(std::is_same_v<value_type, irs::byte_type> ||
                  std::is_same_v<value_type, uint16_t> ||
                  std::is_same_v<value_type, uint32_t>);

    irs::Norm2Encoding encoding;
    if constexpr (std::is_same_v<value_type, irs::byte_type>) {
      encoding = irs::Norm2Encoding::Byte;
    } else if (std::is_same_v<value_type, uint16_t>) {
      encoding = irs::Norm2Encoding::Short;
    } else if (std::is_same_v<value_type, uint32_t>) {
      encoding = irs::Norm2Encoding::Int;
    }

    irs::Norm2Header hdr{encoding};
    hdr.Reset(std::numeric_limits<value_type>::max()-2);
    hdr.Reset(std::numeric_limits<value_type>::max());
    hdr.Reset(std::numeric_limits<value_type>::max()-1);
    ASSERT_EQ(sizeof(value_type), hdr.MaxNumBytes());
    ASSERT_EQ(sizeof(value_type), hdr.NumBytes());

    irs::bstring buf;
    irs::Norm2Header::Write(hdr, buf);
    AssertNorm2Header(buf,
                      sizeof(value_type),
                      std::numeric_limits<value_type>::max()-2,
                      std::numeric_limits<value_type>::max());
  };

  AssertNumBytes(irs::byte_type{}); // 1-byte header
  AssertNumBytes(uint16_t{}); // 2-byte header
  AssertNumBytes(uint32_t{}); // 4-byte header
}

TEST(Norm2HeaderTest, ReadInvalid) {
  ASSERT_FALSE(irs::Norm2Header::Read(irs::bytes_ref::NIL).has_value());
  ASSERT_FALSE(irs::Norm2Header::Read(irs::bytes_ref::EMPTY).has_value());

  // Invalid size
  {
    constexpr irs::byte_type kBuf[3]{};
    static_assert(sizeof kBuf != irs::Norm2Header::ByteSize());
    ASSERT_FALSE(irs::Norm2Header::Read({ kBuf, sizeof kBuf}).has_value());
  }

  // Invalid encoding
  {
    constexpr irs::byte_type kBuf[irs::Norm2Header::ByteSize()]{};
    ASSERT_FALSE(irs::Norm2Header::Read({ kBuf, sizeof kBuf}).has_value());
  }

  // Invalid encoding
  {
    constexpr irs::byte_type kBuf[irs::Norm2Header::ByteSize()]{ 0, 3 };
    ASSERT_FALSE(irs::Norm2Header::Read({ kBuf, sizeof kBuf}).has_value());
  }

  // Invalid version
  {
    constexpr irs::byte_type kBuf[irs::Norm2Header::ByteSize()]{ 42, 1 };
    ASSERT_FALSE(irs::Norm2Header::Read({ kBuf, sizeof kBuf}).has_value());
  }
}

TEST(Norm2HeaderTest, ResetByPayload) {
  auto WriteHeader = [](auto value, irs::bstring& buf) {
    using value_type = decltype(value);

    static_assert(std::is_same_v<value_type, irs::byte_type> ||
                  std::is_same_v<value_type, uint16_t> ||
                  std::is_same_v<value_type, uint32_t>);

    irs::Norm2Encoding encoding;
    if constexpr (std::is_same_v<value_type, irs::byte_type>) {
      encoding = irs::Norm2Encoding::Byte;
    } else if (std::is_same_v<value_type, uint16_t>) {
      encoding = irs::Norm2Encoding::Short;
    } else if (std::is_same_v<value_type, uint32_t>) {
      encoding = irs::Norm2Encoding::Int;
    }

    irs::Norm2Header hdr{encoding};
    hdr.Reset(std::numeric_limits<value_type>::max()-2);
    hdr.Reset(std::numeric_limits<value_type>::max());
    hdr.Reset(std::numeric_limits<value_type>::max()-1);
    ASSERT_EQ(sizeof(value_type), hdr.NumBytes());

    buf.clear();
    irs::Norm2Header::Write(hdr, buf);
    AssertNorm2Header(buf,
                      sizeof(value_type),
                      std::numeric_limits<value_type>::max()-2,
                      std::numeric_limits<value_type>::max());
  };

  irs::Norm2Header acc{irs::Norm2Encoding::Byte};

  // 1-byte header
  {
    irs::bstring buf;
    WriteHeader(irs::byte_type{}, buf);
    auto hdr = irs::Norm2Header::Read(buf);
    ASSERT_TRUE(hdr.has_value());
    acc.Reset(hdr.value());
    buf.clear();
    irs::Norm2Header::Write(acc, buf);

    AssertNorm2Header(buf,
                      sizeof(irs::byte_type),
                      std::numeric_limits<irs::byte_type>::max()-2,
                      std::numeric_limits<irs::byte_type>::max());
  }

  // 2-byte header
  {
    irs::bstring buf;
    WriteHeader(uint16_t{}, buf);
    auto hdr = irs::Norm2Header::Read(buf);
    ASSERT_TRUE(hdr.has_value());
    acc.Reset(hdr.value());
    buf.clear();
    irs::Norm2Header::Write(acc, buf);

    AssertNorm2Header(buf,
                      sizeof(uint16_t),
                      std::numeric_limits<irs::byte_type>::max()-2,
                      std::numeric_limits<uint16_t>::max());
  }

  // 4-byte header
  {
    irs::bstring buf;
    WriteHeader(uint32_t{}, buf);
    auto hdr = irs::Norm2Header::Read(buf);
    ASSERT_TRUE(hdr.has_value());
    acc.Reset(hdr.value());
    buf.clear();
    irs::Norm2Header::Write(acc, buf);

    AssertNorm2Header(buf,
                      sizeof(uint32_t),
                      std::numeric_limits<irs::byte_type>::max()-2,
                      std::numeric_limits<uint32_t>::max());
  }
}

class Norm2TestCase : public tests::index_test_base {
 protected:
  irs::feature_info_provider_t Features() {
    return [](irs::type_info::type_id id) {
      if (irs::type<irs::Norm2>::id() == id) {
        return std::make_pair(
            irs::column_info{irs::type<irs::compression::none>::get(), {}, false},
            &irs::Norm2::MakeWriter);
      }

      return std::make_pair(
          irs::column_info{irs::type<irs::compression::none>::get(), {}, false},
          irs::feature_writer_factory_t{});
    };
  }

  void AssertIndex() {
    index_test_base::assert_index(irs::IndexFeatures::NONE);
    index_test_base::assert_index(
      irs::IndexFeatures::NONE | irs::IndexFeatures::FREQ);
    index_test_base::assert_index(
      irs::IndexFeatures::NONE | irs::IndexFeatures::FREQ
        | irs::IndexFeatures::POS);
    index_test_base::assert_index(
      irs::IndexFeatures::NONE | irs::IndexFeatures::FREQ
        | irs::IndexFeatures::POS | irs::IndexFeatures::OFFS);
    index_test_base::assert_index(
      irs::IndexFeatures::NONE | irs::IndexFeatures::FREQ
        | irs::IndexFeatures::POS | irs::IndexFeatures::PAY);
    index_test_base::assert_index(irs::IndexFeatures::ALL);
    index_test_base::assert_columnstore();
  }

  template<typename T>
  void AssertNormColumn(
      const irs::sub_reader& segment,
      irs::string_ref name,
      const std::vector<std::pair<irs::doc_id_t, uint32_t>>& expected_values);
};


template<typename T>
void Norm2TestCase::AssertNormColumn(
    const irs::sub_reader& segment,
    irs::string_ref name,
    const std::vector<std::pair<irs::doc_id_t, uint32_t>>& expected_docs) {
  static_assert(std::is_same_v<T, irs::byte_type> ||
                std::is_same_v<T, uint16_t> ||
                std::is_same_v<T, uint32_t>);

  auto* field = segment.field(name);
  ASSERT_NE(nullptr, field);
  auto& meta = field->meta();
  ASSERT_EQ(name, meta.name);
  ASSERT_EQ(1, meta.features.size());

  auto it = meta.features.find(irs::type<irs::Norm2>::id());
  ASSERT_NE(it, meta.features.end());
  ASSERT_TRUE(irs::field_limits::valid(it->second));

  auto* column = segment.column(it->second);
  ASSERT_NE(nullptr, column);
  ASSERT_EQ(it->second, column->id());
  ASSERT_TRUE(column->name().null());

  const auto min = std::min_element(std::begin(expected_docs),
                                    std::end(expected_docs),
                                    [](auto& lhs, auto& rhs) {
                                      return lhs.second < rhs.second;
                                    });
  ASSERT_NE(std::end(expected_docs), min);
  const auto max = std::max_element(std::begin(expected_docs),
                                    std::end(expected_docs),
                                    [](auto& lhs, auto& rhs) {
                                      return lhs.second < rhs.second;
                                    });
  ASSERT_NE(std::end(expected_docs), max);
  ASSERT_LE(*min, *max);
  AssertNorm2Header(column->payload(), sizeof(T),
                    min->second, max->second);

  auto values = column->iterator(false);
  auto* cost = irs::get<irs::cost>(*values);
  ASSERT_NE(nullptr, cost);
  ASSERT_EQ(cost->estimate(), expected_docs.size());
  ASSERT_NE(nullptr, values);
  auto* payload = irs::get<irs::payload>(*values);
  ASSERT_NE(nullptr, payload);
  auto* doc = irs::get<irs::document>(*values);
  ASSERT_NE(nullptr, doc);

  irs::Norm2ReaderContext ctx;
  ASSERT_EQ(0, ctx.num_bytes);
  ASSERT_EQ(nullptr, ctx.it);
  ASSERT_EQ(nullptr, ctx.payload);
  ASSERT_EQ(nullptr, ctx.doc);
  ASSERT_TRUE(ctx.Reset(segment, it->second, *doc));
  ASSERT_EQ(sizeof(T), ctx.num_bytes);
  ASSERT_NE(nullptr, ctx.it);
  ASSERT_NE(nullptr, ctx.payload);
  ASSERT_EQ(irs::get<irs::payload>(*ctx.it), ctx.payload);
  ASSERT_EQ(doc, ctx.doc);

  auto reader = irs::Norm2::MakeReader<T>(std::move(ctx));

  for (auto expected_doc = std::begin(expected_docs);
       values->next();
       ++expected_doc) {
    ASSERT_EQ(expected_doc->first, values->value());
    ASSERT_EQ(expected_doc->first, doc->value);
    ASSERT_EQ(sizeof(T), payload->value.size());

    auto* p = payload->value.c_str();
    const auto value = irs::read<T>(p);
    ASSERT_EQ(expected_doc->second, value);
    ASSERT_EQ(value, reader());
  }
}

TEST_P(Norm2TestCase, CheckNorms) {
  const ::frozen::map<frozen::string, uint32_t, 4> kSeedMapping{
      { "name", uint32_t{1}},
      { "same", uint32_t{1} << 8},
      { "duplicated", uint32_t{1} << 15},
      { "prefix", uint32_t{1} << 14} };

  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [count = size_t{0}, &kSeedMapping](tests::document& doc,
           const std::string& name,
           const tests::json_doc_generator::json_value& data) mutable {
      if (data.is_string()) {
        const bool is_name = (name == "name");
        count += static_cast<size_t>(is_name);

        const auto it = kSeedMapping.find(frozen::string{name});
        ASSERT_NE(kSeedMapping.end(), it);

        auto field = std::make_shared<NormField>(name, data.str, count*it->second);
        doc.insert(field);

        if (is_name) {
          doc.sorted = field;
        }
      }
  });

  auto* doc0 = gen.next(); // name == 'A'
  auto* doc1 = gen.next(); // name == 'B'
  auto* doc2 = gen.next(); // name == 'C'
  auto* doc3 = gen.next(); // name == 'D'

  irs::index_writer::init_options opts;
  opts.features = Features();

  // Create actual index
  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_TRUE(insert(*writer,
    doc0->indexed.begin(), doc0->indexed.end(),
    doc0->stored.begin(), doc0->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc2->indexed.begin(), doc2->indexed.end(),
    doc2->stored.begin(), doc2->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc3->indexed.begin(), doc3->indexed.end(),
    doc3->stored.begin(), doc3->stored.end()));
  writer->commit();

  // Create expected index
  auto& expected_index = index();
  expected_index.emplace_back(writer->feature_info());
  expected_index.back().insert(
    doc0->indexed.begin(), doc0->indexed.end(),
    doc0->stored.begin(), doc0->stored.end());
  expected_index.back().insert(
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end());
  expected_index.back().insert(
    doc2->indexed.begin(), doc2->indexed.end(),
    doc2->stored.begin(), doc2->stored.end());
  expected_index.back().insert(
    doc3->indexed.begin(), doc3->indexed.end(),
    doc3->stored.begin(), doc3->stored.end());
  AssertIndex();

  auto reader = open_reader();
  ASSERT_EQ(1, reader.size());
  auto& segment = reader[0];
  ASSERT_EQ(1, segment.size());
  ASSERT_EQ(4, segment.docs_count());
  ASSERT_EQ(4, segment.live_docs_count());

  {
     constexpr frozen::string kName = "duplicated";
     const auto it = kSeedMapping.find(kName);
     ASSERT_NE(kSeedMapping.end(), it);
     const uint32_t seed{it->second};
     AssertNormColumn<uint32_t>(
         segment,
         { kName.data(), kName.size() },
         { {1, seed}, {2, seed*2}, {3, seed*3} });
  }

  {
     constexpr frozen::string kName = "name";
     const auto it = kSeedMapping.find(kName);
     ASSERT_NE(kSeedMapping.end(), it);
     const uint32_t seed{it->second};
     AssertNormColumn<uint32_t>(
         segment,
         { kName.data(), kName.size() },
         { {1, seed}, {2, seed*2}, {3, seed*3}, {4, seed*4} });
  }

  {
     constexpr frozen::string kName = "same";
     const auto it = kSeedMapping.find(kName);
     ASSERT_NE(kSeedMapping.end(), it);
     const uint32_t seed{it->second};
     AssertNormColumn<uint32_t>(
         segment,
         { kName.data(), kName.size() },
         { {1, seed}, {2, seed*2}, {3, seed*3}, {4, seed*4} });
  }

  {
     constexpr frozen::string kName = "prefix";
     const auto it = kSeedMapping.find(kName);
     ASSERT_NE(kSeedMapping.end(), it);
     const uint32_t seed{it->second};
     AssertNormColumn<uint32_t>(
         segment,
         { kName.data(), kName.size() },
         { {1, seed}, {4, seed*4} });
  }
}

TEST_P(Norm2TestCase, CheckNormsConsolidation) {
  const ::frozen::map<frozen::string, uint32_t, 4> kSeedMapping{
      { "name", uint32_t{1}},
      { "same", uint32_t{1} << 5},
      { "duplicated", uint32_t{1} << 12},
      { "prefix", uint32_t{1} << 14} };

  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [count = size_t{0}, &kSeedMapping](tests::document& doc,
           const std::string& name,
           const tests::json_doc_generator::json_value& data) mutable {
      if (data.is_string()) {
        const bool is_name = (name == "name");
        count += static_cast<size_t>(is_name);

        const auto it = kSeedMapping.find(frozen::string{name});
        ASSERT_NE(kSeedMapping.end(), it);

        auto field = std::make_shared<NormField>(name, data.str, count*it->second);
        doc.insert(field);

        if (is_name) {
          doc.sorted = field;
        }
      }
  });

  auto* doc0 = gen.next(); // name == 'A'
  auto* doc1 = gen.next(); // name == 'B'
  auto* doc2 = gen.next(); // name == 'C'
  auto* doc3 = gen.next(); // name == 'D'
  auto* doc4 = gen.next(); // name == 'E'
  auto* doc5 = gen.next(); // name == 'F'
  auto* doc6 = gen.next(); // name == 'G'

  irs::index_writer::init_options opts;
  opts.features = Features();

  // Create actual index
  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_TRUE(insert(*writer,
    doc0->indexed.begin(), doc0->indexed.end(),
    doc0->stored.begin(), doc0->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc2->indexed.begin(), doc2->indexed.end(),
    doc2->stored.begin(), doc2->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc3->indexed.begin(), doc3->indexed.end(),
    doc3->stored.begin(), doc3->stored.end()));
  writer->commit();
  ASSERT_TRUE(insert(*writer,
    doc4->indexed.begin(), doc4->indexed.end(),
    doc4->stored.begin(), doc4->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc5->indexed.begin(), doc5->indexed.end(),
    doc5->stored.begin(), doc5->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc6->indexed.begin(), doc6->indexed.end(),
    doc6->stored.begin(), doc6->stored.end()));
  writer->commit();

  // Create expected index
  auto& expected_index = index();
  expected_index.emplace_back(writer->feature_info());
  expected_index.back().insert(
    doc0->indexed.begin(), doc0->indexed.end(),
    doc0->stored.begin(), doc0->stored.end());
  expected_index.back().insert(
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end());
  expected_index.back().insert(
    doc2->indexed.begin(), doc2->indexed.end(),
    doc2->stored.begin(), doc2->stored.end());
  expected_index.back().insert(
    doc3->indexed.begin(), doc3->indexed.end(),
    doc3->stored.begin(), doc3->stored.end());
  expected_index.emplace_back(writer->feature_info());
  expected_index.back().insert(
    doc4->indexed.begin(), doc4->indexed.end(),
    doc4->stored.begin(), doc4->stored.end());
  expected_index.back().insert(
    doc5->indexed.begin(), doc5->indexed.end(),
    doc5->stored.begin(), doc5->stored.end());
  expected_index.back().insert(
    doc6->indexed.begin(), doc6->indexed.end(),
    doc6->stored.begin(), doc6->stored.end());

  AssertIndex();

  auto reader = open_reader();
  ASSERT_EQ(2, reader.size());

  {
    auto& segment = reader[0];
    ASSERT_EQ(1, segment.size());
    ASSERT_EQ(4, segment.docs_count());
    ASSERT_EQ(4, segment.live_docs_count());

    {
       constexpr frozen::string kName = "duplicated";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {2, seed*2}, {3, seed*3} });
    }

    {
       constexpr frozen::string kName = "name";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {2, seed*2}, {3, seed*3}, {4, seed*4} });
    }

    {
       constexpr frozen::string kName = "same";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {2, seed*2}, {3, seed*3}, {4, seed*4} });
    }

    {
       constexpr frozen::string kName = "prefix";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {4, seed*4} });
    }
  }

  {
    auto& segment = reader[1];
    ASSERT_EQ(1, segment.size());
    ASSERT_EQ(3, segment.docs_count());
    ASSERT_EQ(3, segment.live_docs_count());

    {
       constexpr frozen::string kName = "duplicated";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { { 1, seed*5} });
    }

    {
       constexpr frozen::string kName = "name";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed*5}, {2, seed*6}, {3, seed*7} });
    }

    {
       constexpr frozen::string kName = "same";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed*5}, {2, seed*6}, {3, seed*7} });
    }

    {
       constexpr irs::string_ref kName = "prefix";
       ASSERT_EQ(nullptr, segment.field(kName));
    }
  }

  // Consolidate segments
  {
    const irs::index_utils::consolidate_count consolidate_all;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(consolidate_all)));
    writer->commit();

    // Simulate consolidation
    index().clear();
    index().emplace_back(writer->feature_info());
    auto& segment = index().back();
    expected_index.back().insert(
      doc0->indexed.begin(), doc0->indexed.end(),
      doc0->stored.begin(), doc0->stored.end());
    expected_index.back().insert(
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end());
    expected_index.back().insert(
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end());
    expected_index.back().insert(
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end());
    expected_index.back().insert(
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end());
    expected_index.back().insert(
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end());
    expected_index.back().insert(
      doc6->indexed.begin(), doc6->indexed.end(),
      doc6->stored.begin(), doc6->stored.end());
    for (auto& column : segment.columns()) {
      column.rewrite();
    }
  }

  AssertIndex();

  reader = open_reader();
  ASSERT_EQ(1, reader.size());

  {
    auto& segment = reader[0];
    ASSERT_EQ(1, segment.size());
    ASSERT_EQ(7, segment.docs_count());
    ASSERT_EQ(7, segment.live_docs_count());

    {
       constexpr frozen::string kName = "duplicated";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint16_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {2, seed*2}, {3, seed*3}, {5, seed*5} });
    }

    {
       constexpr frozen::string kName = "name";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<irs::byte_type>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {2, seed*2},
             {3, seed*3}, {4, seed*4},
             {5, seed*5 }, {6, seed*6},
             {7, seed*7} });
    }

    {
       constexpr frozen::string kName = "same";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<irs::byte_type>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed }, {2, seed*2},
             {3, seed*3}, {4, seed*4},
             {5, seed*5 }, {6, seed*6},
             {7, seed*7} });
    }

    {
       constexpr frozen::string kName = "prefix";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {4, seed*4} });
    }
  }
}

TEST_P(Norm2TestCase, CheckNormsConsolidationWithRemovals) {
  const ::frozen::map<frozen::string, uint32_t, 4> kSeedMapping{
      { "name", uint32_t{1}},
      { "same", uint32_t{1} << 5},
      { "duplicated", uint32_t{1} << 12},
      { "prefix", uint32_t{1} << 14} };

  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [count = size_t{0}, &kSeedMapping](tests::document& doc,
           const std::string& name,
           const tests::json_doc_generator::json_value& data) mutable {
      if (data.is_string()) {
        const bool is_name = (name == "name");
        count += static_cast<size_t>(is_name);

        const auto it = kSeedMapping.find(frozen::string{name});
        ASSERT_NE(kSeedMapping.end(), it);

        auto field = std::make_shared<NormField>(name, data.str, count*it->second);
        doc.insert(field);

        if (is_name) {
          doc.sorted = field;
        }
      }
  });

  auto* doc0 = gen.next(); // name == 'A'
  auto* doc1 = gen.next(); // name == 'B'
  auto* doc2 = gen.next(); // name == 'C'
  auto* doc3 = gen.next(); // name == 'D'
  auto* doc4 = gen.next(); // name == 'E'
  auto* doc5 = gen.next(); // name == 'F'
  auto* doc6 = gen.next(); // name == 'G'

  irs::index_writer::init_options opts;
  opts.features = Features();

  // Create actual index
  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);
  ASSERT_TRUE(insert(*writer,
    doc0->indexed.begin(), doc0->indexed.end(),
    doc0->stored.begin(), doc0->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc2->indexed.begin(), doc2->indexed.end(),
    doc2->stored.begin(), doc2->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc3->indexed.begin(), doc3->indexed.end(),
    doc3->stored.begin(), doc3->stored.end()));
  writer->commit();
  ASSERT_TRUE(insert(*writer,
    doc4->indexed.begin(), doc4->indexed.end(),
    doc4->stored.begin(), doc4->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc5->indexed.begin(), doc5->indexed.end(),
    doc5->stored.begin(), doc5->stored.end()));
  ASSERT_TRUE(insert(*writer,
    doc6->indexed.begin(), doc6->indexed.end(),
    doc6->stored.begin(), doc6->stored.end()));
  writer->commit();

  // Create expected index
  auto& expected_index = index();
  expected_index.emplace_back(writer->feature_info());
  expected_index.back().insert(
    doc0->indexed.begin(), doc0->indexed.end(),
    doc0->stored.begin(), doc0->stored.end());
  expected_index.back().insert(
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end());
  expected_index.back().insert(
    doc2->indexed.begin(), doc2->indexed.end(),
    doc2->stored.begin(), doc2->stored.end());
  expected_index.back().insert(
    doc3->indexed.begin(), doc3->indexed.end(),
    doc3->stored.begin(), doc3->stored.end());
  expected_index.emplace_back(writer->feature_info());
  expected_index.back().insert(
    doc4->indexed.begin(), doc4->indexed.end(),
    doc4->stored.begin(), doc4->stored.end());
  expected_index.back().insert(
    doc5->indexed.begin(), doc5->indexed.end(),
    doc5->stored.begin(), doc5->stored.end());
  expected_index.back().insert(
    doc6->indexed.begin(), doc6->indexed.end(),
    doc6->stored.begin(), doc6->stored.end());

  AssertIndex();

  auto reader = open_reader();
  ASSERT_EQ(2, reader.size());

  {
    auto& segment = reader[0];
    ASSERT_EQ(1, segment.size());
    ASSERT_EQ(4, segment.docs_count());
    ASSERT_EQ(4, segment.live_docs_count());

    {
       constexpr frozen::string kName = "duplicated";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {2, seed*2}, {3, seed*3} });
    }

    {
       constexpr frozen::string kName = "name";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {2, seed*2}, {3, seed*3}, {4, seed*4} });
    }

    {
       constexpr frozen::string kName = "same";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {2, seed*2}, {3, seed*3}, {4, seed*4} });
    }

    {
       constexpr frozen::string kName = "prefix";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {4, seed*4} });
    }
  }

  {
    auto& segment = reader[1];
    ASSERT_EQ(1, segment.size());
    ASSERT_EQ(3, segment.docs_count());
    ASSERT_EQ(3, segment.live_docs_count());

    {
       constexpr frozen::string kName = "duplicated";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { { 1, seed*5} });
    }

    {
       constexpr frozen::string kName = "name";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed*5}, {2, seed*6}, {3, seed*7} });
    }

    {
       constexpr frozen::string kName = "same";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed*5}, {2, seed*6}, {3, seed*7} });
    }

    {
       constexpr irs::string_ref kName = "prefix";
       ASSERT_EQ(nullptr, segment.field(kName));
    }
  }

  // Remove document
  {
    auto query_doc3 = irs::iql::query_builder().build("name==D", "C");
    writer->documents().remove(*query_doc3.filter);
    writer->commit();
  }

  // Consolidate segments
  {
    const irs::index_utils::consolidate_count consolidate_all;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(consolidate_all)));
    writer->commit();

    // Simulate consolidation
    index().clear();
    index().emplace_back(writer->feature_info());
    auto& segment = index().back();
    expected_index.back().insert(
      doc0->indexed.begin(), doc0->indexed.end(),
      doc0->stored.begin(), doc0->stored.end());
    expected_index.back().insert(
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end());
    expected_index.back().insert(
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end());
    expected_index.back().insert(
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end());
    expected_index.back().insert(
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end());
    expected_index.back().insert(
      doc6->indexed.begin(), doc6->indexed.end(),
      doc6->stored.begin(), doc6->stored.end());
    for (auto& column : segment.columns()) {
      column.rewrite();
    }
  }

  //FIXME(gnusi)
  // We can't use AssertIndex() to check the
  // whole index because column headers don't match
  //AssertIndex();

  reader = open_reader();
  ASSERT_EQ(1, reader.size());

  {
    auto& segment = reader[0];
    ASSERT_EQ(1, segment.size());
    ASSERT_EQ(6, segment.docs_count());
    ASSERT_EQ(6, segment.live_docs_count());

    {
       constexpr frozen::string kName = "duplicated";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint16_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {2, seed*2}, {3, seed*3}, {4, seed*5}  });
    }

    {
       constexpr frozen::string kName = "name";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<irs::byte_type>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {2, seed*2},
             {3, seed*3}, {4, seed*5},
             {5, seed*6}, {6, seed*7} });
    }

    {
       constexpr frozen::string kName = "same";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<irs::byte_type>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed }, {2, seed*2},
             {3, seed*3}, {4, seed*5 },
             {5, seed*6}, {6, seed*7} });
    }

    {
       constexpr frozen::string kName = "prefix";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint32_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed} });
    }
  }

  ASSERT_TRUE(insert(*writer,
    doc0->indexed.begin(), doc0->indexed.end(),
    doc0->stored.begin(), doc0->stored.end()));
  writer->commit();

  // Consolidate segments
  {
    const irs::index_utils::consolidate_count consolidate_all;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(consolidate_all)));
    writer->commit();
  }

  reader = open_reader();
  ASSERT_EQ(1, reader.size());

  {
    auto& segment = reader[0];
    ASSERT_EQ(1, segment.size());
    ASSERT_EQ(7, segment.docs_count());
    ASSERT_EQ(7, segment.live_docs_count());

    {
       constexpr frozen::string kName = "duplicated";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint16_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {2, seed*2}, {3, seed*3}, {4, seed*5}, { 7, seed} });
    }

    {
       constexpr frozen::string kName = "name";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<irs::byte_type>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {2, seed*2},
             {3, seed*3}, {4, seed*5},
             {5, seed*6}, {6, seed*7}, {7, seed} });
    }

    {
       constexpr frozen::string kName = "same";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<irs::byte_type>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed }, {2, seed*2},
             {3, seed*3}, {4, seed*5 },
             {5, seed*6}, {6, seed*7}, {7, seed}  });
    }

    {
       constexpr frozen::string kName = "prefix";
       const auto it = kSeedMapping.find(kName);
       ASSERT_NE(kSeedMapping.end(), it);
       const uint32_t seed{it->second};
       AssertNormColumn<uint16_t>(
           segment,
           { kName.data(), kName.size() },
           { {1, seed}, {7, seed}  });
    }
  }
}

// Separate definition as MSVC parser fails to do conditional defines in macro expansion
#ifdef IRESEARCH_SSE2
const auto kNorm2TestCaseValues = ::testing::Values(
    tests::format_info{"1_4", "1_0"},
    tests::format_info{"1_4simd", "1_0"});
#else
const auto kNorm2TestCaseValues = ::testing::Values(
    tests::format_info{"1_4", "1_0"});
#endif

INSTANTIATE_TEST_SUITE_P(
  Norm2Test,
  Norm2TestCase,
  ::testing::Combine(
    ::testing::Values(
      &tests::directory<&tests::memory_directory>,
      &tests::directory<&tests::fs_directory>,
      &tests::directory<&tests::mmap_directory>),
    kNorm2TestCaseValues),
  Norm2TestCase::to_string);

} // namespace {
