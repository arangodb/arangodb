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

#include "tests_shared.hpp" 
#include "iql/query_builder.hpp"
#include "store/fs_directory.hpp"
#include "store/mmap_directory.hpp"
#include "store/memory_directory.hpp"
#include "utils/index_utils.hpp"

#include "index_tests.hpp"

#include <thread>

namespace tests {

struct incompatible_attribute : irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();

  incompatible_attribute() NOEXCEPT;
};

REGISTER_ATTRIBUTE(incompatible_attribute);
DEFINE_ATTRIBUTE_TYPE(incompatible_attribute);

incompatible_attribute::incompatible_attribute() NOEXCEPT {
}

NS_BEGIN(templates)

// ----------------------------------------------------------------------------
// --SECTION--                               token_stream_payload implemntation
// ----------------------------------------------------------------------------

token_stream_payload::token_stream_payload(irs::token_stream* impl)
  : impl_(impl) {
    assert(impl_);
    auto& attrs = const_cast<irs::attribute_view&>(impl_->attributes());
    term_ = const_cast<const irs::attribute_view&>(attrs).get<irs::term_attribute>().get();
    assert(term_);
    attrs.emplace(pay_);
}

bool token_stream_payload::next() {
  if (impl_->next()) {
    pay_.value = term_->value();
    return true;
  }
  pay_.value = irs::bytes_ref::NIL;
  return false;
}

// ----------------------------------------------------------------------------
// --SECTION--                                       string_field implemntation
// ----------------------------------------------------------------------------

string_field::string_field(
    const irs::string_ref& name,
    const irs::flags& extra_features /*= irs::flags::empty_instance()*/
): features_({ irs::frequency::type(), irs::position::type() }) {
  features_ |= extra_features;
  this->name(name);
}

string_field::string_field(
    const irs::string_ref& name,
    const irs::string_ref& value,
    const irs::flags& extra_features /*= irs::flags::empty_instance()*/
  ): features_({ irs::frequency::type(), irs::position::type() }),
     value_(value) {
  features_ |= extra_features;
  this->name(name);
}

const irs::flags& string_field::features() const {
  return features_;
}

// reject too long terms
void string_field::value(const irs::string_ref& str) {
  const auto size_len = irs::bytes_io<uint32_t>::vsize(irs::byte_block_pool::block_type::SIZE);
  const auto max_len = (std::min)(str.size(), size_t(irs::byte_block_pool::block_type::SIZE - size_len));
  auto begin = str.begin();
  auto end = str.begin() + max_len;
  value_.assign(begin, end);
}

bool string_field::write(irs::data_output& out) const {
  irs::write_string(out, value_);
  return true;
}

irs::token_stream& string_field::get_tokens() const {
  REGISTER_TIMER_DETAILED();

  stream_.reset(value_);
  return stream_;
}

// ----------------------------------------------------------------------------
// --SECTION--                                   string_ref_field implemntation
// ----------------------------------------------------------------------------

string_ref_field::string_ref_field(
    const irs::string_ref& name,
    const irs::flags& extra_features /*= irs::flags::empty_instance()*/
): features_({ irs::frequency::type(), irs::position::type() }) {
  features_ |= extra_features;
  this->name(name);
}

string_ref_field::string_ref_field(
    const irs::string_ref& name,
    const irs::string_ref& value,
    const irs::flags& extra_features /*= irs::flags::empty_instance()*/
  ): features_({ irs::frequency::type(), irs::position::type() }),
     value_(value) {
  features_ |= extra_features;
  this->name(name);
}

const irs::flags& string_ref_field::features() const {
  return features_;
}

// truncate very long terms
void string_ref_field::value(const irs::string_ref& str) {
  const auto size_len = irs::bytes_io<uint32_t>::vsize(irs::byte_block_pool::block_type::SIZE);
  const auto max_len = (std::min)(str.size(), size_t(irs::byte_block_pool::block_type::SIZE - size_len));

  value_ = irs::string_ref(str.c_str(), max_len);
}

bool string_ref_field::write(irs::data_output& out) const {
  irs::write_string(out, value_);
  return true;
}

irs::token_stream& string_ref_field::get_tokens() const {
  REGISTER_TIMER_DETAILED();

  stream_.reset(value_);
  return stream_;
}

// ----------------------------------------------------------------------------
// --SECTION--                                            europarl_doc_template
// ----------------------------------------------------------------------------

void europarl_doc_template::init() {
  clear();
  indexed.push_back(std::make_shared<tests::templates::string_field>("title"));
  indexed.push_back(std::make_shared<text_field>("title_anl", false));
  indexed.push_back(std::make_shared<text_field>("title_anl_pay", true));
  indexed.push_back(std::make_shared<text_field>("body_anl", false));
  indexed.push_back(std::make_shared<text_field>("body_anl_pay", true));
  {
    insert(std::make_shared<tests::long_field>());
    auto& field = static_cast<tests::long_field&>(indexed.back());
    field.name(irs::string_ref("date"));
  }
  insert(std::make_shared<tests::templates::string_field>("datestr"));
  insert(std::make_shared<tests::templates::string_field>("body"));
  {
    insert(std::make_shared<tests::int_field>());
    auto& field = static_cast<tests::int_field&>(indexed.back());
    field.name(irs::string_ref("id"));
  }
  insert(std::make_shared<string_field>("idstr"));
}

void europarl_doc_template::value(size_t idx, const std::string& value) {
  static auto get_time = [](const std::string& src) {
    std::istringstream ss(src);
    std::tm tmb{};
    char c;
    ss >> tmb.tm_year >> c >> tmb.tm_mon >> c >> tmb.tm_mday;
    return std::mktime( &tmb );
  };

  switch (idx) {
    case 0: // title
      title_ = value;
      indexed.get<tests::templates::string_field>("title")->value(title_);
      indexed.get<text_field>("title_anl")->value(title_);
      indexed.get<text_field>("title_anl_pay")->value(title_);
      break;
    case 1: // dateA
      indexed.get<tests::long_field>("date")->value(get_time(value));
      indexed.get<tests::templates::string_field>("datestr")->value(value);
      break;
    case 2: // body
      body_ = value;
      indexed.get<tests::templates::string_field>("body")->value(body_);
      indexed.get<text_field>("body_anl")->value(body_);
      indexed.get<text_field>("body_anl_pay")->value(body_);
      break;
  }
}

void europarl_doc_template::end() {
  ++idval_;
  indexed.get<tests::int_field>("id")->value(idval_);
  indexed.get<tests::templates::string_field>("idstr")->value(std::to_string(idval_));
}

void europarl_doc_template::reset() {
  idval_ = 0;
}

NS_END // templates

void generic_json_field_factory(
    tests::document& doc,
    const std::string& name,
    const json_doc_generator::json_value& data) {
  if (json_doc_generator::ValueType::STRING == data.vt) {
    doc.insert(std::make_shared<templates::string_field>(
      irs::string_ref(name),
      data.str
    ));
  } else if (json_doc_generator::ValueType::NIL == data.vt) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(irs::null_token_stream::value_null());
  } else if (json_doc_generator::ValueType::BOOL == data.vt && data.b) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(irs::boolean_token_stream::value_true());
  } else if (json_doc_generator::ValueType::BOOL == data.vt && !data.b) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(irs::boolean_token_stream::value_true());
  } else if (data.is_number()) {
    // 'value' can be interpreted as a double
    doc.insert(std::make_shared<tests::double_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::double_field>();
    field.name(iresearch::string_ref(name));
    field.value(data.as_number<double_t>());
  }
}

void payloaded_json_field_factory(
    tests::document& doc,
    const std::string& name,
    const json_doc_generator::json_value& data) {
  typedef templates::text_field<std::string> text_field;

  if (json_doc_generator::ValueType::STRING == data.vt) {
    // analyzed && pyaloaded
    doc.indexed.push_back(std::make_shared<text_field>(
      std::string(name.c_str()) + "_anl_pay",
      data.str,
      true
    ));

    // analyzed field
    doc.indexed.push_back(std::make_shared<text_field>(
      std::string(name.c_str()) + "_anl",
      data.str
    ));

    // not analyzed field
    doc.insert(std::make_shared<templates::string_field>(
      irs::string_ref(name),
      data.str
    ));
  } else if (json_doc_generator::ValueType::NIL == data.vt) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(irs::null_token_stream::value_null());
  } else if (json_doc_generator::ValueType::BOOL == data.vt && data.b) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(irs::boolean_token_stream::value_true());
  } else if (json_doc_generator::ValueType::BOOL == data.vt && !data.b) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(irs::boolean_token_stream::value_false());
  } else if (data.is_number()) {
    // 'value' can be interpreted as a double
    doc.insert(std::make_shared<tests::double_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::double_field>();
    field.name(iresearch::string_ref(name));
    field.value(data.as_number<double_t>());
  }
}

