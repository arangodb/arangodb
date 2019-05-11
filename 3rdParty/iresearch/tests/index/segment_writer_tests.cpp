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

#include "analysis/token_attributes.hpp"
#include "index/segment_writer.hpp"
#include "index/index_tests.hpp"
#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"

NS_LOCAL

class segment_writer_tests: public test_base {
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right before each test).

    test_base::SetUp();
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the destructor).

    test_base::TearDown();
  }
};

NS_END

#ifndef IRESEARCH_DEBUG

TEST_F(segment_writer_tests, invalid_actions) {
  struct token_stream_t: public irs::token_stream {
    irs::attribute_view attrs;
    size_t token_count;
    virtual const irs::attribute_view& attributes() const NOEXCEPT override { return attrs; }
    virtual bool next() override { return --token_count; }
  };

  struct field_t {
    irs::token_stream& token_stream;
    field_t(irs::token_stream& stream): token_stream(stream) {}
    float_t boost() const { return 1.f; }
    const irs::flags& features() const { return irs::flags::empty_instance(); }
    irs::token_stream& get_tokens() { return token_stream; }
    irs::string_ref& name() const { static irs::string_ref value("test_field"); return value; }
    bool write(irs::data_output& out) {
      irs::write_string(out, name());
      return true;
    }
  };

  irs::boolean_token_stream stream;
  stream.reset(true);
  field_t field(stream);

  irs::memory_directory dir;
  auto writer = irs::segment_writer::make(dir, nullptr);
  ASSERT_EQ(0, writer->memory_active());

  // store + store sorted
  {
    irs::segment_writer::update_context ctx;
    writer->begin(ctx);
    ASSERT_TRUE(writer->valid());
    ASSERT_FALSE(writer->insert<irs::Action(int(irs::Action::STORE) | int(irs::Action::STORE_SORTED))>(field));
    ASSERT_FALSE(writer->valid());
    writer->commit();
  }

  // store + store sorted
  {
    irs::segment_writer::update_context ctx;
    writer->begin(ctx);
    ASSERT_TRUE(writer->valid());
    ASSERT_FALSE(writer->insert<irs::Action(int(irs::Action::INDEX) | int(irs::Action::STORE) | int(irs::Action::STORE_SORTED))>(field));
    ASSERT_FALSE(writer->valid());
    writer->commit();
  }

  ASSERT_LT(0, writer->memory_active());

  writer->reset();

  ASSERT_EQ(0, writer->memory_active());
}

#endif

TEST_F(segment_writer_tests, memory_sorted_vs_unsorted) {
  struct field_t {
    const irs::string_ref& name() const {
      static const irs::string_ref value("test_field");
      return value;
    }

    bool write(irs::data_output& out) const {
      irs::write_string(out, name());
      return true;
    }
  } field;

  struct comparator final : irs::comparer {
    virtual bool less(const irs::bytes_ref& lhs, const irs::bytes_ref& rhs) const NOEXCEPT override {
      return lhs < rhs;
    }
  } less;

  irs::memory_directory dir;
  auto writer_sorted = irs::segment_writer::make(dir, &less);
  ASSERT_EQ(0, writer_sorted->memory_active());
  auto writer_unsorted = irs::segment_writer::make(dir, nullptr);
  ASSERT_EQ(0, writer_unsorted->memory_active());

  irs::segment_meta segment;
  segment.name = "foo";
  segment.codec = irs::formats::get("1_1");
  writer_sorted->reset(segment);
  ASSERT_EQ(0, writer_sorted->memory_active());
  writer_unsorted->reset(segment);
  ASSERT_EQ(0, writer_unsorted->memory_active());

  for (size_t i = 0; i < 100; ++i) {
    irs::segment_writer::update_context ctx;
    writer_sorted->begin(ctx);
    ASSERT_TRUE(writer_sorted->valid());
    ASSERT_TRUE(writer_sorted->insert<irs::Action::STORE>(field));
    ASSERT_TRUE(writer_sorted->valid());
    writer_sorted->commit();

    writer_unsorted->begin(ctx);
    ASSERT_TRUE(writer_unsorted->valid());
    ASSERT_TRUE(writer_unsorted->insert<irs::Action::STORE>(field));
    ASSERT_TRUE(writer_unsorted->valid());
    writer_unsorted->commit();
  }

  ASSERT_GT(writer_sorted->memory_active(), 0);
  ASSERT_GT(writer_unsorted->memory_active(), 0);

  // we don't count stored field without comparator
  ASSERT_LT(writer_unsorted->memory_active(), writer_sorted->memory_active());

  writer_sorted->reset();
  ASSERT_EQ(0, writer_sorted->memory_active());

  writer_unsorted->reset();
  ASSERT_EQ(0, writer_unsorted->memory_active());
}