class index_test_case_base : public tests::index_test_base {
 public:
  void clear_writer() {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));
      }
    });

    tests::document const* doc1 = gen.next();
    tests::document const* doc2 = gen.next();
    tests::document const* doc3 = gen.next();
    tests::document const* doc4 = gen.next();
    tests::document const* doc5 = gen.next();
    tests::document const* doc6 = gen.next();

    // test import/insert/deletes/existing all empty after clear
    {
      irs::memory_directory data_dir;
      auto writer = open_writer();

      writer->commit(); // create initial empty segment

      // populate 'import' dir
      {
        auto data_writer = irs::index_writer::make(data_dir, codec(), irs::OM_CREATE);
        ASSERT_TRUE(insert(*data_writer,
          doc1->indexed.begin(), doc1->indexed.end(),
          doc1->stored.begin(), doc1->stored.end()
        ));
        ASSERT_TRUE(insert(*data_writer,
          doc2->indexed.begin(), doc2->indexed.end(),
          doc2->stored.begin(), doc2->stored.end()
        ));
        ASSERT_TRUE(insert(*data_writer,
          doc3->indexed.begin(), doc3->indexed.end(),
          doc3->stored.begin(), doc3->stored.end()
        ));
        data_writer->commit();

        auto reader = irs::directory_reader::open(data_dir);
        ASSERT_EQ(1, reader.size());
        ASSERT_EQ(3, reader.docs_count());
        ASSERT_EQ(3, reader.live_docs_count());
      }

      {
        auto reader = irs::directory_reader::open(dir(), codec());
        ASSERT_EQ(0, reader.size());
        ASSERT_EQ(0, reader.docs_count());
        ASSERT_EQ(0, reader.live_docs_count());
      }

      // add sealed segment
      {
        ASSERT_TRUE(insert(*writer,
          doc4->indexed.begin(), doc4->indexed.end(),
          doc4->stored.begin(), doc4->stored.end()
        ));
        ASSERT_TRUE(insert(*writer,
          doc5->indexed.begin(), doc5->indexed.end(),
          doc5->stored.begin(), doc5->stored.end()
        ));
        writer->commit();
      }

      {
        auto reader = irs::directory_reader::open(dir(), codec());
        ASSERT_EQ(1, reader.size());
        ASSERT_EQ(2, reader.docs_count());
        ASSERT_EQ(2, reader.live_docs_count());
      }

      // add insert/remove/import
      {
        auto query_doc4 = iresearch::iql::query_builder().build("name==D", std::locale::classic());
        auto reader = irs::directory_reader::open(data_dir);

        ASSERT_TRUE(insert(*writer,
          doc6->indexed.begin(), doc6->indexed.end(),
          doc6->stored.begin(), doc6->stored.end()
        ));
        writer->documents().remove(std::move(query_doc4.filter));
        ASSERT_TRUE(writer->import(irs::directory_reader::open(data_dir)));
      }

      size_t file_count = 0;

      {
        dir().visit([&file_count](std::string&)->bool{ ++file_count; return true; });
      }

      writer->clear();

      // should be empty after clear
      {
        auto reader = irs::directory_reader::open(dir(), codec());
        ASSERT_EQ(0, reader.size());
        ASSERT_EQ(0, reader.docs_count());
        ASSERT_EQ(0, reader.live_docs_count());
        size_t file_count_post_clear = 0;
        dir().visit([&file_count_post_clear](std::string const&)->bool{
          ++file_count_post_clear;
          return true;
        });
        ASSERT_EQ(file_count + 1, file_count_post_clear); // +1 extra file for new empty index meta
      }

      writer->commit();

      // should be empty after commit (no new files or uncomited changes)
      {
        auto reader = irs::directory_reader::open(dir(), codec());
        ASSERT_EQ(0, reader.size());
        ASSERT_EQ(0, reader.docs_count());
        ASSERT_EQ(0, reader.live_docs_count());
        size_t file_count_post_commit = 0;
        dir().visit([&file_count_post_commit](std::string&)->bool{ ++file_count_post_commit; return true; });
        ASSERT_EQ(file_count + 1, file_count_post_commit); // +1 extra file for new empty index meta
      }
    }

    // test creation of an empty writer
    {
      irs::memory_directory dir;
      auto writer = irs::index_writer::make(dir, codec(), irs::OM_CREATE);
      ASSERT_THROW(irs::directory_reader::open(dir), irs::index_not_found); // throws due to missing index

      {
        size_t file_count = 0;

        dir.visit([&file_count](std::string&)->bool{ ++file_count; return true; });
        ASSERT_EQ(0, file_count); // empty dierctory
      }

      writer->clear();

      {
        size_t file_count = 0;

        dir.visit([&file_count](std::string&)->bool{ ++file_count; return true; });
        ASSERT_EQ(1, file_count); // +1 file for new empty index meta
      }

      auto reader = irs::directory_reader::open(dir);
      ASSERT_EQ(0, reader.size());
      ASSERT_EQ(0, reader.docs_count());
      ASSERT_EQ(0, reader.live_docs_count());
    }

    // ensue double clear does not increment meta
    {
      auto writer = open_writer();

      ASSERT_TRUE(insert(*writer,
        doc1->indexed.begin(), doc1->indexed.end(),
        doc1->stored.begin(), doc1->stored.end()
      ));
      writer->commit();

      size_t file_count0 = 0;
      dir().visit([&file_count0](std::string&)->bool{ ++file_count0; return true; });

      writer->clear();

      size_t file_count1 = 0;
      dir().visit([&file_count1](std::string&)->bool{ ++file_count1; return true; });
      ASSERT_EQ(file_count0 + 1, file_count1); // +1 extra file for new empty index meta

      writer->clear();

      size_t file_count2 = 0;
      dir().visit([&file_count2](std::string&)->bool{ ++file_count2; return true; });
      ASSERT_EQ(file_count1, file_count2);
    }
  }

  void concurrent_read_index() {
    // write test docs
    {
      tests::json_doc_generator gen(
        resource("simple_single_column_multi_term.json"),
        &tests::payloaded_json_field_factory
      );
      add_segment(gen);
    }

    auto& expected_index = index();
    auto actual_reader = irs::directory_reader::open(dir(), codec());
    ASSERT_FALSE(!actual_reader);
    ASSERT_EQ(1, actual_reader.size());
    ASSERT_EQ(expected_index.size(), actual_reader.size());

    size_t thread_count = 16; // arbitrary value > 1
    std::vector<tests::field_reader> expected_segments;
    std::vector<const irs::term_reader*> expected_terms; // used to validate terms
    std::vector<irs::seek_term_iterator::ptr> expected_term_itrs; // used to validate docs

    auto& actual_segment = actual_reader[0];
    auto actual_terms = actual_segment.field("name_anl_pay");
    ASSERT_FALSE(!actual_terms);

    for (size_t i = 0; i < thread_count; ++i) {
      expected_segments.emplace_back(expected_index[0]);
      expected_terms.emplace_back(expected_segments.back().field("name_anl_pay"));
      ASSERT_TRUE(nullptr != expected_terms.back());
      expected_term_itrs.emplace_back(expected_terms.back()->iterator());
      ASSERT_FALSE(!expected_term_itrs.back());
    }

    std::mutex mutex;

    // validate terms async
    {
      irs::async_utils::thread_pool pool(thread_count, thread_count);

      {
        std::lock_guard<std::mutex> lock(mutex);

        for (size_t i = 0; i < thread_count; ++i) {
          auto& act_terms = actual_terms;
          auto& exp_terms = expected_terms[i];

          pool.run([&mutex, &act_terms, &exp_terms]()->void {
            {
              // wait for all threads to be registered
              std::lock_guard<std::mutex> lock(mutex);
            }

            auto act_term_itr = act_terms->iterator();
            auto exp_terms_itr = exp_terms->iterator();
            ASSERT_FALSE(!act_term_itr);
            ASSERT_FALSE(!exp_terms_itr);

            while (act_term_itr->next()) {
              ASSERT_TRUE(exp_terms_itr->next());
              ASSERT_EQ(exp_terms_itr->value(), act_term_itr->value());
            }

            ASSERT_FALSE(exp_terms_itr->next());
          });
        }
      }

      pool.stop();
    }

    // validate docs async
    {
      auto actual_term_itr = actual_terms->iterator();

      while (actual_term_itr->next()) {
        for (size_t i = 0; i < thread_count; ++i) {
          ASSERT_TRUE(expected_term_itrs[i]->next());
          ASSERT_EQ(expected_term_itrs[i]->value(), actual_term_itr->value());
        }

        irs::async_utils::thread_pool pool(thread_count, thread_count);

        {
          std::lock_guard<std::mutex> lock(mutex);

          for (size_t i = 0; i < thread_count; ++i) {
            auto& act_term_itr = actual_term_itr;
            auto& exp_term_itr = expected_term_itrs[i];

            pool.run([&mutex, &act_term_itr, &exp_term_itr]()->void {
              const irs::flags features({ irs::frequency::type(), irs::offset::type(), irs::position::type(), irs::payload::type() });
              irs::doc_iterator::ptr act_docs_itr;
              irs::doc_iterator::ptr exp_docs_itr;

              {
                // wait for all threads to be registered
                std::lock_guard<std::mutex> lock(mutex);

                // iterators are not thread-safe
                act_docs_itr = act_term_itr->postings(features); // this step creates 3 internal iterators
                exp_docs_itr = exp_term_itr->postings(features); // this step creates 3 internal iterators
              }

              auto& actual_attrs = act_docs_itr->attributes();
              auto& expected_attrs = exp_docs_itr->attributes();
              ASSERT_EQ(expected_attrs.features(), actual_attrs.features());

              auto& actual_freq = actual_attrs.get<irs::frequency>();
              auto& expected_freq = expected_attrs.get<irs::frequency>();
              ASSERT_FALSE(!actual_freq);
              ASSERT_FALSE(!expected_freq);

              auto& actual_pos = actual_attrs.get<irs::position>();
              auto& expected_pos = expected_attrs.get<irs::position>();
              ASSERT_FALSE(!actual_pos);
              ASSERT_FALSE(!expected_pos);

              while (act_docs_itr->next()) {
                ASSERT_TRUE(exp_docs_itr->next());
                ASSERT_EQ(exp_docs_itr->value(), act_docs_itr->value());
                ASSERT_EQ(expected_freq->value, actual_freq->value);

                auto& actual_offs = actual_pos->attributes().get<irs::offset>();
                auto& expected_offs = expected_pos->attributes().get<irs::offset>();
                ASSERT_FALSE(!actual_offs);
                ASSERT_FALSE(!expected_offs);

                auto& actual_pay = actual_pos->attributes().get<irs::payload>();
                auto& expected_pay = expected_pos->attributes().get<irs::payload>();
                ASSERT_FALSE(!actual_pay);
                ASSERT_FALSE(!expected_pay);

                while(actual_pos->next()) {
                  ASSERT_TRUE(expected_pos->next());
                  ASSERT_EQ(expected_pos->value(), actual_pos->value());
                  ASSERT_EQ(expected_offs->start, actual_offs->start);
                  ASSERT_EQ(expected_offs->end, actual_offs->end);
                  ASSERT_EQ(expected_pay->value, actual_pay->value);
                }

                ASSERT_FALSE(expected_pos->next());
              }

              ASSERT_FALSE(exp_docs_itr->next());
            });
          }
        }

        pool.stop();
      }

      for (size_t i = 0; i < thread_count; ++i) {
        ASSERT_FALSE(expected_term_itrs[i]->next());
      }
    }
  }

  void open_writer_check_directory_allocator() {
    // use global allocator everywhere
    {
      irs::memory_directory dir;
      ASSERT_FALSE(dir.attributes().get<irs::memory_allocator>());
      ASSERT_EQ(&irs::memory_allocator::global(), &irs::directory_utils::get_allocator(dir));

      // open writer
      auto writer = irs::index_writer::make(dir, codec(), irs::OM_CREATE);
      ASSERT_NE(nullptr, writer);
      ASSERT_FALSE(dir.attributes().get<irs::memory_allocator>());
      ASSERT_EQ(&irs::memory_allocator::global(), &irs::directory_utils::get_allocator(dir));
    }

    // use global allocator in directory
    {
      irs::memory_directory dir;
      ASSERT_FALSE(dir.attributes().get<irs::memory_allocator>());
      ASSERT_EQ(&irs::memory_allocator::global(), &irs::directory_utils::get_allocator(dir));

      // open writer
      irs::index_writer::options options;
      options.memory_pool_size = 42;
      auto writer = irs::index_writer::make(dir, codec(), irs::OM_CREATE, options);
      ASSERT_NE(nullptr, writer);
      auto* alloc_attr = dir.attributes().get<irs::memory_allocator>();
      ASSERT_NE(nullptr, alloc_attr);
      ASSERT_NE(nullptr, *alloc_attr);
      ASSERT_NE(&irs::memory_allocator::global(), alloc_attr->get());
    }

    // use memory directory allocator everywhere
    {
      irs::memory_directory dir(42);
      auto* alloc_attr_before = dir.attributes().get<irs::memory_allocator>();
      ASSERT_NE(nullptr, alloc_attr_before);
      ASSERT_NE(nullptr, *alloc_attr_before);
      ASSERT_EQ(alloc_attr_before->get(), &irs::directory_utils::get_allocator(dir));

      // open writer
      auto writer = irs::index_writer::make(dir, codec(), irs::OM_CREATE);
      ASSERT_NE(nullptr, writer);
      auto* alloc_attr_after = dir.attributes().get<irs::memory_allocator>();
      ASSERT_EQ(alloc_attr_after, alloc_attr_before);
      ASSERT_EQ(*alloc_attr_after, *alloc_attr_before);
      ASSERT_EQ(alloc_attr_after->get(), &irs::directory_utils::get_allocator(dir));
    }
  }

  void open_writer_check_lock() {
    {
      // open writer
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);
      ASSERT_NE(nullptr, writer);
      // can't open another writer at the same time on the same directory
      ASSERT_THROW(irs::index_writer::make(dir(), codec(), irs::OM_CREATE), irs::lock_obtain_failed);
      ASSERT_EQ(0, writer->buffered_docs());
    }

    {
      // open writer
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);
      ASSERT_NE(nullptr, writer);

      writer->commit();
      iresearch::directory_cleaner::clean(dir());
      // can't open another writer at the same time on the same directory
      ASSERT_THROW(irs::index_writer::make(dir(), codec(), irs::OM_CREATE), irs::lock_obtain_failed);
      ASSERT_EQ(0, writer->buffered_docs());
    }

    {
      // open writer
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);
      ASSERT_NE(nullptr, writer);

      ASSERT_EQ(0, writer->buffered_docs());
    }

    {
      // open writer with NOLOCK hint
      irs::index_writer::options options0;
      options0.lock_repository = false;
      auto writer0 = irs::index_writer::make(dir(), codec(), irs::OM_CREATE, options0);
      ASSERT_NE(nullptr, writer0);

      // can open another writer at the same time on the same directory
      irs::index_writer::options options1;
      options1.lock_repository = false;
      auto writer1 = irs::index_writer::make(dir(), codec(), irs::OM_CREATE, options1);
      ASSERT_NE(nullptr, writer1);

      ASSERT_EQ(0, writer0->buffered_docs());
      ASSERT_EQ(0, writer1->buffered_docs());
    }

    {
      // open writer with NOLOCK hint
      irs::index_writer::options options0;
      options0.lock_repository = false;
      auto writer0 = irs::index_writer::make(dir(), codec(), irs::OM_CREATE, options0);
      ASSERT_NE(nullptr, writer0);

      // can open another writer at the same time on the same directory and acquire lock
      auto writer1 = irs::index_writer::make(dir(), codec(), irs::OM_CREATE | irs::OM_APPEND);
      ASSERT_NE(nullptr, writer1);

      ASSERT_EQ(0, writer0->buffered_docs());
      ASSERT_EQ(0, writer1->buffered_docs());
    }

    {
      // open writer with NOLOCK hint
      irs::index_writer::options options0;
      options0.lock_repository = false;
      auto writer0 = irs::index_writer::make(dir(), codec(), irs::OM_CREATE, options0);
      ASSERT_NE(nullptr, writer0);
      writer0->commit();

      // can open another writer at the same time on the same directory and acquire lock
      auto writer1 = irs::index_writer::make(dir(), codec(), irs::OM_APPEND);
      ASSERT_NE(nullptr, writer1);

      ASSERT_EQ(0, writer0->buffered_docs());
      ASSERT_EQ(0, writer1->buffered_docs());
    }
  }

  void writer_check_open_modes() {
    // APPEND to nonexisting index, shoud fail
    ASSERT_THROW(irs::index_writer::make(dir(), codec(), irs::OM_APPEND), irs::file_not_found);
    // read index in empty directory, should fail
    ASSERT_THROW(irs::directory_reader::open(dir(), codec()), irs::index_not_found);

    // create empty index
    {
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);

      writer->commit();
    }

    // read empty index, it should not fail
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(0, reader.live_docs_count());
      ASSERT_EQ(0, reader.docs_count());
      ASSERT_EQ(0, reader.size());
      ASSERT_EQ(reader.begin(), reader.end());
    }

    // append to index
    {
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_APPEND);
      tests::json_doc_generator gen(
        resource("simple_sequential.json"), 
        &tests::generic_json_field_factory
      );
      tests::document const* doc1 = gen.next();
      ASSERT_EQ(0, writer->buffered_docs());
      ASSERT_TRUE(
        insert(*writer,
          doc1->indexed.begin(), doc1->indexed.end(),
          doc1->stored.begin(), doc1->stored.end()
        )
      );
      ASSERT_EQ(1, writer->buffered_docs());
      writer->commit();
      ASSERT_EQ(0, writer->buffered_docs());
    }

    // read index, it should not fail
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.live_docs_count());
      ASSERT_EQ(1, reader.docs_count());
      ASSERT_EQ(1, reader.size());
      ASSERT_NE(reader.begin(), reader.end());
    }

    // append to index
    {
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_APPEND | irs::OM_CREATE);
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &tests::generic_json_field_factory
      );
      tests::document const* doc1 = gen.next();
      ASSERT_EQ(0, writer->buffered_docs());
      ASSERT_TRUE(
        insert(*writer,
          doc1->indexed.begin(), doc1->indexed.end(),
          doc1->stored.begin(), doc1->stored.end()
        )
      );
      ASSERT_EQ(1, writer->buffered_docs());
      writer->commit();
      ASSERT_EQ(0, writer->buffered_docs());
    }

    // read index, it should not fail
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(2, reader.live_docs_count());
      ASSERT_EQ(2, reader.docs_count());
      ASSERT_EQ(2, reader.size());
      ASSERT_NE(reader.begin(), reader.end());
    }
  }

  void writer_transaction_isolation() {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      [] (tests::document& doc, const std::string& name, const json_doc_generator::json_value& data) {
        if (json_doc_generator::ValueType::STRING == data.vt) {
          doc.insert(std::make_shared<templates::string_field>(
            irs::string_ref(name),
            data.str
          ));
        }
      });
      tests::document const* doc1 = gen.next();
      tests::document const* doc2 = gen.next();

      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);

      ASSERT_TRUE(
        insert(*writer,
          doc1->indexed.begin(), doc1->indexed.end(),
          doc1->stored.begin(), doc1->stored.end()
        )
      );
      ASSERT_EQ(1, writer->buffered_docs());
      writer->begin(); // start transaction #1 
      ASSERT_EQ(0, writer->buffered_docs());
      ASSERT_TRUE(
        insert(*writer,
          doc2->indexed.begin(), doc2->indexed.end(),
          doc2->stored.begin(), doc2->stored.end()
        )
      ); // add another document while transaction in opened
      ASSERT_EQ(1, writer->buffered_docs());
      writer->commit(); // finish transaction #1
      ASSERT_EQ(1, writer->buffered_docs()); // still have 1 buffered document not included into transaction #1

      // check index, 1 document in 1 segment 
      {
        auto reader = irs::directory_reader::open(dir(), codec());
        ASSERT_EQ(1, reader.live_docs_count());
        ASSERT_EQ(1, reader.docs_count());
        ASSERT_EQ(1, reader.size());
        ASSERT_NE(reader.begin(), reader.end());
      }

      writer->commit(); // transaction #2
      ASSERT_EQ(0, writer->buffered_docs());
      // check index, 2 documents in 2 segments
      {
        auto reader = irs::directory_reader::open(dir(), codec());
        ASSERT_EQ(2, reader.live_docs_count());
        ASSERT_EQ(2, reader.docs_count());
        ASSERT_EQ(2, reader.size());
        ASSERT_NE(reader.begin(), reader.end());
      }

      // check documents
      {
        auto reader = irs::directory_reader::open(dir(), codec());
        irs::bytes_ref actual_value;

        // segment #1
        {
          auto& segment = reader[0];
          const auto* column = segment.column_reader("name");
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
          ASSERT_FALSE(docsItr->next());
        }

        // segment #1
        {
          auto& segment = reader[1];
          auto* column = segment.column_reader("name");
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
          ASSERT_FALSE(docsItr->next());
        }
      }
  }

  void writer_begin_rollback() {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"), 
      &tests::generic_json_field_factory
    );

    irs::bytes_ref actual_value;

    tests::document const* doc1 = gen.next();
    tests::document const* doc2 = gen.next();
    tests::document const* doc3 = gen.next();

    {
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);

      ASSERT_TRUE(
        insert(*writer,
          doc1->indexed.begin(), doc1->indexed.end(),
          doc1->stored.begin(), doc1->stored.end()
        )
      );
      writer->rollback(); // does nothing
      ASSERT_EQ(1, writer->buffered_docs());
      ASSERT_TRUE(writer->begin());
      ASSERT_FALSE(writer->begin()); // try to begin already opened transaction 

      // index still does not exist
      ASSERT_THROW(irs::directory_reader::open(dir(), codec()), irs::index_not_found);

      writer->rollback(); // rollback transaction
      writer->rollback(); // does nothing
      ASSERT_EQ(0, writer->buffered_docs());

      writer->commit(); // commit

      // check index, it should be empty 
      {
        auto reader = irs::directory_reader::open(dir(), codec());
        ASSERT_EQ(0, reader.live_docs_count());
        ASSERT_EQ(0, reader.docs_count());
        ASSERT_EQ(0, reader.size());
        ASSERT_EQ(reader.begin(), reader.end());
      }
    }

    // test rolled-back index can still be opened after directory cleaner run
    {
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);
      ASSERT_TRUE(insert(*writer,
        doc2->indexed.begin(), doc2->indexed.end(),
        doc2->stored.begin(), doc2->stored.end()
      ));
      ASSERT_TRUE(writer->begin()); // prepare for commit tx #1
      writer->commit(); // commit tx #1
      auto file_count = 0;
      auto dir_visitor = [&file_count](std::string&)->bool { ++file_count; return true; };
      irs::directory_utils::remove_all_unreferenced(dir()); // clear any unused files before taking count
      dir().visit(dir_visitor);
      auto file_count_before = file_count;
      ASSERT_TRUE(insert(*writer,
        doc3->indexed.begin(), doc3->indexed.end(),
        doc3->stored.begin(), doc3->stored.end()
      ));
      ASSERT_TRUE(writer->begin()); // prepare for commit tx #2
      writer->rollback(); // rollback tx #2
      iresearch::directory_utils::remove_all_unreferenced(dir());
      file_count = 0;
      dir().visit(dir_visitor);
      ASSERT_EQ(file_count_before, file_count); // ensure rolled back file refs were released

      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = reader[0]; // assume 0 is id of first/only segment
      auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }
  }

  void concurrent_read_single_column_smoke() {
    tests::json_doc_generator gen(resource("simple_sequential.json"), &tests::generic_json_field_factory);
    std::vector<const tests::document*> expected_docs;

    // write some data into columnstore
    auto writer = open_writer();
    for (auto* doc = gen.next(); doc; doc = gen.next()) {
      ASSERT_TRUE(insert(*writer,
        doc->indexed.end(), doc->indexed.end(), 
        doc->stored.begin(), doc->stored.end()
      ));
      expected_docs.push_back(doc);
    }
    writer->commit();

    auto reader = open_reader();

    // 1-st iteration: noncached
    // 2-nd iteration: cached
    for (size_t i = 0; i < 2; ++i) {
      auto read_columns = [&expected_docs, &reader] () {
        size_t i = 0;
        irs::bytes_ref actual_value;
        for (auto& segment: reader) {
          auto* column = segment.column_reader("name");
          if (!column) {
            return false;
          }
          auto values = column->values();
          const auto docs = segment.docs_count();
          for (iresearch::doc_id_t doc = (iresearch::type_limits<iresearch::type_t::doc_id_t>::min)(), max = segment.docs_count(); doc <= max; ++doc) {
            if (!values(doc, actual_value)) {
              return false;
            }

            auto* expected_doc = expected_docs[i];
            auto expected_name = expected_doc->stored.get<tests::templates::string_field>("name")->value();
            if (expected_name != irs::to_string<irs::string_ref>(actual_value.c_str())) {
              return false;
            }

            ++i;
          }
        }

        return true;
      };

      std::mutex mutex;
      bool ready = false;
      std::condition_variable ready_cv;

      auto wait_for_all = [&mutex, &ready, &ready_cv]() {
        // wait for all threads to be registered
        std::unique_lock<std::mutex> lock(mutex);
        while (!ready) {
          ready_cv.wait(lock);
        }
      };

      const auto thread_count = 10;
      std::vector<int> results(thread_count, 0);
      std::vector<std::thread> pool;

      for (size_t i = 0; i < thread_count; ++i) {
        auto& result = results[i];
        pool.emplace_back(std::thread([&wait_for_all, &result, &read_columns] () {
          wait_for_all();
          result = static_cast<int>(read_columns());
        }));
      }

      // all threads registered... go, go, go...
      {
        std::lock_guard<decltype(mutex)> lock(mutex);
        ready = true;
        ready_cv.notify_all();
      }

      for (auto& thread : pool) {
        thread.join();
      }

      ASSERT_TRUE(std::all_of(
        results.begin(), results.end(), [] (int res) { return 1 == res; }
      ));
    }
  }
  
  void concurrent_read_multiple_columns() {
    struct csv_doc_template_t: public tests::csv_doc_generator::doc_template {
      virtual void init() {
        clear();
        reserve(2);
        insert(std::make_shared<tests::templates::string_field>("id"));
        insert(std::make_shared<tests::templates::string_field>("label"));
      }

      virtual void value(size_t idx, const irs::string_ref& value) {
        switch(idx) {
         case 0:
          indexed.get<tests::templates::string_field>("id")->value(value);
          break;
         case 1:
          indexed.get<tests::templates::string_field>("label")->value(value);
        }
      }
    };

    // write columns 
    {
      csv_doc_template_t csv_doc_template;
      tests::csv_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template);
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);

      const tests::document* doc;
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(*writer,
          doc->indexed.end(), doc->indexed.end(), 
          doc->stored.begin(), doc->stored.end()
        ));
      }
      writer->commit();
    }

    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());

    // read columns 
    {
      auto visit_column = [&segment] (const iresearch::string_ref& column_name) {
        auto* meta = segment.column(column_name);
        if (!meta) {
          return false;
        }

        irs::doc_id_t expected_id = 0;
        csv_doc_template_t csv_doc_template;
        tests::csv_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template);
        auto visitor = [&gen, &column_name, &expected_id] (irs::doc_id_t id, const irs::bytes_ref& actual_value) {
          if (id != ++expected_id) {
            return false;
          }

          auto* doc = gen.next();

          if (!doc) {
            return false;
          }

          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          if (!field) {
            return false;
          }

          if (field->value() != irs::to_string<irs::string_ref>(actual_value.c_str())) {
            return false;
          }

          return true;
        };

        auto* column = segment.column_reader(meta->id);

        if (!column) {
          return false;
        }

        return column->visit(visitor);
      };

      auto read_column_offset = [&segment](const iresearch::string_ref& column_name, irs::doc_id_t offset) {
        auto* meta = segment.column(column_name);
        if (!meta) {
          return false;
        }

        irs::doc_id_t expected_id = 0;
        csv_doc_template_t csv_doc_template;
        tests::csv_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template);
        const tests::document* doc = nullptr;

        auto column = segment.column_reader(meta->id);
        if (!column) {
          return false;
        }
        auto reader = column->values();

        irs::bytes_ref actual_value;

        // skip first 'offset' docs
        doc = gen.next();
        for (irs::doc_id_t id = 0; id < offset && doc; ++id) {
          doc = gen.next();
        }

        if (!doc) {
          // not enough documents to skip
          return false;
        }

        while (doc) {
          if (!reader(offset + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value)) {
            return false;
          }

          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          if (!field) {
            return false;
          }

          if (field->value() != irs::to_string<irs::string_ref>(actual_value.c_str())) {
            return false;
          }

          ++offset;
          doc = gen.next();
        }

        return true;
      };

      auto iterate_column = [&segment](const irs::string_ref& column_name) {
        auto* meta = segment.column(column_name);
        if (!meta) {
          return false;
        }

        irs::doc_id_t expected_id = 0;
        csv_doc_template_t csv_doc_template;
        tests::csv_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template);
        const tests::document* doc = nullptr;

        auto column = segment.column_reader(meta->id);

        if (!column) {
          return false;
        }

        auto it = column->iterator();

        if (!it) {
          return false;
        }

        auto& payload = it->attributes().get<irs::payload_iterator>();

        if (!payload || payload->next()) {
          return false;
        }

        doc = gen.next();

        if (!doc) {
          return false;
        }

        while (doc) {
          if (!it->next()) {
            return false;
          }

          if (!payload->next()) {
            return false;
          }

          if (++expected_id != it->value()) {
            return false;
          }

          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          if (!field) {
            return false;
          }

          if (field->value() != irs::to_string<irs::string_ref>(payload->value().c_str())) {
            return false;
          }

          doc = gen.next();
        }

        return true;
      };

      const auto thread_count = 9;
      std::vector<int> results(thread_count, 0);
      std::vector<std::thread> pool;

      const iresearch::string_ref id_column = "id";
      const iresearch::string_ref label_column = "label";

      std::mutex mutex;
      bool ready = false;
      std::condition_variable ready_cv;

      auto wait_for_all = [&mutex, &ready, &ready_cv]() {
        // wait for all threads to be registered
        std::unique_lock<std::mutex> lock(mutex);
        while (!ready) {
          ready_cv.wait(lock);
        }
      };

      // add visiting threads
      auto i = 0;
      for (auto max = thread_count/3; i < max; ++i) {
        auto& result = results[i];
        auto& column_name = i % 2 ? id_column : label_column;
        pool.emplace_back(std::thread([&wait_for_all, &result, &visit_column, column_name] () {
          wait_for_all();
          result = static_cast<int>(visit_column(column_name));
        }));
      }

      // add reading threads
      irs::doc_id_t skip = 0;
      for (; i < 2*(thread_count/3); ++i) {
        auto& result = results[i];
        auto& column_name = i % 2 ? id_column : label_column;
        pool.emplace_back(std::thread([&wait_for_all, &result, &read_column_offset, column_name, skip] () {
          wait_for_all();
          result = static_cast<int>(read_column_offset(column_name, skip));
        }));
        skip += 10000;
      }

      // add iterating threads
      for (; i < thread_count; ++i) {
        auto& result = results[i];
        auto& column_name = i % 2 ? id_column : label_column;
        pool.emplace_back(std::thread([&wait_for_all, &result, &iterate_column, column_name] () {
          wait_for_all();
          result = static_cast<int>(iterate_column(column_name));
        }));
      }

      // all threads registered... go, go, go...
      {
        std::lock_guard<decltype(mutex)> lock(mutex);
        ready = true;
        ready_cv.notify_all();
      }

      for (auto& thread : pool) {
        thread.join();
      }

      ASSERT_TRUE(std::all_of(
        results.begin(), results.end(), [] (int res) { return 1 == res; }
      ));
    }
  }

  void read_empty_doc_attributes() {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      &tests::generic_json_field_factory
    );
    tests::document const* doc1 = gen.next();
    tests::document const* doc2 = gen.next();
    tests::document const* doc3 = gen.next();
    tests::document const* doc4 = gen.next();

    // write documents without attributes
    {
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);

      // fields only
      ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end()));
      ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end()));
      ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end()));
      ASSERT_TRUE(insert(*writer, doc4->indexed.begin(), doc4->indexed.end()));
      writer->commit();
    }

    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());

    const auto* column = segment.column_reader("name");
    ASSERT_EQ(nullptr, column);
  }

  void read_write_doc_attributes_sparse_column_sparse_mask() {
    // sparse_column<sparse_mask_block>

    static const irs::doc_id_t MAX_DOCS = 1500;
    static const iresearch::string_ref column_name = "id";

    // write documents
    {
      struct stored {
        const irs::string_ref& name() { return column_name; }
        bool write(irs::data_output& out) { return true; }
      } field;

      irs::doc_id_t docs_count = 0;
      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      auto ctx = writer->documents();

      do {
        auto doc = ctx.insert();

        if (docs_count % 2) {
          doc.insert(irs::action::store, field);
        }
      } while (++docs_count < MAX_DOCS); // insert MAX_DOCS/2 documents

      { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // check number of documents in the column
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_EQ(MAX_DOCS/2, column->size());
      }

      // visit values (not cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          expected_doc += 2;
          ++docs_count;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // read values
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();
        irs::bytes_ref actual_value;

        // read (not cached)
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_EQ(i % 2, values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }

        // read (cached)
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_EQ(i % 2, values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          expected_doc += 2;
          ++docs_count;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - iterate (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          expected_doc += 2;
          ++docs_count;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // iterate over column (not cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // read values
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();
        irs::bytes_ref actual_value;

        // read (cached)
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_EQ(i % 2, values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          expected_doc += 2;
          ++docs_count;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - seek (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          expected_doc += 2;
          ++docs_count;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // seek over column (not cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // seek over column (not cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          ASSERT_EQ(expected_doc, it->seek(expected_doc - 1)); // seek before the existing key (value should remain the same)
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // seek over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        size_t docs_count = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          ASSERT_EQ(expected_doc, it->seek(expected_doc)); // seek to the existing key (value should remain the same)
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // seek to the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        size_t docs_count = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        expected_doc += 2;
        ++docs_count;

        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // seek before the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        size_t docs_count = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
        ASSERT_TRUE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        expected_doc += 2;
        ++docs_count;

        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          expected_doc += 2;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(MAX_DOCS, it->seek(MAX_DOCS));
        ASSERT_TRUE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(MAX_DOCS, it->seek(MAX_DOCS - 1));
        ASSERT_TRUE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek to after the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        // can't seek backwards
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS - 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        size_t docs_count = 0;

        for (;;) {
          it->seek(expected_doc);

          if (irs::type_limits<irs::type_t::doc_id_t>::eof(it->value())) {
            break;
          }

          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          ++docs_count;

          auto next_expected_doc = expected_doc + 2;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_TRUE(payload->next());
            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

            // can't seek backwards
            ASSERT_EQ(next_expected_doc, it->seek(expected_doc));
            ASSERT_TRUE(payload->next());
            ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

            next_expected_doc += 2;
            ++docs_count;
          }

          expected_doc = next_expected_doc;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS/2, docs_count);
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        const irs::doc_id_t min_doc = 2;
        irs::doc_id_t expected_doc = MAX_DOCS;
        size_t docs_count = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);

        for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS;) {
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          ++docs_count;

          auto next_expected_doc = expected_doc + 2;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_TRUE(payload->next());
            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

            next_expected_doc += 2;
          }

          expected_doc -= 2;
        }
        ASSERT_EQ(MAX_DOCS/2, docs_count);

        // seek before the first document
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        it->seek(expected_doc);
        expected_doc = min_doc;
        ASSERT_EQ(min_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        auto next_expected_doc = expected_doc + 2;
        for (auto i = 0; i < steps_forward; ++i) {
          ASSERT_TRUE(it->next());
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          next_expected_doc += 2;
        }
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = MAX_DOCS;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        auto next_expected_doc = expected_doc + 2;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          next_expected_doc += 2;
        }

        expected_doc -= 2;
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        size_t docs_count = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // read values
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();
        irs::bytes_ref actual_value;

        // read (cached)
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_EQ(i % 2, values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          expected_doc += 2;
          ++docs_count;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }
    }
  }

  void read_write_doc_attributes_dense_column_dense_mask() {
    // dense_fixed_length_column<dense_mask_block>

    static const irs::doc_id_t MAX_DOCS
      = 1024*1024 // full index block
      + 2051;     // tail index block
    static const iresearch::string_ref column_name = "id";

    // write documents
    {
      struct stored {
        const irs::string_ref& name() { return column_name; }
        bool write(irs::data_output& out) { return true; }
      } field;

      irs::doc_id_t docs_count = 0;
      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      auto ctx = writer->documents();

      do {
        ctx.insert().insert(irs::action::store, field);
      } while (++docs_count < MAX_DOCS); // insert MAX_DOCS documents

      { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // check number of documents in the column
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_EQ(MAX_DOCS, column->size());
      }

      // visit values (not cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          ++expected_doc;
          ++docs_count;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // read values
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();
        irs::bytes_ref actual_value;

        // not cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          ++expected_doc;
          ++docs_count;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, it->value());
          ++expected_doc;
          ++docs_count;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - iterate (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          ++expected_doc;
          ++docs_count;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      {
        // iterate over column (not cached)
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, it->value());
          ++expected_doc;
          ++docs_count;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // read values
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();
        irs::bytes_ref actual_value;

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          ++expected_doc;
          ++docs_count;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, it->value());
          ++expected_doc;
          ++docs_count;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - seek (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          ++expected_doc;
          ++docs_count;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // seek over column (not cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ++expected_doc;
          ++docs_count;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek to the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        size_t docs_count = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ++expected_doc;
        ++docs_count;

        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, it->value());
          ++expected_doc;
          ++docs_count;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek before the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        size_t docs_count = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
        ++expected_doc;
        ++docs_count;

        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, it->value());
          ++expected_doc;
          ++docs_count;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        ASSERT_EQ(MAX_DOCS, it->seek(MAX_DOCS));

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        ASSERT_EQ(MAX_DOCS - 1, it->seek(MAX_DOCS - 1));

        ASSERT_TRUE(it->next());
        ASSERT_EQ(MAX_DOCS, it->value());

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
      }

      // seek to after the end + next + seek before end
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        it->seek(MAX_DOCS+1);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());

        // can't seek backwards
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS - 1));

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        size_t docs_count = 0;

        for (;;) {
          it->seek(expected_doc);

          if (irs::type_limits<irs::type_t::doc_id_t>::eof(it->value())) {
            break;
          }

          ++docs_count;
          ASSERT_EQ(expected_doc, it->value());

          auto next_expected_doc = expected_doc + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_EQ(next_expected_doc, it->value());

            // can't seek backwards
            ASSERT_EQ(next_expected_doc, it->seek(expected_doc));

            ++next_expected_doc;
            ++docs_count;
          }

          expected_doc = next_expected_doc;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        const irs::doc_id_t min_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_doc = MAX_DOCS;
        size_t docs_count = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS;) {
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

          ++docs_count;
          ASSERT_EQ(expected_doc, it->seek(expected_doc));

          auto next_expected_doc = expected_doc + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_EQ(next_expected_doc, it->value());
            ++next_expected_doc;
          }

          --expected_doc;
        }
        ASSERT_EQ(MAX_DOCS, docs_count);

        // seek before the first document
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        it->seek(expected_doc);
        expected_doc = min_doc;
        ASSERT_EQ(min_doc, it->seek(expected_doc));

        auto next_expected_doc = expected_doc + 1;
        for (auto i = 0; i < steps_forward; ++i) {
          ASSERT_TRUE(it->next());
          ASSERT_EQ(next_expected_doc, it->value());
          ++next_expected_doc;
        }
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t expected_doc = MAX_DOCS;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));

        auto next_expected_doc = expected_doc + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_EQ(next_expected_doc, it->value());
          ++next_expected_doc;
        }

        --expected_doc;
        it->seek(expected_doc);
      }

      // read values
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();
        irs::bytes_ref actual_value;

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          ++expected_doc;
          ++docs_count;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>()); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, it->value());
          ++expected_doc;
          ++docs_count;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }
    }
  }

  void read_write_doc_attributes_dense_column_dense_fixed_length() {
    // dense_fixed_length_column<dense_fixed_length_block>

    static const irs::doc_id_t MAX_DOCS = 1500;
    static const iresearch::string_ref column_name = "id";

    // write documents
    {
      struct stored {
        const irs::string_ref& name() { return column_name; }

        bool write(irs::data_output& out) {
          irs::write_string(
            out,
            irs::numeric_utils::numeric_traits<uint64_t>::raw_ref(value)
          );
          return true;
        }

        uint64_t value{};
      } field;

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      auto ctx = writer->documents();

      do {
        ctx.insert().insert(irs::action::store, field);
      } while (++field.value < MAX_DOCS); // insert MAX_DOCS documents

      { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // check number of documents in the column
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_EQ(MAX_DOCS, column->size());
      }

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
          if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
            return false;
          }

          ++expected_doc;
          ++expected_value;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // not cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
          ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
        }

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
          ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
          if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
            return false;
          }

          ++expected_doc;
          ++expected_value;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), expected_value);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - iterate (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
          if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
            return false;
          }

          ++expected_doc;
          ++expected_value;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      {
        // iterate over column (not cached)
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), expected_value);
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
          ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
          if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
            return false;
          }

          ++expected_doc;
          ++expected_value;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), expected_value);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - seek (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
          if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
            return false;
          }

          ++expected_doc;
          ++expected_value;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // seek over column (not cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek to the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;

        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek before the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;

        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        auto expected_doc = MAX_DOCS;
        auto expected_value = MAX_DOCS-1;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        auto expected_doc = MAX_DOCS-1;
        auto expected_value = expected_doc-1;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
        ASSERT_TRUE(it->next());
        ASSERT_TRUE(payload->next());
        actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek to after the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        // can't seek backwards
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS - 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        for (;;) {
          it->seek(expected_doc);

          if (irs::type_limits<irs::type_t::doc_id_t>::eof(it->value())) {
            break;
          }

          ASSERT_TRUE(payload->next());
          auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          auto next_expected_doc = expected_doc + 1;
          auto next_expected_value = expected_value + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_TRUE(payload->next());
            actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

            // can't seek backwards
            ASSERT_EQ(next_expected_doc, it->seek(expected_doc));
            ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

            ++next_expected_doc;
            ++next_expected_value;
          }

          expected_doc = next_expected_doc;
          expected_value = next_expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        const irs::doc_id_t min_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_doc = MAX_DOCS;
        irs::doc_id_t expected_value = expected_doc - 1;
        size_t docs_count = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS;) {
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ++docs_count;

          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          auto next_expected_doc = expected_doc + 1;
          auto next_expected_value = expected_value + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_TRUE(payload->next());
            actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

            ++next_expected_doc;
            ++next_expected_value;
          }

          --expected_doc;
          --expected_value;
        }
        ASSERT_EQ(MAX_DOCS, docs_count);

        // seek before the first document
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        it->seek(expected_doc);
        expected_doc = min_doc;
        expected_value = expected_doc - 1;
        ASSERT_EQ(min_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward; ++i) {
          ASSERT_TRUE(it->next());
          ASSERT_TRUE(payload->next());
          actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++next_expected_doc;
          ++next_expected_value;
        }
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = MAX_DOCS;
        irs::doc_id_t expected_value = expected_doc - 1;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++next_expected_doc;
          ++next_expected_value;
        }

        --expected_doc;
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
          ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
          if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
            return false;
          }

          ++expected_doc;
          ++expected_value;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), expected_value);
      }
    }
  }

  void read_write_doc_attributes_dense_column_dense_variable_length() {
    // sparse_column<dense_block>

    static const irs::doc_id_t MAX_DOCS = 1500;
    static const iresearch::string_ref column_name = "id";

    // write documents
    {
      struct stored {
        const irs::string_ref& name() { return column_name; }

        bool write(irs::data_output& out) {
          auto str = std::to_string(value);
          if (value % 2) {
            str.append(column_name.c_str(), column_name.size());
          }

          irs::write_string(out, str);
          return true;
        }

        uint64_t value{};
      } field;

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      auto ctx = writer->documents();

      do {
        ctx.insert().insert(irs::action::store, field);
      } while (++field.value < MAX_DOCS); // insert MAX_DOCS documents

      { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // check number of documents in the column
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_EQ(MAX_DOCS, column->size());
      }

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          ++expected_doc;
          ++expected_value;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // not cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 2) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        }

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 2) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          ++expected_doc;
          ++expected_value;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, expected_value);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - iterate (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          ++expected_doc;
          ++expected_value;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      {
        // iterate over column (not cached)
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 2) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          ++expected_doc;
          ++expected_value;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, expected_value);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - seek (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          ++expected_doc;
          ++expected_value;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // seek over column (not cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_value_str, actual_str_value);

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek to the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        ++expected_doc;
        ++expected_value;

        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_value_str);

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek before the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        ++expected_doc;
        ++expected_value;

        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_value_str);

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        auto expected_doc = MAX_DOCS;
        auto expected_value = MAX_DOCS-1;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        auto expected_doc = MAX_DOCS-1;
        auto expected_value = expected_doc-1;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        ++expected_doc;
        ++expected_value;
        expected_value_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_TRUE(it->next());
        ASSERT_TRUE(payload->next());
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, irs::to_string<irs::string_ref>(payload->value().c_str()));

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek to after the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        // can't seek backwards
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS - 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        for (;;) {
          it->seek(expected_doc);

          if (irs::type_limits<irs::type_t::doc_id_t>::eof(it->value())) {
            break;
          }

          ASSERT_TRUE(payload->next());
          auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_value_str);

          auto next_expected_doc = expected_doc + 1;
          auto next_expected_value = expected_value + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_TRUE(payload->next());
            actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
            auto next_expected_value_str  = std::to_string(next_expected_value);

            if (next_expected_value % 2) {
              next_expected_value_str.append(column_name.c_str(), column_name.size());
            }

            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            // can't seek backwards
            ASSERT_EQ(next_expected_doc, it->seek(expected_doc));
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            ++next_expected_doc;
            ++next_expected_value;
          }

          expected_doc = next_expected_doc;
          expected_value = next_expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        const irs::doc_id_t min_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_doc = MAX_DOCS;
        irs::doc_id_t expected_value = expected_doc - 1;
        size_t docs_count = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS;) {
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ++docs_count;

          ASSERT_EQ(expected_value_str, actual_value_str);

          auto next_expected_doc = expected_doc + 1;
          auto next_expected_value = expected_value + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_TRUE(payload->next());
            actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
            auto next_expected_value_str  = std::to_string(next_expected_value);

            if (next_expected_value % 2) {
              next_expected_value_str.append(column_name.c_str(), column_name.size());
            }

            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            ++next_expected_doc;
            ++next_expected_value;
          }

          --expected_doc;
          --expected_value;
        }
        ASSERT_EQ(MAX_DOCS, docs_count);

        // seek before the first document
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        it->seek(expected_doc);
        expected_doc = min_doc;
        expected_value = expected_doc - 1;
        ASSERT_EQ(min_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward; ++i) {
          ASSERT_TRUE(it->next());
          ASSERT_TRUE(payload->next());
          actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          auto next_expected_value_str  = std::to_string(next_expected_value);
          if (next_expected_value % 2) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          ++next_expected_doc;
          ++next_expected_value;
        }
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = MAX_DOCS;
        irs::doc_id_t expected_value = expected_doc - 1;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto next_expected_value_str  = std::to_string(next_expected_value);

          if (next_expected_value % 2) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          ++next_expected_doc;
          ++next_expected_value;
        }

        --expected_doc;
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 2) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          ++expected_doc;
          ++expected_value;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, expected_value);
      }
    }
  }

  void read_write_doc_attributes_sparse_column_dense_variable_length() {
    // sparse_column<dense_block>

    static const irs::doc_id_t BLOCK_SIZE = 1024;
    static const irs::doc_id_t MAX_DOCS = 1500;
    static const iresearch::string_ref column_name = "id";

    // write documents
    {
      struct stored {
        explicit stored(const irs::string_ref& name) NOEXCEPT
          : column_name(name) {
        }
        const irs::string_ref& name() { return column_name; }

        bool write(irs::data_output& out) {
          auto str = std::to_string(value);
          if (value % 2) {
            str.append(column_name.c_str(), column_name.size());
          }

          irs::write_string(out, str);
          return true;
        }

        uint64_t value{};
        const irs::string_ref column_name;
      } field(column_name), gap("gap");

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      auto ctx = writer->documents();

      do {
        ctx.insert().insert(irs::action::store, field);
      } while (++field.value < BLOCK_SIZE); // insert MAX_DOCS documents

      ctx.insert().insert(irs::action::store, gap); // gap
      ++field.value;

      do {
        ctx.insert().insert(irs::action::store, field);
      } while (++field.value <= MAX_DOCS); // insert MAX_DOCS documents

      { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // check number of documents in the column
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_EQ(MAX_DOCS, column->size());
      }

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          ++expected_doc;
          ++expected_value;

          if (expected_doc == BLOCK_SIZE + 1) {
            ++expected_doc; // gap
            ++expected_value;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // not cached
        {
          irs::doc_id_t i = 0;

          for (; i < BLOCK_SIZE; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
            auto expected_str_value = std::to_string(i);
            if (i % 2) {
              expected_str_value.append(column_name.c_str(), column_name.size());
            }
            ASSERT_EQ(expected_str_value, actual_str_value);
          }

          ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value)); // gap

          for (++i; i < MAX_DOCS; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
            auto expected_str_value = std::to_string(i);
            if (i % 2) {
              expected_str_value.append(column_name.c_str(), column_name.size());
            }
            ASSERT_EQ(expected_str_value, actual_str_value);
          }
        }

        // cached
        {
          irs::doc_id_t i = 0;

          for (; i < BLOCK_SIZE; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
            auto expected_str_value = std::to_string(i);
            if (i % 2) {
              expected_str_value.append(column_name.c_str(), column_name.size());
            }
            ASSERT_EQ(expected_str_value, actual_str_value);
          }

          ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value)); // gap

          for (++i; i < MAX_DOCS; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
            auto expected_str_value = std::to_string(i);
            if (i % 2) {
              expected_str_value.append(column_name.c_str(), column_name.size());
            }
            ASSERT_EQ(expected_str_value, actual_str_value);
          }
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          ++expected_doc;
          ++expected_value;

          if (expected_doc == BLOCK_SIZE + 1) {
            ++expected_doc; // gap
            ++expected_value;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++expected_doc;
          ++expected_value;
          ++docs_count;

          if (docs_count == BLOCK_SIZE) {
            ++expected_doc; // gap
            ++expected_value;
          }
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - iterate (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          ++expected_doc;
          ++expected_value;

          if (expected_doc == BLOCK_SIZE + 1) {
            ++expected_doc; // gap
            ++expected_value;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      {
        // iterate over column (not cached)
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++docs_count;
          ++expected_doc;
          ++expected_value;

          if (expected_doc == BLOCK_SIZE + 1) {
            ++expected_doc; // gap
            ++expected_value;
          }
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // cached
        {
          irs::doc_id_t i = 0;

          for (; i < BLOCK_SIZE; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
            auto expected_str_value = std::to_string(i);
            if (i % 2) {
              expected_str_value.append(column_name.c_str(), column_name.size());
            }
            ASSERT_EQ(expected_str_value, actual_str_value);
          }

          ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value)); // gap

          for (++i; i < MAX_DOCS; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
            auto expected_str_value = std::to_string(i);
            if (i % 2) {
              expected_str_value.append(column_name.c_str(), column_name.size());
            }
            ASSERT_EQ(expected_str_value, actual_str_value);
          }
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          ++expected_doc;
          ++expected_value;

          if (expected_doc == BLOCK_SIZE+1) {
            ++expected_doc; // gap
            ++expected_value;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++docs_count;
          ++expected_doc;
          ++expected_value;

          if (expected_doc == BLOCK_SIZE+1) {
            ++expected_doc; // gap
            ++expected_value;
          }
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - seek (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          ++expected_doc;
          ++expected_value;

          if (expected_doc == BLOCK_SIZE+1) {
            ++expected_doc; // gap
            ++expected_value;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // seek over column (not cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; expected_doc <= MAX_DOCS+1; ) {
          if (expected_doc == BLOCK_SIZE+1) {
            ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
            ++expected_doc; // gap
            ++expected_value;
          } else {
            ASSERT_EQ(expected_doc, it->seek(expected_doc));
          }

          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_value_str, actual_str_value);

          ++expected_doc;
          ++expected_value;
          ++docs_count;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek to the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        ++docs_count;
        ++expected_doc;
        ++expected_value;

        for (; it->next(); ) {
          if (expected_doc == BLOCK_SIZE+1) {
            ++expected_doc; // gap
            ++expected_value;
          }

          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_value_str);

          ++docs_count;
          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek before the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        ++docs_count;
        ++expected_doc;
        ++expected_value;

        for (; it->next(); ) {
          if (expected_doc == BLOCK_SIZE+1) {
            ++expected_doc; // gap
            ++expected_value;
          }

          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_value_str);

          ++docs_count;
          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        auto expected_doc = MAX_DOCS+1;
        auto expected_value = MAX_DOCS;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        auto expected_doc = MAX_DOCS;
        auto expected_value = expected_doc-1;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        ++expected_doc;
        ++expected_value;
        expected_value_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_TRUE(it->next());
        ASSERT_TRUE(payload->next());
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, irs::to_string<irs::string_ref>(payload->value().c_str()));

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek to after the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 2));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        // can't seek backwards
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        for (;;) {
          if (expected_doc == BLOCK_SIZE+1) {
            ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
            ++expected_doc; // gap
            ++expected_value;
          } else {
            if (expected_doc > MAX_DOCS+1) {
              ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
            } else {
              ASSERT_EQ(expected_doc, it->seek(expected_doc));
            }
          }

          if (irs::type_limits<irs::type_t::doc_id_t>::eof(it->value())) {
            break;
          }

          ++docs_count;

          ASSERT_TRUE(payload->next());
          auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_value_str);

          auto next_expected_doc = expected_doc + 1;
          auto next_expected_value = expected_value + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            if (next_expected_doc == BLOCK_SIZE + 1) {
              ++next_expected_doc; // gap
              ++next_expected_value;
            }

            ASSERT_TRUE(payload->next());
            actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
            auto next_expected_value_str  = std::to_string(next_expected_value);

            if (next_expected_value % 2) {
              next_expected_value_str.append(column_name.c_str(), column_name.size());
            }

            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            // can't seek backwards
            ASSERT_EQ(next_expected_doc, it->seek(expected_doc));
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            ++docs_count;
            ++next_expected_doc;
            ++next_expected_value;
          }

          expected_doc = next_expected_doc;
          expected_value = next_expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        const irs::doc_id_t min_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_doc = MAX_DOCS+1;
        irs::doc_id_t expected_value = expected_doc - 1;
        size_t docs_count = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS+1;) {
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ++docs_count;

          ASSERT_EQ(expected_value_str, actual_value_str);

          auto next_expected_doc = expected_doc + 1;
          auto next_expected_value = expected_value + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            if (next_expected_doc == BLOCK_SIZE+1) {
              ++next_expected_doc; // gap
              ++next_expected_value;
            }

            ASSERT_TRUE(payload->next());
            actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
            auto next_expected_value_str  = std::to_string(next_expected_value);

            if (next_expected_value % 2) {
              next_expected_value_str.append(column_name.c_str(), column_name.size());
            }

            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            ++next_expected_doc;
            ++next_expected_value;
          }

          --expected_doc;
          --expected_value;

          if (expected_doc == BLOCK_SIZE+1) {
            --expected_doc; // gap
            --expected_value;
          }
        }
        ASSERT_EQ(MAX_DOCS, docs_count);

        // seek before the first document
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        it->seek(expected_doc);
        expected_doc = min_doc;
        expected_value = expected_doc - 1;
        ASSERT_EQ(min_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward; ++i) {
          ASSERT_TRUE(it->next());
          ASSERT_TRUE(payload->next());
          actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          auto next_expected_value_str  = std::to_string(next_expected_value);
          if (next_expected_value % 2) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          ++next_expected_doc;
          ++next_expected_value;
        }
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = MAX_DOCS;
        irs::doc_id_t expected_value = expected_doc - 1;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          if (next_expected_doc == BLOCK_SIZE+1) {
            ++next_expected_doc; // gap
            ++next_expected_value;
          }

          ASSERT_TRUE(payload->next());
          actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto next_expected_value_str  = std::to_string(next_expected_value);

          if (next_expected_value % 2) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          ++next_expected_doc;
          ++next_expected_value;
        }

        --expected_doc;
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // cached
        {
          irs::doc_id_t i = 0;

          for (; i < BLOCK_SIZE; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
            auto expected_str_value = std::to_string(i);
            if (i % 2) {
              expected_str_value.append(column_name.c_str(), column_name.size());
            }
            ASSERT_EQ(expected_str_value, actual_str_value);
          }

          ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value)); // gap

          for (++i; i < MAX_DOCS; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
            auto expected_str_value = std::to_string(i);
            if (i % 2) {
              expected_str_value.append(column_name.c_str(), column_name.size());
            }
            ASSERT_EQ(expected_str_value, actual_str_value);
          }
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          ++expected_doc;
          ++expected_value;

          if (expected_doc == BLOCK_SIZE+1) {
            ++expected_doc; // gap
            ++expected_value;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str = std::to_string(expected_value);

          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++docs_count;
          ++expected_doc;
          ++expected_value;

          if (expected_doc == BLOCK_SIZE+1) {
            ++expected_doc; // gap
            ++expected_value;
          }
        }
        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }
    }
  }

  void read_write_doc_attributes_sparse_column_dense_fixed_offset() {
    // sparse_column<dense_fixed_length_block>

    // border case for sparse fixed offset columns, e.g.
    // |--------------|------------|
    // |doc           | value_size |
    // |--------------|------------|
    // | 1            | 0          |
    // | .            | 0          |
    // | .            | 0          |
    // | .            | 0          |
    // | BLOCK_SIZE-1 | 1          | <-- end of column block
    // | BLOCK_SIZE+1 | 0          |
    // | .            | 0          |
    // | .            | 0          |
    // | MAX_DOCS     | 1          |
    // |--------------|------------|

    static const irs::doc_id_t BLOCK_SIZE = 1024;
    static const irs::doc_id_t MAX_DOCS = 1500;
    static const irs::string_ref column_name = "id";
    size_t inserted = 0;

    // write documents
    {
      struct stored {
        explicit stored(const irs::string_ref& name) NOEXCEPT
          : column_name(name) {
        }

        const irs::string_ref& name() NOEXCEPT { return column_name; }

        bool write(irs::data_output& out) {
          if (value == BLOCK_SIZE-1) {
            out.write_byte(0);
          } else if (value == MAX_DOCS) {
            out.write_byte(1);
          }

          return true;
        }

        uint32_t value{};
        const irs::string_ref column_name;
      } field(column_name), gap("gap");

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      auto ctx = writer->documents();

      do {
        ctx.insert().insert(irs::action::store, field);
        ++inserted;
      } while (++field.value < BLOCK_SIZE); // insert BLOCK_SIZE documents

      ctx.insert().insert(irs::action::store, gap); // gap
      ++field.value;

      do {
        ctx.insert().insert(irs::action::store, field);
        ++inserted;
      } while (++field.value < (1+MAX_DOCS)); // insert BLOCK_SIZE documents

      { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // check number of documents in the column
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_EQ(MAX_DOCS, column->size());
      }

      // visit values (not cached)
      {
        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          ++expected_doc;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
          }

          if (count == BLOCK_SIZE) {
            if (irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)) != actual_data) {
              return false;
            }
          } else if (count == MAX_DOCS) {
            if (irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)) != actual_data) {
              return false;
            }
          } else if (!actual_data.empty()) {
            return false;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // not cached
        {
          irs::doc_id_t i = 0;
          for (; i < BLOCK_SIZE-1; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
          }

          ASSERT_TRUE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)), actual_value);
          ASSERT_FALSE(values(++i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

          for (++i; i <= MAX_DOCS-1; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
          }

          ASSERT_TRUE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)), actual_value);
        }

        // cached
        {
          irs::doc_id_t i = 0;
          for (; i < BLOCK_SIZE-1; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
          }

          ASSERT_TRUE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)), actual_value);
          ASSERT_FALSE(values(++i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

          for (++i; i <= MAX_DOCS-1; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
          }

          ASSERT_TRUE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)), actual_value);
        }
      }

      // visit values (cached)
      {
        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          ++expected_doc;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
          }

          if (count == BLOCK_SIZE) {
            if (irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)) != actual_data) {
              return false;
            }
          } else if (count == MAX_DOCS) {
            if (irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)) != actual_data) {
              return false;
            }
          } else if (!actual_data.empty()) {
            return false;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_data = payload->value();

          ASSERT_EQ(expected_doc, it->value());

          ++expected_doc;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
          }

          if (count == BLOCK_SIZE) {
            ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)), actual_data);
          } else if (count == MAX_DOCS) {
            ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)), actual_data);
          } else {
            ASSERT_EQ(irs::bytes_ref::NIL, actual_data);
          }
        }
        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, count);
      }
    }
  }

  void read_write_doc_attributes_dense_column_dense_fixed_offset() {
    // dense_fixed_length_column<dense_fixed_length_block>

    // border case for dense fixed offset columns, e.g.
    // |--------------|------------|
    // |doc           | value_size |
    // |--------------|------------|
    // | 1            | 0          |
    // | .            | 0          |
    // | .            | 0          |
    // | .            | 0          |
    // | BLOCK_SIZE-1 | 1          | <-- end of column block
    // | BLOCK_SIZE   | 0          |
    // | .            | 0          |
    // | .            | 0          |
    // | MAX_DOCS     | 1          |
    // |--------------|------------|

    static const irs::doc_id_t MAX_DOCS = 1500;
    static const irs::doc_id_t BLOCK_SIZE = 1024;
    static const iresearch::string_ref column_name = "id";

    // write documents
    {
      struct stored {
        const irs::string_ref& name() { return column_name; }

        bool write(irs::data_output& out) {
          if (value == BLOCK_SIZE-1) {
            out.write_byte(0);
          } else if (value == MAX_DOCS-1) {
            out.write_byte(1);
          }
          return true;
        }

        uint64_t value{};
      } field;

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      auto ctx = writer->documents();

      do {
        ctx.insert().insert(irs::action::store, field);
      } while (++field.value < MAX_DOCS); // insert MAX_DOCS documents

      { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // check number of documents in the column
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_EQ(MAX_DOCS, column->size());
      }

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        size_t count = 0;
        auto visitor = [&count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          ++expected_doc;
          ++count;

          if (count == BLOCK_SIZE) {
            if (irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)) != actual_data) {
              return false;
            }
          } else if (count == MAX_DOCS) {
            if (irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)) != actual_data) {
              return false;
            }
          } else if (!actual_data.empty()) {
            return false;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        irs::doc_id_t i = 0;
        for (; i < BLOCK_SIZE-1; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }

        ASSERT_TRUE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)), actual_value);

        for (++i; i < MAX_DOCS-1; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }

        ASSERT_TRUE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)), actual_value);
      }


      // visit values (cached)
      {
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        size_t count = 0;
        auto visitor = [&count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          ++expected_doc;
          ++count;

          if (count == BLOCK_SIZE) {
            if (irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)) != actual_data) {
              return false;
            }
          } else if (count == MAX_DOCS) {
            if (irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)) != actual_data) {
              return false;
            }
          } else if (!actual_data.empty()) {
            return false;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_data = payload->value();

          ASSERT_EQ(expected_doc, it->value());

          ++expected_doc;
          ++count;

          if (count == BLOCK_SIZE) {
            ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)), actual_data);
          } else if (count == MAX_DOCS) {
            ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)), actual_data);
          } else {
            ASSERT_EQ(irs::bytes_ref::NIL, actual_data);
          }
        }
        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(MAX_DOCS, count);
      }
    }
  }

  void read_write_doc_attributes_sparse_column_dense_fixed_length() {
    // sparse_column<dense_fixed_length_block>

    static const irs::doc_id_t BLOCK_SIZE = 1024;
    static const irs::doc_id_t MAX_DOCS = 1500;
    static const irs::string_ref column_name = "id";
    size_t inserted = 0;

    // write documents
    {
      struct stored {
        explicit stored(const irs::string_ref& name) NOEXCEPT
          : column_name(name) {
        }

        const irs::string_ref& name() NOEXCEPT { return column_name; }

        bool write(irs::data_output& out) {
          irs::write_string(
            out,
            irs::numeric_utils::numeric_traits<uint32_t>::raw_ref(value)
          );
          return true;
        }

        uint32_t value{};
        const irs::string_ref column_name;
      } field(column_name), gap("gap");

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      auto ctx = writer->documents();

      do {
        ctx.insert().insert(irs::action::store, field);
        ++inserted;
      } while (++field.value < BLOCK_SIZE); // insert BLOCK_SIZE documents

      ctx.insert().insert(irs::action::store, gap); // gap
      ++field.value;

      do {
        ctx.insert().insert(irs::action::store, field);
        ++inserted;
      } while (++field.value < (1+MAX_DOCS)); // insert BLOCK_SIZE documents

      { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // check number of documents in the column
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_EQ(MAX_DOCS, column->size());
      }

      // visit values (not cached)
      {
        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&count, &expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
          if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
            return false;
          }

          ++expected_doc;
          ++expected_value;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
            ++expected_value;
          }
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // not cached
        {
          irs::doc_id_t i = 0;
          for (; i < BLOCK_SIZE; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
            ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
          }

          ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

          for (++i; i < MAX_DOCS; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
            ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
          }
        }

        // cached
        {
          irs::doc_id_t i = 0;
          for (; i < BLOCK_SIZE; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
            ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
          }

          ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

          for (++i; i < MAX_DOCS; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
            ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
          }
        }
      }

      // visit values (cached)
      {
        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&count, &expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
          if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
            return false;
          }

          ++expected_doc;
          ++expected_value;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
            ++expected_value;
          }
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
            ++expected_value;
          }
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), expected_value);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - iterate (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&count, &expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
          if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
            return false;
          }

          ++expected_doc;
          ++expected_value;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
            ++expected_value;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      {
        // iterate over column (not cached)
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
            ++expected_value;
          }
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), expected_value);
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // cached
        irs::doc_id_t i = 0;
        for (; i < BLOCK_SIZE; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
          ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
        }

        ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

        for (++i; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
          ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
        }
      }

      // visit values (cached)
      {
        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&count, &expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
          if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
            return false;
          }

          ++expected_doc;
          ++expected_value;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
            ++expected_value;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
            ++expected_value;
          }
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), expected_value);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - seek (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&count, &expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
          if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
            return false;
          }

          ++expected_doc;
          ++expected_value;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
            ++expected_value;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // seek over column (not cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; expected_doc <= 1+MAX_DOCS; ) {
          if (expected_doc == 1025) {
            ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
            ++expected_doc;
            ++expected_value;
          } else {
            ASSERT_EQ(expected_doc, it->seek(expected_doc));
          }
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(1+MAX_DOCS, expected_value);
      }

      // seek to the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;

        for (; it->next(); ) {
          if (expected_doc == 1025) {
            ++expected_doc; // gap
            ++expected_value;
          }

          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(1+MAX_DOCS, expected_value);
      }

      // seek before the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;

        for (; it->next(); ) {
          if (expected_doc == 1025) {
            ++expected_doc; // gap
            ++expected_value;
          }

          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(1+MAX_DOCS, expected_value);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        auto expected_doc = MAX_DOCS+1;
        auto expected_value = MAX_DOCS;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        auto expected_doc = MAX_DOCS;
        auto expected_value = MAX_DOCS-1;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
        ASSERT_TRUE(it->next());
        ASSERT_TRUE(payload->next());
        actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek to after the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 2));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        // can't seek backwards
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS - 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // FIXME revisit
      // seek to gap + next(x5)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t expected_doc = BLOCK_SIZE+2;
        irs::doc_id_t expected_value = expected_doc-1;

        ASSERT_EQ(expected_doc, it->seek(expected_doc-1));
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        for (; it->next(); ) {
          ++expected_doc;
          ++expected_value;

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_TRUE(payload->next());
          auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        for (;;) {
          if (expected_doc == 1025) {
            ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
            ++expected_doc; // gap
            ++expected_value;
          } else {
            if (expected_doc > MAX_DOCS+1) {
              ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
            } else {
              ASSERT_EQ(expected_doc, it->seek(expected_doc));
            }
          }

          if (irs::type_limits<irs::type_t::doc_id_t>::eof(it->value())) {
            break;
          }

          ASSERT_TRUE(payload->next());
          auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          auto next_expected_doc = expected_doc + 1;
          auto next_expected_value = expected_value + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            if (next_expected_doc == 1025) {
              ++next_expected_doc; // gap
              ++next_expected_value;
            }

            ASSERT_TRUE(payload->next());
            actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

            // can't seek backwards
            ASSERT_EQ(next_expected_doc, it->seek(expected_doc));
            ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

            ++next_expected_doc;
            ++next_expected_value;
          }

          expected_doc = next_expected_doc;
          expected_value = next_expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(1+MAX_DOCS, expected_value);
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        const irs::doc_id_t min_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_doc = MAX_DOCS;
        irs::doc_id_t expected_value = expected_doc - 1;
        size_t docs_count = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS;) {
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          if (expected_doc == 1025) {
            ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
            expected_doc++;
            expected_value++;
          } else {
            if (expected_doc > MAX_DOCS+1) {
              ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
            } else {
              ASSERT_EQ(expected_doc, it->seek(expected_doc));
            }
          }

          ASSERT_TRUE(payload->next());
          auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ++docs_count;

          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          auto next_expected_doc = expected_doc + 1;
          auto next_expected_value = expected_value + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            if (next_expected_doc == 1025) {
              ++next_expected_doc; // gap
              ++next_expected_value;
            }

            ASSERT_TRUE(payload->next());
            actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

            ++next_expected_doc;
            ++next_expected_value;
          }

          --expected_doc;
          --expected_value;

          if (expected_doc == 1025) {
            // gap
            --expected_doc;
            --expected_value;
          }
        }
        ASSERT_EQ(MAX_DOCS-1, docs_count);

        // seek before the first document
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        it->seek(expected_doc);
        expected_doc = min_doc;
        expected_value = expected_doc - 1;
        ASSERT_EQ(min_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward; ++i) {
          ASSERT_TRUE(it->next());
          ASSERT_TRUE(payload->next());
          actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++next_expected_doc;
          ++next_expected_value;
        }
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = MAX_DOCS;
        irs::doc_id_t expected_value = expected_doc - 1;

        if (expected_doc == 1025) {
          ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
          expected_doc++;
          expected_value++;
        } else {
          ASSERT_EQ(expected_doc, it->seek(expected_doc));
        }
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          if (next_expected_doc == 1025) {
            next_expected_doc++; // gap
            next_expected_value++;
          }

          ASSERT_TRUE(payload->next());
          actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++next_expected_doc;
          ++next_expected_value;
        }

        --expected_doc;
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // cached
        irs::doc_id_t i = 0;
        for (; i < BLOCK_SIZE; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
          ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
        }

        ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

        for (++i; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
          ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
        }
      }

      // visit values (cached)
      {
        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        auto visitor = [&count, &expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
          if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
            return false;
          }

          ++expected_doc;
          ++expected_value;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
            ++expected_value;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        size_t count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;

          if (++count == BLOCK_SIZE) {
            ++expected_doc; // gap
            ++expected_value;
          }
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), expected_value);
      }
    }
  }

  void read_write_doc_attributes_sparse_column_dense_mask() {
    // sparse_column<dense_mask_block>

    static const irs::doc_id_t BLOCK_SIZE = 1024;
    static const irs::doc_id_t MAX_DOCS
      = BLOCK_SIZE*BLOCK_SIZE // full index block
      + 2051;               // tail index block
    static const iresearch::string_ref column_name = "id";

    // write documents
    {
      struct stored {
        explicit stored(const irs::string_ref& name) NOEXCEPT
          : column_name(name) {
        }
        const irs::string_ref& name() { return column_name; }
        bool write(irs::data_output&) { return true; }
        const irs::string_ref column_name;
      } field(column_name), gap("gap");

      irs::doc_id_t docs_count = 0;
      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      auto ctx = writer->documents();

      do {
        ctx.insert().insert(irs::action::store, field);
      } while (++docs_count < BLOCK_SIZE); // insert BLOCK_SIZE documents

      ctx.insert().insert(irs::action::store, gap);

      do {
        ctx.insert().insert(irs::action::store, field);
      } while (++docs_count < MAX_DOCS); // insert BLOCK_SIZE documents

      { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // check number of documents in the column
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_EQ(MAX_DOCS, column->size());
      }

      // visit values (not cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          ++expected_doc;
          ++docs_count;

          if (docs_count == BLOCK_SIZE) {
            ++expected_doc; // gap
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // read values
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();
        irs::bytes_ref actual_value;

        // not cached
        {
          irs::doc_id_t i = 0;
          for (; i < BLOCK_SIZE; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
          }

          ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

          for (++i; i < MAX_DOCS; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
          }
        }

        // cached
        {
          irs::doc_id_t i = 0;
          for (; i < BLOCK_SIZE; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
          }

          ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

          for (++i; i < MAX_DOCS; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
          }
        }
      }

      // visit values (not cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          ++expected_doc;
          ++docs_count;

          if (docs_count == BLOCK_SIZE) {
            ++expected_doc; // gap
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data
          ++expected_doc;
          ++docs_count;

          if (docs_count == BLOCK_SIZE) {
            // gap
            ++expected_doc;
          }
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - iterate (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          ++expected_doc;
          ++docs_count;

          if (BLOCK_SIZE == docs_count) {
            // gap
            ++expected_doc;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      {
        // iterate over column (not cached)
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data
          ++expected_doc;
          ++docs_count;

          if (BLOCK_SIZE == docs_count) {
            // gap
            ++expected_doc;
          }
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // read values
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();
        irs::bytes_ref actual_value;

        // cached
        {
          irs::doc_id_t i = 0;
          for (; i < BLOCK_SIZE; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
          }

          // gap
          ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

          for (++i; i < MAX_DOCS; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
          }
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          ++expected_doc;
          ++docs_count;

          if (BLOCK_SIZE == docs_count) {
            // gap
            ++expected_doc;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data
          ++expected_doc;
          ++docs_count;

          if (BLOCK_SIZE == docs_count) {
            // gap
            ++expected_doc;
          }
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - seek (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          ++expected_doc;
          ++docs_count;

          if (BLOCK_SIZE == docs_count) {
            // gap
            ++expected_doc;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // seek over column (not cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; expected_doc <= MAX_DOCS+1; ) {
          if (expected_doc == 1+BLOCK_SIZE) {
            ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
            ++expected_doc; // gap
          } else {
            ASSERT_EQ(expected_doc, it->seek(expected_doc));
          }
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data
          ++expected_doc;
          ++docs_count;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek to begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        size_t docs_count = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ++expected_doc;
        ++docs_count;

        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data
          ++expected_doc;
          ++docs_count;

          if (docs_count == BLOCK_SIZE) {
            ++expected_doc; // gap
          }
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek before begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        size_t docs_count = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
        ++expected_doc;
        ++docs_count;

        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, it->value());
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data
          ++expected_doc;
          ++docs_count;

          if (docs_count == BLOCK_SIZE) {
            ++expected_doc; // gap
          }
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        ASSERT_EQ(MAX_DOCS+1, it->seek(MAX_DOCS+1));

        ASSERT_TRUE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data
        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        ASSERT_EQ(MAX_DOCS, it->seek(MAX_DOCS));

        ASSERT_TRUE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data

        ASSERT_TRUE(it->next());
        ASSERT_EQ(MAX_DOCS+1, it->value());

        ASSERT_TRUE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
      }

      // seek to after the end + next + seek before end
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        it->seek(MAX_DOCS+2);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_FALSE(payload->next());

        // can't seek backwards
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS));
        ASSERT_FALSE(payload->next());

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
      }

      // seek to gap + next(x5)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t expected_doc = BLOCK_SIZE+2;
        size_t docs_count = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc-1));
        ASSERT_EQ(expected_doc, it->value());

        for (; it->next(); ) {
          ++expected_doc;
          ++docs_count;

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        size_t docs_count = 0;

        for (;;) {
          if (docs_count == BLOCK_SIZE) {
            ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
            ++expected_doc; // gap
          } else {
            if (expected_doc > MAX_DOCS+1) {
              ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
            } else {
              ASSERT_EQ(expected_doc, it->seek(expected_doc));
            }
          }

          if (irs::type_limits<irs::type_t::doc_id_t>::eof(it->value())) {
            break;
          }

          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data

          ++docs_count;
          ASSERT_EQ(expected_doc, it->value());

          auto next_expected_doc = expected_doc + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_EQ(next_expected_doc, it->value());

            ASSERT_TRUE(payload->next());
            ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data

            // can't seek backwards
            ASSERT_EQ(next_expected_doc, it->seek(expected_doc));

            ++next_expected_doc;
            ++docs_count;

            if (docs_count == BLOCK_SIZE) {
              ++next_expected_doc; // gap
            }
          }

          expected_doc = next_expected_doc;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        const irs::doc_id_t min_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_doc = MAX_DOCS+1;
        size_t docs_count = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS+1;) {
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_TRUE(payload);
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

          ++docs_count;

          if (expected_doc == BLOCK_SIZE+1) {
            ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
            ++expected_doc; // gap
          } else {
            ASSERT_EQ(expected_doc, it->seek(expected_doc));
          }

          auto next_expected_doc = expected_doc + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            if (next_expected_doc == BLOCK_SIZE+1) {
              ++next_expected_doc; // gap
            }

            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_TRUE(payload->next());
            ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data
            ++next_expected_doc;
          }

          --expected_doc;

          if (expected_doc == BLOCK_SIZE+1) {
            --expected_doc; // gap
          }
        }
        ASSERT_EQ(MAX_DOCS, docs_count);

        // seek before the first document
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        ASSERT_EQ(min_doc, it->seek(expected_doc));
        expected_doc = min_doc;
        ASSERT_EQ(min_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data

        auto next_expected_doc = expected_doc + 1;
        for (auto i = 0; i < steps_forward; ++i) {
          if (next_expected_doc == BLOCK_SIZE+1) {
            ++next_expected_doc; // gap
          }
          ASSERT_TRUE(it->next());
          ASSERT_EQ(next_expected_doc, it->value());
          ++next_expected_doc;
        }
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t expected_doc = MAX_DOCS;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data

        auto next_expected_doc = expected_doc + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data
          ++next_expected_doc;
        }

        --expected_doc;
        it->seek(expected_doc);
      }

      // read values
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();
        irs::bytes_ref actual_value;

        // cached
        {
          irs::doc_id_t i = 0;
          for (; i < BLOCK_SIZE; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
          }

          ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

          for (++i; i < MAX_DOCS; ++i) {
            const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
            ASSERT_TRUE(values(doc, actual_value));
            ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
          }
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          if (!actual_data.null()) {
            return false;
          }

          ++expected_doc;
          ++docs_count;

          if (docs_count == BLOCK_SIZE) {
            ++expected_doc; // gap
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          if (docs_count == BLOCK_SIZE) {
            ++expected_doc; // gap
          }

          ASSERT_TRUE(payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value()); // mask block has no data

          ASSERT_EQ(expected_doc, it->value());
          ++expected_doc;
          ++docs_count;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }
    }
  }

  void read_write_doc_attributes_sparse_column_sparse_variable_length() {
    // sparse_column<sparse_block>

    static const irs::doc_id_t MAX_DOCS = 1500;
    static const iresearch::string_ref column_name = "id";
    size_t inserted = 0;

    // write documents
    {
      struct stored {
        const irs::string_ref& name() { return column_name; }

        bool write(irs::data_output& out) {
          auto str = std::to_string(value);
          if (value % 3) {
            str.append(column_name.c_str(), column_name.size());
          }

          irs::write_string(out, str);
          return true;
        }

        uint64_t value{};
      } field;

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      auto ctx = writer->documents();

      do {
        auto doc = ctx.insert();

        if (field.value % 2) {
          doc.insert(irs::action::store, field);
          ++inserted;
        }
      } while (++field.value < MAX_DOCS); // insert MAX_DOCS documents

      { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // check number of documents in the column
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_EQ(MAX_DOCS/2, column->size());
      }

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          expected_doc += 2;
          expected_value += 2;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // not cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();

          if (i % 2) {
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
            auto expected_str_value = std::to_string(i);
            if (i % 3) {
              expected_str_value.append(column_name.c_str(), column_name.size());
            }
            ASSERT_EQ(expected_str_value, actual_str_value);
          } else {
            ASSERT_FALSE(values(doc, actual_value));
          }
        }

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();

          if (i % 2) {
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
            auto expected_str_value = std::to_string(i);
            if (i % 3) {
              expected_str_value.append(column_name.c_str(), column_name.size());
            }
            ASSERT_EQ(expected_str_value, actual_str_value);
          } else {
            ASSERT_FALSE(values(doc, actual_value));
          }
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          expected_doc += 2;
          expected_value += 2;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(inserted, docs);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - iterate (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          expected_doc += 2;
          expected_value += 2;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      {
        // iterate over column (not cached)
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);


        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(inserted, docs);
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();

          if (i % 2) {
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
            auto expected_str_value = std::to_string(i);
            if (i % 3) {
              expected_str_value.append(column_name.c_str(), column_name.size());
            }
            ASSERT_EQ(expected_str_value, actual_str_value);
          } else {
            ASSERT_FALSE(values(doc, actual_value));
          }
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          expected_doc += 2;
          expected_value += 2;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(inserted, docs);
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - seek (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(this->dir(), this->codec());
      ASSERT_EQ(1, reader.size());

      auto& segment = *(reader.begin());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit values (not cached)
      {
        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          expected_doc += 2;
          expected_value += 2;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // seek over column (not cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          ASSERT_EQ(expected_value_str, actual_str_value);

          ASSERT_EQ(expected_doc, it->seek(expected_value)); // seek before the existing key (value should remain the same)
          ASSERT_TRUE(payload->next());
          actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(inserted, docs);
      }

      // seek over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->seek(expected_value));
          ASSERT_TRUE(payload->next());
          auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          ASSERT_EQ(expected_value_str, actual_str_value);

          ASSERT_EQ(expected_doc, it->seek(expected_doc)); // seek to the existing key (value should remain the same)
          ASSERT_TRUE(payload->next());
          actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(inserted, docs);
      }

      // seek to the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        expected_doc += 2;
        expected_value += 2;
        ++docs;

        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_value_str);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(inserted, docs);
      }

      // seek before the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;

        ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        expected_doc += 2;
        expected_value += 2;
        ++docs;

        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_value_str);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(inserted, docs);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        auto expected_doc = MAX_DOCS;
        auto expected_value = MAX_DOCS-1;
        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        it->seek(expected_doc);
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_value_str);

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        auto expected_value = MAX_DOCS-1;
        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        it->seek(expected_value);
        ASSERT_TRUE(payload->next());
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        ASSERT_EQ(MAX_DOCS, it->value());
        ASSERT_EQ(expected_value_str, actual_value_str);

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek to after the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        // can't seek backwards
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS - 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;

        for (;;) {
          it->seek(expected_doc);

          if (irs::type_limits<irs::type_t::doc_id_t>::eof(it->value())) {
            break;
          }

          ASSERT_TRUE(payload->next());
          auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_value_str);

          ++docs;

          auto next_expected_doc = expected_doc + 2;
          auto next_expected_value = expected_value + 2;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_TRUE(payload->next());
            actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
            auto next_expected_value_str  = std::to_string(next_expected_value);

            if (next_expected_value % 3) {
              next_expected_value_str.append(column_name.c_str(), column_name.size());
            }

            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            // can't seek backwards
            ASSERT_EQ(next_expected_doc, it->seek(expected_doc));
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            next_expected_doc += 2;
            next_expected_value += 2;
            ++docs;
          }

          expected_doc = next_expected_doc;
          expected_value = next_expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(inserted, docs);
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        const irs::doc_id_t min_doc = 2;
        irs::doc_id_t expected_doc = MAX_DOCS;
        irs::doc_id_t expected_value = expected_doc - 1;
        size_t docs = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS;) {
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_value_str, actual_value_str);

          ++docs;

          auto next_expected_doc = expected_doc + 2;
          auto next_expected_value = expected_value + 2;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_TRUE(payload->next());
            actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
            auto next_expected_value_str  = std::to_string(next_expected_value);

            if (next_expected_value % 3) {
              next_expected_value_str.append(column_name.c_str(), column_name.size());
            }

            ASSERT_EQ(next_expected_doc, it->value());
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            next_expected_doc += 2;
            next_expected_value += 2;
          }

          expected_doc -= 2;
          expected_value -= 2;
        }

        ASSERT_EQ(inserted, docs);

        // seek before the first document
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        it->seek(expected_doc);
        expected_doc = min_doc;
        expected_value = expected_doc - 1;
        ASSERT_EQ(min_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 2;
        auto next_expected_value = expected_value + 2;
        for (auto i = 0; i < steps_forward; ++i) {
          ASSERT_TRUE(it->next());
          ASSERT_TRUE(payload->next());
          actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

          auto next_expected_value_str  = std::to_string(next_expected_value);
          if (next_expected_value % 3) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          next_expected_doc += 2;
          next_expected_value += 2;
        }
      }

      // seek backwards + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = MAX_DOCS;
        irs::doc_id_t expected_value = expected_doc - 1;

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_TRUE(payload->next());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 2;
        auto next_expected_value = expected_value + 2;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto next_expected_value_str  = std::to_string(next_expected_value);

          if (next_expected_value % 3) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          next_expected_doc += 2;
          next_expected_value += 2;
        }

        expected_doc -= 2;
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
      }

      // seek over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(expected_doc));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(MAX_DOCS + 1));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // read values
      {
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();

          if (i % 2) {
            ASSERT_TRUE(values(doc, actual_value));
            const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
            auto expected_str_value = std::to_string(i);
            if (i % 3) {
              expected_str_value.append(column_name.c_str(), column_name.size());
            }
            ASSERT_EQ(expected_str_value, actual_str_value);
          } else {
            ASSERT_FALSE(values(doc, actual_value));
          }
        }
      }

      // visit values (cached)
      {
        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
          if (expected_doc != actual_doc) {
            return false;
          }

          const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

          auto expected_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_str.append(column_name.c_str(), column_name.size());
          }

          if (expected_str != actual_str) {
            return false;
          }

          expected_doc += 2;
          expected_value += 2;
          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; it->next(); ) {
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          auto expected_value_str  = std::to_string(expected_value);

          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, it->value());
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }
        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(inserted, docs);
      }
    }
  }

  void iterate_fields() {
    std::vector<irs::string_ref> names {
     "06D36", "0OY4F", "1DTSP", "1KCSY", "2NGZD", "3ME9S", "4UIR7",
     "68QRT", "6XTTH", "7NDWJ", "9QXBA", "A8MSE", "CNH1B", "I4EWS",
     "JXQKH", "KPQ7R", "LK1MG", "M47KP", "NWCBQ", "OEKKW", "RI1QG",
     "TD7H7", "U56E5", "UKETS", "UZWN7", "V4DLA", "W54FF", "Z4K42",
     "ZKQCU", "ZPNXJ"
    };

    ASSERT_TRUE(std::is_sorted(names.begin(), names.end()));

    struct {
      const irs::string_ref& name() const {
        return name_;
      }

      float_t boost() const {
        return 1.f;
      }

      const irs::flags& features() const {
        return irs::flags::empty_instance();
      }

      irs::token_stream& get_tokens() const {
        stream_.reset(name_);
        return stream_;
      }

      irs::string_ref name_;
      mutable irs::string_token_stream stream_;
    } field;

    // insert attributes
    {
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);
      ASSERT_NE(nullptr, writer);

      {
        auto ctx = writer->documents();
        auto doc = ctx.insert();

        for (auto& name : names) {
          field.name_ = name;
          doc.insert(irs::action::index, field);
        }

        ASSERT_TRUE(doc);
      }

      writer->commit();
    }

    // iterate over fields
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      auto actual = segment.fields();

      for (auto expected = names.begin(); expected != names.end();) {
        ASSERT_TRUE(actual->next());
        ASSERT_EQ(*expected, actual->value().meta().name);
        ++expected;
      }
      ASSERT_FALSE(actual->next());
      ASSERT_FALSE(actual->next());
    }

    // seek over fields
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      auto actual = segment.fields();

      for (auto expected = names.begin(), prev = expected; expected != names.end();) {
        ASSERT_TRUE(actual->seek(*expected));
        ASSERT_EQ(*expected, actual->value().meta().name);

        if (prev != expected) {
          ASSERT_TRUE(actual->seek(*prev)); // can't seek backwards
          ASSERT_EQ(*expected, actual->value().meta().name);
        }

        // seek to the same value
        ASSERT_TRUE(actual->seek(*expected));
        ASSERT_EQ(*expected, actual->value().meta().name);

        prev = expected;
        ++expected;
      }
      ASSERT_FALSE(actual->next()); // reached the end
      ASSERT_FALSE(actual->seek(names.front())); // can't seek backwards
      ASSERT_FALSE(actual->next());
    }

    // seek before the first element
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      auto actual = segment.fields();
      auto expected = names.begin();

      const auto key = irs::string_ref("0");
      ASSERT_TRUE(key < names.front());
      ASSERT_TRUE(actual->seek(key));
      ASSERT_EQ(*expected, actual->value().meta().name);

      ++expected;
      for (auto prev = names.begin(); expected != names.end();) {
        ASSERT_TRUE(actual->next());
        ASSERT_EQ(*expected, actual->value().meta().name);

        if (prev != expected) {
          ASSERT_TRUE(actual->seek(*prev)); // can't seek backwards
          ASSERT_EQ(*expected, actual->value().meta().name);
        }

        // seek to the same value
        ASSERT_TRUE(actual->seek(*expected));
        ASSERT_EQ(*expected, actual->value().meta().name);

        prev = expected;
        ++expected;
      }
      ASSERT_FALSE(actual->next()); // reached the end
      ASSERT_FALSE(actual->seek(names.front())); // can't seek backwards
      ASSERT_FALSE(actual->next());
    }

    // seek after the last element
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      auto actual = segment.fields();

      const auto key = irs::string_ref("~");
      ASSERT_TRUE(key > names.back());
      ASSERT_FALSE(actual->seek(key));
      ASSERT_FALSE(actual->next()); // reached the end
      ASSERT_FALSE(actual->seek(names.front())); // can't seek backwards
    }

    // seek in between
    {
      std::vector<std::pair<irs::string_ref, irs::string_ref>> seeks {
        { "0B", names[1] }, { names[1], names[1] }, { "0", names[1] },
        { "D", names[13] }, { names[13], names[13] }, { names[12], names[13] },
        { "P", names[20] }, { "Z", names[27] }
      };

      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      auto actual = segment.fields();

      for (auto& seek : seeks) {
        auto& key = seek.first;
        auto& expected = seek.second;

        ASSERT_TRUE(actual->seek(key));
        ASSERT_EQ(expected, actual->value().meta().name);
      }

      const auto key = irs::string_ref("~");
      ASSERT_TRUE(key > names.back());
      ASSERT_FALSE(actual->seek(key));
      ASSERT_FALSE(actual->next()); // reached the end
      ASSERT_FALSE(actual->seek(names.front())); // can't seek backwards
    }

    // seek in between + next
    {
      std::vector<std::pair<irs::string_ref, size_t>> seeks {
        { "0B", 1 },  { "D", 13 }, { "O", 19 }, { "P", 20 }, { "Z", 27 }
      };

      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      for (auto& seek : seeks) {
        auto& key = seek.first;
        auto expected = names.begin() + seek.second;

        auto actual = segment.fields();

        ASSERT_TRUE(actual->seek(key));
        ASSERT_EQ(*expected, actual->value().meta().name);

        for (++expected; expected != names.end(); ++expected) {
          ASSERT_TRUE(actual->next());
          ASSERT_EQ(*expected, actual->value().meta().name);
        }

        ASSERT_FALSE(actual->next()); // reached the end
        ASSERT_FALSE(actual->seek(names.front())); // can't seek backwards
      }
    }
  }

  void iterate_attributes() {
    std::vector<irs::string_ref> names {
     "06D36", "0OY4F", "1DTSP", "1KCSY", "2NGZD", "3ME9S", "4UIR7",
     "68QRT", "6XTTH", "7NDWJ", "9QXBA", "A8MSE", "CNH1B", "I4EWS",
     "JXQKH", "KPQ7R", "LK1MG", "M47KP", "NWCBQ", "OEKKW", "RI1QG",
     "TD7H7", "U56E5", "UKETS", "UZWN7", "V4DLA", "W54FF", "Z4K42",
     "ZKQCU", "ZPNXJ"
    };

    ASSERT_TRUE(std::is_sorted(names.begin(), names.end()));

    struct {
      const irs::string_ref& name() const {
        return name_;
      }

      bool write(irs::data_output&) const NOEXCEPT {
        return true;
      }

      irs::string_ref name_;

    } field;

    // insert attributes
    {
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);
      ASSERT_NE(nullptr, writer);

      {
        auto ctx = writer->documents();
        auto doc = ctx.insert();

        for (auto& name : names) {
          field.name_ = name;
          doc.insert(irs::action::store, field);
        }

        ASSERT_TRUE(doc);
      }

      writer->commit();
    }

    // iterate over attributes
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      auto actual = segment.columns();

      for (auto expected = names.begin(); expected != names.end();) {
        ASSERT_TRUE(actual->next());
        ASSERT_EQ(*expected, actual->value().name);
        ++expected;
      }
      ASSERT_FALSE(actual->next());
      ASSERT_FALSE(actual->next());
    }

    // seek over attributes
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      auto actual = segment.columns();

      for (auto expected = names.begin(), prev = expected; expected != names.end();) {
        ASSERT_TRUE(actual->seek(*expected));
        ASSERT_EQ(*expected, actual->value().name);

        if (prev != expected) {
          ASSERT_TRUE(actual->seek(*prev)); // can't seek backwards
          ASSERT_EQ(*expected, actual->value().name);
        }

        // seek to the same value
        ASSERT_TRUE(actual->seek(*expected));
        ASSERT_EQ(*expected, actual->value().name);

        prev = expected;
        ++expected;
      }
      ASSERT_FALSE(actual->next()); // reached the end
      ASSERT_FALSE(actual->seek(names.front())); // can't seek backwards
      ASSERT_FALSE(actual->next());
    }

    // seek before the first element
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      auto actual = segment.columns();
      auto expected = names.begin();

      const auto key = irs::string_ref("0");
      ASSERT_TRUE(key < names.front());
      ASSERT_TRUE(actual->seek(key));
      ASSERT_EQ(*expected, actual->value().name);

      ++expected;
      for (auto prev = names.begin(); expected != names.end();) {
        ASSERT_TRUE(actual->next());
        ASSERT_EQ(*expected, actual->value().name);

        if (prev != expected) {
          ASSERT_TRUE(actual->seek(*prev)); // can't seek backwards
          ASSERT_EQ(*expected, actual->value().name);
        }

        // seek to the same value
        ASSERT_TRUE(actual->seek(*expected));
        ASSERT_EQ(*expected, actual->value().name);

        prev = expected;
        ++expected;
      }
      ASSERT_FALSE(actual->next()); // reached the end
      ASSERT_FALSE(actual->seek(names.front())); // can't seek backwards
      ASSERT_FALSE(actual->next());
    }

    // seek after the last element
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      auto actual = segment.columns();

      const auto key = irs::string_ref("~");
      ASSERT_TRUE(key > names.back());
      ASSERT_FALSE(actual->seek(key));
      ASSERT_FALSE(actual->next()); // reached the end
      ASSERT_FALSE(actual->seek(names.front())); // can't seek backwards
    }

    // seek in between
    {
      std::vector<std::pair<irs::string_ref, irs::string_ref>> seeks {
        { "0B", names[1] }, { names[1], names[1] }, { "0", names[1] },
        { "D", names[13] }, { names[13], names[13] }, { names[12], names[13] },
        { "P", names[20] }, { "Z", names[27] }
      };

      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      auto actual = segment.columns();

      for (auto& seek : seeks) {
        auto& key = seek.first;
        auto& expected = seek.second;

        ASSERT_TRUE(actual->seek(key));
        ASSERT_EQ(expected, actual->value().name);
      }

      const auto key = irs::string_ref("~");
      ASSERT_TRUE(key > names.back());
      ASSERT_FALSE(actual->seek(key));
      ASSERT_FALSE(actual->next()); // reached the end
      ASSERT_FALSE(actual->seek(names.front())); // can't seek backwards
    }

    // seek in between + next
    {
      std::vector<std::pair<irs::string_ref, size_t>> seeks {
        { "0B", 1 },  { "D", 13 }, { "O", 19 }, { "P", 20 }, { "Z", 27 }
      };

      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      for (auto& seek : seeks) {
        auto& key = seek.first;
        auto expected = names.begin() + seek.second;

        auto actual = segment.columns();

        ASSERT_TRUE(actual->seek(key));
        ASSERT_EQ(*expected, actual->value().name);

        for (++expected; expected != names.end(); ++expected) {
          ASSERT_TRUE(actual->next());
          ASSERT_EQ(*expected, actual->value().name);
        }

        ASSERT_FALSE(actual->next()); // reached the end
        ASSERT_FALSE(actual->seek(names.front())); // can't seek backwards
      }
    }
  }

  void read_write_doc_attributes() {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      &tests::generic_json_field_factory
    );
    tests::document const* doc1 = gen.next();
    tests::document const* doc2 = gen.next();
    tests::document const* doc3 = gen.next();
    tests::document const* doc4 = gen.next();

    // write documents
    {
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);

      // attributes only
      ASSERT_TRUE(insert(*writer, doc1->indexed.end(), doc1->indexed.end(), doc1->stored.begin(), doc1->stored.end()));
      ASSERT_TRUE(insert(*writer, doc2->indexed.end(), doc2->indexed.end(), doc2->stored.begin(), doc2->stored.end()));
      ASSERT_TRUE(insert(*writer, doc3->indexed.end(), doc3->indexed.end(), doc3->stored.begin(), doc3->stored.end()));
      ASSERT_TRUE(insert(*writer, doc4->indexed.end(), doc4->indexed.end(), doc4->stored.begin(), doc4->stored.end()));
      writer->commit();
    }

    // check inserted values:
    // - random read (not cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      // read attribute from invalid column
      {
        ASSERT_EQ(nullptr, segment.column_reader("invalid_column"));
      }

      // check number of values in the column
      {
        const auto* column = segment.column_reader("name");
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(4, column->size());
      }

      // read attributes from 'name' column (dense)
      {
        irs::bytes_ref actual_value;

        const auto* column = segment.column_reader("name");
        ASSERT_NE(nullptr, column);
        auto value_reader = column->values();

        ASSERT_TRUE(value_reader(2, actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
        ASSERT_TRUE(value_reader(4, actual_value));
        ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
        ASSERT_TRUE(value_reader(1, actual_value));
        ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
        ASSERT_TRUE(value_reader(3, actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_FALSE(value_reader(5, actual_value)); // invalid document id
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'name' value in doc3
      }

      // iterate over 'name' column (cached)
      {
        auto column = segment.column_reader("name");
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
          { 1, "A" }, { 2, "B" }, { 3, "C" }, { 4, "D" }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_value.first, it->value());
          ASSERT_EQ(expected_value.second, actual_str_value);
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(i, expected_values.size());
      }

      // read attributes from 'prefix' column (sparse)
      {
        irs::bytes_ref actual_value;
        const auto* column = segment.column_reader("prefix");
        ASSERT_NE(nullptr, column);
        auto value_reader = column->values();
        ASSERT_TRUE(value_reader(1, actual_value));
        ASSERT_EQ("abcd", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'prefix' value in doc1
        ASSERT_FALSE(value_reader(2, actual_value)); // doc2 does not contain 'prefix' column
        ASSERT_EQ("abcd", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'prefix' value in doc1
        ASSERT_TRUE(value_reader(4, actual_value));
        ASSERT_EQ("abcde", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'prefix' value in doc4
        ASSERT_FALSE(value_reader(3, actual_value)); // doc3 does not contain 'prefix' column
        ASSERT_EQ("abcde", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'prefix' value in doc4
        ASSERT_FALSE(value_reader(5, actual_value)); // invalid document id
        ASSERT_EQ("abcde", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'prefix' value in doc4
      }

      // iterate over 'prefix' column (cached)
      {
        auto column = segment.column_reader("prefix");
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
          { 1, "abcd" }, { 4, "abcde" }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_value.first, it->value());
          ASSERT_EQ(expected_value.second, actual_str_value);
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(i, expected_values.size());
      }
    }

    // check inserted values:
    // - iterate (not cached)
    // - random read (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());

      // read attribute from invalid column
      {
        ASSERT_EQ(nullptr, segment.column_reader("invalid_column"));
      }

      {
        // iterate over 'name' column (not cached)
        auto column = segment.column_reader("name");
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
          { 1, "A" }, { 2, "B" }, { 3, "C" }, { 4, "D" }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_value.first, it->value());
          ASSERT_EQ(expected_value.second, actual_str_value);
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(i, expected_values.size());
      }

      // read attributes from 'name' column (dense)
      {
        irs::bytes_ref actual_value;
        const auto* column = segment.column_reader("name");
        ASSERT_NE(nullptr, column);
        auto value_reader = column->values();
        ASSERT_TRUE(value_reader(2, actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
        ASSERT_TRUE(value_reader(4, actual_value));
        ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
        ASSERT_TRUE(value_reader(1, actual_value));
        ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
        ASSERT_TRUE(value_reader(3, actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_FALSE(value_reader(5, actual_value)); // invalid document id
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'name' value in doc3
      }

      // iterate over 'name' column (cached)
      {
        auto column = segment.column_reader("name");
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
          { 1, "A" }, { 2, "B" }, { 3, "C" }, { 4, "D" }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_value.first, it->value());
          ASSERT_EQ(expected_value.second, actual_str_value);
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(i, expected_values.size());
      }

      {
        // iterate over 'prefix' column (not cached)
        auto column = segment.column_reader("prefix");
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
          { 1, "abcd" }, { 4, "abcde" }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_value.first, it->value());
          ASSERT_EQ(expected_value.second, actual_str_value);
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(i, expected_values.size());
      }

      // read attributes from 'prefix' column (sparse)
      {
        irs::bytes_ref actual_value;
        const auto* column = segment.column_reader("prefix");
        ASSERT_NE(nullptr, column);
        auto value_reader = column->values();
        ASSERT_TRUE(value_reader(1, actual_value));
        ASSERT_EQ("abcd", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'prefix' value in doc1
        ASSERT_FALSE(value_reader(2, actual_value)); // doc2 does not contain 'prefix' column
        ASSERT_EQ("abcd", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'prefix' value in doc1
        ASSERT_TRUE(value_reader(4, actual_value));
        ASSERT_EQ("abcde", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'prefix' value in doc4
        ASSERT_FALSE(value_reader(3, actual_value)); // doc3 does not contain 'prefix' column
        ASSERT_EQ("abcde", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'prefix' value in doc4
        ASSERT_FALSE(value_reader(5, actual_value)); // invalid document id
        ASSERT_EQ("abcde", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'prefix' value in doc4
      }

      // iterate over 'prefix' column (cached)
      {
        auto column = segment.column_reader("prefix");
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
          { 1, "abcd" }, { 4, "abcde" }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_value.first, it->value());
          ASSERT_EQ(expected_value.second, actual_str_value);
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        ASSERT_EQ(i, expected_values.size());
      }
    }
  }

  void read_write_doc_attributes_big() {
    struct csv_doc_template_t: public tests::csv_doc_generator::doc_template {
      virtual void init() {
        clear();
        reserve(2);
        insert(std::make_shared<tests::templates::string_field>("id"));
        insert(std::make_shared<tests::templates::string_field>("label"));
      }

      virtual void value(size_t idx, const irs::string_ref& value) {
        switch(idx) {
         case 0:
          indexed.get<tests::templates::string_field>("id")->value(value);
          break;
         case 1:
          indexed.get<tests::templates::string_field>("label")->value(value);
        }
      }
    };

    csv_doc_template_t csv_doc_template;
    tests::csv_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template);
    size_t docs_count = 0;

    // write attributes 
    {
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);

      const tests::document* doc;
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(*writer, doc->indexed.end(), doc->indexed.end(), doc->stored.begin(), doc->stored.end()));
        ++docs_count;
      }
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(dir());
      ASSERT_EQ(1, reader.size());

      auto& segment = reader[0];
      auto columns = segment.columns();
      ASSERT_TRUE(columns->next());
      ASSERT_EQ("id", columns->value().name);
      ASSERT_EQ(0, columns->value().id);
      ASSERT_TRUE(columns->next());
      ASSERT_EQ("label", columns->value().name);
      ASSERT_EQ(1, columns->value().id);
      ASSERT_FALSE(columns->next());
      ASSERT_FALSE(columns->next());

      // check 'id' column
      {
        const iresearch::string_ref column_name = "id";
        auto* meta = segment.column(column_name);
        ASSERT_NE(nullptr, meta);

        // visit column (not cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
            if (id != ++expected_id) {
              return false;
            }

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);

            if (!field) {
              return false;
            }

            const auto actual_value = irs::to_string<irs::string_ref>(in.c_str());
            if (field->value() != actual_value) {
              return false;
            }

            return true;
          };

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          ASSERT_EQ(column, segment.column_reader(meta->id));
          ASSERT_TRUE(column->visit(visitor));
        }

        // random access
        {
          const tests::document* doc = nullptr;
          irs::bytes_ref actual_value;
          auto column = segment.column_reader(meta->id);
          ASSERT_NE(nullptr, column);
          auto reader = column->values();

          irs::doc_id_t id = 0;
          gen.reset();
          while ((doc = gen.next())) {
            ++id;
            ASSERT_TRUE(reader(id, actual_value));

            auto* field = doc->stored.get<tests::templates::string_field>(column_name);

            ASSERT_NE(nullptr, field);
            ASSERT_EQ(field->value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
          }
        }

        // visit column (cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
            if (id != ++expected_id) {
              return false;
            }

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);

            if (!field) {
              return false;
            }

            const auto actual_value = irs::to_string<irs::string_ref>(in.c_str());
            if (field->value() != actual_value) {
              return false;
            }

            return true;
          };

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          ASSERT_EQ(column, segment.column_reader(meta->id));
          ASSERT_TRUE(column->visit(visitor));
        }

        // iterate over column (cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          for (; it->next(); ) {
            ++expected_id;

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);

            ASSERT_TRUE(payload->next());
            const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

            ASSERT_EQ(expected_id, it->value());
            ASSERT_EQ(field->value(), actual_value_str);
          }

          ASSERT_FALSE(it->next());
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
          ASSERT_EQ(docs_count, expected_id);
        }
      }

      // check 'label' column
      {
        const iresearch::string_ref column_name = "label";
        auto* meta = segment.column(column_name);
        ASSERT_NE(nullptr, meta);

        // visit column (not cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
            if (id != ++expected_id) {
              return false;
            }

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);

            if (!field) {
              return false;
            }

            if (field->value() != irs::to_string<irs::string_ref>(in.c_str())) {
              return false;
            }

            return true;
          };

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          ASSERT_EQ(column, segment.column_reader(meta->id));
          ASSERT_TRUE(column->visit(visitor));
        }

        // random access
        {
          const tests::document* doc = nullptr;

          irs::bytes_ref actual_value;
          auto column = segment.column_reader(meta->id);
          ASSERT_NE(nullptr, column);
          auto reader = column->values();

          irs::doc_id_t id = 0;
          while ((doc = gen.next())) {
            ASSERT_TRUE(reader(++id, actual_value));

            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);
            ASSERT_EQ(field->value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
          }
        }

        // visit column (cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
            if (id != ++expected_id) {
              return false;
            }

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);

            if (!field) {
              return false;
            }

            if (field->value() != irs::to_string<irs::string_ref>(in.c_str())) {
              return false;
            }

            return true;
          };

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          ASSERT_EQ(column, segment.column_reader(meta->id));
          ASSERT_TRUE(column->visit(visitor));
        }

        // iterate over 'label' column (cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          for (; it->next(); ) {
            ++expected_id;

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);
            ASSERT_TRUE(payload->next());
            const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

            ASSERT_EQ(expected_id, it->value());
            ASSERT_EQ(field->value(), actual_value_str);
          }

          ASSERT_FALSE(it->next());
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
          ASSERT_EQ(docs_count, expected_id);
        }
      }
    }

    // check inserted values:
    // - visit (not cached)
    // - iterate (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = irs::directory_reader::open(dir());
      ASSERT_EQ(1, reader.size());

      auto& segment = reader[0];
      auto columns = segment.columns();
      ASSERT_TRUE(columns->next());
      ASSERT_EQ("id", columns->value().name);
      ASSERT_EQ(0, columns->value().id);
      ASSERT_TRUE(columns->next());
      ASSERT_EQ("label", columns->value().name);
      ASSERT_EQ(1, columns->value().id);
      ASSERT_FALSE(columns->next());
      ASSERT_FALSE(columns->next());

      // check 'id' column
      {
        const iresearch::string_ref column_name = "id";
        auto* meta = segment.column(column_name);
        ASSERT_NE(nullptr, meta);

        // visit column (not cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
            if (id != ++expected_id) {
              return false;
            }

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);

            if (!field) {
              return false;
            }

            const auto actual_value = irs::to_string<irs::string_ref>(in.c_str());
            if (field->value() != actual_value) {
              return false;
            }

            return true;
          };

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          ASSERT_EQ(column, segment.column_reader(meta->id));
          ASSERT_TRUE(column->visit(visitor));
        }

        // iterate over column (not cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          for (; it->next(); ) {
            ++expected_id;

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);
            ASSERT_TRUE(payload->next());
            const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

            ASSERT_EQ(expected_id, it->value());
            ASSERT_EQ(field->value(), actual_value_str);
          }

          ASSERT_FALSE(it->next());
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
          ASSERT_EQ(docs_count, expected_id);
        }

        // random access
        {
          const tests::document* doc = nullptr;
          irs::bytes_ref actual_value;
          auto column = segment.column_reader(meta->id);
          ASSERT_NE(nullptr, column);
          auto reader = column->values();

          irs::doc_id_t id = 0;
          gen.reset();
          while ((doc = gen.next())) {
            ++id;
            ASSERT_TRUE(reader(id, actual_value));

            auto* field = doc->stored.get<tests::templates::string_field>(column_name);

            ASSERT_NE(nullptr, field);
            ASSERT_EQ(field->value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
          }
        }

        // visit column (cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
            if (id != ++expected_id) {
              return false;
            }

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);

            if (!field) {
              return false;
            }

            const auto actual_value = irs::to_string<irs::string_ref>(in.c_str());
            if (field->value() != actual_value) {
              return false;
            }

            return true;
          };

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          ASSERT_EQ(column, segment.column_reader(meta->id));
          ASSERT_TRUE(column->visit(visitor));
        }

        // iterate over column (cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          for (; it->next(); ) {
            ++expected_id;

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);
            ASSERT_TRUE(payload->next());
            const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

            ASSERT_EQ(expected_id, it->value());
            ASSERT_EQ(field->value(), actual_value_str);
          }

          ASSERT_FALSE(it->next());
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
          ASSERT_EQ(docs_count, expected_id);
        }
      }

      // check 'label' column
      {
        const iresearch::string_ref column_name = "label";
        auto* meta = segment.column(column_name);
        ASSERT_NE(nullptr, meta);

        // visit column (not cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
            if (id != ++expected_id) {
              return false;
            }

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);

            if (!field) {
              return false;
            }

            if (field->value() != irs::to_string<irs::string_ref>(in.c_str())) {
              return false;
            }

            return true;
          };

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          ASSERT_EQ(column, segment.column_reader(meta->id));
          ASSERT_TRUE(column->visit(visitor));
        }

        // iterate over 'label' column (not cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          for (; it->next(); ) {
            ++expected_id;

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);
            ASSERT_TRUE(payload->next());
            const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

            ASSERT_EQ(expected_id, it->value());
            ASSERT_EQ(field->value(), actual_value_str);
          }

          ASSERT_FALSE(it->next());
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
          ASSERT_EQ(docs_count, expected_id);
        }

        // random access
        {
          const tests::document* doc = nullptr;

          irs::bytes_ref actual_value;
          auto column = segment.column_reader(meta->id);
          ASSERT_NE(nullptr, column);
          auto reader = column->values();

          irs::doc_id_t id = 0;
          while ((doc = gen.next())) {
            ASSERT_TRUE(reader(++id, actual_value));

            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);
            ASSERT_EQ(field->value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
          }
        }

        // visit column (cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id] (irs::doc_id_t id, const irs::bytes_ref& in) {
            if (id != ++expected_id) {
              return false;
            }

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);

            if (!field) {
              return false;
            }

            if (field->value() != irs::to_string<irs::string_ref>(in.c_str())) {
              return false;
            }

            return true;
          };

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          ASSERT_EQ(column, segment.column_reader(meta->id));
          ASSERT_TRUE(column->visit(visitor));
        }

        // iterate over 'label' column (cached)
        {
          gen.reset();
          irs::doc_id_t expected_id = 0;

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

          for (; it->next(); ) {
            ++expected_id;

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);
            ASSERT_TRUE(payload->next());
            const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value().c_str());

            ASSERT_EQ(expected_id, it->value());
            ASSERT_EQ(field->value(), actual_value_str);
          }

          ASSERT_FALSE(it->next());
          ASSERT_FALSE(payload->next());
          ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
          ASSERT_EQ(docs_count, expected_id);
        }
      }
    }
  }

  void insert_doc_with_null_empty_term() {
    class field {
      public:
      field(std::string&& name, const irs::string_ref& value)
        : name_(std::move(name)),
        value_(value) {}
      field(field&& other) NOEXCEPT
        : stream_(std::move(other.stream_)),
          name_(std::move(other.name_)),
          value_(std::move(other.value_)) {
      }
      irs::string_ref name() const { return name_; }
      float_t boost() const { return 1.f; }
      irs::token_stream& get_tokens() const {
        stream_.reset(value_);
        return stream_;
      }
      const irs::flags& features() const {
        return irs::flags::empty_instance();
      }

      private:
      mutable irs::string_token_stream stream_;
      std::string name_;
      irs::string_ref value_;
    }; // field

    // write docs with empty terms
    {
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);
      // doc0: empty, nullptr
      {
        std::vector<field> doc;
        doc.emplace_back(std::string("name"), irs::string_ref("", 0));
        doc.emplace_back(std::string("name"), irs::string_ref::NIL);
        ASSERT_TRUE(insert(*writer, doc.begin(), doc.end()));
      }
      // doc1: nullptr, empty, nullptr
      {
        std::vector<field> doc;
        doc.emplace_back(std::string("name1"), irs::string_ref::NIL);
        doc.emplace_back(std::string("name1"), irs::string_ref("", 0));
        doc.emplace_back(std::string("name"), irs::string_ref::NIL);
        ASSERT_TRUE(insert(*writer, doc.begin(), doc.end()));
      }
      writer->commit();
    }

    // check fields with empty terms
    {
      auto reader = irs::directory_reader::open(dir());
      ASSERT_EQ(1, reader.size());
      auto& segment = reader[0];

      {
        size_t count = 0;
        auto fields = segment.fields();
        while (fields->next()) {
          ++count;
        }
        ASSERT_EQ(2, count);
      }

      {
        auto* field = segment.field("name");
        ASSERT_NE(nullptr, field);
        ASSERT_EQ(1, field->size());
        ASSERT_EQ(2, field->docs_count());
        auto term = field->iterator();
        ASSERT_TRUE(term->next());
        ASSERT_EQ(0, term->value().size());
        ASSERT_FALSE(term->next());
      }

      {
        auto* field = segment.field("name1");
        ASSERT_NE(nullptr, field);
        ASSERT_EQ(1, field->size());
        ASSERT_EQ(1, field->docs_count());
        auto term = field->iterator();
        ASSERT_TRUE(term->next());
        ASSERT_EQ(0, term->value().size());
        ASSERT_FALSE(term->next());
      }
    }
  }

  void writer_bulk_insert() {
    class indexed_and_stored_field {
     public:
      indexed_and_stored_field(std::string&& name, const irs::string_ref& value, bool stored_valid = true, bool indexed_valid = true)
        : name_(std::move(name)), value_(value), stored_valid_(stored_valid) {
        if (!indexed_valid) {
          features_.add<tests::incompatible_attribute>();
        }
      }
      indexed_and_stored_field(indexed_and_stored_field&& other) NOEXCEPT
        : features_(std::move(other.features_)),
          stream_(std::move(other.stream_)),
          name_(std::move(other.name_)),
          value_(std::move(other.value_)),
          stored_valid_(other.stored_valid_) {
      }
      irs::string_ref name() const { return name_; }
      float_t boost() const { return 1.f; }
      irs::token_stream& get_tokens() const {
        stream_.reset(value_);
        return stream_;
      }
      const irs::flags& features() const {
        return features_;
      }
      bool write(irs::data_output& out) const NOEXCEPT {
        irs::write_string(out, value_);
        return stored_valid_;
      }

     private:
      irs::flags features_;
      mutable irs::string_token_stream stream_;
      std::string name_;
      irs::string_ref value_;
      bool stored_valid_;
    }; // indexed_and_stored_field

    class indexed_field {
     public:
      indexed_field(std::string&& name, const irs::string_ref& value, bool valid = true)
        : name_(std::move(name)), value_(value) {
        if (!valid) {
          features_.add<tests::incompatible_attribute>();
        }
      }
      indexed_field(indexed_field&& other) NOEXCEPT
        : features_(std::move(other.features_)),
          stream_(std::move(other.stream_)),
          name_(std::move(other.name_)),
          value_(std::move(other.value_)) {
      }
      irs::string_ref name() const { return name_; }
      float_t boost() const { return 1.f; }
      irs::token_stream& get_tokens() const {
        stream_.reset(value_);
        return stream_;
      }
      const irs::flags& features() const {
        return features_;
      }

     private:
      irs::flags features_;
      mutable irs::string_token_stream stream_;
      std::string name_;
      irs::string_ref value_;
    }; // indexed_field

    struct stored_field {
      stored_field(const irs::string_ref& name, const irs::string_ref& value, bool valid = true)
        : name_(name), value_(value), valid_(valid) {
      }

      const irs::string_ref& name() const {
        return name_;
      }

      bool write(irs::data_output& out) const {
        write_string(out, value_);
        return valid_;
      }

      irs::string_ref name_;
      irs::string_ref value_;
      bool valid_;
    }; // stored_field

    // insert documents
    auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);

    size_t i = 0;
    const size_t max = 8;
    bool states[max];
    std::fill(std::begin(states), std::end(states), true);

    auto ctx = writer->documents();

    do {
      auto doc = ctx.insert();
      auto& state = states[i];

      switch (i) {
        case 0: { // doc0
          indexed_field indexed("indexed", "doc0");
          state &= doc.insert(irs::action::index, indexed);
          stored_field stored("stored", "doc0");
          state &= doc.insert(irs::action::store, stored);
          indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc0");
          state &= doc.insert(irs::action::index_store, indexed_and_stored);
          ASSERT_TRUE(doc);
        } break;
        case 1: { // doc1
          // indexed and stored fields can be indexed/stored only
          indexed_and_stored_field indexed("indexed", "doc1");
          state &= doc.insert(irs::action::index, indexed);
          indexed_and_stored_field stored("stored", "doc1");
          state &= doc.insert(irs::action::store, stored);
          ASSERT_TRUE(doc);
        } break;
        case 2: { // doc2 (will be dropped since it contains invalid stored field)
          indexed_and_stored_field indexed("indexed", "doc2");
          state &= doc.insert(irs::action::index, indexed);
          stored_field stored("stored", "doc2", false);
          state &= doc.insert(irs::action::store, stored);
          ASSERT_FALSE(doc);
        } break;
        case 3: { // doc3 (will be dropped since it contains invalid indexed field)
          indexed_field indexed("indexed", "doc3", false);
          state &= doc.insert(irs::action::index, indexed);
          stored_field stored("stored", "doc3");
          state &= doc.insert(irs::action::store, stored);
          ASSERT_FALSE(doc);
        } break;
        case 4: { // doc4 (will be dropped since it contains invalid indexed and stored field)
          indexed_and_stored_field indexed_and_stored("indexed", "doc4", false, false);
          state &= doc.insert(irs::action::index_store, indexed_and_stored);
          stored_field stored("stored", "doc4");
          state &= doc.insert(irs::action::store, stored);
          ASSERT_FALSE(doc);
        } break;
        case 5: { // doc5 (will be dropped since it contains failed stored field)
          indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc5", false); // will fail on store, but will pass on index
          state &= doc.insert(irs::action::index_store, indexed_and_stored);
          stored_field stored("stored", "doc5");
          state &= doc.insert(irs::action::store, stored);
          ASSERT_FALSE(doc);
        } break;
        case 6: { // doc6 (will be dropped since it contains failed indexed field)
          indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc6", true, false); // will fail on index, but will pass on store
          state &= doc.insert(irs::action::index_store, indexed_and_stored);
          stored_field stored("stored", "doc6");
          state &= doc.insert(irs::action::store, stored);
          ASSERT_FALSE(doc);
        } break;
        case 7: { // valid insertion of last doc will mark bulk insert result as valid
          indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc7", true, true); // will be indexed, and will be stored
          state &= doc.insert(irs::action::index_store, indexed_and_stored);
          stored_field stored("stored", "doc7");
          state &= doc.insert(irs::action::store, stored);
          ASSERT_TRUE(doc);
        } break;
      }
    } while (++i != max);

    ASSERT_TRUE(states[0]); // successfully inserted
    ASSERT_TRUE(states[1]); // successfully inserted
    ASSERT_FALSE(states[2]); // skipped
    ASSERT_FALSE(states[3]); // skipped
    ASSERT_FALSE(states[4]); // skipped
    ASSERT_FALSE(states[5]); // skipped
    ASSERT_FALSE(states[6]); // skipped
    ASSERT_TRUE(states[7]); // successfully inserted

    { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
    writer->commit();

    // check index
    {
      auto reader = irs::directory_reader::open(dir());
      ASSERT_EQ(1, reader.size());
      auto& segment = reader[0];
      ASSERT_EQ(8, reader->docs_count()); // we have 8 documents in total
      ASSERT_EQ(3, reader->live_docs_count()); // 5 of which marked as deleted

      std::unordered_set<std::string> expected_values { "doc0", "doc1", "doc7" };
      std::unordered_set<std::string> actual_values;
      irs::bytes_ref value;

      const auto* column_reader = segment.column_reader("stored");
      ASSERT_NE(nullptr, column_reader);
      auto column = column_reader->values();

      auto it = segment.docs_iterator();
      while (it->next()) {
        ASSERT_TRUE(column(it->value(), value));
        actual_values.emplace(irs::to_string<std::string>(value.c_str()));
      }
      ASSERT_EQ(expected_values, actual_values);
    }
  }

  void writer_atomicity_check() {
    struct override_sync_directory : directory_mock {
      typedef std::function<bool (const std::string&)> sync_f;

      override_sync_directory(irs::directory& impl, sync_f&& sync)
        : directory_mock(impl), sync_(std::move(sync)) {
      }

      virtual bool sync(const std::string& name) NOEXCEPT override {
        try {
          if (sync_(name)) {
            return true;
          }
        } catch (...) {
          return false;
        }

        return directory_mock::sync(name);
      }

      sync_f sync_;
    }; // invalid_sync_directory 

    // create empty index
    {
      auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);

      writer->commit();
    }

    // error while commiting index (during sync in index_meta_writer)
    {
      override_sync_directory override_dir(
        dir(), [this] (const irs::string_ref& name) {
          throw irs::io_error();
          return true;
      });

      tests::json_doc_generator gen(
        resource("simple_sequential.json"), 
        &tests::generic_json_field_factory
      );
      tests::document const* doc1 = gen.next();
      tests::document const* doc2 = gen.next();
      tests::document const* doc3 = gen.next();
      tests::document const* doc4 = gen.next();

      auto writer = irs::index_writer::make(override_dir, codec(), irs::OM_APPEND);

      ASSERT_TRUE(insert(*writer,
        doc1->indexed.begin(), doc1->indexed.end(),
        doc1->stored.begin(), doc1->stored.end()
      ));
      ASSERT_TRUE(insert(*writer,
        doc2->indexed.begin(), doc2->indexed.end(),
        doc2->stored.begin(), doc2->stored.end()
      ));
      ASSERT_TRUE(insert(*writer,
        doc3->indexed.begin(), doc3->indexed.end(),
        doc3->stored.begin(), doc3->stored.end()
      ));
      ASSERT_TRUE(insert(*writer,
        doc4->indexed.begin(), doc4->indexed.end(),
        doc4->stored.begin(), doc4->stored.end()
      ));
      ASSERT_THROW(writer->commit(), irs::io_error);
    }

    // error while commiting index (during sync in index_writer)
    {
      override_sync_directory override_dir(
        dir(), [this] (const irs::string_ref& name) {
        if (irs::starts_with(name, "_")) {
          throw irs::io_error();
        }
        return false;
      });

      tests::json_doc_generator gen(
        resource("simple_sequential.json"), 
        &tests::generic_json_field_factory
      );
      tests::document const* doc1 = gen.next();
      tests::document const* doc2 = gen.next();
      tests::document const* doc3 = gen.next();
      tests::document const* doc4 = gen.next();

      auto writer = irs::index_writer::make(override_dir, codec(), irs::OM_APPEND);

      ASSERT_TRUE(insert(*writer,
        doc1->indexed.begin(), doc1->indexed.end(),
        doc1->stored.begin(), doc1->stored.end()
      ));
      ASSERT_TRUE(insert(*writer,
        doc2->indexed.begin(), doc2->indexed.end(),
        doc2->stored.begin(), doc2->stored.end()
      ));
      ASSERT_TRUE(insert(*writer,
        doc3->indexed.begin(), doc3->indexed.end(),
        doc3->stored.begin(), doc3->stored.end()
      ));
      ASSERT_TRUE(insert(*writer,
        doc4->indexed.begin(), doc4->indexed.end(),
        doc4->stored.begin(), doc4->stored.end()
      ));
      ASSERT_THROW(writer->commit(), irs::io_error);
    }

    // check index, it should be empty 
    {
      auto reader = irs::directory_reader::open(dir(), codec());
      ASSERT_EQ(0, reader.live_docs_count());
      ASSERT_EQ(0, reader.docs_count());
      ASSERT_EQ(0, reader.size());
      ASSERT_EQ(reader.begin(), reader.end());
    }
  }
}; // index_test_case

class memory_test_case_base : public index_test_case_base {
protected:
  virtual irs::directory* get_directory() override {
    return new irs::memory_directory();
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
}; // memory_test_case_base

class fs_test_case_base : public index_test_case_base { 
protected:
  virtual void SetUp() override {
    index_test_case_base::SetUp();
    MSVC_ONLY(_setmaxstdio(2048)); // workaround for error: EMFILE - Too many open files
  }

  virtual irs::directory* get_directory() override {
    auto dir = test_dir();

    dir /= "index";

    return new iresearch::fs_directory(dir.utf8());
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
}; // fs_test_case_base

class mmap_test_case_base : public index_test_case_base {
protected:
  virtual void SetUp() override {
    index_test_case_base::SetUp();
    MSVC_ONLY(_setmaxstdio(2048)); // workaround for error: EMFILE - Too many open files
  }

  virtual irs::directory* get_directory() override {
    auto dir = test_dir();

    dir /= "index";

    return new iresearch::mmap_directory(dir.utf8());
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
}; // mmap_test_case_base

namespace cases {

template<typename Base>
class tfidf : public Base {
 public:
  using Base::assert_index;
  using Base::get_codec;
  using Base::get_directory;

  void assert_index(size_t skip = 0) const {
    assert_index(irs::flags(), skip);
    assert_index(irs::flags{ irs::document::type() }, skip);
    assert_index(irs::flags{ irs::document::type(), irs::frequency::type() }, skip);
    assert_index(irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type() }, skip);
    assert_index(irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::offset::type() }, skip);
    assert_index(irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::payload::type() }, skip);
    assert_index(irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::payload::type(), irs::offset::type() }, skip);
  }
};

} // cases
} // tests

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_index_test 
  : public tests::cases::tfidf<tests::memory_test_case_base> {
}; // memory_index_test