TEST_F(segment_writer_tests, insert_sorted_without_comparator) {
  struct field_t {
    const irs::string_ref& name() const {
      static const irs::string_ref value("test_field");
      return value;
    }

    bool write(irs::data_output& out) const {
      irs::write_string(out, name());
      return true;
    }
  } field;

  irs::memory_directory dir;
  auto writer = irs::segment_writer::make(dir, nullptr);
  ASSERT_EQ(0, writer->memory_active());

  irs::segment_meta segment;
  segment.name = "foo";
  segment.codec = irs::formats::get("1_1");
  writer->reset(segment);
  ASSERT_EQ(0, writer->memory_active());

  for (size_t i = 0; i < 100; ++i) {
    irs::segment_writer::update_context ctx;
    writer->begin(ctx);
    ASSERT_TRUE(writer->valid());
    ASSERT_FALSE(writer->insert<irs::Action::STORE_SORTED>(field));
    ASSERT_FALSE(writer->valid());
    writer->commit();
  }

  // we don't count stored field without comparator
  ASSERT_GT(writer->memory_active(), 0);

  writer->reset();

  ASSERT_EQ(0, writer->memory_active());
}

TEST_F(segment_writer_tests, memory_store_sorted_field) {
  struct field_t {
    const irs::string_ref& name() const { 
      static const irs::string_ref value("test_field"); 
      return value; 
    }

    bool write(irs::data_output& out) const {
      irs::write_string(out, name());
      return true;
    }
  } field;

  struct comparator final : irs::comparer {
    virtual bool less(const irs::bytes_ref& lhs, const irs::bytes_ref& rhs) const NOEXCEPT override {
      return lhs < rhs;
    }
  } less;

  irs::memory_directory dir;
  auto writer = irs::segment_writer::make(dir, &less);
  ASSERT_EQ(0, writer->memory_active());

  irs::segment_meta segment;
  segment.name = "foo";
  segment.codec = irs::formats::get("1_1");
  writer->reset(segment);
  ASSERT_EQ(0, writer->memory_active());

  for (size_t i = 0; i < 100; ++i) {
    irs::segment_writer::update_context ctx;
    writer->begin(ctx);
    ASSERT_TRUE(writer->valid());
    ASSERT_TRUE(writer->insert<irs::Action::STORE_SORTED>(field));
    ASSERT_TRUE(writer->valid());
    writer->commit();
  }

  // we don't count stored field without comparator
  ASSERT_GT(writer->memory_active(), 0);

  writer->reset();

  ASSERT_EQ(0, writer->memory_active());
}

TEST_F(segment_writer_tests, memory_store_field_sorted) {
  struct field_t {
    const irs::string_ref& name() const { 
      static const irs::string_ref value("test_field"); 
      return value; 
    }

    bool write(irs::data_output& out) const {
      irs::write_string(out, name());
      return true;
    }
  } field;

  struct comparator final : irs::comparer {
    virtual bool less(const irs::bytes_ref& lhs, const irs::bytes_ref& rhs) const NOEXCEPT override {
      return lhs < rhs;
    }
  } less;

  irs::memory_directory dir;
  auto writer = irs::segment_writer::make(dir, &less);
  ASSERT_EQ(0, writer->memory_active());

  irs::segment_meta segment;
  segment.name = "foo";
  segment.codec = irs::formats::get("1_1");
  writer->reset(segment);
  ASSERT_EQ(0, writer->memory_active());

  for (size_t i = 0; i < 100; ++i) {
    irs::segment_writer::update_context ctx;
    writer->begin(ctx);
    ASSERT_TRUE(writer->valid());
    ASSERT_TRUE(writer->insert<irs::Action::STORE>(field));
    ASSERT_TRUE(writer->valid());
    writer->commit();
  }

  // we don't count stored field without comparator
  ASSERT_GT(writer->memory_active(), 0);

  writer->reset();

  ASSERT_EQ(0, writer->memory_active());
}