TEST_F(memory_index_test, arango_demo_docs) {
  {
    tests::json_doc_generator gen(
      resource("arango_demo.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }
  assert_index();
}

TEST_F(memory_index_test, check_fields_order) {
  iterate_fields();
}

TEST_F(memory_index_test, check_attributes_order) {
  iterate_attributes();
}

TEST_F(memory_index_test, read_write_doc_attributes) {
  read_write_doc_attributes_sparse_column_sparse_variable_length();
  read_write_doc_attributes_sparse_column_dense_variable_length();
  read_write_doc_attributes_sparse_column_dense_fixed_length();
  read_write_doc_attributes_sparse_column_dense_fixed_offset();
  read_write_doc_attributes_sparse_column_sparse_mask();
  read_write_doc_attributes_sparse_column_dense_mask();
  read_write_doc_attributes_dense_column_dense_variable_length();
  read_write_doc_attributes_dense_column_dense_fixed_length();
  read_write_doc_attributes_dense_column_dense_fixed_offset();
  read_write_doc_attributes_dense_column_dense_mask();
  read_write_doc_attributes_big();
  read_write_doc_attributes();
  read_empty_doc_attributes();
}

TEST_F(memory_index_test, clear_writer) {
  clear_writer();
}

TEST_F(memory_index_test, open_writer) {
  open_writer_check_lock();
  open_writer_check_directory_allocator();
}

TEST_F(memory_index_test, check_writer_open_modes) {
  writer_check_open_modes();
}

TEST_F(memory_index_test, writer_transaction_isolation) {
  writer_transaction_isolation();
}

TEST_F(memory_index_test, writer_atomicity_check) {
  writer_atomicity_check();
}

TEST_F(memory_index_test, writer_bulk_insert) {
  writer_bulk_insert();
}

TEST_F(memory_index_test, writer_begin_rollback) {
  writer_begin_rollback();
}

TEST_F(memory_index_test, insert_null_empty_term) {
  insert_doc_with_null_empty_term();
}

TEST_F(memory_index_test, europarl_docs) {
  {
    tests::templates::europarl_doc_template doc;
    tests::delim_doc_generator gen(resource("europarl.subset.txt"), doc);
    add_segment(gen);
  }
  assert_index();
}

TEST_F(memory_index_test, monarch_eco_onthology) {
  {
    tests::json_doc_generator gen(
      resource("ECO_Monarch.json"),
      &tests::payloaded_json_field_factory);
    add_segment(gen);
  }
  assert_index();
}

TEST_F(memory_index_test, concurrent_read_column_mt) {
  concurrent_read_single_column_smoke();
  concurrent_read_multiple_columns();
}

TEST_F(memory_index_test, concurrent_read_index_mt) {
  concurrent_read_index();
}

TEST_F(memory_index_test, concurrent_add_mt) {
  tests::json_doc_generator gen(resource("simple_sequential.json"), &tests::generic_json_field_factory);
  std::vector<const tests::document*> docs;

  for (const tests::document* doc; (doc = gen.next()) != nullptr; docs.emplace_back(doc)) {}

  {
    auto writer = open_writer();

    std::thread thread0([&writer, docs](){
      for (size_t i = 0, count = docs.size(); i < count; i += 2) {
        auto& doc = docs[i];
        ASSERT_TRUE(insert(*writer,
          doc->indexed.begin(), doc->indexed.end(),
          doc->stored.begin(), doc->stored.end()
        ));
      }
    });
    std::thread thread1([&writer, docs](){
      for (size_t i = 1, count = docs.size(); i < count; i += 2) {
        auto& doc = docs[i];
        ASSERT_TRUE(insert(*writer,
          doc->indexed.begin(), doc->indexed.end(),
          doc->stored.begin(), doc->stored.end()
        ));
      }
    });

    thread0.join();
    thread1.join();
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader.size() == 1 || reader.size() == 2); // can be 1 if thread0 finishes before thread1 starts
    ASSERT_EQ(docs.size(), reader.docs_count());
  }
}

TEST_F(memory_index_test, concurrent_add_remove_mt) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        irs::string_ref(name),
        data.str
      ));
    }
  });
  std::vector<const tests::document*> docs;
  std::atomic<bool> first_doc(false);

  for (const tests::document* doc; (doc = gen.next()) != nullptr; docs.emplace_back(doc)) {}

  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    std::thread thread0([&writer, docs, &first_doc]() {
      auto& doc = docs[0];
      insert(*writer,
        doc->indexed.begin(), doc->indexed.end(),
        doc->stored.begin(), doc->stored.end()
      );
      first_doc = true;

      for (size_t i = 2, count = docs.size(); i < count; i += 2) { // skip first doc
        auto& doc = docs[i];
        insert(*writer,
          doc->indexed.begin(), doc->indexed.end(),
          doc->stored.begin(), doc->stored.end()
        );
      }
    });
    std::thread thread1([&writer, docs](){
      for (size_t i = 1, count = docs.size(); i < count; i += 2) {
        auto& doc = docs[i];
        insert(*writer,
          doc->indexed.begin(), doc->indexed.end(),
          doc->stored.begin(), doc->stored.end()
        );
      }
    });
    std::thread thread2([&writer,&query_doc1, &first_doc](){
      while(!first_doc); // busy-wait until first document loaded
      writer->documents().remove(std::move(query_doc1.filter));
    });

    thread0.join();
    thread1.join();
    thread2.join();
    writer->commit();

    std::unordered_set<irs::string_ref> expected = { "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "~", "!", "@", "#", "$", "%" };
    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader.size() == 1 || reader.size() == 2 || reader.size() == 3); // can be 1 if thread0 finishes before thread1 starts, can be 2 if thread0 and thread1 finish before thread2 starts
    ASSERT_TRUE(reader.docs_count() == docs.size() || reader.docs_count() == docs.size() - 1); // removed doc might have been on its own segment

    irs::bytes_ref actual_value;
    for (size_t i = 0, count = reader.size(); i < count; ++i) {
      auto& segment = reader[i];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      while(docsItr->next()) {
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ(1, expected.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }
    }

    ASSERT_TRUE(expected.empty());
  }
}

TEST_F(memory_index_test, concurrent_add_remove_overlap_commit_mt) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        irs::string_ref(name),
        data.str
      ));
    }
  });

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();

  // remove added docs, add same docs again commit separate thread before end of add
  {
    std::condition_variable cond;
    std::mutex mutex;
    auto query_doc1_doc2 = iresearch::iql::query_builder().build("name==A || name==B", std::locale::classic());
    auto writer = open_writer();
    SCOPED_LOCK_NAMED(mutex, lock);
    std::atomic<bool> stop(false);
    std::thread thread([&cond, &mutex, &writer, &stop]()->void {
      SCOPED_LOCK(mutex);
      writer->commit();
      stop = true;
      cond.notify_all();
    });

    // initial add docs
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();

    // remove docs
    writer->documents().remove(*(query_doc1_doc2.filter.get()));

    // re-add docs into a single segment
    {
      auto ctx = writer->documents();

      {
        auto doc = ctx.insert();
        doc.insert(irs::action::index, doc1->indexed.begin(), doc1->indexed.end());
        doc.insert(irs::action::store, doc1->indexed.begin(), doc1->indexed.end());
      }
      {
        auto doc = ctx.insert();
        doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end());
        doc.insert(irs::action::store, doc2->indexed.begin(), doc2->indexed.end());
      }

      // commit from a separate thread before end of add
      lock.unlock();
      std::mutex cond_mutex;
      SCOPED_LOCK_NAMED(cond_mutex, cond_lock);
      auto result = cond.wait_for(cond_lock, std::chrono::milliseconds(100)); // assume thread commits within 100 msec

      // MSVC 2015/2017 seems to sporadically notify condition variables without explicit request
      MSVC2015_ONLY(while(!stop && result == std::cv_status::no_timeout) result = cond.wait_for(cond_lock, std::chrono::milliseconds(100)));
      MSVC2017_ONLY(while(!stop && result == std::cv_status::no_timeout) result = cond.wait_for(cond_lock, std::chrono::milliseconds(100)));

      // FIXME TODO add once segment_context will not block flush_all()
      //ASSERT_TRUE(stop);
    }

    thread.join();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.docs_count());
    ASSERT_EQ(2, reader.live_docs_count());
  }

  // remove added docs, add same docs again commit separate thread after end of add
  {
    auto query_doc1_doc2 = iresearch::iql::query_builder().build("name==A || name==B", std::locale::classic());
    auto writer = open_writer();

    // initial add docs
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();

    // remove docs
    writer->documents().remove(*(query_doc1_doc2.filter.get()));

    // re-add docs into a single segment
    {
      auto ctx = writer->documents();

      {
        auto doc = ctx.insert();
        doc.insert(irs::action::index, doc1->indexed.begin(), doc1->indexed.end());
        doc.insert(irs::action::store, doc1->indexed.begin(), doc1->indexed.end());
      }
      {
        auto doc = ctx.insert();
        doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end());
        doc.insert(irs::action::store, doc2->indexed.begin(), doc2->indexed.end());
      }
    }

    std::thread thread([&writer]()->void {
      writer->commit();
    });
    thread.join();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.docs_count());
    ASSERT_EQ(2, reader.live_docs_count());
  }
}

TEST_F(memory_index_test, document_context) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        irs::string_ref(name),
        data.str
      ));
    }
  });

  irs::bytes_ref actual_value;
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  struct {
    std::condition_variable cond;
    std::mutex cond_mutex;
    std::mutex mutex;
    irs::string_ref const& name() { return irs::string_ref::EMPTY; }
    bool write(irs::data_output& out) {
      {
        SCOPED_LOCK(cond_mutex);
        cond.notify_all();
      }
      SCOPED_LOCK(mutex);
      return true;
    }
  } field;

  // during insert across commit blocks
  {
    auto writer = open_writer();
    SCOPED_LOCK_NAMED(field.cond_mutex, field_cond_lock); // wait for insertion to start
    SCOPED_LOCK_NAMED(field.mutex, field_lock); // prevent field from finishing

    writer->documents().insert().insert(irs::action::store, doc1->stored.begin(), doc1->stored.end()); // ensure segment is prsent in the active flush_context

    std::thread thread0([&writer, &field]()->void {
      writer->documents().insert().insert(irs::action::store, field);
    });

    ASSERT_EQ(std::cv_status::no_timeout, field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000))); // wait for insertion to start

    std::atomic<bool> stop(false);
    std::thread thread1([&writer, &field, &stop]()->void {
      writer->commit();
      stop = true;
      SCOPED_LOCK(field.cond_mutex);
      field.cond.notify_all();
    });

    auto result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100));

    // MSVC 2015/2017 seems to sporadically notify condition variables without explicit request
    MSVC2015_ONLY(while(!stop && result == std::cv_status::no_timeout) result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)));
    MSVC2017_ONLY(while(!stop && result == std::cv_status::no_timeout) result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)));

    ASSERT_EQ(std::cv_status::timeout, result); // verify commit() blocks
    field_lock.unlock();
    ASSERT_EQ(std::cv_status::no_timeout, field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000))); // verify commit() finishes
    thread0.join();
    thread1.join();
  }

  // during replace across commit blocks (single doc)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    SCOPED_LOCK_NAMED(field.cond_mutex, field_cond_lock); // wait for insertion to start
    SCOPED_LOCK_NAMED(field.mutex, field_lock); // prevent field from finishing

    std::thread thread0([&writer, &query_doc1, &field]()->void {
      writer->documents().replace(*query_doc1.filter).insert(irs::action::store, field);
    });

    ASSERT_EQ(std::cv_status::no_timeout, field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000))); // wait for insertion to start

    std::atomic<bool> commit(false);
    std::thread thread1([&writer, &field, &commit]()->void {
      writer->commit();
      commit = true;
      SCOPED_LOCK(field.cond_mutex);
      field.cond.notify_all();
    });

    auto result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)); // verify commit() blocks

    // MSVC 2015/2017 seems to sporadically notify condition variables without explicit request
    MSVC2015_ONLY(while(!commit && result == std::cv_status::no_timeout) result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)));
    MSVC2017_ONLY(while(!commit && result == std::cv_status::no_timeout) result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)));

    ASSERT_EQ(std::cv_status::timeout, result);
    field_lock.unlock();
    ASSERT_EQ(std::cv_status::no_timeout, field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000))); // verify commit() finishes
    thread0.join();
    thread1.join();
  }

  // during replace across commit blocks (functr)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    SCOPED_LOCK_NAMED(field.cond_mutex, field_cond_lock); // wait for insertion to start
    SCOPED_LOCK_NAMED(field.mutex, field_lock); // prevent field from finishing

    std::thread thread0([&writer, &query_doc1, &field]()->void {
      writer->documents().replace(
        *query_doc1.filter,
        [&field](irs::segment_writer::document& doc)->bool {
          doc.insert(irs::action::store, field);
          return false;
        }
      );
    });

    ASSERT_EQ(std::cv_status::no_timeout, field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000))); // wait for insertion to start

    std::atomic<bool> commit(false);
    std::thread thread1([&writer, &field, &commit]()->void {
      writer->commit();
      commit = true;
      SCOPED_LOCK(field.cond_mutex);
      field.cond.notify_all();
    });

    auto result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)); // verify commit() blocks

    // MSVC 2015/2017 seems to sporadically notify condition variables without explicit request
    MSVC2015_ONLY(while(!commit && result == std::cv_status::no_timeout) result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)));
    MSVC2017_ONLY(while(!commit && result == std::cv_status::no_timeout) result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)));

    ASSERT_EQ(std::cv_status::timeout, result);
    field_lock.unlock();
    ASSERT_EQ(std::cv_status::no_timeout, field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000))); // verify commit() finishes
    thread0.join();
    thread1.join();
  }

  // holding document_context after insert across commit does not block
  {
    auto writer = open_writer();
    auto ctx = writer->documents();
    SCOPED_LOCK_NAMED(field.cond_mutex, field_cond_lock); // wait for insertion to start

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    std::thread thread1([&writer, &field]()->void {
      writer->commit();
      SCOPED_LOCK(field.cond_mutex);
      field.cond.notify_all();
    });

    ASSERT_EQ(std::cv_status::no_timeout, field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000))); // verify commit() finishes
    { irs::index_writer::documents_context(std::move(ctx)); } // release ctx before join() in case of test failure
    thread1.join();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // holding document_context after remove across commit does not block
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));

    auto ctx = writer->documents();
    SCOPED_LOCK_NAMED(field.cond_mutex, field_cond_lock); // wait for insertion to start
    ctx.remove(*(query_doc1.filter));
    std::atomic<bool> commit(false); // FIXME TODO remove once segment_context will not block flush_all()
    std::thread thread1([&writer, &field, &commit]()->void {
      writer->commit();
      commit = true;
      SCOPED_LOCK(field.cond_mutex);
      field.cond.notify_all();
    });

    auto result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000)); // verify commit() finishes FIXME TODO remove once segment_context will not block flush_all()

    // MSVC 2015/2017 seems to sporadically notify condition variables without explicit request FIXME TODO remove once segment_context will not block flush_all()
    MSVC2015_ONLY(while(!commit && result == std::cv_status::no_timeout) result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)));
    MSVC2017_ONLY(while(!commit && result == std::cv_status::no_timeout) result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)));

    ASSERT_EQ(std::cv_status::timeout, result); field_cond_lock.unlock(); // verify commit() finishes FIXME TODO use below once segment_context will not block flush_all()
    //ASSERT_EQ(std::cv_status::no_timeout, field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000))); // verify commit() finishes
    { irs::index_writer::documents_context(std::move(ctx)); } // release ctx before join() in case of test failure
    thread1.join();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // holding document_context after replace across commit does not block (single doc)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    auto ctx = writer->documents();
    SCOPED_LOCK_NAMED(field.cond_mutex, field_cond_lock); // wait for insertion to start
    {
      auto doc = ctx.replace(*(query_doc1.filter));
      doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end());
      doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end());
    }
    std::atomic<bool> commit(false); // FIXME TODO remove once segment_context will not block flush_all()
    std::thread thread1([&writer, &field, &commit]()->void {
      writer->commit();
      commit = true;
      SCOPED_LOCK(field.cond_mutex);
      field.cond.notify_all();
    });

    auto result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000)); // verify commit() finishes FIXME TODO remove once segment_context will not block flush_all()

    // MSVC 2015/2017 seems to sporadically notify condition variables without explicit request FIXME TODO remove once segment_context will not block flush_all()
    MSVC2015_ONLY(while(!commit && result == std::cv_status::no_timeout) result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)));
    MSVC2017_ONLY(while(!commit && result == std::cv_status::no_timeout) result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)));

    ASSERT_EQ(std::cv_status::timeout, result); field_cond_lock.unlock(); // verify commit() finishes FIXME TODO use below once segment_context will not block flush_all()
    //ASSERT_EQ(std::cv_status::no_timeout, field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000))); // verify commit() finishes
    { irs::index_writer::documents_context(std::move(ctx)); } // release ctx before join() in case of test failure
    thread1.join();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // holding document_context after replace across commit does not block (functr)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    auto ctx = writer->documents();
    SCOPED_LOCK_NAMED(field.cond_mutex, field_cond_lock); // wait for insertion to start
    ctx.replace(
      *(query_doc1.filter),
      [&doc2](irs::segment_writer::document& doc)->bool {
        doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end());
        doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end());
        return false;
      }
    );
    std::atomic<bool> commit(false); // FIXME TODO remove once segment_context will not block flush_all()
    std::thread thread1([&writer, &field, &commit]()->void {
      writer->commit();
      commit = true;
      SCOPED_LOCK(field.cond_mutex);
      field.cond.notify_all();
    });

    auto result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000)); // verify commit() finishes FIXME TODO remove once segment_context will not block flush_all()

    // MSVC 2015/2017 seems to sporadically notify condition variables without explicit request FIXME TODO remove once segment_context will not block flush_all()
    MSVC2015_ONLY(while(!commit && result == std::cv_status::no_timeout) result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)));
    MSVC2017_ONLY(while(!commit && result == std::cv_status::no_timeout) result = field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(100)));

    ASSERT_EQ(std::cv_status::timeout, result); field_cond_lock.unlock(); // verify commit() finishes FIXME TODO use below once segment_context will not block flush_all()
    // ASSERT_EQ(std::cv_status::no_timeout, field.cond.wait_for(field_cond_lock, std::chrono::milliseconds(1000))); // verify commit() finishes
    { irs::index_writer::documents_context(std::move(ctx)); } // release ctx before join() in case of test failure
    thread1.join();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // rollback empty
  {
    auto writer = open_writer();

    {
      auto ctx = writer->documents();

      ctx.reset();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc1->indexed.begin(), doc1->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc1->stored.begin(), doc1->stored.end()));
      }
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // rollback inserts
  {
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    {
      auto ctx = writer->documents();

      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
      ctx.reset();
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // rollback inserts + some more
  {
    auto writer = open_writer();

    {
      auto ctx = writer->documents();

      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc1->indexed.begin(), doc1->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc1->stored.begin(), doc1->stored.end()));
      }
      ctx.reset();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // rollback multiple inserts + some more
  {
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    {
      auto ctx = writer->documents();

      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
      ctx.reset();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc3->indexed.begin(), doc3->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc3->stored.begin(), doc3->stored.end()));
      }
      ctx.reset();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc4->indexed.begin(), doc4->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc4->stored.begin(), doc4->stored.end()));
      }
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }

  // rollback inserts split over multiple segment_writers
  {
    irs::index_writer::options options;
    options.segment_docs_max = 1; // each doc will have its own segment
    auto writer = open_writer(irs::OM_CREATE, options);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    {
      auto ctx = writer->documents();

      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc3->indexed.begin(), doc3->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc3->stored.begin(), doc3->stored.end()));
      }
      ctx.reset();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc4->indexed.begin(), doc4->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc4->stored.begin(), doc4->stored.end()));
      }
    }

    writer->commit();

   auto reader = iresearch::directory_reader::open(dir(), codec());
   ASSERT_EQ(2, reader.size());

   {
      auto& segment = reader[0]; // assume 0 is id of first segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of second segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }
  }

  // rollback removals
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    {
      auto ctx = writer->documents();

      ctx.remove(*(query_doc1.filter));
      ctx.reset();
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // rollback removals + some more
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    {
      auto ctx = writer->documents();

      ctx.remove(*(query_doc1.filter));
      ctx.reset();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // rollback removals split over multiple segment_writers
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    irs::index_writer::options options;
    options.segment_docs_max = 1; // each doc will have its own segment
    auto writer = open_writer(irs::OM_CREATE, options);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    {
      auto ctx = writer->documents();

      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
      ctx.remove(*(query_doc1.filter));
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc3->indexed.begin(), doc3->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc3->stored.begin(), doc3->stored.end()));
      }
      ctx.remove(*(query_doc2.filter));
      ctx.reset();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc4->indexed.begin(), doc4->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc4->stored.begin(), doc4->stored.end()));
      }
    }

    writer->commit();

   auto reader = iresearch::directory_reader::open(dir(), codec());
   ASSERT_EQ(2, reader.size());

   {
      auto& segment = reader[0]; // assume 0 is id of first segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of second segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }
  }

  // rollback replace (single doc)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    {
      auto ctx = writer->documents();

      {
        auto doc = ctx.replace(*(query_doc1.filter));
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
      ctx.reset();
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // rollback replace (single doc) + some more
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    {
      auto ctx = writer->documents();

      {
        auto doc = ctx.replace(*(query_doc1.filter));
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
      ctx.reset();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc3->indexed.begin(), doc3->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc3->stored.begin(), doc3->stored.end()));
      }
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // rollback replacements (single doc) split over multiple segment_writers
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    irs::index_writer::options options;
    options.segment_docs_max = 1; // each doc will have its own segment
    auto writer = open_writer(irs::OM_CREATE, options);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    {
      auto ctx = writer->documents();

      {
        auto doc = ctx.replace(*(query_doc1.filter));
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
      {
        auto doc = ctx.replace(*(query_doc2.filter));
        ASSERT_TRUE(doc.insert(irs::action::index, doc3->indexed.begin(), doc3->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc3->stored.begin(), doc3->stored.end()));
      }
      ctx.reset();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc4->indexed.begin(), doc4->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc4->stored.begin(), doc4->stored.end()));
      }
    }

    writer->commit();

   auto reader = iresearch::directory_reader::open(dir(), codec());
   ASSERT_EQ(2, reader.size());

   {
      auto& segment = reader[0]; // assume 0 is id of first segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of second segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }
  }

  // rollback replace (functr)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    {
      auto ctx = writer->documents();

      ctx.replace(
        *(query_doc1.filter),
        [&doc2](irs::segment_writer::document& doc)->bool {
          doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end());
          doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end());
          return false;
        }
      );
      ctx.reset();
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // rollback replace (functr) + some more
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    {
      auto ctx = writer->documents();

      ctx.replace(
        *(query_doc1.filter),
        [&doc2](irs::segment_writer::document& doc)->bool {
          doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end());
          doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end());
          return false;
        }
      );
      ctx.reset();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc3->indexed.begin(), doc3->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc3->stored.begin(), doc3->stored.end()));
      }
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // rollback replacements (functr) split over multiple segment_writers
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    irs::index_writer::options options;
    options.segment_docs_max = 1; // each doc will have its own segment
    auto writer = open_writer(irs::OM_CREATE, options);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    {
      auto ctx = writer->documents();

      ctx.replace(
        *(query_doc1.filter),
        [&doc2](irs::segment_writer::document& doc)->bool {
          doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end());
          doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end());
          return false;
        }
      );
      ctx.replace(
        *(query_doc2.filter),
        [&doc3](irs::segment_writer::document& doc)->bool {
          doc.insert(irs::action::index, doc3->indexed.begin(), doc3->indexed.end());
          doc.insert(irs::action::store, doc3->stored.begin(), doc3->stored.end());
          return false;
        }
      );
      ctx.reset();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc4->indexed.begin(), doc4->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc4->stored.begin(), doc4->stored.end()));
      }
    }

    writer->commit();

   auto reader = iresearch::directory_reader::open(dir(), codec());
   ASSERT_EQ(2, reader.size());

   {
      auto& segment = reader[0]; // assume 0 is id of first segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of second segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }
  }

  // segment flush due to memory bytes limit (same flush_context)
  {
    irs::index_writer::options options;
    options.segment_memory_max = 1; // arbitaty size < 1 document (first doc will always aquire a new segment_writer)
    auto writer = open_writer(irs::OM_CREATE, options);

    {
      auto ctx = writer->documents();

      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc1->indexed.begin(), doc1->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc1->stored.begin(), doc1->stored.end()));
      }
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of first segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of second segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }
  }

  // segment flush due to memory bytes limit (split over different flush_contexts)
  {
    irs::index_writer::options options;
    options.segment_memory_max = 1; // arbitaty size < 1 document (first doc will always aquire a new segment_writer)
    auto writer = open_writer(irs::OM_CREATE, options);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
/* FIXME TODO use below once segment_context will not block flush_all()
    {
      auto ctx = writer->documents(); // will reuse segment_context from above

      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
      writer->commit();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc3->indexed.begin(), doc3->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc3->stored.begin(), doc3->stored.end()));
      }
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of first segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of second segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
*/
  }

  // segment flush due to document count limit (same flush_context)
  {
    irs::index_writer::options options;
    options.segment_docs_max = 1; // each doc will have its own segment
    auto writer = open_writer(irs::OM_CREATE, options);

    {
      auto ctx = writer->documents();

      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc1->indexed.begin(), doc1->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc1->stored.begin(), doc1->stored.end()));
      }
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of first segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of second segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }
  }

  // segment flush due to document count limit (split over different flush_contexts)
  {
    irs::index_writer::options options;
    options.segment_docs_max = 1; // each doc will have its own segment
    auto writer = open_writer(irs::OM_CREATE, options);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
/* FIXME TODO use below once segment_context will not block flush_all()
    {
      auto ctx = writer->documents(); // will reuse segment_context from above

      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end()));
      }
      writer->commit();
      {
        auto doc = ctx.insert();
        ASSERT_TRUE(doc.insert(irs::action::index, doc3->indexed.begin(), doc3->indexed.end()));
        ASSERT_TRUE(doc.insert(irs::action::store, doc3->stored.begin(), doc3->stored.end()));
      }
    }

    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of first segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of second segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
*/
  }

  // reuse of same segment initially with indexed fields then with only stored fileds
  {
    auto writer = open_writer();
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit(); // ensure flush() is called
    writer->documents().insert().insert(
      irs::action::store,
      doc2->stored.begin(),
      doc2->stored.end()
    ); // document without any indexed attributes (reuse segment writer)
    writer->commit();

    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of first/old segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(irs::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 0 is id of first/new segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      std::unordered_set<irs::string_ref> expected = { "B" };
      ASSERT_EQ(1, column->size());
      ASSERT_TRUE(column->visit([&expected](irs::doc_id_t, const irs::bytes_ref& data)->bool {
        auto* value = data.c_str();
        auto actual_value = irs::ref_cast<char>(irs::vread_string<irs::string_ref>(value));
        return 1 == expected.erase(actual_value);
      }));
      ASSERT_TRUE(expected.empty());
    }
  }
}