TEST_F(segment_writer_tests, memory_store_field_unsorted) {
  struct field_t {
    const irs::string_ref& name() const { 
      static const irs::string_ref value("test_field"); 
      return value; 
    }

    bool write(irs::data_output& out) const {
      irs::write_string(out, name());
      return true;
    }
  } field;

  irs::memory_directory dir;
  auto writer = irs::segment_writer::make(dir, nullptr);
  ASSERT_EQ(0, writer->memory_active());

  irs::segment_meta segment;
  segment.name = "foo";
  segment.codec = irs::formats::get("1_1");
  writer->reset(segment);
  ASSERT_EQ(0, writer->memory_active());

  for (size_t i = 0; i < 100; ++i) {
    irs::segment_writer::update_context ctx;
    writer->begin(ctx);
    ASSERT_TRUE(writer->valid());
    ASSERT_TRUE(writer->insert<irs::Action::STORE>(field));
    ASSERT_TRUE(writer->valid());
    writer->commit();
  }

  ASSERT_GT(writer->memory_active(), 0);

  writer->reset();

  ASSERT_EQ(0, writer->memory_active());
}

TEST_F(segment_writer_tests, memory_index_field) {
  struct token_stream_t: public irs::token_stream {
    irs::attribute_view attrs;
    size_t token_count;
    virtual const irs::attribute_view& attributes() const NOEXCEPT override { return attrs; }
    virtual bool next() override { return --token_count; }
  };

  struct field_t {
    irs::token_stream& token_stream;
    field_t(irs::token_stream& stream): token_stream(stream) {}
    float_t boost() const { return 1.f; }
    const irs::flags& features() const { return irs::flags::empty_instance(); }
    irs::token_stream& get_tokens() { return token_stream; }
    irs::string_ref& name() const { static irs::string_ref value("test_field"); return value; }
  };

  irs::boolean_token_stream stream;
  stream.reset(true);
  field_t field(stream);

  irs::memory_directory dir;
  auto writer = irs::segment_writer::make(dir, nullptr);
  ASSERT_EQ(0, writer->memory_active());

  for (size_t i = 0; i < 100; ++i) {
    irs::segment_writer::update_context ctx;
    writer->begin(ctx);
    ASSERT_TRUE(writer->valid());
    ASSERT_TRUE(writer->insert<irs::Action::INDEX>(field));
    ASSERT_TRUE(writer->valid());
    writer->commit();
  }

  ASSERT_LT(0, writer->memory_active());

  writer->reset();

  ASSERT_EQ(0, writer->memory_active());
}

TEST_F(segment_writer_tests, index_field) {
  struct token_stream_t: public irs::token_stream {
    irs::attribute_view attrs;
    size_t token_count;
    virtual const irs::attribute_view& attributes() const NOEXCEPT override { return attrs; }
    virtual bool next() override { return --token_count; }
  };

  struct field_t {
    irs::token_stream& token_stream;
    field_t(irs::token_stream& stream): token_stream(stream) {}
    float_t boost() const { return 1.f; }
    const irs::flags& features() const { return irs::flags::empty_instance(); }
    irs::token_stream& get_tokens() { return token_stream; }
    irs::string_ref& name() const { static irs::string_ref value("test_field"); return value; }
  };

  // test missing token_stream attributes (increment)
  {
    irs::memory_directory dir;
    auto writer = irs::segment_writer::make(dir, nullptr);
    irs::segment_writer::update_context ctx;
    token_stream_t stream;
    field_t field(stream);
    irs::term_attribute term;

    stream.attrs.emplace<irs::term_attribute>(term);
    stream.token_count = 10;

    writer->begin(ctx);
    ASSERT_TRUE(writer->valid());
    ASSERT_FALSE(writer->insert<irs::Action::INDEX>(field));
    ASSERT_FALSE(writer->valid());
    writer->commit();
  }

  // test missing token_stream attributes (term_attribute)
  {
    irs::memory_directory dir;
    auto writer = irs::segment_writer::make(dir, nullptr);
    irs::segment_writer::update_context ctx;
    token_stream_t stream;
    field_t field(stream);
    irs::increment inc;

    stream.attrs.emplace<irs::increment>(inc);
    stream.token_count = 10;

    writer->begin(ctx);
    ASSERT_TRUE(writer->valid());
    ASSERT_FALSE(writer->insert<irs::Action::INDEX>( field));
    ASSERT_FALSE(writer->valid());
    writer->commit();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