TEST_F(memory_index_test, doc_removal) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        irs::string_ref(name),
        data.str
      ));
    }
  });

  irs::bytes_ref actual_value;

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();
  tests::document const* doc5 = gen.next();
  tests::document const* doc6 = gen.next();
  tests::document const* doc7 = gen.next();
  tests::document const* doc8 = gen.next();
  tests::document const* doc9 = gen.next();

  // new segment: add
  {
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add + remove 1st (as reference)
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->documents().remove(*(query_doc1.filter.get()));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add + remove 1st (as unique_ptr)
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->documents().remove(std::move(query_doc1.filter));
    writer->documents().remove(std::unique_ptr<irs::filter>(nullptr)); // test nullptr filter ignored
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add + remove 1st (as shared_ptr)
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->documents().remove(std::shared_ptr<iresearch::filter>(std::move(query_doc1.filter)));
    writer->documents().remove(std::shared_ptr<irs::filter>(nullptr)); // test nullptr filter ignored
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: remove + add
  {
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->documents().remove(std::move(query_doc2.filter)); // not present yet
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add + remove + readd
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->documents().remove(std::move(query_doc1.filter));
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add + remove, old segment: remove
  {
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto query_doc3 = iresearch::iql::query_builder().build("name==C", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->documents().remove(std::move(query_doc3.filter));
    writer->commit(); // document mask with 'doc3' created
    writer->documents().remove(std::move(query_doc2.filter));
    writer->commit(); // new document mask with 'doc2','doc3' created

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add + add, old segment: remove + remove + add
  {
    auto query_doc1_doc2 = iresearch::iql::query_builder().build("name==A||name==B", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc1_doc2.filter));
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add, old segment: remove
  {
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->documents().remove(std::move(query_doc2.filter));
    writer->documents().remove(std::unique_ptr<irs::filter>(nullptr)); // test nullptr filter ignored
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of old segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of new segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }

  // new segment: add + remove, old segment: remove
  {
    auto query_doc1_doc3 = iresearch::iql::query_builder().build("name==A || name==C", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->documents().remove(std::move(query_doc1_doc3.filter));
    writer->documents().remove(std::shared_ptr<irs::filter>(nullptr)); // test nullptr filter ignored
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of old segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of new segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }
  }

  // new segment: add + remove, old segment: add + remove old-old segment: remove
  {
    auto query_doc2_doc6_doc9 = iresearch::iql::query_builder().build("name==B||name==F||name==I", std::locale::classic());
    auto query_doc3_doc7 = iresearch::iql::query_builder().build("name==C||name==G", std::locale::classic());
    auto query_doc4 = iresearch::iql::query_builder().build("name==D", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    )); // A
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    )); // B
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    )); // C
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    )); // D
    writer->documents().remove(std::move(query_doc4.filter));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    )); // E
    ASSERT_TRUE(insert(*writer,
      doc6->indexed.begin(), doc6->indexed.end(),
      doc6->stored.begin(), doc6->stored.end()
    )); // F
    ASSERT_TRUE(insert(*writer,
      doc7->indexed.begin(), doc7->indexed.end(),
      doc7->stored.begin(), doc7->stored.end()
    )); // G
    writer->documents().remove(std::move(query_doc3_doc7.filter));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc8->indexed.begin(), doc8->indexed.end(),
      doc8->stored.begin(), doc8->stored.end()
    )); // H
    ASSERT_TRUE(insert(*writer,
      doc9->indexed.begin(), doc9->indexed.end(),
      doc9->stored.begin(), doc9->stored.end()
    )); // I
    writer->documents().remove(std::move(query_doc2_doc6_doc9.filter));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(3, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of old-old segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of old segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("E", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc5
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[2]; // assume 2 is id of new segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc8
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST_F(memory_index_test, doc_update) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        irs::string_ref(name),
        data.str
      ));
    }
  });

  irs::bytes_ref actual_value;

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  // new segment update (as reference)
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(update(*writer,
      *(query_doc1.filter.get()),
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment update (as unique_ptr)
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(update(*writer,
      std::move(query_doc1.filter),
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment update (as shared_ptr)
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(update(*writer,
      std::shared_ptr<iresearch::filter>(std::move(query_doc1.filter)),
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // old segment update
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(update(*writer,
      std::move(query_doc1.filter),
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of old segment
      auto terms = segment.field("same");
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of new segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }

  // 3x updates (same segment)
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto query_doc3 = iresearch::iql::query_builder().build("name==C", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(update(*writer,
      std::move(query_doc1.filter),
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(update(*writer,
      std::move(query_doc2.filter),
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(update(*writer,
      std::move(query_doc3.filter),
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }

  // 3x updates (different segments)
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto query_doc3 = iresearch::iql::query_builder().build("name==C", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(update(*writer,
      std::move(query_doc1.filter),
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(update(*writer,
      std::move(query_doc2.filter),
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(update(*writer,
      std::move(query_doc3.filter),
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }

  // no matching documnts
  {
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(update(*writer,
      std::move(query_doc2.filter),
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    )); // non-existent document
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // update + delete (same segment)
  {
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(update(*writer,
      *(query_doc2.filter),
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->documents().remove(*(query_doc2.filter)); // remove no longer existent
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // update + delete (different segments)
  {
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(update(*writer,
      *(query_doc2.filter),
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->commit();
    writer->documents().remove(*(query_doc2.filter)); // remove no longer existent
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of old segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of new segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }

  // delete + update (same segment)
  {
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->documents().remove(*(query_doc2.filter));
    ASSERT_TRUE(update(*writer,
      *(query_doc2.filter),
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    )); // update no longer existent
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // delete + update (different segments)
  {
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    writer->documents().remove(*(query_doc2.filter));
    writer->commit();
    ASSERT_TRUE(update(*writer,
      *(query_doc2.filter),
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    )); // update no longer existent
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // delete + update then update (2nd - update of modified doc) (same segment)
  {
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto query_doc3 = iresearch::iql::query_builder().build("name==C", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->documents().remove(*(query_doc2.filter));
    ASSERT_TRUE(update(*writer,
      *(query_doc2.filter),
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(update(*writer,
      *(query_doc3.filter),
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // delete + update then update (2nd - update of modified doc) (different segments)
  {
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto query_doc3 = iresearch::iql::query_builder().build("name==C", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    writer->documents().remove(*(query_doc2.filter));
    writer->commit();
    ASSERT_TRUE(update(*writer,
      *(query_doc2.filter),
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(update(*writer,
      *(query_doc3.filter),
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // new segment failed update (due to field features mismatch or failed serializer)
  {
    class test_field: public tests::field_base {
     public:
      iresearch::flags features_;
      iresearch::string_token_stream tokens_;
      bool write_result_;
      virtual bool write(iresearch::data_output& out) const override { 
        out.write_byte(1);
        return write_result_;
      }
      virtual iresearch::token_stream& get_tokens() const override { return const_cast<test_field*>(this)->tokens_; }
      virtual const iresearch::flags& features() const override { return features_; }
    };

    tests::json_doc_generator gen(resource("simple_sequential.json"), &tests::generic_json_field_factory);
    auto doc1 = gen.next();
    auto doc2 = gen.next();
    auto doc3 = gen.next();
    auto doc4 = gen.next();
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();
    auto test_field0 = std::make_shared<test_field>();
    auto test_field1 = std::make_shared<test_field>();
    auto test_field2 = std::make_shared<test_field>();
    auto test_field3 = std::make_shared<test_field>();
    std::string test_field_name("test_field");

    test_field0->features_.add<iresearch::offset>().add<iresearch::frequency>(); // feature superset
    test_field1->features_.add<iresearch::offset>(); // feature subset of 'test_field0'
    test_field2->features_.add<iresearch::offset>();
    test_field3->features_.add<iresearch::increment>();
    test_field0->name(test_field_name);
    test_field1->name(test_field_name);
    test_field2->name(test_field_name);
    test_field3->name(test_field_name);
    test_field0->tokens_.reset("data");
    test_field1->tokens_.reset("data");
    test_field2->tokens_.reset("data");
    test_field3->tokens_.reset("data");
    test_field0->write_result_ = true;
    test_field1->write_result_ = true;
    test_field2->write_result_ = false;
    test_field3->write_result_ = true;

    const_cast<tests::document*>(doc1)->insert(test_field0, true, true); // inject field
    const_cast<tests::document*>(doc2)->insert(test_field1, true, true); // inject field
    const_cast<tests::document*>(doc3)->insert(test_field2, true, true); // inject field
    const_cast<tests::document*>(doc4)->insert(test_field3, true, true); // inject field

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    )); // field features subset
    ASSERT_FALSE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    )); // serializer returs false
    ASSERT_FALSE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    )); // field features differ
    ASSERT_FALSE(update(*writer,
      *(query_doc1.filter.get()), 
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment update with single-doc functr
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->documents().replace(
      *query_doc1.filter,
      [&doc2](irs::segment_writer::document& doc)->bool {
        doc.insert(irs::action::index, doc2->indexed.begin(), doc2->indexed.end());
        doc.insert(irs::action::store, doc2->stored.begin(), doc2->stored.end());
        return false;
      }
    );
    writer->commit();

    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment update with multiple-doc functr
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();
    std::vector<tests::document const*> docs = { doc2, doc3 };
    size_t i = 0;

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->documents().replace(
      *query_doc1.filter,
      [&docs, &i](irs::segment_writer::document& doc)->bool {
        doc.insert(irs::action::index, docs[i]->indexed.begin(), docs[i]->indexed.end());
        doc.insert(irs::action::store, docs[i]->stored.begin(), docs[i]->stored.end());
        return ++i < docs.size();
      }
    );
    writer->commit();

    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // new segment update with multiple-doc functr + rollback due to exception
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();
    std::vector<tests::document const*> docs = { doc2, doc3 };
    size_t i = 0;

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_ANY_THROW(writer->documents().replace(
      *query_doc1.filter,
      [&docs, &i](irs::segment_writer::document& doc)->bool {
        doc.insert(irs::action::index, docs[i]->indexed.begin(), docs[i]->indexed.end());
        doc.insert(irs::action::store, docs[i]->stored.begin(), docs[i]->stored.end());
        if (++i >= docs.size()) throw "some error";
        return true;
      }
    ));
    writer->commit();

    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(memory_index_test, import_reader) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        irs::string_ref(name),
        data.str
      ));
    }
  });

  irs::bytes_ref actual_value;

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  // add a reader with 1 segment no docs
  {
    irs::memory_directory data_dir;
    auto data_writer = irs::index_writer::make(data_dir, codec(), irs::OM_CREATE);
    auto writer = open_writer();

    writer->commit(); // ensure the writer has an initial completed state

    // check meta counter
    {
      irs::index_meta meta;
      std::string filename;
      auto meta_reader = codec()->get_index_meta_reader();
      ASSERT_NE(nullptr, meta_reader);
      ASSERT_TRUE(meta_reader->last_segments_file(dir(), filename));
      meta_reader->read(dir(), meta, filename);
      ASSERT_EQ(0, meta.counter());
    }

    data_writer->commit();
    ASSERT_TRUE(writer->import(irs::directory_reader::open(data_dir, codec())));
    writer->commit();

    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_EQ(0, reader.size());
    ASSERT_EQ(0, reader.docs_count());

    // insert a document and check the meta counter again
    {
      ASSERT_TRUE(insert(*writer,
        doc1->indexed.begin(), doc1->indexed.end(),
        doc1->stored.begin(), doc1->stored.end()
      ));
      writer->commit();

      irs::index_meta meta;
      std::string filename;
      auto meta_reader = codec()->get_index_meta_reader();
      ASSERT_NE(nullptr, meta_reader);
      ASSERT_TRUE(meta_reader->last_segments_file(dir(), filename));
      meta_reader->read(dir(), meta, filename);
      ASSERT_EQ(1, meta.counter());
    }
  }

  // add a reader with 1 segment no live-docs
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    irs::memory_directory data_dir;
    auto data_writer = irs::index_writer::make(data_dir, codec(), irs::OM_CREATE);
    auto writer = open_writer();

    writer->commit(); // ensure the writer has an initial completed state

    // check meta counter
    {
      irs::index_meta meta;
      std::string filename;
      auto meta_reader = codec()->get_index_meta_reader();
      ASSERT_NE(nullptr, meta_reader);
      ASSERT_TRUE(meta_reader->last_segments_file(dir(), filename));
      meta_reader->read(dir(), meta, filename);
      ASSERT_EQ(1, meta.counter());
    }

    ASSERT_TRUE(insert(*data_writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    data_writer->commit();
    data_writer->documents().remove(std::move(query_doc1.filter));
    data_writer->commit();
    writer->commit(); // ensure the writer has an initial completed state
    ASSERT_TRUE(writer->import(irs::directory_reader::open(data_dir, codec())));
    writer->commit();

    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_EQ(0, reader.size());
    ASSERT_EQ(0, reader.docs_count());

    // insert a document and check the meta counter again
    {
      ASSERT_TRUE(insert(*writer,
        doc1->indexed.begin(), doc1->indexed.end(),
        doc1->stored.begin(), doc1->stored.end()
      ));
      writer->commit();

      irs::index_meta meta;
      std::string filename;
      auto meta_reader = codec()->get_index_meta_reader();
      ASSERT_NE(nullptr, meta_reader);
      ASSERT_TRUE(meta_reader->last_segments_file(dir(), filename));
      meta_reader->read(dir(), meta, filename);
      ASSERT_EQ(2, meta.counter());
    }
  }

  // add a reader with 1 full segment
  {
    iresearch::memory_directory data_dir;
    auto data_writer = iresearch::index_writer::make(data_dir, codec(), iresearch::OM_CREATE);
    auto writer = open_writer();

    ASSERT_TRUE(insert(*data_writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*data_writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    data_writer->commit();
    ASSERT_TRUE(writer->import(iresearch::directory_reader::open(data_dir, codec())));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(2, segment.docs_count());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // add a reader with 1 sparse segment
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    iresearch::memory_directory data_dir;
    auto data_writer = iresearch::index_writer::make(data_dir, codec(), iresearch::OM_CREATE);
    auto writer = open_writer();

    ASSERT_TRUE(insert(*data_writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*data_writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    data_writer->documents().remove(std::move(query_doc1.filter));
    data_writer->commit();
    ASSERT_TRUE(writer->import(iresearch::directory_reader::open(data_dir, codec())));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(1, segment.docs_count());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // add a reader with 2 full segments
  {
    iresearch::memory_directory data_dir;
    auto data_writer = iresearch::index_writer::make(data_dir, codec(), iresearch::OM_CREATE);
    auto writer = open_writer();

    ASSERT_TRUE(insert(*data_writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*data_writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    data_writer->commit();
    ASSERT_TRUE(insert(*data_writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*data_writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    data_writer->commit();
    ASSERT_TRUE(writer->import(iresearch::directory_reader::open(data_dir, codec())));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(4, segment.docs_count());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }

  // add a reader with 2 sparse segments
  {
    auto query_doc2_doc3 = iresearch::iql::query_builder().build("name==B||name==C", std::locale::classic());
    iresearch::memory_directory data_dir;
    auto data_writer = iresearch::index_writer::make(data_dir, codec(), iresearch::OM_CREATE);
    auto writer = open_writer();

    ASSERT_TRUE(insert(*data_writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*data_writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    data_writer->commit();
    ASSERT_TRUE(insert(*data_writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*data_writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    data_writer->documents().remove(std::move(query_doc2_doc3.filter));
    data_writer->commit();
    ASSERT_TRUE(writer->import(iresearch::directory_reader::open(data_dir, codec())));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(2, segment.docs_count());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }

  // add a reader with 2 mixed segments
  {
    auto query_doc4 = iresearch::iql::query_builder().build("name==D", std::locale::classic());
    iresearch::memory_directory data_dir;
    auto data_writer = iresearch::index_writer::make(data_dir, codec(), iresearch::OM_CREATE);
    auto writer = open_writer();

    ASSERT_TRUE(insert(*data_writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*data_writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    data_writer->commit();
    ASSERT_TRUE(insert(*data_writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*data_writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    data_writer->documents().remove(std::move(query_doc4.filter));
    data_writer->commit();
    ASSERT_TRUE(writer->import(iresearch::directory_reader::open(data_dir, codec())));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(3, segment.docs_count());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // new: add + add + delete, old: import
  {
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    iresearch::memory_directory data_dir;
    auto data_writer = iresearch::index_writer::make(data_dir, codec(), iresearch::OM_CREATE);
    auto writer = open_writer();

    ASSERT_TRUE(insert(*data_writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*data_writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    data_writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->documents().remove(std::move(query_doc2.filter)); // should not match any documents
    ASSERT_TRUE(writer->import(iresearch::directory_reader::open(data_dir, codec())));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of imported segment
      ASSERT_EQ(2, segment.docs_count());
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of original segment
      ASSERT_EQ(1, segment.docs_count());
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST_F(memory_index_test, refresh_reader) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        irs::string_ref(name),
        data.str
      ));
    }
  });

  irs::bytes_ref actual_value;

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  // initial state (1st segment 2 docs)
  {
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
  }

  // refreshable reader
  auto reader = iresearch::directory_reader::open(dir(), codec());

  // validate state
  {
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // modify state (delete doc2)
  {
    auto writer = open_writer(irs::OM_APPEND);
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());

    writer->documents().remove(std::move(query_doc2.filter));
    writer->commit();
  }

  // validate state pre/post refresh (existing segment changed)
  {
    {
      ASSERT_EQ(1, reader.size());
      auto& segment = reader[0]; // assume 0 is id of first/only segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }

    {
      reader = reader.reopen();
      ASSERT_EQ(1, reader.size());
      auto& segment = reader[0]; // assume 0 is id of first/only segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }
  }

  // modify state (2nd segment 2 docs)
  {
    auto writer = open_writer(irs::OM_APPEND);

    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
  }

  // validate state pre/post refresh (new segment added)
  {
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());

    reader = reader.reopen();
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of first segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of second segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }
  }

  // modify state (delete doc1)
  {
    auto writer = open_writer(irs::OM_APPEND);
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());

    writer->documents().remove(std::move(query_doc1.filter));
    writer->commit();
  }

  // validate state pre/post refresh (old segment removed)
  {
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of first segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of second segment
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }

    reader = reader.reopen();
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of second segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(memory_index_test, reuse_segment_writer) {
  tests::json_doc_generator gen0(resource("arango_demo.json"), &tests::generic_json_field_factory);
  tests::json_doc_generator gen1(resource("simple_sequential.json"), &tests::generic_json_field_factory);
  auto writer = open_writer();

  // populate initial 2 very small segments
  {
    {
      auto& index_ref = const_cast<tests::index_t&>(index());
      index_ref.emplace_back();
      gen0.reset();
      write_segment(*writer, index_ref.back(), gen0);
      writer->commit();
    }

    {
      auto& index_ref = const_cast<tests::index_t&>(index());
      index_ref.emplace_back();
      gen1.reset();
      write_segment(*writer, index_ref.back(), gen1);
      writer->commit();
    }
  }

  // populate initial small segment
  {
    auto& index_ref = const_cast<tests::index_t&>(index());
    index_ref.emplace_back();
    gen0.reset();
    write_segment(*writer, index_ref.back(), gen0);
    gen1.reset();
    write_segment(*writer, index_ref.back(), gen1);
    writer->commit();
  }

  // populate initial large segment
  {
    auto& index_ref = const_cast<tests::index_t&>(index());
    index_ref.emplace_back();

    for(size_t i = 100; i > 0; --i) {
      gen0.reset();
      write_segment(*writer, index_ref.back(), gen0);
      gen1.reset();
      write_segment(*writer, index_ref.back(), gen1);
    }

    writer->commit();
  }

  // populate and validate small segments in hopes of triggering segment_writer reuse
  // 10 iterations, although 2 should be enough since index_wirter::flush_context_pool_.size() == 2
  for(size_t i = 10; i > 0; --i) {
    auto& index_ref = const_cast<tests::index_t&>(index());
    index_ref.emplace_back();

    // add varying sized segments
    for (size_t j = 0; j < i; ++j) {
      // add test documents
      if (i%3 == 0 || i%3 == 1) {
        gen0.reset();
        write_segment(*writer, index_ref.back(), gen0);
      }

      // add different test docs (overlap to make every 3rd segment contain docs from both sources)
      if (i%3 == 1 || i%3 == 2) {
        gen1.reset();
        write_segment(*writer, index_ref.back(), gen1);
      }
    }

    writer->commit();
  }

  assert_index();

  // merge all segments
  {
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));
    writer->commit();
  }
}

TEST_F(memory_index_test, segment_column_user_system) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [](tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      // add 2 identical fields (without storing) to trigger non-default norm value
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));
      }
  });

  // document to add a system column not present in subsequent documents
  tests::document doc0;

  // add 2 identical fields (without storing) to trigger non-default norm value
  for (size_t i = 2; i; --i) {
    doc0.insert(std::make_shared<tests::templates::string_field>(
      irs::string_ref("test-field"),
      "test-value",
      irs::flags({ irs::norm::type() }) // trigger addition of a system column
    ), true, false);
  }

  irs::bytes_ref actual_value;
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  auto writer = open_writer();

  ASSERT_TRUE(insert(*writer,
    doc0.indexed.begin(), doc0.indexed.end(),
    doc0.stored.begin(), doc0.stored.end()
  ));
  ASSERT_TRUE(insert(*writer,
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end()
  ));
  ASSERT_TRUE(insert(*writer,
    doc2->indexed.begin(), doc2->indexed.end(),
    doc2->stored.begin(), doc2->stored.end()
  ));
  writer->commit();

  std::unordered_set<irs::string_ref> expectedName = { "A", "B" };

  // validate segment
  auto reader = irs::directory_reader::open(dir(), codec());
  ASSERT_EQ(1, reader.size());
  auto& segment = reader[0]; // assume 0 is id of first/only segment
  ASSERT_EQ(3, segment.docs_count()); // total count of documents

  auto* field = segment.field("test-field"); // 'norm' column added by doc0 above
  ASSERT_NE(nullptr, field);
  auto* column = segment.column_reader(field->meta().norm); // system column
  ASSERT_NE(nullptr, column);

  column = segment.column_reader("name");
  ASSERT_NE(nullptr, column);
  auto values = column->values();
  ASSERT_EQ(expectedName.size() + 1, segment.docs_count()); // total count of documents (+1 for doc0)
  auto terms = segment.field("same");
  ASSERT_NE(nullptr, terms);
  auto termItr = terms->iterator();
  ASSERT_TRUE(termItr->next());

  for (auto docsItr = termItr->postings(irs::flags()); docsItr->next();) {
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
  }

  ASSERT_TRUE(expectedName.empty());
}

TEST_F(memory_index_test, import_concurrent) {
  struct store {
    store(const irs::format::ptr& codec)
      : dir(irs::memory::make_unique<irs::memory_directory>()) {
      writer = irs::index_writer::make(*dir, codec, irs::OM_CREATE);
      writer->commit();
      reader = irs::directory_reader::open(*dir);
    }

    store(store&& rhs) NOEXCEPT
      : dir(std::move(rhs.dir)),
        writer(std::move(rhs.writer)),
        reader(rhs.reader) {
    }

    store(const store&) = delete;
    store& operator=(const store&) = delete;

    std::unique_ptr<irs::memory_directory> dir;
    irs::index_writer::ptr writer;
    irs::directory_reader reader;
  };

  std::vector<store> stores;
  stores.reserve(4);
  for (size_t i = 0; i < stores.capacity(); ++i) {
    stores.emplace_back(codec());
  }
  std::vector<std::thread> workers;

  std::set<std::string> names;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [&names] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));

        if (name == "name") {
          names.emplace(data.str.data, data.str.size);
        }
      }
  });

  const auto count = 10;
  for (auto& store : stores) {
    for (auto i = 0; i < count; ++i) {
      auto* doc = gen.next();

      if (!doc) {
        break;
      }

      ASSERT_TRUE(insert(*store.writer,
        doc->indexed.begin(), doc->indexed.end(),
        doc->stored.begin(), doc->stored.end()
      ));
    }
    store.writer->commit();
    store.reader = irs::directory_reader::open(*store.dir);
  }

  std::mutex mutex;
  std::condition_variable ready_cv;
  bool ready = false;

  auto wait_for_all = [&mutex, &ready, &ready_cv]() {
    // wait for all threads to be registered
    std::unique_lock<std::remove_reference<decltype(mutex)>::type> lock(mutex);
    while (!ready) {
      ready_cv.wait(lock);
    }
  };

  irs::memory_directory dir;
  irs::index_writer::ptr writer = irs::index_writer::make(dir, codec(), irs::OM_CREATE);
  irs::bytes_ref actual_value;

  for (auto& store : stores) {
    workers.emplace_back([&wait_for_all, &writer, &store]() {
      wait_for_all();
      writer->import(store.reader);
    });
  }

  // all threads are registered... go, go, go...
  {
    std::lock_guard<decltype(mutex)> lock(mutex);
    ready = true;
    ready_cv.notify_all();
  }

  // wait for workers to finish
  for (auto& worker: workers) {
    worker.join();
  }

  writer->commit(); // commit changes

  auto reader = iresearch::directory_reader::open(dir);
  ASSERT_EQ(workers.size(), reader.size());
  ASSERT_EQ(names.size(), reader.docs_count());
  ASSERT_EQ(names.size(), reader.live_docs_count());

  size_t removed = 0;
  for (auto& segment : reader) {
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    while (docsItr->next()) {
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ(1, names.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      ++removed;
    }
    ASSERT_FALSE(docsItr->next());
  }
  ASSERT_EQ(removed, reader.docs_count());
  ASSERT_TRUE(names.empty());
}

TEST_F(memory_index_test, concurrent_consolidation) {
  auto writer = open_writer(dir());
  ASSERT_NE(nullptr, writer);

  std::set<std::string> names;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [&names] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));

        if (name == "name") {
          names.emplace(data.str.data, data.str.size);
        }
      }
  });

  // insert multiple small segments
  size_t size = 0;
  while (const auto* doc = gen.next()) {
    ASSERT_TRUE(insert(*writer,
      doc->indexed.begin(), doc->indexed.end(),
      doc->stored.begin(), doc->stored.end()
    ));
    writer->commit();
    ++size;
  }
  ASSERT_EQ(size-1, irs::directory_cleaner::clean(dir()));

  auto consolidate_range = [](
      std::set<const irs::segment_meta*>& candidates,
      const irs::index_meta& meta,
      size_t begin,
      size_t end
  ) {
    if (begin > meta.size() || end > meta.size()) {
      return;
    }

    for (;begin < end; ++begin) {
      candidates.emplace(&meta[begin].meta);
    }
  };

  std::mutex mutex;
  bool ready = false;
  std::condition_variable ready_cv;

  auto wait_for_all = [&mutex, &ready, &ready_cv]() {
    // wait for all threads to be registered
    std::unique_lock<std::mutex> lock(mutex);
    while (!ready) {
      ready_cv.wait(lock);
    }
  };

  const auto thread_count = 10;
  std::vector<std::thread> pool;

  for (size_t i = 0; i < thread_count; ++i) {
    pool.emplace_back(std::thread([&wait_for_all, &consolidate_range, &writer, i] () mutable {
      wait_for_all();

      size_t num_segments = irs::integer_traits<size_t>::const_max;

      while (num_segments > 1) {
        auto policy = [&consolidate_range, &i, &num_segments] (
            std::set<const irs::segment_meta*>& candidates,
            const irs::index_meta& meta,
            const irs::index_writer::consolidating_segments_t&
        ) mutable {
          num_segments = meta.size();
          consolidate_range(candidates, meta, i, i+2);
        };

        if (writer->consolidate(policy)) {
          writer->commit();
        }

        i = (i + 1) % num_segments;
      }
    }));
  }

  // all threads registered... go, go, go...
  {
    std::lock_guard<decltype(mutex)> lock(mutex);
    ready = true;
    ready_cv.notify_all();
  }

  for (auto& thread : pool) {
    thread.join();
  }

  writer->commit();

  irs::bytes_ref actual_value;
  auto reader = iresearch::directory_reader::open(this->dir(), codec());
  ASSERT_EQ(1, reader.size());

  ASSERT_EQ(names.size(), reader.docs_count());
  ASSERT_EQ(names.size(), reader.live_docs_count());

  size_t removed = 0;
  auto& segment = reader[0];
  const auto* column = segment.column_reader("name");
  ASSERT_NE(nullptr, column);
  auto values = column->values();
  auto terms = segment.field("same");
  ASSERT_NE(nullptr, terms);
  auto termItr = terms->iterator();
  ASSERT_TRUE(termItr->next());
  auto docsItr = termItr->postings(iresearch::flags());
  while (docsItr->next()) {
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ(1, names.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    ++removed;
  }
  ASSERT_FALSE(docsItr->next());

  ASSERT_EQ(removed, reader.docs_count());
  ASSERT_TRUE(names.empty());
}

TEST_F(memory_index_test, concurrent_consolidation_dedicated_commit) {
  auto writer = open_writer(dir());
  ASSERT_NE(nullptr, writer);

  std::set<std::string> names;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [&names] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));

        if (name == "name") {
          names.emplace(data.str.data, data.str.size);
        }
      }
  });

  // insert multiple small segments
  size_t size = 0;
  while (const auto* doc = gen.next()) {
    ASSERT_TRUE(insert(*writer,
      doc->indexed.begin(), doc->indexed.end(),
      doc->stored.begin(), doc->stored.end()
    ));
    writer->commit();
    ++size;
  }
  ASSERT_EQ(size-1, irs::directory_cleaner::clean(dir()));

  auto consolidate_range = [](
      std::set<const irs::segment_meta*>& candidates,
      const irs::index_meta& meta,
      size_t begin,
      size_t end
  ) {
    if (begin > meta.size() || end > meta.size()) {
      return;
    }

    for (;begin < end; ++begin) {
      candidates.emplace(&meta[begin].meta);
    }
  };

  std::mutex mutex;
  bool ready = false;
  std::condition_variable ready_cv;

  auto wait_for_all = [&mutex, &ready, &ready_cv]() {
    // wait for all threads to be registered
    std::unique_lock<std::mutex> lock(mutex);
    while (!ready) {
      ready_cv.wait(lock);
    }
  };

  const auto thread_count = 10;
  std::vector<std::thread> pool;

  for (size_t i = 0; i < thread_count; ++i) {
    pool.emplace_back(std::thread([&wait_for_all, &consolidate_range, &writer, i] () mutable {
      wait_for_all();

      size_t num_segments = irs::integer_traits<size_t>::const_max;

      while (num_segments > 1) {
        auto policy = [&consolidate_range, &i, &num_segments] (
            std::set<const irs::segment_meta*>& candidates,
            const irs::index_meta& meta,
            const irs::index_writer::consolidating_segments_t&
        ) mutable {
          num_segments = meta.size();
          consolidate_range(candidates, meta, i, i+2);
        };

        writer->consolidate(policy);

        i = (i + 1) % num_segments;
      }
    }));
  }

  // add dedicated commit thread
  std::atomic<bool> shutdown(false);
  std::thread commit_thread([&wait_for_all, &writer, &shutdown]() {
    wait_for_all();

    while (!shutdown.load()) {
      writer->commit();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });

  // all threads registered... go, go, go...
  {
    std::lock_guard<decltype(mutex)> lock(mutex);
    ready = true;
    ready_cv.notify_all();
  }

  for (auto& thread : pool) {
    thread.join();
  }

  // wait for commit thread to finish
  shutdown = true;
  commit_thread.join();

  writer->commit();

  irs::bytes_ref actual_value;
  auto reader = iresearch::directory_reader::open(this->dir(), codec());
  ASSERT_EQ(1, reader.size());

  ASSERT_EQ(names.size(), reader.docs_count());
  ASSERT_EQ(names.size(), reader.live_docs_count());

  size_t removed = 0;
  auto& segment = reader[0];
  const auto* column = segment.column_reader("name");
  ASSERT_NE(nullptr, column);
  auto values = column->values();
  auto terms = segment.field("same");
  ASSERT_NE(nullptr, terms);
  auto termItr = terms->iterator();
  ASSERT_TRUE(termItr->next());
  auto docsItr = termItr->postings(iresearch::flags());
  while (docsItr->next()) {
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ(1, names.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    ++removed;
  }
  ASSERT_FALSE(docsItr->next());

  ASSERT_EQ(removed, reader.docs_count());
  ASSERT_TRUE(names.empty());
}

TEST_F(memory_index_test, concurrent_consolidation_two_phase_dedicated_commit) {
  auto writer = open_writer(dir());
  ASSERT_NE(nullptr, writer);

  std::set<std::string> names;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [&names] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));

        if (name == "name") {
          names.emplace(data.str.data, data.str.size);
        }
      }
  });

  // insert multiple small segments
  size_t size = 0;
  while (const auto* doc = gen.next()) {
    ASSERT_TRUE(insert(*writer,
      doc->indexed.begin(), doc->indexed.end(),
      doc->stored.begin(), doc->stored.end()
    ));
    writer->commit();
    ++size;
  }
  ASSERT_EQ(size-1, irs::directory_cleaner::clean(dir()));

  auto consolidate_range = [](
      std::set<const irs::segment_meta*>& candidates,
      const irs::index_meta& meta,
      size_t begin,
      size_t end
  ) {
    if (begin > meta.size() || end > meta.size()) {
      return;
    }

    for (;begin < end; ++begin) {
      candidates.emplace(&meta[begin].meta);
    }
  };

  std::mutex mutex;
  bool ready = false;
  std::condition_variable ready_cv;

  auto wait_for_all = [&mutex, &ready, &ready_cv]() {
    // wait for all threads to be registered
    std::unique_lock<std::mutex> lock(mutex);
    while (!ready) {
      ready_cv.wait(lock);
    }
  };

  const auto thread_count = 10;
  std::vector<std::thread> pool;

  for (size_t i = 0; i < thread_count; ++i) {
    pool.emplace_back(std::thread([&wait_for_all, &consolidate_range, &writer, i] () mutable {
      wait_for_all();

      size_t num_segments = irs::integer_traits<size_t>::const_max;

      while (num_segments > 1) {
        auto policy = [&consolidate_range, &i, &num_segments] (
            std::set<const irs::segment_meta*>& candidates,
            const irs::index_meta& meta,
            const irs::index_writer::consolidating_segments_t&
        ) mutable {
          num_segments = meta.size();
          consolidate_range(candidates, meta, i, i+2);
        };

        writer->consolidate(policy);

        i = (i + 1) % num_segments;
      }
    }));
  }

  // add dedicated commit thread
  std::atomic<bool> shutdown(false);
  std::thread commit_thread([&wait_for_all, &writer, &shutdown]() {
    wait_for_all();

    while (!shutdown.load()) {
      writer->begin();
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      writer->commit();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

  });

  // all threads registered... go, go, go...
  {
    std::lock_guard<decltype(mutex)> lock(mutex);
    ready = true;
    ready_cv.notify_all();
  }

  for (auto& thread : pool) {
    thread.join();
  }

  // wait for commit thread to finish
  shutdown = true;
  commit_thread.join();

  writer->commit();

  irs::bytes_ref actual_value;
  auto reader = iresearch::directory_reader::open(this->dir(), codec());
  ASSERT_EQ(1, reader.size());

  ASSERT_EQ(names.size(), reader.docs_count());
  ASSERT_EQ(names.size(), reader.live_docs_count());

  size_t removed = 0;
  auto& segment = reader[0];
  const auto* column = segment.column_reader("name");
  ASSERT_NE(nullptr, column);
  auto values = column->values();
  auto terms = segment.field("same");
  ASSERT_NE(nullptr, terms);
  auto termItr = terms->iterator();
  ASSERT_TRUE(termItr->next());
  auto docsItr = termItr->postings(iresearch::flags());
  while (docsItr->next()) {
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ(1, names.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    ++removed;
  }
  ASSERT_FALSE(docsItr->next());

  ASSERT_EQ(removed, reader.docs_count());
  ASSERT_TRUE(names.empty());
}

TEST_F(memory_index_test, concurrent_consolidation_cleanup) {
  auto writer = open_writer(dir());
  ASSERT_NE(nullptr, writer);

  std::set<std::string> names;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [&names] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));

        if (name == "name") {
          names.emplace(data.str.data, data.str.size);
        }
      }
  });

  // insert multiple small segments
  size_t size = 0;
  while (const auto* doc = gen.next()) {
    ASSERT_TRUE(insert(*writer,
      doc->indexed.begin(), doc->indexed.end(),
      doc->stored.begin(), doc->stored.end()
    ));
    writer->commit();
    ++size;
  }
  ASSERT_EQ(size-1, irs::directory_cleaner::clean(dir()));

  auto consolidate_range = [](
      std::set<const irs::segment_meta*>& candidates,
      const irs::index_meta& meta,
      size_t begin,
      size_t end
  ) {
    if (begin > meta.size() || end > meta.size()) {
      return;
    }

    for (;begin < end; ++begin) {
      candidates.emplace(&meta[begin].meta);
    }
  };

  std::mutex mutex;
  bool ready = false;
  std::condition_variable ready_cv;

  auto wait_for_all = [&mutex, &ready, &ready_cv]() {
    // wait for all threads to be registered
    std::unique_lock<std::mutex> lock(mutex);
    while (!ready) {
      ready_cv.wait(lock);
    }
  };

  const auto thread_count = 10;
  std::vector<std::thread> pool;

  auto& dir = this->dir();
  for (size_t i = 0; i < thread_count; ++i) {
    pool.emplace_back(std::thread([&wait_for_all, &consolidate_range, &writer, &dir, i] () mutable {
      wait_for_all();

      size_t num_segments = irs::integer_traits<size_t>::const_max;

      while (num_segments > 1) {
        auto policy = [&consolidate_range, &i, &num_segments, &dir] (
            std::set<const irs::segment_meta*>& candidates,
            const irs::index_meta& meta,
            const irs::index_writer::consolidating_segments_t&
        ) mutable {
          num_segments = meta.size();
          consolidate_range(candidates, meta, i, i+2);
        };

        if (writer->consolidate(policy)) {
          writer->commit();
          irs::directory_cleaner::clean(const_cast<irs::directory&>(dir));
        }

        i = (i + 1) % num_segments;
      }
    }));
  }

  // all threads registered... go, go, go...
  {
    std::lock_guard<decltype(mutex)> lock(mutex);
    ready = true;
    ready_cv.notify_all();
  }

  for (auto& thread : pool) {
    thread.join();
  }

  writer->commit();
  irs::directory_cleaner::clean(const_cast<irs::directory&>(dir));

  irs::bytes_ref actual_value;
  auto reader = iresearch::directory_reader::open(this->dir(), codec());
  ASSERT_EQ(1, reader.size());

  ASSERT_EQ(names.size(), reader.docs_count());
  ASSERT_EQ(names.size(), reader.live_docs_count());

  size_t removed = 0;
  auto& segment = reader[0];
  const auto* column = segment.column_reader("name");
  ASSERT_NE(nullptr, column);
  auto values = column->values();
  auto terms = segment.field("same");
  ASSERT_NE(nullptr, terms);
  auto termItr = terms->iterator();
  ASSERT_TRUE(termItr->next());
  auto docsItr = termItr->postings(iresearch::flags());
  while (docsItr->next()) {
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ(1, names.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    ++removed;
  }
  ASSERT_FALSE(docsItr->next());

  ASSERT_EQ(removed, reader.docs_count());
  ASSERT_TRUE(names.empty());
}

TEST_F(memory_index_test, consolidate_invalid_candidate) {
  auto writer = open_writer(dir());
  ASSERT_NE(nullptr, writer);

  auto check_consolidating_segments = [](
      std::set<const irs::segment_meta*>& candidates,
      const irs::index_meta& meta,
      const irs::index_writer::consolidating_segments_t& consolidating_segments
  ) {
    ASSERT_TRUE(consolidating_segments.empty());
  };

  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));
      }
  });

  tests::document const* doc1 = gen.next();

  // segment 1
  ASSERT_TRUE(insert(*writer,
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end()
  ));
  writer->commit();
  ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

  // null candidate
  {
    auto invalid_candidate_policy = [](
        std::set<const irs::segment_meta*>& candidates,
        const irs::index_meta& /*meta*/,
        const irs::index_writer::consolidating_segments_t&
    ) {
      candidates.insert(nullptr);
    };

    ASSERT_FALSE(writer->consolidate(invalid_candidate_policy)); // invalid candidate
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments)); // check registered consolidation
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
  }

  // invalid candidate
  {
    const irs::segment_meta meta("foo", nullptr, 6, 5, false, irs::segment_meta::file_set());

    auto invalid_candidate_policy = [&meta](
        std::set<const irs::segment_meta*>& candidates,
        const irs::index_meta& /*meta*/,
        const irs::index_writer::consolidating_segments_t&
    ) {
      candidates.insert(&meta);
    };

    ASSERT_FALSE(writer->consolidate(invalid_candidate_policy)); // invalid candidate
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments)); // check registered consolidation
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
  }
}

TEST_F(memory_index_test, consolidate_single_segment) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));
      }
  });

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();

  auto all_features = irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::payload::type(), irs::offset::type() };
  irs::bytes_ref actual_value;

  std::vector<size_t> expected_consolidating_segments;
  auto check_consolidating_segments = [&expected_consolidating_segments](
      std::set<const irs::segment_meta*>& candidates,
      const irs::index_meta& meta,
      const irs::index_writer::consolidating_segments_t& consolidating_segments
  ) {
    ASSERT_EQ(expected_consolidating_segments.size(), consolidating_segments.size());
    for (auto i : expected_consolidating_segments) {
      auto& expected_consolidating_segment = meta[i];
      ASSERT_TRUE(consolidating_segments.end() != consolidating_segments.find(&expected_consolidating_segment.meta));
    }
  };

  // single segment without deletes
  {
    auto writer = open_writer(dir());
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // nothing to consolidate
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments)); // check segments registered for consolidation
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
  }

  size_t count = 0;
  auto get_number_of_files_in_segments = [&count](const std::string& name) NOEXCEPT {
    count += size_t(name.size() && '_' == name[0]);
    return true;
  };

  // single segment with deletes
  {
    auto writer = open_writer(dir());
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    writer->documents().remove(*query_doc1.filter);
    writer->commit();
    ASSERT_EQ(3, irs::directory_cleaner::clean(dir())); // segments_1 + stale segment meta + unused column store
    ASSERT_EQ(1, iresearch::directory_reader::open(this->dir(), codec()).size());

    // get number of files in 1st segment
    count = 0;
    dir().visit(get_number_of_files_in_segments);

    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // nothing to consolidate
    expected_consolidating_segments = { 0 }; // expect first segment to be marked for consolidation
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments)); // check segments registered for consolidation
    writer->commit();
    ASSERT_EQ(1+count, irs::directory_cleaner::clean(dir())); // +1 for segments_2

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    tests::assert_index(this->dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(this->dir(), codec());
    ASSERT_EQ(1, reader.size());

    // assume 0 is 'merged' segment
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(1, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST_F(memory_index_test, segment_consolidate_long_running) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));
      }
  });

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  irs::bytes_ref actual_value;
  auto all_features = irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::payload::type(), irs::offset::type() };

  size_t count = 0;
  auto get_number_of_files_in_segments = [&count](const std::string& name) NOEXCEPT {
    count += size_t(name.size() && '_' == name[0]);
    return true;
  };

  // long running transaction
  {
    tests::blocking_directory dir(this->dir(), "_3.cs");
    auto writer = open_writer(dir);
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir)); // segments_1

    // retrieve total number of segment files
    dir.visit(get_number_of_files_in_segments);

    dir.intermediate_commits_lock.lock(); // acquire directory lock, and block consolidation

    std::thread consolidation_thread([&writer, &dir]() {
      ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

      const std::vector<size_t> expected_consolidating_segments{ 0, 1 };
      auto check_consolidating_segments = [&expected_consolidating_segments](
          std::set<const irs::segment_meta*>& candidates,
          const irs::index_meta& meta,
          const irs::index_writer::consolidating_segments_t& consolidating_segments
      ) {
        ASSERT_EQ(expected_consolidating_segments.size(), consolidating_segments.size());
        for (auto i : expected_consolidating_segments) {
          auto& expected_consolidating_segment = meta[i];
          ASSERT_TRUE(consolidating_segments.end() != consolidating_segments.find(&expected_consolidating_segment.meta));
        }
      };
      ASSERT_TRUE(writer->consolidate(check_consolidating_segments)); // check segments registered for consolidation
    });

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

    bool has = false;
    dir.exists(has, dir.blocker);

    while (!has) {
      dir.exists(has, dir.blocker);
      ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

      SCOPED_LOCK_NAMED(dir.policy_lock, policy_guard);
      dir.policy_applied.wait_for(policy_guard, std::chrono::milliseconds(1000));
    }

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

    // add several segments in background
    // segment 3
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->commit(); // commit transaction
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir)); // segments_2

    // add several segments in background
    // segment 4
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit(); // commit transaction
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir)); // segments_3

    dir.intermediate_commits_lock.unlock(); // finish consolidation
    consolidation_thread.join(); // wait for the consolidation to complete
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir)); // segments_3
    writer->commit(); // commit consolidation
    ASSERT_EQ(1+count, irs::directory_cleaner::clean(dir)); // +1 for segments_4

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    expected.emplace_back();
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    tests::assert_index(this->dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(this->dir(), codec());
    ASSERT_EQ(3, reader.size());

    // assume 0 is 'segment 3'
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(1, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    // assume 1 is 'segment 4'
    {
      auto& segment = reader[1];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(1, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    // assume 2 is merged segment
    {
      auto& segment = reader[2];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }
  }

  // long running transaction + segment removal
  {
    SetUp(); // recreate directory
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());

    tests::blocking_directory dir(this->dir(), "_3.cs");
    auto writer = open_writer(dir);
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

    // retrieve total number of segment files
    count = 0;
    dir.visit(get_number_of_files_in_segments);

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir)); // segments_1

    dir.intermediate_commits_lock.lock(); // acquire directory lock, and block consolidation

    std::thread consolidation_thread([&writer, &dir]() {
      // consolidation will fail because of
      ASSERT_FALSE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

      auto check_consolidating_segments = [](
          std::set<const irs::segment_meta*>& candidates,
          const irs::index_meta& meta,
          const irs::index_writer::consolidating_segments_t& consolidating_segments
      ) {
        ASSERT_TRUE(consolidating_segments.empty());
      };
      ASSERT_TRUE(writer->consolidate(check_consolidating_segments)); // check segments registered for consolidation
    });

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

    bool has = false;
    dir.exists(has, dir.blocker);

    while (!has) {
      dir.exists(has, dir.blocker);
      ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

      SCOPED_LOCK_NAMED(dir.policy_lock, policy_guard);
      dir.policy_applied.wait_for(policy_guard, std::chrono::milliseconds(1000));
    }

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

    // add several segments in background
    // segment 3
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->documents().remove(*query_doc1.filter);
    writer->commit(); // commit transaction
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir)); // segments_2

    // add several segments in background
    // segment 4
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit(); // commit transaction
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir)); // segments_3

    dir.intermediate_commits_lock.unlock(); // finish consolidation
    consolidation_thread.join(); // wait for the consolidation to complete
    ASSERT_EQ(2*count-1+1, irs::directory_cleaner::clean(dir)); // files from segment 1 and 3 (without segment meta) + segments_3
    writer->commit(); // commit consolidation
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir)); // consolidation failed

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.emplace_back();
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    expected.emplace_back();
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    tests::assert_index(this->dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(this->dir(), codec());
    ASSERT_EQ(3, reader.size());

    // assume 0 is 'segment 2'
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(1, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    // assume 1 is 'segment 3'
    {
      auto& segment = reader[1];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(1, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_FALSE(docsItr->next());
    }

    // assume 1 is 'segment 4'
    {
      auto& segment = reader[2];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(1, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }
  }

  // long running transaction + document removal
  {
    SetUp(); // recreate directory
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());

    tests::blocking_directory dir(this->dir(), "_3.cs");
    auto writer = open_writer(dir);
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir)); // segments_1

    // retrieve total number of segment files
    count = 0;
    dir.visit(get_number_of_files_in_segments);

    dir.intermediate_commits_lock.lock(); // acquire directory lock, and block consolidation

    std::thread consolidation_thread([&writer, &dir]() {
      // consolidation will fail because of
      ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

      const std::vector<size_t> expected_consolidating_segments{ 0, 1 };
      auto check_consolidating_segments = [&expected_consolidating_segments](
          std::set<const irs::segment_meta*>& candidates,
          const irs::index_meta& meta,
          const irs::index_writer::consolidating_segments_t& consolidating_segments
      ) {
        ASSERT_EQ(expected_consolidating_segments.size(), consolidating_segments.size());
        for (auto i : expected_consolidating_segments) {
          auto& expected_consolidating_segment = meta[i];
          ASSERT_TRUE(consolidating_segments.end() != consolidating_segments.find(&expected_consolidating_segment.meta));
        }
      };
      ASSERT_TRUE(writer->consolidate(check_consolidating_segments)); // check segments registered for consolidation
    });

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

    bool has = false;
    dir.exists(has, dir.blocker);

    while (!has) {
      dir.exists(has, dir.blocker);
      ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

      SCOPED_LOCK_NAMED(dir.policy_lock, policy_guard);
      dir.policy_applied.wait_for(policy_guard, std::chrono::milliseconds(1000));
    }

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

    // remove doc1 in background
    writer->documents().remove(*query_doc1.filter);
    writer->commit(); // commit transaction
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir)); // unused column store

    dir.intermediate_commits_lock.unlock(); // finish consolidation
    consolidation_thread.join(); // wait for the consolidation to complete
    ASSERT_EQ(2, irs::directory_cleaner::clean(dir)); // segment_2 + stale segment 1 meta
    writer->commit(); // commit consolidation
    ASSERT_EQ(count+2, irs::directory_cleaner::clean(dir)); // files from segments 1 (+1 for doc mask) and 2, segment_3

    // validate structure (does not take removals into account)
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    tests::assert_index(this->dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(this->dir(), codec());
    ASSERT_EQ(1, reader.size());

    // assume 0 is 'merged segment'
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(3, segment.docs_count()); // total count of documents
      ASSERT_EQ(2, segment.live_docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      // including deleted docs
      {
        auto docsItr = termItr->postings(iresearch::flags());
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_FALSE(docsItr->next());
      }

      // only live docs
      {
        auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_FALSE(docsItr->next());
      }
    }
  }

  // long running transaction + document removal
  {
    SetUp(); // recreate directory
    auto query_doc1_doc4 = iresearch::iql::query_builder().build("name==A||name==D", std::locale::classic());

    tests::blocking_directory dir(this->dir(), "_3.cs");
    auto writer = open_writer(dir);
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir)); // segments_1

    // retrieve total number of segment files
    count = 0;
    dir.visit(get_number_of_files_in_segments);

    dir.intermediate_commits_lock.lock(); // acquire directory lock, and block consolidation

    std::thread consolidation_thread([&writer, &dir]() {
      // consolidation will fail because of
      ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

      const std::vector<size_t> expected_consolidating_segments{ 0, 1 };
      auto check_consolidating_segments = [&expected_consolidating_segments](
          std::set<const irs::segment_meta*>& candidates,
          const irs::index_meta& meta,
          const irs::index_writer::consolidating_segments_t& consolidating_segments
      ) {
        ASSERT_EQ(expected_consolidating_segments.size(), consolidating_segments.size());
        for (auto i : expected_consolidating_segments) {
          auto& expected_consolidating_segment = meta[i];
          ASSERT_TRUE(consolidating_segments.end() != consolidating_segments.find(&expected_consolidating_segment.meta));
        }
      };
      ASSERT_TRUE(writer->consolidate(check_consolidating_segments)); // check segments registered for consolidation
    });

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

    bool has = false;
    dir.exists(has, dir.blocker);

    while (!has) {
      dir.exists(has, dir.blocker);
      ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

      SCOPED_LOCK_NAMED(dir.policy_lock, policy_guard);
      dir.policy_applied.wait_for(policy_guard, std::chrono::milliseconds(1000));
    }

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));

    // remove doc1 in background
    writer->documents().remove(*query_doc1_doc4.filter);
    writer->commit(); // commit transaction
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir)); //  unused column store

    dir.intermediate_commits_lock.unlock(); // finish consolidation
    consolidation_thread.join(); // wait for the consolidation to complete
    ASSERT_EQ(3, irs::directory_cleaner::clean(dir)); // segment_2 + stale segment 1 meta + stale segment 2 meta
    writer->commit(); // commit consolidation
    ASSERT_EQ(count+3, irs::directory_cleaner::clean(dir)); // files from segments 1 (+1 for doc mask) and 2 (+1 for doc mask), segment_3

    // validate structure (does not take removals into account)
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    tests::assert_index(this->dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(this->dir(), codec());
    ASSERT_EQ(1, reader.size());

    // assume 0 is 'merged segment'
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(4, segment.docs_count()); // total count of documents
      ASSERT_EQ(2, segment.live_docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      // including deleted docs
      {
        auto docsItr = termItr->postings(iresearch::flags());
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
        ASSERT_FALSE(docsItr->next());
      }

      // only live docs
      {
        auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_FALSE(docsItr->next());
      }
    }
  }
}

TEST_F(memory_index_test, segment_consolidate_clear_commit) {
  std::vector<size_t> expected_consolidating_segments;
  auto check_consolidating_segments = [&expected_consolidating_segments](
      std::set<const irs::segment_meta*>& candidates,
      const irs::index_meta& meta,
      const irs::index_writer::consolidating_segments_t& consolidating_segments
  ) {
    ASSERT_EQ(expected_consolidating_segments.size(), consolidating_segments.size());
    for (auto i : expected_consolidating_segments) {
      auto& expected_consolidating_segment = meta[i];
      ASSERT_TRUE(consolidating_segments.end() != consolidating_segments.find(&expected_consolidating_segment.meta));
    }
  };

  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));
      }
  });

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();
  tests::document const* doc5 = gen.next();
  tests::document const* doc6 = gen.next();

  irs::bytes_ref actual_value;
  auto all_features = irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::payload::type(), irs::offset::type() };

  size_t count = 0;

// FIXME
//  // 2-phase: clear + consolidate
//  {
//    auto writer = open_writer();
//    ASSERT_NE(nullptr, writer);
//
//    // segment 1
//    ASSERT_TRUE(insert(*writer,
//      doc1->indexed.begin(), doc1->indexed.end(),
//      doc1->stored.begin(), doc1->stored.end()
//    ));
//    writer->commit();
//
//    // segment 2
//    ASSERT_TRUE(insert(*writer,
//      doc2->indexed.begin(), doc2->indexed.end(),
//      doc2->stored.begin(), doc2->stored.end()
//    ));
//    writer->commit();
//
//    // segment 3
//    ASSERT_TRUE(insert(*writer,
//      doc3->indexed.begin(), doc3->indexed.end(),
//      doc3->stored.begin(), doc3->stored.end()
//    ));
//
//    writer->begin();
//    writer->clear();
//    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate
//    writer->commit(); // commit transaction
//
//    auto reader = iresearch::directory_reader::open(dir(), codec());
//    ASSERT_EQ(0, reader.size());
//  }
//
//  // 2-phase: consolidate + clear
//  {
//    auto writer = open_writer();
//    ASSERT_NE(nullptr, writer);
//
//    // segment 1
//    ASSERT_TRUE(insert(*writer,
//      doc1->indexed.begin(), doc1->indexed.end(),
//      doc1->stored.begin(), doc1->stored.end()
//    ));
//    writer->commit();
//
//    // segment 2
//    ASSERT_TRUE(insert(*writer,
//      doc2->indexed.begin(), doc2->indexed.end(),
//      doc2->stored.begin(), doc2->stored.end()
//    ));
//    writer->commit();
//
//    // segment 3
//    ASSERT_TRUE(insert(*writer,
//      doc3->indexed.begin(), doc3->indexed.end(),
//      doc3->stored.begin(), doc3->stored.end()
//    ));
//
//    writer->begin();
//    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate
//    writer->clear();
//    writer->commit(); // commit transaction
//
//    auto reader = iresearch::directory_reader::open(dir(), codec());
//    ASSERT_EQ(0, reader.size());
//  }

  // consolidate + clear
  {
    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();

    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    writer->clear();

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    writer->commit(); // commit transaction

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(0, reader.size());
  }

  // clear + consolidate
  {
    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();

    writer->clear();
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    writer->commit(); // commit transaction

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(0, reader.size());
  }
}

TEST_F(memory_index_test, segment_consolidate_commit) {
  std::vector<size_t> expected_consolidating_segments;
  auto check_consolidating_segments = [&expected_consolidating_segments](
      std::set<const irs::segment_meta*>& candidates,
      const irs::index_meta& meta,
      const irs::index_writer::consolidating_segments_t& consolidating_segments
  ) {
    ASSERT_EQ(expected_consolidating_segments.size(), consolidating_segments.size());
    for (auto i : expected_consolidating_segments) {
      auto& expected_consolidating_segment = meta[i];
      ASSERT_TRUE(consolidating_segments.end() != consolidating_segments.find(&expected_consolidating_segment.meta));
    }
  };

  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));
      }
  });

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();
  tests::document const* doc5 = gen.next();
  tests::document const* doc6 = gen.next();

  irs::bytes_ref actual_value;
  auto all_features = irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::payload::type(), irs::offset::type() };

  size_t count = 0;
  auto get_number_of_files_in_segments = [&count](const std::string& name) NOEXCEPT {
    count += size_t(name.size() && '_' == name[0]);
    return true;
  };

  // consolidate without deletes
  {
    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();

    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_1

    // count number of files in segments
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));

    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // all segments are already marked for consolidation
    ASSERT_FALSE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    writer->commit(); // commit transaction (will commit nothing)
    ASSERT_EQ(1+count, irs::directory_cleaner::clean(dir())); // +1 for corresponding segments_* file
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // nothing to consolidate
    writer->commit(); // commit transaction (will commit nothing)

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());

    // assume 0 is merged segment
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }
  }

  // consolidate without deletes
  {
    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_1

    // count number of files in segments
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));

    // segment 3
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // can't consolidate segments that are already marked for consolidation
    ASSERT_FALSE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
    writer->commit(); // commit transaction (will commit segment 3 + consolidation)
    ASSERT_EQ(1+count, irs::directory_cleaner::clean(dir())); // +1 for segments_*

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.emplace_back();
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    // assume 0 is merged segment
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }

    // assume 1 is the newly created segment (doc3+doc4)
    {
      auto& segment = reader[1];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }
  }

  // consolidate without deletes
  {
    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_1

    // count number of files in segments
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));

    // segment 3
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir())); // segments_1
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // can't consolidate segments that are already marked for consolidation
    ASSERT_FALSE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir())); // segments_1

    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir())); // segments_1
    writer->commit(); // commit transaction (will commit segment 3 + consolidation)
    ASSERT_EQ(count+1, irs::directory_cleaner::clean(dir())); // +1 for segments_*

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.emplace_back();
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    expected.back().add(doc5->indexed.begin(), doc5->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());

    // assume 0 is merged segment
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }

    // assume 1 is the newly crated segment
    {
      auto& segment = reader[1];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(3, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("E", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST_F(memory_index_test, consolidate_check_consolidating_segments) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));
      }
  });

  auto writer = open_writer();
  ASSERT_NE(nullptr, writer);

  // ensure consolidating segments is empty
  {
    auto check_consolidating_segments = [](
        std::set<const irs::segment_meta*>& candidates,
        const irs::index_meta& meta,
        const irs::index_writer::consolidating_segments_t& consolidating_segments
    ) {
      ASSERT_TRUE(consolidating_segments.empty());
    };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));
  }

  const size_t SEGMENTS_COUNT = 10;
  for (size_t i = 0; i < SEGMENTS_COUNT; ++i) {
    const auto* doc = gen.next();
    ASSERT_TRUE(insert(*writer,
      doc->indexed.begin(), doc->indexed.end(),
      doc->stored.begin(), doc->stored.end()
    ));
    writer->commit();
  }

  // register 'SEGMENTS_COUNT/2' consolidations
  for (size_t i = 0, j = 0; i < SEGMENTS_COUNT/2; ++i) {
    auto merge_adjacent = [&j](
        std::set<const irs::segment_meta*>& candidates,
        const irs::index_meta& meta,
        const irs::index_writer::consolidating_segments_t& consolidating_segments
    ) {
      ASSERT_TRUE(j < meta.size());
      candidates.emplace(&meta[j++].meta);
      ASSERT_TRUE(j < meta.size());
      candidates.emplace(&meta[j++].meta);
    };

    ASSERT_TRUE(writer->consolidate(merge_adjacent));
  };

  // check all segments registered
  {
    auto check_consolidating_segments = [](
        std::set<const irs::segment_meta*>& candidates,
        const irs::index_meta& meta,
        const irs::index_writer::consolidating_segments_t& consolidating_segments
    ) {
      ASSERT_EQ(meta.size(), consolidating_segments.size());
      for (auto& segment : meta) {
        ASSERT_TRUE(consolidating_segments.end() != consolidating_segments.find(&segment.meta));
      }
    };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));
  }

  writer->commit(); // commit pending consolidations

  // ensure consolidating segments is empty
  {
    auto check_consolidating_segments = [](
        std::set<const irs::segment_meta*>& candidates,
        const irs::index_meta& meta,
        const irs::index_writer::consolidating_segments_t& consolidating_segments
    ) {
      ASSERT_TRUE(consolidating_segments.empty());
    };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));
  }

  // validate structure
  irs::bytes_ref actual_value;
  const auto all_features = irs::flags{
    irs::document::type(),
    irs::frequency::type(),
    irs::position::type(),
    irs::payload::type(),
    irs::offset::type()
  };
  gen.reset();
  tests::index_t expected;
  for (auto i = 0; i < SEGMENTS_COUNT/2; ++i) {
    expected.emplace_back();
    const auto* doc = gen.next();
    expected.back().add(doc->indexed.begin(), doc->indexed.end());
    doc = gen.next();
    expected.back().add(doc->indexed.begin(), doc->indexed.end());
  }
  tests::assert_index(dir(), codec(), expected, all_features);

  auto reader = iresearch::directory_reader::open(dir(), codec());
  ASSERT_EQ(SEGMENTS_COUNT/2, reader.size());

  std::string expected_name = "A";

  for (auto i = 0; i < SEGMENTS_COUNT/2; ++i) {
    auto& segment = reader[i];
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    ASSERT_EQ(2, segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ(expected_name, irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ++expected_name[0];
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ(expected_name, irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
    ++expected_name[0];
  }
}

TEST_F(memory_index_test, segment_consolidate_pending_commit) {
  std::vector<size_t> expected_consolidating_segments;
  auto check_consolidating_segments = [&expected_consolidating_segments](
      std::set<const irs::segment_meta*>& candidates,
      const irs::index_meta& meta,
      const irs::index_writer::consolidating_segments_t& consolidating_segments
  ) {
    ASSERT_EQ(expected_consolidating_segments.size(), consolidating_segments.size());
    for (auto i : expected_consolidating_segments) {
      auto& expected_consolidating_segment = meta[i];
      ASSERT_TRUE(consolidating_segments.end() != consolidating_segments.find(&expected_consolidating_segment.meta));
    }
  };

  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));
      }
  });

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();
  tests::document const* doc5 = gen.next();
  tests::document const* doc6 = gen.next();

  irs::bytes_ref actual_value;
  auto all_features = irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::payload::type(), irs::offset::type() };

  size_t count = 0;
  auto get_number_of_files_in_segments = [&count](const std::string& name) NOEXCEPT {
    count += size_t(name.size() && '_' == name[0]);
    return true;
  };

  // consolidate without deletes
  {
    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();

    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_1

    // count number of files in segments
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));

    ASSERT_FALSE(writer->begin()); // begin transaction (will not start transaction)
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
    // all segments are already marked for consolidation
    ASSERT_FALSE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    writer->commit(); // commit transaction (will commit consolidation)
    ASSERT_EQ(1+count, irs::directory_cleaner::clean(dir())); // +1 for corresponding segments_* file

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // nothing to consolidate

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    writer->commit(); // commit transaction (will commit nothing)

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());

    // assume 0 is merged segment
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }
  }

  // consolidate without deletes
  {
    SetUp();
    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_1

    // count number of files in segments
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));

    // segment 3
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));

    ASSERT_TRUE(writer->begin()); // begin transaction
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { 0, 1};
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // can't consolidate segments that are already marked for consolidation
    ASSERT_FALSE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));

    // check consolidating segments
    expected_consolidating_segments = { 0, 1};
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
    writer->commit(); // commit transaction (will commit segment 3)
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_2

    // check consolidating segments
    expected_consolidating_segments = { 0, 1};
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    writer->commit(); // commit pending merge
    ASSERT_EQ(1+count, irs::directory_cleaner::clean(dir())); // segments_2

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    // assume 0 is the existing segment
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }

    // assume 1 is merged segment
    {
      auto& segment = reader[1];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }
  }

  // consolidate without deletes
  {
    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_1

    // count number of files in segments
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));

    // segment 3
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
    ASSERT_TRUE(writer->begin()); // begin transaction
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { 0, 1};
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // can't consolidate segments that are already marked for consolidation
    ASSERT_FALSE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));

    // check consolidating segments
    expected_consolidating_segments = { 0, 1};
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
    writer->commit(); // commit transaction (will commit segment 3)
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir()));

    // check consolidating segments
    expected_consolidating_segments = { 0, 1};
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_TRUE(insert(*writer,
      doc6->indexed.begin(), doc6->indexed.end(),
      doc6->stored.begin(), doc6->stored.end()
    ));

    writer->commit(); // commit pending merge, segment 4 (doc5 + doc6)
    ASSERT_EQ(count+1, irs::directory_cleaner::clean(dir())); // +1 for segments

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.emplace_back();
    expected.back().add(doc5->indexed.begin(), doc5->indexed.end());
    expected.back().add(doc6->indexed.begin(), doc6->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(3, reader.size());

    // assume 0 is the existing segment
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }

    // assume 1 is merged segment
    {
      auto& segment = reader[1];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }

    // assume 2 is the last added segment
    {
      auto& segment = reader[2];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("E", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("F", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }
  }

  // consolidate with deletes
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());

    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_1

    // count number of files in segments
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
    writer->documents().remove(*query_doc1.filter);
    ASSERT_TRUE(writer->begin()); // begin transaction
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { 0, 1};
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // can't consolidate segments that are already marked for consolidation
    ASSERT_FALSE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));

    // check consolidating segments
    expected_consolidating_segments = { 0, 1};
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    writer->commit(); // commit transaction (will commit removal)
    ASSERT_EQ(3, irs::directory_cleaner::clean(dir())); // segments_2 + stale segment 1 meta  + unused column store

    // check consolidating segments
    expected_consolidating_segments = { 0, 1};
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    writer->commit(); // commit pending merge
    ASSERT_EQ(count+2, irs::directory_cleaner::clean(dir())); // +1 for segments, +1 for segment 1 doc mask

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // validate structure (doesn't take removals into account)
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());

    // assume 0 is merged segment
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(3, segment.docs_count()); // total count of documents
      ASSERT_EQ(2, segment.live_docs_count()); // total count of live documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      // with deleted docs
      {
        auto docsItr = termItr->postings(iresearch::flags());
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
        ASSERT_FALSE(docsItr->next());
      }

      // without deleted docs
      {
        auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
        ASSERT_FALSE(docsItr->next());
      }
    }
  }

  // consolidate with deletes
  {
    auto query_doc1_doc4 = iresearch::iql::query_builder().build("name==A||name==D", std::locale::classic());

    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_1

    // count number of files in segments
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
    writer->documents().remove(*query_doc1_doc4.filter);
    ASSERT_TRUE(writer->begin()); // begin transaction
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // can't consolidate segments that are already marked for consolidation
    ASSERT_FALSE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    writer->commit(); // commit transaction (will commit removal)
    ASSERT_EQ(4, irs::directory_cleaner::clean(dir())); // segments_2 + stale segment 1 meta + stale segment 2 meta + unused column store

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    writer->commit(); // commit pending merge
    ASSERT_EQ(count+3, irs::directory_cleaner::clean(dir())); // +1 for segments, +1 for segment 1 doc mask, +1 for segment 2 doc mask

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // validate structure (doesn't take removals into account)
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader.size());

    // assume 0 is merged segment
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(4, segment.docs_count()); // total count of documents
      ASSERT_EQ(2, segment.live_docs_count()); // total count of live documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      // with deleted docs
      {
        auto docsItr = termItr->postings(iresearch::flags());
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
        ASSERT_FALSE(docsItr->next());
      }

      // without deleted docs
      {
        auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
        ASSERT_FALSE(docsItr->next());
      }
    }
  }

  // consolidate with deletes + inserts
  {
    auto query_doc1_doc4 = iresearch::iql::query_builder().build("name==A||name==D", std::locale::classic());

    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_1

    // count number of files in segments
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
    writer->documents().remove(*query_doc1_doc4.filter);
    ASSERT_TRUE(writer->begin()); // begin transaction
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // can't consolidate segments that are already marked for consolidation
    ASSERT_FALSE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));
    writer->commit(); // commit transaction (will commit removal)
    ASSERT_EQ(4, irs::directory_cleaner::clean(dir())); // segments_2 + stale segment 1 meta + stale segment 2 meta + unused column store

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    writer->commit(); // commit pending merge
    ASSERT_EQ(count+3, irs::directory_cleaner::clean(dir())); // +1 for segments, +1 for segment 1 doc mask, +1 for segment 2 doc mask

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // validate structure (doesn't take removals into account)
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    expected.emplace_back();
    expected.back().add(doc5->indexed.begin(), doc5->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());

    // assume 0 is merged segment
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(4, segment.docs_count()); // total count of documents
      ASSERT_EQ(2, segment.live_docs_count()); // total count of live documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      // with deleted docs
      {
        auto docsItr = termItr->postings(iresearch::flags());
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
        ASSERT_FALSE(docsItr->next());
      }

      // without deleted docs
      {
        auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
        ASSERT_FALSE(docsItr->next());
      }
    }

    // assume 1 is the recently added segment
    {
      auto& segment = reader[1];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(1, segment.docs_count()); // total count of documents
      ASSERT_EQ(1, segment.live_docs_count()); // total count of live documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("E", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    }
  }

  // consolidate with deletes + inserts
  {
    auto query_doc1_doc4 = iresearch::iql::query_builder().build("name==A||name==D", std::locale::classic());

    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // segment 2
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_1

    // count number of files in segments
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_TRUE(writer->begin()); // begin transaction
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // can't consolidate segments that are already marked for consolidation
    ASSERT_FALSE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    writer->commit(); // commit transaction
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_2

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    writer->documents().remove(*query_doc1_doc4.filter);
    writer->commit(); // commit pending merge + removal
    ASSERT_EQ(count+6, irs::directory_cleaner::clean(dir())); // +1 for segments, +1 for segment 1 doc mask, +1 for segment 1 meta, +1 for segment 2 doc mask, +1 for segment 2 meta + unused column store

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // validate structure (doesn't take removals into account)
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc5->indexed.begin(), doc5->indexed.end());
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());

    // assume 1 is the recently added segment
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(1, segment.docs_count()); // total count of documents
      ASSERT_EQ(1, segment.live_docs_count()); // total count of live documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("E", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    }

    // assume 0 is merged segment
    {
      auto& segment = reader[1];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(4, segment.docs_count()); // total count of documents
      ASSERT_EQ(2, segment.live_docs_count()); // total count of live documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      // with deleted docs
      {
        auto docsItr = termItr->postings(iresearch::flags());
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
        ASSERT_FALSE(docsItr->next());
      }

      // without deleted docs
      {
        auto docsItr = segment.mask(termItr->postings(iresearch::flags()));
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
        ASSERT_TRUE(docsItr->next());
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
        ASSERT_FALSE(docsItr->next());
      }
    }
  }

  // consolidate with deletes + inserts
  {
    auto query_doc3_doc4 = iresearch::iql::query_builder().build("name==C||name==D", std::locale::classic());

    auto writer = open_writer();
    ASSERT_NE(nullptr, writer);

    // segment 1
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // segment 2
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));

    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_1

    size_t num_files_segment_2 = count;
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));
    num_files_segment_2 = count - num_files_segment_2;

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_TRUE(writer->begin()); // begin transaction
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // count number of files in segments
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));

    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()))); // consolidate

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    // can't consolidate segments that are already marked for consolidation
    ASSERT_FALSE(writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    size_t num_files_consolidation_segment = count;
    count = 0;
    ASSERT_TRUE(dir().visit(get_number_of_files_in_segments));
    num_files_consolidation_segment = count - num_files_consolidation_segment;

    ASSERT_EQ(0, irs::directory_cleaner::clean(dir()));

    writer->commit(); // commit transaction
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir())); // segments_2

    // check consolidating segments
    expected_consolidating_segments = { 0, 1 };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    writer->documents().remove(*query_doc3_doc4.filter);

    // commit pending merge + removal
    // pending consolidation will fail (because segment 2 will have no live docs after applying removals)
    writer->commit();

    // check consolidating segments
    expected_consolidating_segments = { };
    ASSERT_TRUE(writer->consolidate(check_consolidating_segments));

    ASSERT_EQ(num_files_consolidation_segment+num_files_segment_2+2, irs::directory_cleaner::clean(dir())); // +2 for segments_2 + unused column store

    // validate structure (doesn't take removals into account)
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.emplace_back();
    expected.back().add(doc5->indexed.begin(), doc5->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader.size());

    // assume 0 is first segment
    {
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
      ASSERT_EQ(2, segment.live_docs_count()); // total count of live documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }

    // assume 1 is the recently added segment
    {
      auto& segment = reader[1];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(1, segment.docs_count()); // total count of documents
      ASSERT_EQ(1, segment.live_docs_count()); // total count of live documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("E", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    }
  }
}

TEST_F(memory_index_test, consolidate_progress) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    &tests::generic_json_field_factory
  );
  auto* doc1 = gen.next();
  auto* doc2 = gen.next();
  auto policy = irs::index_utils::consolidation_policy(
    irs::index_utils::consolidate_count()
  );

  // test default progress (false)
  {
    irs::memory_directory dir;
    auto writer = irs::index_writer::make(dir, get_codec(), irs::OM_CREATE);
    ASSERT_TRUE(insert(
      *writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit(); // create segment0
    ASSERT_TRUE(insert(
      *writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit(); // create segment1

    auto reader = irs::directory_reader::open(dir, get_codec());

    ASSERT_EQ(2, reader.size());
    ASSERT_EQ(1, reader[0].docs_count());
    ASSERT_EQ(1, reader[1].docs_count());

    irs::merge_writer::flush_progress_t progress;

    ASSERT_TRUE(writer->consolidate(policy, get_codec(), progress));
    writer->commit(); // write consolidated segment

    reader = irs::directory_reader::open(dir, get_codec());

    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(2, reader[0].docs_count());
  }

  // test always-false progress
  {
    irs::memory_directory dir;
    auto writer = irs::index_writer::make(dir, get_codec(), irs::OM_CREATE);
    ASSERT_TRUE(insert(
      *writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit(); // create segment0
    ASSERT_TRUE(insert(
      *writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit(); // create segment1

    auto reader = irs::directory_reader::open(dir, get_codec());

    ASSERT_EQ(2, reader.size());
    ASSERT_EQ(1, reader[0].docs_count());
    ASSERT_EQ(1, reader[1].docs_count());

    irs::merge_writer::flush_progress_t progress = []()->bool { return false; };

    ASSERT_FALSE(writer->consolidate(policy, get_codec(), progress));
    writer->commit(); // write consolidated segment

    reader = irs::directory_reader::open(dir, get_codec());

    ASSERT_EQ(2, reader.size());
    ASSERT_EQ(1, reader[0].docs_count());
    ASSERT_EQ(1, reader[1].docs_count());
  }

  size_t progress_call_count = 0;

  const size_t MAX_DOCS = 32768;

  // test always-true progress
  {
    irs::memory_directory dir;
    auto writer = irs::index_writer::make(dir, get_codec(), irs::OM_CREATE);

    for (auto size = 0; size < MAX_DOCS; ++size) {
      ASSERT_TRUE(insert(
        *writer,
        doc1->indexed.begin(), doc1->indexed.end(),
        doc1->stored.begin(), doc1->stored.end()
      ));
    }
    writer->commit(); // create segment0

    for (auto size = 0; size < MAX_DOCS; ++size) {
      ASSERT_TRUE(insert(
        *writer,
        doc2->indexed.begin(), doc2->indexed.end(),
        doc2->stored.begin(), doc2->stored.end()
      ));
    }
    writer->commit(); // create segment1

    auto reader = irs::directory_reader::open(dir, get_codec());

    ASSERT_EQ(2, reader.size());
    ASSERT_EQ(MAX_DOCS, reader[0].docs_count());
    ASSERT_EQ(MAX_DOCS, reader[1].docs_count());

    irs::merge_writer::flush_progress_t progress =
      [&progress_call_count]()->bool { ++progress_call_count; return true; };

    ASSERT_TRUE(writer->consolidate(policy, get_codec(), progress));
    writer->commit(); // write consolidated segment

    reader = irs::directory_reader::open(dir, get_codec());

    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(2*MAX_DOCS, reader[0].docs_count());
  }

  ASSERT_TRUE(progress_call_count); // there should have been at least some calls

  // test limited-true progress
  for (size_t i = 1; i < progress_call_count; ++i) { // +1 for pre-decrement in 'progress'
    size_t call_count = i;
    irs::memory_directory dir;
    auto writer = irs::index_writer::make(dir, get_codec(), irs::OM_CREATE);
    for (auto size = 0; size < MAX_DOCS; ++size) {
      ASSERT_TRUE(insert(
        *writer,
        doc1->indexed.begin(), doc1->indexed.end(),
        doc1->stored.begin(), doc1->stored.end()
      ));
    }
    writer->commit(); // create segment0

    for (auto size = 0; size < MAX_DOCS; ++size) {
      ASSERT_TRUE(insert(
        *writer,
        doc2->indexed.begin(), doc2->indexed.end(),
        doc2->stored.begin(), doc2->stored.end()
      ));
    }
    writer->commit(); // create segment1

    auto reader = irs::directory_reader::open(dir, get_codec());

    ASSERT_EQ(2, reader.size());
    ASSERT_EQ(MAX_DOCS, reader[0].docs_count());
    ASSERT_EQ(MAX_DOCS, reader[1].docs_count());

    irs::merge_writer::flush_progress_t progress =
      [&call_count]()->bool { return --call_count; };

    ASSERT_FALSE(writer->consolidate(policy, get_codec(), progress));
    writer->commit(); // write consolidated segment

    reader = irs::directory_reader::open(dir, get_codec());

    ASSERT_EQ(2, reader.size());
    ASSERT_EQ(MAX_DOCS, reader[0].docs_count());
    ASSERT_EQ(MAX_DOCS, reader[1].docs_count());
  }
}

TEST_F(memory_index_test, segment_consolidate) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));
      }
  });

  irs::bytes_ref actual_value;

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();
  tests::document const* doc5 = gen.next();
  tests::document const* doc6 = gen.next();

  auto always_merge = irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count());
  auto all_features = irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::payload::type(), irs::offset::type() };

  // remove empty new segment
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->documents().remove(std::move(query_doc1.filter));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(0, reader.size());
  }

  // remove empty old segment
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc1.filter));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(0, reader.size());
  }

  // remove empty old, defragment new
  {
    auto query_doc1_doc2 = iresearch::iql::query_builder().build("name==A||name==B", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->documents().remove(std::move(query_doc1_doc2.filter));
    writer->commit();

    ASSERT_TRUE(writer->consolidate(always_merge));
    writer->commit();

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    ASSERT_EQ(1, segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // remove empty old, defragment new
  {
    auto query_doc1_doc2 = iresearch::iql::query_builder().build("name==A||name==B", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->documents().remove(std::move(query_doc1_doc2.filter));
    writer->commit();
    ASSERT_TRUE(writer->consolidate(always_merge));
    writer->commit();

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    ASSERT_EQ(1, segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // remove empty old, defragment old
  {
    auto query_doc1_doc2 = iresearch::iql::query_builder().build("name==A||name==B", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc1_doc2.filter));
    writer->commit();
    ASSERT_TRUE(writer->consolidate(always_merge));
    writer->commit();

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(1, segment.docs_count()); // total count of documents
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // remove empty old, defragment old
  {
    auto query_doc1_doc2 = iresearch::iql::query_builder().build("name==A||name==B", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc1_doc2.filter));
    writer->commit();
    ASSERT_TRUE(writer->consolidate(always_merge));
    writer->commit();

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(1, segment.docs_count()); // total count of documents
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  auto merge_if_masked = [](
      std::set<const irs::segment_meta*>& candidates,
      const irs::index_meta& meta,
      const irs::index_writer::consolidating_segments_t&
  )->void {
    for (auto& segment: meta) {
      if (segment.meta.live_docs_count != segment.meta.docs_count) {
        candidates.insert(&segment.meta);
      }
    }
  };

  // do defragment old segment with uncommited removal (i.e. do not consider uncomitted removals)
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc1.filter));
    writer->commit();
    ASSERT_TRUE(writer->consolidate(merge_if_masked));
    writer->commit();

    {
      auto reader = iresearch::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = reader[0]; // assume 0 is id of first/only segment
      ASSERT_EQ(1, segment.docs_count()); // total count of documents
    }
  }

  // do not defragment old segment with uncommited removal (i.e. do not consider uncomitted removals)
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc1.filter));
    ASSERT_TRUE(writer->consolidate(merge_if_masked));
    writer->commit();

    {
      auto reader = iresearch::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = reader[0]; // assume 0 is id of first/only segment
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
    }

    ASSERT_TRUE(writer->consolidate(merge_if_masked)); // previous removal now committed and considered
    writer->commit();

    {
      auto reader = iresearch::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = reader[0]; // assume 0 is id of first/only segment
      ASSERT_EQ(1, segment.docs_count()); // total count of documents
    }
  }

  // merge new+old segment
  {
    auto query_doc1_doc3 = iresearch::iql::query_builder().build("name==A||name==C", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->documents().remove(std::move(query_doc1_doc3.filter));
    writer->commit();
    ASSERT_TRUE(writer->consolidate(always_merge));
    writer->commit();

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(2, segment.docs_count()); // total count of documents
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }

  // merge new+old segment
  {
    auto query_doc1_doc3 = iresearch::iql::query_builder().build("name==A||name==C", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->documents().remove(std::move(query_doc1_doc3.filter));
    writer->commit();
    ASSERT_TRUE(writer->consolidate(always_merge));
    writer->commit();

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(2, segment.docs_count()); // total count of documents
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }

  // merge old+old segment
  {
    auto query_doc1_doc3 = iresearch::iql::query_builder().build("name==A||name==C", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc1_doc3.filter));
    writer->commit();
    ASSERT_TRUE(writer->consolidate(always_merge));
    writer->commit();

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(2, segment.docs_count()); // total count of documents
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }

  // merge old+old segment
  {
    auto query_doc1_doc3 = iresearch::iql::query_builder().build("name==A||name==C", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc1_doc3.filter));
    writer->commit();
    ASSERT_TRUE(writer->consolidate(always_merge));
    writer->commit();

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(2, segment.docs_count()); // total count of documents
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }

  // merge old+old+old segment
  {
    auto query_doc1_doc3_doc5 = iresearch::iql::query_builder().build("name==A||name==C||name==E", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc6->indexed.begin(), doc6->indexed.end(),
      doc6->stored.begin(), doc6->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc1_doc3_doc5.filter));
    writer->commit();
    ASSERT_TRUE(writer->consolidate(always_merge));
    writer->commit();

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    expected.back().add(doc6->indexed.begin(), doc6->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(3, segment.docs_count()); // total count of documents
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("F", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc6
    ASSERT_FALSE(docsItr->next());
  }

  // merge old+old+old segment
  {
    auto query_doc1_doc3_doc5 = iresearch::iql::query_builder().build("name==A||name==C||name==E", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc6->indexed.begin(), doc6->indexed.end(),
      doc6->stored.begin(), doc6->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc1_doc3_doc5.filter));
    writer->commit();
    ASSERT_TRUE(writer->consolidate(always_merge));
    writer->commit();

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc2->indexed.begin(), doc2->indexed.end());
    expected.back().add(doc4->indexed.begin(), doc4->indexed.end());
    expected.back().add(doc6->indexed.begin(), doc6->indexed.end());
    tests::assert_index(dir(), codec(), expected, all_features);

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(3, segment.docs_count()); // total count of documents
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("F", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc6
    ASSERT_FALSE(docsItr->next());
  }

  // merge two segments with different fields
  {
    auto writer = open_writer();
    // add 1st segment
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc6->indexed.begin(), doc6->indexed.end(),
      doc6->stored.begin(), doc6->stored.end()
    ));
    writer->commit();

    // add 2nd segment
    tests::json_doc_generator gen(
      resource("simple_sequential_upper_case.json"),
      [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
        if (data.is_string()) {
          doc.insert(std::make_shared<tests::templates::string_field>(
            irs::string_ref(name),
            data.str
          ));
        }
    });

    auto doc1_1 = gen.next();
    auto doc1_2 = gen.next();
    auto doc1_3 = gen.next();
    ASSERT_TRUE(insert(*writer,
      doc1_1->indexed.begin(), doc1_1->indexed.end(),
      doc1_1->stored.begin(), doc1_1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc1_2->indexed.begin(), doc1_2->indexed.end(),
      doc1_2->stored.begin(), doc1_2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc1_3->indexed.begin(), doc1_3->indexed.end(),
      doc1_3->stored.begin(), doc1_3->stored.end()
    ));

    // defragment segments
    writer->commit();
    ASSERT_TRUE(writer->consolidate(always_merge));
    writer->commit();

    // validate merged segment 
    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(6, segment.docs_count()); // total count of documents

    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    const auto* upper_case_column = segment.column_reader("NAME");
    ASSERT_NE(nullptr, upper_case_column);
    auto upper_case_values = upper_case_column->values();

    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("F", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc6
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(upper_case_values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1_1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(upper_case_values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1_2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(upper_case_values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1_3
    ASSERT_FALSE(docsItr->next());
  }

  // merge two segments with different fields
  {
    auto writer = open_writer();
    // add 1st segment
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc6->indexed.begin(), doc6->indexed.end(),
      doc6->stored.begin(), doc6->stored.end()
    ));
    writer->commit();

    // add 2nd segment
    tests::json_doc_generator gen(
      resource("simple_sequential_upper_case.json"),
      [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
        if (data.is_string()) {
          doc.insert(std::make_shared<tests::templates::string_field>(
            irs::string_ref(name),
            data.str
          ));
        }
    });

    auto doc1_1 = gen.next();
    auto doc1_2 = gen.next();
    auto doc1_3 = gen.next();
    ASSERT_TRUE(insert(*writer,
      doc1_1->indexed.begin(), doc1_1->indexed.end(),
      doc1_1->stored.begin(), doc1_1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc1_2->indexed.begin(), doc1_2->indexed.end(),
      doc1_2->stored.begin(), doc1_2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc1_3->indexed.begin(), doc1_3->indexed.end(),
      doc1_3->stored.begin(), doc1_3->stored.end()
    ));
    writer->commit();

    // defragment segments
    ASSERT_TRUE(writer->consolidate(always_merge));
    writer->commit();

    // validate merged segment 
    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(6, segment.docs_count()); // total count of documents

    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    const auto* upper_case_column = segment.column_reader("NAME");
    ASSERT_NE(nullptr, upper_case_column);
    auto upper_case_values = upper_case_column->values();

    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("F", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc6
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(upper_case_values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1_1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(upper_case_values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1_2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(upper_case_values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1_3
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(memory_index_test, segment_consolidate_policy) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        irs::string_ref(name),
        data.str
      ));
    }
  });

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();
  tests::document const* doc5 = gen.next();
  tests::document const* doc6 = gen.next();

  // bytes size policy (merge)
  {
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc6->indexed.begin(), doc6->indexed.end(),
      doc6->stored.begin(), doc6->stored.end()
    ));
    writer->commit();
    irs::index_utils::consolidate_bytes options;
    options.threshold = 1;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(options))); // value garanteeing merge
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size()); // 1+(2|3)

    // check 1st segment
    {
      std::unordered_set<irs::string_ref> expectedName = { "A", "B", "C", "D" };
      auto& segment = reader[0];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      irs::bytes_ref actual_value;
      for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }

      ASSERT_TRUE(expectedName.empty());
    }

    // check 2nd (merged) segment
    {
      std::unordered_set<irs::string_ref> expectedName = { "E", "F" };
      auto& segment = reader[1];
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      irs::bytes_ref actual_value;
      for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }

      ASSERT_TRUE(expectedName.empty());
    }
  }

  // bytes size policy (not modified)
  {
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));
    writer->commit();
    irs::index_utils::consolidate_bytes options;
    options.threshold = 0;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(options))); // value garanteeing non-merge
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      std::unordered_set<irs::string_ref> expectedName = { "A", "B", "C", "D" };
      irs::bytes_ref actual_value;

      auto& segment = reader[0]; // assume 0 is id of first segment
      ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }

      ASSERT_TRUE(expectedName.empty());
    }

    {
      std::unordered_set<irs::string_ref> expectedName = { "E" };
      irs::bytes_ref actual_value;

      auto& segment = reader[1]; // assume 1 is id of second segment
      ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }

      ASSERT_TRUE(expectedName.empty());
    }
  }

  // valid segment bytes_accum policy (merge)
  {
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    irs::index_utils::consolidate_bytes_accum options;
    options.threshold = 1;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(options))); // value garanteeing merge
    writer->commit();
    // segments merged because segment[0] is a candidate and needs to be merged with something

    std::unordered_set<irs::string_ref> expectedName = { "A", "B" };
    irs::bytes_ref actual_value;

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());

    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    }

    ASSERT_TRUE(expectedName.empty());
  }

  // valid segment bytes_accum policy (not modified)
  {
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    irs::index_utils::consolidate_bytes_accum options;
    options.threshold = 0;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(options))); // value garanteeing non-merge
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      std::unordered_set<irs::string_ref> expectedName = { "A" };
      irs::bytes_ref actual_value;

      auto& segment = reader[0]; // assume 0 is id of first segment
      ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }

      ASSERT_TRUE(expectedName.empty());
    }

    {
      std::unordered_set<irs::string_ref> expectedName = { "B" };
      irs::bytes_ref actual_value;

      auto& segment = reader[1]; // assume 1 is id of second segment
      ASSERT_EQ(expectedName.size(), segment.docs_count());
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }

      ASSERT_TRUE(expectedName.empty());
    }
  }

  // valid docs count policy (merge)
  {
    auto query_doc2_doc3_doc4 = irs::iql::query_builder().build("name==B||name==C||name==D", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc2_doc3_doc4.filter));
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));
    writer->commit();
    irs::index_utils::consolidate_docs_live options;
    options.threshold = 1;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(options))); // value garanteeing merge
    writer->commit();

    std::unordered_set<irs::string_ref> expectedName = { "A", "E" };
    irs::bytes_ref actual_value;

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());

    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    }

    ASSERT_TRUE(expectedName.empty());
  }

  // valid docs count policy (not modified)
  {
    auto query_doc2_doc3_doc4 = iresearch::iql::query_builder().build("name==B||name==C||name==D", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc2_doc3_doc4.filter));
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));
    writer->commit();
    irs::index_utils::consolidate_docs_live options;
    options.threshold = 0;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(options))); // value garanteeing non-merge
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      std::unordered_set<irs::string_ref> expectedName = { "A" };
      irs::bytes_ref actual_value;

      auto& segment = reader[0]; // assume 0 is id of first segment
      ASSERT_EQ(expectedName.size() + 3, segment.docs_count()); // total count of documents (+3 == B, C, D masked)
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      for (auto docsItr = segment.mask(termItr->postings(iresearch::flags())); docsItr->next();) {
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }

      ASSERT_TRUE(expectedName.empty());
    }

    {
      std::unordered_set<irs::string_ref> expectedName = { "E" };
      irs::bytes_ref actual_value;

      auto& segment = reader[1]; // assume 1 is id of second segment
      ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }

      ASSERT_TRUE(expectedName.empty());
    }
  }

  // valid segment fill policy (merge)
  {
    auto query_doc2_doc4 = iresearch::iql::query_builder().build("name==B||name==D", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc2_doc4.filter));
    writer->commit();
    irs::index_utils::consolidate_docs_fill options;
    options.threshold = 1;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(options))); // value garanteeing merge
    writer->commit();

    std::unordered_set<irs::string_ref> expectedName = { "A", "C" };
    irs::bytes_ref actual_value;

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());

    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    }

    ASSERT_TRUE(expectedName.empty());
  }

  // valid segment fill policy (not modified)
  {
    auto query_doc2_doc4 = iresearch::iql::query_builder().build("name==B||name==D", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc2_doc4.filter));
    writer->commit();
    irs::index_utils::consolidate_docs_fill options;
    options.threshold = 0;
    ASSERT_TRUE(writer->consolidate(irs::index_utils::consolidation_policy(options))); // value garanteeing non-merge
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(2, reader.size());

    {
      std::unordered_set<irs::string_ref> expectedName = { "A" };
      irs::bytes_ref actual_value;

      auto& segment = reader[0]; // assume 0 is id of first segment
      ASSERT_EQ(expectedName.size() + 1, segment.docs_count()); // total count of documents (+1 == B masked)
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      for (auto docsItr = segment.mask(termItr->postings(iresearch::flags())); docsItr->next();) {
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }

      ASSERT_TRUE(expectedName.empty());
    }

    {
      std::unordered_set<irs::string_ref> expectedName = { "C" };
      irs::bytes_ref actual_value;

      auto& segment = reader[1]; // assume 1 is id of second segment
      ASSERT_EQ(expectedName.size() + 1, segment.docs_count()); // total count of documents (+1 == D masked)
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());

      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      for (auto docsItr = segment.mask(termItr->postings(iresearch::flags())); docsItr->next();) {
        ASSERT_TRUE(values(docsItr->value(), actual_value));
        ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }

      ASSERT_TRUE(expectedName.empty());
    }
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                               fs_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class fs_index_test 
  : public tests::cases::tfidf<tests::fs_test_case_base>{
}; // fs_index_test

TEST_F(fs_index_test, clear_writer) {
  clear_writer();
}

TEST_F(fs_index_test, open_writer) {
  open_writer_check_lock();
}

TEST_F(fs_index_test, check_fields_order) {
  iterate_fields();
}

TEST_F(fs_index_test, check_attributes_order) {
  iterate_attributes();
}

TEST_F(fs_index_test, read_write_doc_attributes) {
  //static_assert(false, "add check: seek to the end, seek backwards, seek to the end again");
  read_write_doc_attributes_sparse_column_sparse_variable_length();
  read_write_doc_attributes_sparse_column_dense_variable_length();
  read_write_doc_attributes_sparse_column_dense_fixed_length();
  read_write_doc_attributes_sparse_column_dense_fixed_offset();
  read_write_doc_attributes_sparse_column_sparse_mask();
  read_write_doc_attributes_sparse_column_dense_mask();
  read_write_doc_attributes_dense_column_dense_variable_length();
  read_write_doc_attributes_dense_column_dense_fixed_length();
  read_write_doc_attributes_dense_column_dense_fixed_offset();
  read_write_doc_attributes_dense_column_dense_mask();
  read_write_doc_attributes_big();
  read_write_doc_attributes();
  read_empty_doc_attributes();
}

TEST_F(fs_index_test, writer_transaction_isolation) {
  writer_transaction_isolation();
}

TEST_F(fs_index_test, create_empty_index) {
  writer_check_open_modes();
}

TEST_F(fs_index_test, concurrent_read_column_mt) {
  concurrent_read_single_column_smoke();
  concurrent_read_multiple_columns();
}

TEST_F(fs_index_test, concurrent_read_index_mt) {
  concurrent_read_index();
}

TEST_F(fs_index_test, writer_atomicity_check) {
  writer_atomicity_check();
}

TEST_F(fs_index_test, insert_null_empty_term) {
  insert_doc_with_null_empty_term();
}

TEST_F(fs_index_test, writer_begin_rollback) {
  writer_begin_rollback();
}

TEST_F(fs_index_test, arango_demo_docs) {
  {
    tests::json_doc_generator gen(
      resource("arango_demo.json"), 
      &tests::generic_json_field_factory
    );
    add_segment(gen);
  }
  assert_index();
}

TEST_F(fs_index_test, europarl_docs) {
  {
    tests::templates::europarl_doc_template doc;
    tests::delim_doc_generator gen(resource("europarl.subset.txt"), doc);
    add_segment(gen);
  }
  assert_index();
}

TEST_F(fs_index_test, writer_close) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), 
    &tests::generic_json_field_factory
  );
  auto& directory = dir();
  auto* doc = gen.next();

  {
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc->indexed.begin(), doc->indexed.end(),
      doc->stored.begin(), doc->stored.end()
    ));
    writer->commit();
  } // ensure writer is closed

  std::vector<std::string> files;
  auto list_files = [&files] (std::string& name) {
    files.emplace_back(std::move(name));
    return true;
  };
  ASSERT_TRUE(directory.visit(list_files));

  // file removal should pass for all files (especially valid for Microsoft Windows)
  for (auto& file: files) {
    ASSERT_TRUE(directory.remove(file));
  }

  // validate that all files have been removed
  files.clear();
  ASSERT_TRUE(directory.visit(list_files));
  ASSERT_TRUE(files.empty());
}

// ----------------------------------------------------------------------------
// --SECTION--                             mmap_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class mmap_index_test
  : public tests::cases::tfidf<tests::mmap_test_case_base>{
}; // mmap_index_test

TEST_F(mmap_index_test, open_writer) {
  open_writer_check_lock();
}

TEST_F(mmap_index_test, check_fields_order) {
  iterate_fields();
}

TEST_F(mmap_index_test, check_attributes_order) {
  iterate_attributes();
}

TEST_F(mmap_index_test, read_write_doc_attributes) {
  //static_assert(false, "add check: seek to the end, seek backwards, seek to the end again");
  read_write_doc_attributes_sparse_column_sparse_variable_length();
  read_write_doc_attributes_sparse_column_dense_variable_length();
  read_write_doc_attributes_sparse_column_dense_fixed_length();
  read_write_doc_attributes_sparse_column_dense_fixed_offset();
  read_write_doc_attributes_sparse_column_sparse_mask();
  read_write_doc_attributes_sparse_column_dense_mask();
  read_write_doc_attributes_dense_column_dense_variable_length();
  read_write_doc_attributes_dense_column_dense_fixed_length();
  read_write_doc_attributes_dense_column_dense_fixed_offset();
  read_write_doc_attributes_dense_column_dense_mask();
  read_write_doc_attributes_big();
  read_write_doc_attributes();
  read_empty_doc_attributes();
}

TEST_F(mmap_index_test, writer_transaction_isolation) {
  writer_transaction_isolation();
}

TEST_F(mmap_index_test, create_empty_index) {
  writer_check_open_modes();
}

TEST_F(mmap_index_test, concurrent_read_column_mt) {
  concurrent_read_single_column_smoke();
  concurrent_read_multiple_columns();
}

TEST_F(mmap_index_test, concurrent_read_index_mt) {
  concurrent_read_index();
}

TEST_F(mmap_index_test, writer_atomicity_check) {
  writer_atomicity_check();
}

TEST_F(mmap_index_test, insert_null_empty_term) {
  insert_doc_with_null_empty_term();
}

TEST_F(mmap_index_test, writer_begin_rollback) {
  writer_begin_rollback();
}

TEST_F(mmap_index_test, arango_demo_docs) {
  {
    tests::json_doc_generator gen(
      resource("arango_demo.json"), 
      &tests::generic_json_field_factory
    );
    add_segment(gen);
  }
  assert_index();
}

TEST_F(mmap_index_test, europarl_docs) {
  {
    tests::templates::europarl_doc_template doc;
    tests::delim_doc_generator gen(resource("europarl.subset.txt"), doc);
    add_segment(gen);
  }
  assert_index();
}

TEST_F(mmap_index_test, monarch_eco_onthology) {
  {
    tests::json_doc_generator gen(
      resource("ECO_Monarch.json"),
      &tests::payloaded_json_field_factory);
    add_segment(gen);
  }
  assert_index();
}

TEST_F(mmap_index_test, writer_close) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), 
    &tests::generic_json_field_factory
  );
  auto& directory = dir();
  auto* doc = gen.next();

  {
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc->indexed.begin(), doc->indexed.end(),
      doc->stored.begin(), doc->stored.end()
    ));
    writer->commit();
  } // ensure writer is closed

  std::vector<std::string> files;
  auto list_files = [&files] (std::string& name) {
    files.emplace_back(std::move(name));
    return true;
  };
  ASSERT_TRUE(directory.visit(list_files));

  // file removal should pass for all files (especially valid for Microsoft Windows)
  for (auto& file: files) {
    ASSERT_TRUE(directory.remove(file));
  }

  // validate that all files have been removed
  files.clear();
  ASSERT_TRUE(directory.visit(list_files));
  ASSERT_TRUE(files.empty());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
