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
#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"
#include "document/field.hpp"
#include "iql/query_builder.hpp"
#include "formats/formats_10.hpp"
#include "search/filter.hpp"
#include "search/term_filter.hpp"
#include "store/fs_directory.hpp"
#include "store/mmap_directory.hpp"
#include "store/memory_directory.hpp"
#include "index/index_reader.hpp"
#include "utils/async_utils.hpp"
#include "utils/index_utils.hpp"
#include "utils/numeric_utils.hpp"

#include "index_tests.hpp"

#include <thread>

namespace ir = iresearch;

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

token_stream_payload::token_stream_payload(ir::token_stream* impl)
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
  pay_.value = ir::bytes_ref::nil;
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
    const ir::string_ref& name, 
    const irs::string_ref& value,
    const irs::flags& extra_features /*= irs::flags::empty_instance()*/
  ): features_({ irs::frequency::type(), irs::position::type() }),
     value_(value) {
  features_ |= extra_features;
  this->name(name);
}

const ir::flags& string_field::features() const {
  return features_;
}

// reject too long terms
void string_field::value(const ir::string_ref& str) {
  const auto size_len = ir::vencode_size_32(ir::byte_block_pool::block_type::SIZE);
  const auto max_len = (std::min)(str.size(), size_t(ir::byte_block_pool::block_type::SIZE - size_len));
  auto begin = str.begin();
  auto end = str.begin() + max_len;
  value_ = std::string(begin, end);
}

bool string_field::write(ir::data_output& out) const {
  ir::write_string(out, value_);
  return true;
}

ir::token_stream& string_field::get_tokens() const {
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
      ir::string_ref(name),
      data.str
    ));
  } else if (json_doc_generator::ValueType::NIL == data.vt) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(ir::null_token_stream::value_null());
  } else if (json_doc_generator::ValueType::BOOL == data.vt && data.b) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(ir::boolean_token_stream::value_true());
  } else if (json_doc_generator::ValueType::BOOL == data.vt && !data.b) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(ir::boolean_token_stream::value_true());
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
      ir::string_ref(name),
      data.str
    ));
  } else if (json_doc_generator::ValueType::NIL == data.vt) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(ir::null_token_stream::value_null());
  } else if (json_doc_generator::ValueType::BOOL == data.vt && data.b) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(ir::boolean_token_stream::value_true());
  } else if (json_doc_generator::ValueType::BOOL == data.vt && !data.b) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(ir::boolean_token_stream::value_false());
  } else if (data.is_number()) {
    // 'value' can be interpreted as a double
    doc.insert(std::make_shared<tests::double_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::double_field>();
    field.name(iresearch::string_ref(name));
    field.value(data.as_number<double_t>());
  }
}

const irs::columnstore_iterator::value_type INVALID{
  irs::type_limits<irs::type_t::doc_id_t>::invalid(),
  irs::bytes_ref::nil
};

const irs::columnstore_iterator::value_type EOFMAX{
  irs::type_limits<irs::type_t::doc_id_t>::eof(),
  irs::bytes_ref::nil
};

class index_test_case_base : public tests::index_test_base {
 public:
  void clear_writer() {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          ir::string_ref(name),
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
        writer->remove(std::move(query_doc4.filter));
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
        dir().visit([&file_count_post_clear](std::string&)->bool{ ++file_count_post_clear; return true; });
        ASSERT_EQ(file_count + 1, file_count_post_clear); // 1 extra file for new empty index meta
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

  void open_writer_check_lock() {
    {
      // open writer
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);
      // can't open another writer at the same time on the same directory
      ASSERT_THROW(ir::index_writer::make(dir(), codec(), ir::OM_CREATE), ir::lock_obtain_failed);
      writer->buffered_docs();
    }

    {
      // open writer
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);

      writer->commit();
      iresearch::directory_cleaner::clean(dir());
      // can't open another writer at the same time on the same directory
      ASSERT_THROW(ir::index_writer::make(dir(), codec(), ir::OM_CREATE), ir::lock_obtain_failed);
      writer->buffered_docs();
    }

    {
      // open writer
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);

      writer->buffered_docs();
    }
  }

  void profile_bulk_index(
      size_t num_insert_threads,
      size_t num_import_threads,
      size_t num_update_threads,
      size_t batch_size,
      ir::index_writer::ptr writer = nullptr,
      std::atomic<size_t>* commit_count = nullptr) {
    struct csv_doc_template_t: public tests::delim_doc_generator::doc_template {
      virtual void init() {
        clear();
        reserve(2);
        insert(std::make_shared<tests::templates::string_field>("id"));
        insert(std::make_shared<tests::templates::string_field>("label"));
      }

      virtual void value(size_t idx, const std::string& value) {
        switch(idx) {
         case 0:
          indexed.get<tests::templates::string_field>("id")->value(value);
          break;
         case 1:
          indexed.get<tests::templates::string_field>("label")->value(value);
        }
      }
    };

    iresearch::timer_utils::init_stats(true);

    std::atomic<bool> import_again(true);
    iresearch::memory_directory import_dir;
    std::atomic<size_t> import_docs_count(0);
    size_t import_interval = 10000;
    iresearch::directory_reader import_reader;
    std::atomic<size_t> parsed_docs_count(0);
    size_t update_skip = 1000;
    size_t writer_batch_size = batch_size ? batch_size : (std::numeric_limits<size_t>::max)();
    std::atomic<size_t> local_writer_commit_count(0);
    std::atomic<size_t>& writer_commit_count = commit_count ? *commit_count : local_writer_commit_count;
    std::atomic<size_t> writer_import_count(0);
    auto thread_count = (std::max)((size_t)1, num_insert_threads);
    auto total_threads = thread_count + num_import_threads + num_update_threads;
    ir::async_utils::thread_pool thread_pool(total_threads, total_threads);
    std::mutex mutex;
    std::mutex commit_mutex;

    if (!writer) {
      writer = open_writer();
    }

    // initialize reader data source for import threads
    {
      auto import_writer = iresearch::index_writer::make(import_dir, codec(), iresearch::OM_CREATE);

      {
        REGISTER_TIMER_NAMED_DETAILED("init - setup");
        tests::json_doc_generator import_gen(resource("simple_sequential.json"), &tests::generic_json_field_factory);

        for (const tests::document* doc; (doc = import_gen.next());) {
          REGISTER_TIMER_NAMED_DETAILED("init - insert");
          insert(*import_writer, doc->indexed.begin(), doc->indexed.end(), doc->stored.begin(), doc->stored.end());
        }
      }

      {
        REGISTER_TIMER_NAMED_DETAILED("init - commit");
        import_writer->commit();
      }

      REGISTER_TIMER_NAMED_DETAILED("init - open");
      import_reader = iresearch::directory_reader::open(import_dir);
    }

    {
      std::lock_guard<std::mutex> lock(mutex);

      // register insertion jobs
      for (size_t i = 0; i < thread_count; ++i) {
        thread_pool.run([&mutex, &commit_mutex, &writer, thread_count, i, writer_batch_size, &parsed_docs_count, &writer_commit_count, &import_again, this]()->void {
          {
            // wait for all threads to be registered
            std::lock_guard<std::mutex> lock(mutex);
          }

          csv_doc_template_t csv_doc_template;
          tests::delim_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template, ',');
          size_t gen_skip = i;

          for(size_t count = 0;; ++count) {
            // assume docs generated in same order and skip docs not meant for this thread
            if (gen_skip--) {
              if (!gen.next()) {
                break;
              }

              continue;
            }

            gen_skip = thread_count - 1;

            {
              REGISTER_TIMER_NAMED_DETAILED("parse");
              csv_doc_template.init();

              if (!gen.next()) {
                break;
              }
            }

            ++parsed_docs_count;

            {
              REGISTER_TIMER_NAMED_DETAILED("load");
              insert(*writer,
                csv_doc_template.indexed.begin(), csv_doc_template.indexed.end(), 
                csv_doc_template.stored.begin(), csv_doc_template.stored.end()
              );
            }

            if (count >= writer_batch_size) {
              TRY_SCOPED_LOCK_NAMED(commit_mutex, commit_lock);

              // break commit chains by skipping commit if one is already in progress
              if (!commit_lock) {
                continue;
              }

              {
                REGISTER_TIMER_NAMED_DETAILED("commit");
                writer->commit();
              }

              count = 0;
              ++writer_commit_count;
            }
          }

          {
            REGISTER_TIMER_NAMED_DETAILED("commit");
            writer->commit();
          }

          ++writer_commit_count;
          import_again.store(false); // stop any import threads, on completion of any insert thread
        });
      }

      // register import jobs
      for (size_t i = 0; i < num_import_threads; ++i) {
        thread_pool.run([&mutex, &writer, import_reader, &import_docs_count, &import_again, &import_interval, &writer_import_count, this]()->void {
          {
            // wait for all threads to be registered
            std::lock_guard<std::mutex> lock(mutex);
          }

          // ensure there will be at least 1 commit if scheduled
          do {
            import_docs_count += import_reader.docs_count();

            {
              REGISTER_TIMER_NAMED_DETAILED("import");
              writer->import(import_reader);
            }

            ++writer_import_count;
            std::this_thread::sleep_for(std::chrono::milliseconds(import_interval));
          } while (import_again.load());
        });
      }

      // register update jobs
      for (size_t i = 0; i < num_update_threads; ++i) {
        thread_pool.run([&mutex, &commit_mutex, &writer, num_update_threads, i, update_skip, writer_batch_size, &writer_commit_count, this]()->void {
          {
            // wait for all threads to be registered
            std::lock_guard<std::mutex> lock(mutex);
          }

          csv_doc_template_t csv_doc_template;
          tests::delim_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template, ',');
          size_t doc_skip = update_skip;
          size_t gen_skip = i;

          for(size_t count = 0;; ++count) {
            // assume docs generated in same order and skip docs not meant for this thread
            if (gen_skip--) {
              if (!gen.next()) {
                break;
              }

              continue;
            }

            gen_skip = num_update_threads - 1;

            if (doc_skip--) {
              continue;// skip docs originally meant to be updated by this thread
            }

            doc_skip = update_skip;

            {
              REGISTER_TIMER_NAMED_DETAILED("parse");
              csv_doc_template.init();

              if (!gen.next()) {
                break;
              }
            }

            {
              auto filter = iresearch::by_term::make();
              auto key_field = csv_doc_template.indexed.begin()->name();
              auto key_term = csv_doc_template.indexed.get<tests::templates::string_field>(key_field)->value();
              auto value_field = (++(csv_doc_template.indexed.begin()))->name();
              auto value_term = csv_doc_template.indexed.get<tests::templates::string_field>(value_field)->value();
              std::string updated_term(value_term.c_str(), value_term.size());

              static_cast<iresearch::by_term&>(*filter).field(key_field).term(key_term);
              updated_term.append(value_term.c_str(), value_term.size()); // double up term
              csv_doc_template.indexed.get<tests::templates::string_field>(value_field)->value(updated_term);
              csv_doc_template.insert(std::make_shared<tests::templates::string_field>("updated"));

              REGISTER_TIMER_NAMED_DETAILED("update");
              update(*writer,
                std::move(filter),
                csv_doc_template.indexed.begin(), csv_doc_template.indexed.end(), 
                csv_doc_template.stored.begin(), csv_doc_template.stored.end()
              );
            }

            if (count >= writer_batch_size) {
              TRY_SCOPED_LOCK_NAMED(commit_mutex, commit_lock);

              // break commit chains by skipping commit if one is already in progress
              if (!commit_lock) {
                continue;
              }

              {
                REGISTER_TIMER_NAMED_DETAILED("commit");
                writer->commit();
              }

              count = 0;
              ++writer_commit_count;
            }
          }

          {
            REGISTER_TIMER_NAMED_DETAILED("commit");
            writer->commit();
          }

          ++writer_commit_count;
        });
      }
    }

    thread_pool.stop();

    // ensure all data have been commited
    writer->commit();

    auto path = fs::path(test_dir()).append("profile_bulk_index.log");
    std::ofstream out(path.native());

    flush_timers(out);

    out.close();
    std::cout << "Path to timing log: " << fs::absolute(path).string() << std::endl;

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(true, 1 <= reader.size()); // not all commits might produce a new segment, some might merge with concurrent commits
    ASSERT_TRUE(writer_commit_count * iresearch::index_writer::THREAD_COUNT + writer_import_count >= reader.size());

    size_t indexed_docs_count = 0;
    size_t imported_docs_count = 0;
    size_t updated_docs_count = 0;
    auto imported_visitor = [&imported_docs_count](iresearch::doc_id_t, const irs::bytes_ref&)->bool {
      ++imported_docs_count;
      return true;
    };
    auto updated_visitor = [&updated_docs_count](iresearch::doc_id_t, const irs::bytes_ref&)->bool {
      ++updated_docs_count;
      return true;
    };

    for (size_t i = 0, count = reader.size(); i < count; ++i) {
      indexed_docs_count += reader[i].live_docs_count();

      const auto* column = reader[i].column_reader("same");
      if (column) {
        // field present in all docs from simple_sequential.json
        column->visit(imported_visitor);
      }

      column = reader[i].column_reader("updated");
      if (column) {
        // field insterted by updater threads
        column->visit(updated_visitor);
      }
    }

    ASSERT_EQ(parsed_docs_count + imported_docs_count, indexed_docs_count);
    ASSERT_EQ(imported_docs_count, import_docs_count);
    ASSERT_TRUE(imported_docs_count == num_import_threads || imported_docs_count); // at least some imports took place if import enabled
    ASSERT_TRUE(updated_docs_count == num_update_threads || updated_docs_count); // at least some updates took place if update enabled
  }

  void profile_bulk_index_dedicated_cleanup(size_t num_threads, size_t batch_size, size_t cleanup_interval) {
    auto* directory = &dir();
    std::atomic<bool> working(true);
    ir::async_utils::thread_pool thread_pool(1, 1);

    thread_pool.run([cleanup_interval, directory, &working]()->void {
      while (working.load()) {
        iresearch::directory_cleaner::clean(*directory);
        std::this_thread::sleep_for(std::chrono::milliseconds(cleanup_interval));
      }
    });

    {
      auto finalizer = ir::make_finally([&working]()->void{working = false;});
      profile_bulk_index(num_threads, 0, 0, batch_size);
    }

    thread_pool.stop();
  }

  void profile_bulk_index_dedicated_commit(size_t insert_threads, size_t commit_threads, size_t commit_interval) {
    auto* directory = &dir();
    std::atomic<bool> working(true);
    std::atomic<size_t> writer_commit_count(0);

    commit_threads = (std::max)(size_t(1), commit_threads);

    ir::async_utils::thread_pool thread_pool(commit_threads, commit_threads);
    auto writer = open_writer();

    for (size_t i = 0; i < commit_threads; ++i) {
      thread_pool.run([commit_interval, directory, &working, &writer, &writer_commit_count]()->void {
        while (working.load()) {
          {
            REGISTER_TIMER_NAMED_DETAILED("commit");
            writer->commit();
          }
          ++writer_commit_count;
          std::this_thread::sleep_for(std::chrono::milliseconds(commit_interval));
        }
      });
    }

    {
      auto finalizer = ir::make_finally([&working]()->void{working = false;});
      profile_bulk_index(insert_threads, 0, 0, 0, writer, &writer_commit_count);
    }

    thread_pool.stop();
  }

  void profile_bulk_index_dedicated_consolidate(size_t num_threads, size_t batch_size, size_t consolidate_interval) {
    ir::index_writer::consolidation_policy_t policy = [](const ir::directory& dir, const ir::index_meta& meta)->ir::index_writer::consolidation_acceptor_t {
      return [](const ir::segment_meta& meta)->bool { return true; }; // merge every segment
    };
    auto* directory = &dir();
    std::atomic<bool> working(true);
    ir::async_utils::thread_pool thread_pool(2, 2);
    auto writer = open_writer();

    thread_pool.run([consolidate_interval, directory, &working, &writer, &policy]()->void {
      while (working.load()) {
        writer->consolidate(policy, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(consolidate_interval));
      }
    });
    thread_pool.run([consolidate_interval, directory, &working, &writer, &policy]()->void {
      while (working.load()) {
        writer->consolidate(policy, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(consolidate_interval));

        // remove unused segments from memory/fs directory to avoid space related exceptions
        iresearch::directory_cleaner::clean(*directory);
      }
    });

    {
      auto finalizer = ir::make_finally([&working]()->void{working = false;});
      profile_bulk_index(num_threads, 0, 0, batch_size, writer);
    }

    thread_pool.stop();
    writer->consolidate(policy, false);
    writer->commit();

    struct dummy_doc_template_t: public tests::delim_doc_generator::doc_template {
      virtual void init() {}
      virtual void value(size_t idx, const std::string& value) {};
    };
    dummy_doc_template_t dummy_doc_template;
    tests::delim_doc_generator gen(resource("simple_two_column.csv"), dummy_doc_template, ',');
    size_t docs_count = 0;

    // determine total number of docs in source data
    while (gen.next()) {
      ++docs_count;
    }

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(docs_count, reader[0].docs_count());
  }

  void writer_check_open_modes() {
    // APPEND to nonexisting index, shoud fail
    ASSERT_THROW(ir::index_writer::make(dir(), codec(), ir::OM_APPEND), ir::file_not_found);
    // read index in empty directory, should fail
    ASSERT_THROW(ir::directory_reader::open(dir(), codec()), ir::index_not_found);

    // create empty index
    {
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);

      writer->commit();
    }

    // read empty index, it should not fail
    {
      auto reader = ir::directory_reader::open(dir(), codec());
      ASSERT_EQ(0, reader.live_docs_count());
      ASSERT_EQ(0, reader.docs_count());
      ASSERT_EQ(0, reader.size());
      ASSERT_EQ(reader.begin(), reader.end());
    }

    // append to index
    {
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_APPEND);
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

    // read empty index, it should not fail
    {
      auto reader = ir::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.live_docs_count());
      ASSERT_EQ(1, reader.docs_count());
      ASSERT_EQ(1, reader.size());
      ASSERT_NE(reader.begin(), reader.end());
    }
  }

  void writer_transaction_isolation() {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      [] (tests::document& doc, const std::string& name, const json_doc_generator::json_value& data) {
        if (json_doc_generator::ValueType::STRING == data.vt) {
          doc.insert(std::make_shared<templates::string_field>(
            ir::string_ref(name),
            data.str
          ));
        }
      });
      tests::document const* doc1 = gen.next();
      tests::document const* doc2 = gen.next();

      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);

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
        auto reader = ir::directory_reader::open(dir(), codec());
        ASSERT_EQ(1, reader.live_docs_count());
        ASSERT_EQ(1, reader.docs_count());
        ASSERT_EQ(1, reader.size());
        ASSERT_NE(reader.begin(), reader.end());
      }

      writer->commit(); // transaction #2
      ASSERT_EQ(0, writer->buffered_docs());
      // check index, 2 documents in 2 segments
      {
        auto reader = ir::directory_reader::open(dir(), codec());
        ASSERT_EQ(2, reader.live_docs_count());
        ASSERT_EQ(2, reader.docs_count());
        ASSERT_EQ(2, reader.size());
        ASSERT_NE(reader.begin(), reader.end());
      }

      // check documents
      {
        auto reader = ir::directory_reader::open(dir(), codec());
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
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);

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
      ASSERT_THROW(ir::directory_reader::open(dir(), codec()), ir::index_not_found);

      writer->rollback(); // rollback transaction
      writer->rollback(); // does nothing
      ASSERT_EQ(0, writer->buffered_docs());

      writer->commit(); // commit

      // check index, it should be empty 
      {
        auto reader = ir::directory_reader::open(dir(), codec());
        ASSERT_EQ(0, reader.live_docs_count());
        ASSERT_EQ(0, reader.docs_count());
        ASSERT_EQ(0, reader.size());
        ASSERT_EQ(reader.begin(), reader.end());
      }
    }

    // test rolled-back index can still be opened after directory cleaner run
    {
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);
      ASSERT_TRUE(insert(*writer,
        doc2->indexed.begin(), doc2->indexed.end(),
        doc2->stored.begin(), doc2->stored.end()
      ));
      ASSERT_TRUE(writer->begin()); // prepare for commit tx #1
      writer->commit(); // commit tx #2
      auto file_count = 0;
      auto dir_visitor = [&file_count](std::string&)->bool { ++file_count; return true; };
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

      auto reader = ir::directory_reader::open(dir(), codec());
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
    struct csv_doc_template_t: public tests::delim_doc_generator::doc_template {
      virtual void init() {
        clear();
        reserve(2);
        insert(std::make_shared<tests::templates::string_field>("id"));
        insert(std::make_shared<tests::templates::string_field>("label"));
      }

      virtual void value(size_t idx, const std::string& value) {
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
      tests::delim_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template, ',');
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);

      const tests::document* doc;
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(*writer,
          doc->indexed.end(), doc->indexed.end(), 
          doc->stored.begin(), doc->stored.end()
        ));
      }
      writer->commit();
    }

    auto reader = ir::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());

    // read columns 
    {
      auto visit_column = [&segment] (const iresearch::string_ref& column_name) {
        auto* meta = segment.column(column_name);
        if (!meta) {
          return false;
        }

        ir::doc_id_t expected_id = 0;
        csv_doc_template_t csv_doc_template;
        tests::delim_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template, ',');
        auto visitor = [&gen, &column_name, &expected_id] (ir::doc_id_t id, const irs::bytes_ref& actual_value) {
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

      auto read_column_offset = [&segment] (const iresearch::string_ref& column_name, ir::doc_id_t offset) {
        auto* meta = segment.column(column_name);
        if (!meta) {
          return false;
        }

        ir::doc_id_t expected_id = 0;
        csv_doc_template_t csv_doc_template;
        tests::delim_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template, ',');
        const tests::document* doc = nullptr;

        auto column = segment.column_reader(meta->id);
        if (!column) {
          return false;
        }
        auto reader = column->values();

        irs::bytes_ref actual_value;

        // skip first 'offset' docs
        doc = gen.next();
        for (ir::doc_id_t id = 0; id < offset && doc; ++id) {
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

        ir::doc_id_t expected_id = 0;
        csv_doc_template_t csv_doc_template;
        tests::delim_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template, ',');
        const tests::document* doc = nullptr;

        auto column = segment.column_reader(meta->id);

        if (!column) {
          return false;
        }

        auto it = column->iterator();

        if (!it) {
          return false;
        }

        auto& value = it->value();
        auto& value_str = value.second;

        doc = gen.next();

        if (!doc) {
          return false;
        }

        while (doc) {
          if (!it->next()) {
            return false;
          }

          if (++expected_id != value.first) {
            return false;
          }

          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          if (!field) {
            return false;
          }

          if (field->value() != irs::to_string<irs::string_ref>(value_str.c_str())) {
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
      ir::doc_id_t skip = 0;
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
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);

      // fields only
      ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end()));
      ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end()));
      ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end()));
      ASSERT_TRUE(insert(*writer, doc4->indexed.begin(), doc4->indexed.end()));
      writer->commit();
    }

    auto reader = ir::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());

    const auto* column = segment.column_reader("name");
    ASSERT_EQ(nullptr, column);
  }

  void read_write_doc_attributes_sparse_mask() {
    static const irs::doc_id_t MAX_DOCS = 1500;
    static const iresearch::string_ref column_name = "id";

    // write documents
    {
      struct stored {
        const irs::string_ref& name() { return column_name; }
        bool write(irs::data_output& out) { return true; }
      } field;

      irs::doc_id_t docs_count = 0;
      auto inserter = [&docs_count, &field](const irs::index_writer::document& doc) {
        if (docs_count % 2) {
          doc.insert(irs::action::store, field);
        }
        return ++docs_count < MAX_DOCS;
      };

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      writer->insert(inserter); // insert MAX_DOCS/2 documents
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

          if (irs::bytes_ref::nil != actual_data) {
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
          ASSERT_EQ(irs::bytes_ref::nil, actual_value);
        }

        // read (cached)
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_EQ(i % 2, values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::nil, actual_value);
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

          if (irs::bytes_ref::nil != actual_data) {
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
        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          expected_doc += 2;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
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
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

          if (irs::bytes_ref::nil != actual_data) {
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          expected_doc += 2;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
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
          ASSERT_EQ(irs::bytes_ref::nil, actual_value);
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

          if (irs::bytes_ref::nil != actual_data) {
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          expected_doc += 2;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
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
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

          if (irs::bytes_ref::nil != actual_data) {
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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(&actual_value, &(it->seek(expected_doc)));
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_EQ(EOFMAX, it->seek(expected_doc));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_EQ(EOFMAX, it->seek(MAX_DOCS + 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // seek over column (not cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(&actual_value, &(it->seek(expected_doc)));
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          ASSERT_EQ(&actual_value, &(it->seek(expected_doc-1))); // seek before the existing key (value should remain the same)
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_EQ(EOFMAX, it->seek(expected_doc));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_EQ(EOFMAX, it->seek(MAX_DOCS + 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // seek over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = 2;
        size_t docs_count = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(&actual_value, &(it->seek(expected_doc-1)));
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          ASSERT_EQ(&actual_value, &(it->seek(expected_doc))); // seek to the existing key (value should remain the same)
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_EQ(EOFMAX, it->seek(expected_doc));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_EQ(EOFMAX, it->seek(MAX_DOCS + 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // seek to the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = 2;
        size_t docs_count = 0;

        ASSERT_EQ(&actual_value, &(it->seek(expected_doc)));
        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        expected_doc += 2;
        ++docs_count;

        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          expected_doc += 2;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // seek before the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = 2;
        size_t docs_count = 0;

        ASSERT_EQ(&actual_value, &(it->seek(expected_doc-1)));
        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        expected_doc += 2;
        ++docs_count;

        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          expected_doc += 2;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS));
        ASSERT_EQ(MAX_DOCS, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS-1));
        ASSERT_EQ(MAX_DOCS, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek to after the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS+1));
        ASSERT_EQ(EOFMAX, actual_value);

        // can't seek backwards
        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS-1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = 2;
        size_t docs_count = 0;

        for (;;) {
          ASSERT_EQ(&actual_value, &it->seek(expected_doc));

          if (EOFMAX == actual_value) {
            break;
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          ++docs_count;

          auto next_expected_doc = expected_doc + 2;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_EQ(next_expected_doc, actual_value.first);
            ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

            // can't seek backwards
            ASSERT_EQ(&actual_value, &it->seek(expected_doc));
            ASSERT_EQ(next_expected_doc, actual_value.first);
            ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

            next_expected_doc += 2;
            ++docs_count;
          }

          expected_doc = next_expected_doc;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
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

          auto& actual_value = it->value();
          ASSERT_EQ(INVALID, actual_value);

          ASSERT_EQ(&actual_value, &it->seek(expected_doc));

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          ++docs_count;

          auto next_expected_doc = expected_doc + 2;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_EQ(next_expected_doc, actual_value.first);
            ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

            next_expected_doc += 2;
          }

          expected_doc -= 2;
        }
        ASSERT_EQ(MAX_DOCS/2, docs_count);

        // seek before the first document
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        expected_doc = min_doc;
        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        ASSERT_EQ(min_doc, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        auto next_expected_doc = expected_doc + 2;
        for (auto i = 0; i < steps_forward; ++i) {
          ASSERT_TRUE(it->next());
          ASSERT_EQ(next_expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = MAX_DOCS;

        ASSERT_EQ(&actual_value, &it->seek(expected_doc));

        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        auto next_expected_doc = expected_doc + 2;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_EQ(next_expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          next_expected_doc += 2;
        }

        expected_doc -= 2;
        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        ASSERT_EQ(actual_value, EOFMAX);
      }

      // seek over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = 2;
        size_t docs_count = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(&actual_value, &(it->seek(expected_doc-1)));

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          expected_doc += 2;
          ++docs_count;
        }

        ASSERT_EQ(EOFMAX, it->seek(expected_doc));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_EQ(EOFMAX, it->seek(MAX_DOCS + 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
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
          ASSERT_EQ(irs::bytes_ref::nil, actual_value);
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

          if (irs::bytes_ref::nil != actual_data) {
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = 2;
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          expected_doc += 2;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }
    }
  }

  void read_write_doc_attributes_dense_mask() {
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
      auto inserter = [&docs_count, &field](const irs::index_writer::document& doc) {
        doc.insert(irs::action::store, field);
        return ++docs_count < MAX_DOCS;
      };

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      writer->insert(inserter); // insert MAX_DOCS documents
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

          if (irs::bytes_ref::nil != actual_data) {
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
          ASSERT_EQ(irs::bytes_ref::nil, actual_value);
        }

        // cached
        for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::nil, actual_value);
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

          if (irs::bytes_ref::nil != actual_data) {
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          ++expected_doc;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

          if (irs::bytes_ref::nil != actual_data) {
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          ++expected_doc;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
          ASSERT_EQ(irs::bytes_ref::nil, actual_value);
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

          if (irs::bytes_ref::nil != actual_data) {
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          ++expected_doc;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

          if (irs::bytes_ref::nil != actual_data) {
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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(&actual_value, &(it->seek(expected_doc)));
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          ++expected_doc;
          ++docs_count;
        }

        ASSERT_EQ(EOFMAX, it->seek(expected_doc));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_EQ(EOFMAX, it->seek(MAX_DOCS + 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek to the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        size_t docs_count = 0;

        ASSERT_EQ(&actual_value, &(it->seek(expected_doc)));
        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        ++expected_doc;
        ++docs_count;

        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          ++expected_doc;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek before the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        size_t docs_count = 0;

        ASSERT_EQ(&actual_value, &(it->seek(expected_doc-1)));
        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        ++expected_doc;
        ++docs_count;

        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          ++expected_doc;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(MAX_DOCS, docs_count);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS));
        ASSERT_EQ(MAX_DOCS, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS-1));
        ASSERT_EQ(MAX_DOCS-1, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        ASSERT_TRUE(it->next());
        ASSERT_EQ(MAX_DOCS, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek to after the end + next + seek before end
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        it->seek(MAX_DOCS+1);
        ASSERT_EQ(EOFMAX, actual_value);

        // can't seek backwards
        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS - 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        size_t docs_count = 0;

        for (;;) {
          ASSERT_EQ(&actual_value, &it->seek(expected_doc));

          if (EOFMAX == actual_value) {
            break;
          }

          ++docs_count;

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          auto next_expected_doc = expected_doc + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_EQ(next_expected_doc, actual_value.first);
            ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

            // can't seek backwards
            ASSERT_EQ(&actual_value, &it->seek(expected_doc));
            ASSERT_EQ(next_expected_doc, actual_value.first);
            ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

            ++next_expected_doc;
            ++docs_count;
          }

          expected_doc = next_expected_doc;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
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

          auto& actual_value = it->value();
          ASSERT_EQ(INVALID, actual_value);

          ASSERT_EQ(&actual_value, &it->seek(expected_doc));

          ++docs_count;

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          auto next_expected_doc = expected_doc + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            ASSERT_EQ(next_expected_doc, actual_value.first);
            ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

            ++next_expected_doc;
          }

          --expected_doc;
        }
        ASSERT_EQ(MAX_DOCS, docs_count);

        // seek before the first document
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        expected_doc = min_doc;
        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        ASSERT_EQ(min_doc, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        auto next_expected_doc = expected_doc + 1;
        for (auto i = 0; i < steps_forward; ++i) {
          ASSERT_TRUE(it->next());

          ASSERT_EQ(next_expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = MAX_DOCS;

        ASSERT_EQ(&actual_value, &it->seek(expected_doc));

        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

        auto next_expected_doc = expected_doc + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_EQ(next_expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          ++next_expected_doc;
        }

        --expected_doc;
        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);
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
          ASSERT_EQ(irs::bytes_ref::nil, actual_value);
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

          if (irs::bytes_ref::nil != actual_data) {
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t docs_count = 0;
        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        for (; it->next(); ) {
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(irs::bytes_ref::nil, actual_value.second);

          ++expected_doc;
          ++docs_count;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }
    }
  }

  void read_write_doc_attributes_dense_fixed_length() {
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

      auto inserter = [&field](const irs::index_writer::document& doc) {
        doc.insert(irs::action::store, field);
        return ++field.value < MAX_DOCS;
      };

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      writer->insert(inserter); // insert MAX_DOCS documents
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(&actual_value, &(it->seek(expected_doc)));
          const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_EQ(EOFMAX, it->seek(expected_doc));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_EQ(EOFMAX, it->seek(MAX_DOCS + 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek to the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        ASSERT_EQ(&actual_value, &(it->seek(expected_doc)));
        const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;

        for (; it->next(); ) {
          const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek before the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        ASSERT_EQ(&actual_value, &(it->seek(expected_doc-1)));
        const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;

        for (; it->next(); ) {
          const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        auto expected_doc = MAX_DOCS;
        auto expected_value = MAX_DOCS-1;

        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        auto expected_doc = MAX_DOCS-1;
        auto expected_value = expected_doc-1;
        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
        ASSERT_TRUE(it->next());
        actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());
        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek to after the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS + 1));
        ASSERT_EQ(EOFMAX, actual_value);

        // can't seek backwards
        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS - 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        for (;;) {
          ASSERT_EQ(&actual_value, &it->seek(expected_doc));

          if (EOFMAX == actual_value) {
            break;
          }

          auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          auto next_expected_doc = expected_doc + 1;
          auto next_expected_value = expected_value + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

            ASSERT_EQ(next_expected_doc, actual_value.first);
            ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

            // can't seek backwards
            ASSERT_EQ(&actual_value, &it->seek(expected_doc));
            ASSERT_EQ(next_expected_doc, actual_value.first);
            ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

            ++next_expected_doc;
            ++next_expected_value;
          }

          expected_doc = next_expected_doc;
          expected_value = next_expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
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

          auto& actual_value = it->value();
          ASSERT_EQ(INVALID, actual_value);

          ASSERT_EQ(&actual_value, &it->seek(expected_doc));

          auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ++docs_count;

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          auto next_expected_doc = expected_doc + 1;
          auto next_expected_value = expected_value + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

            ASSERT_EQ(next_expected_doc, actual_value.first);
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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        expected_doc = min_doc;
        expected_value = expected_doc - 1;
        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());
        ASSERT_EQ(min_doc, actual_value.first);
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward; ++i) {
          ASSERT_TRUE(it->next());
          actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(next_expected_doc, actual_value.first);
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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = MAX_DOCS;
        irs::doc_id_t expected_value = expected_doc - 1;

        ASSERT_EQ(&actual_value, &it->seek(expected_doc));

        auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(next_expected_doc, actual_value.first);
          ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++next_expected_doc;
          ++next_expected_value;
        }

        --expected_doc;
        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        ASSERT_EQ(actual_value, EOFMAX);
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), expected_value);
      }
    }
  }

  void read_write_doc_attributes_dense_variable_length() {
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

      auto inserter = [&field](const irs::index_writer::document& doc) {
        doc.insert(irs::action::store, field);
        return ++field.value < MAX_DOCS;
      };

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      writer->insert(inserter); // insert MAX_DOCS documents
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(&actual_value, &(it->seek(expected_doc)));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++expected_doc;
          ++expected_value;
        }

        ASSERT_EQ(EOFMAX, it->seek(expected_doc));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_EQ(EOFMAX, it->seek(MAX_DOCS + 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(MAX_DOCS, expected_value);
      }


      // seek to the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        ASSERT_EQ(&actual_value, &(it->seek(expected_doc)));
        const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        auto expected_value_str = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value_str, actual_value_str);

        ++expected_doc;
        ++expected_value;

        for (; it->next(); ) {
          const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_value_str);

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek before the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        ASSERT_EQ(&actual_value, &(it->seek(expected_doc-1)));
        const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        auto expected_value_str = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }
        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value_str, actual_value_str);

        ++expected_doc;
        ++expected_value;

        for (; it->next(); ) {
          const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_value_str);

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(MAX_DOCS, expected_value);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        auto expected_doc = MAX_DOCS;
        auto expected_value = MAX_DOCS-1;

        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }
        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value_str, actual_value_str);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        auto expected_doc = MAX_DOCS-1;
        auto expected_value = expected_doc-1;
        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }
        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value_str, actual_value_str);

        ++expected_doc;
        ++expected_value;
        expected_value_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }
        ASSERT_TRUE(it->next());
        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value_str, irs::to_string<irs::string_ref>(actual_value.second.c_str()));

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek to after the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS + 1));
        ASSERT_EQ(EOFMAX, actual_value);

        // can't seek backwards
        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS - 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;

        for (;;) {
          ASSERT_EQ(&actual_value, &it->seek(expected_doc));

          if (EOFMAX == actual_value) {
            break;
          }

          auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_value_str);

          auto next_expected_doc = expected_doc + 1;
          auto next_expected_value = expected_value + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

            auto next_expected_value_str  = std::to_string(next_expected_value);
            if (next_expected_value % 2) {
              next_expected_value_str.append(column_name.c_str(), column_name.size());
            }

            ASSERT_EQ(next_expected_doc, actual_value.first);
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            // can't seek backwards
            ASSERT_EQ(&actual_value, &it->seek(expected_doc));
            ASSERT_EQ(next_expected_doc, actual_value.first);
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            ++next_expected_doc;
            ++next_expected_value;
          }

          expected_doc = next_expected_doc;
          expected_value = next_expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
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

          auto& actual_value = it->value();
          ASSERT_EQ(INVALID, actual_value);

          ASSERT_EQ(&actual_value, &it->seek(expected_doc));

          auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ++docs_count;

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_value_str);

          auto next_expected_doc = expected_doc + 1;
          auto next_expected_value = expected_value + 1;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

            auto next_expected_value_str  = std::to_string(next_expected_value);
            if (next_expected_value % 2) {
              next_expected_value_str.append(column_name.c_str(), column_name.size());
            }

            ASSERT_EQ(next_expected_doc, actual_value.first);
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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        expected_doc = min_doc;
        expected_value = expected_doc - 1;
        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());
        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }
        ASSERT_EQ(min_doc, actual_value.first);
        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward; ++i) {
          ASSERT_TRUE(it->next());
          actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto next_expected_value_str  = std::to_string(next_expected_value);
          if (next_expected_value % 2) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, actual_value.first);
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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = MAX_DOCS;
        irs::doc_id_t expected_value = expected_doc - 1;

        ASSERT_EQ(&actual_value, &it->seek(expected_doc));

        auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto next_expected_value_str  = std::to_string(next_expected_value);
          if (next_expected_value % 2) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, actual_value.first);
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          ++next_expected_doc;
          ++next_expected_value;
        }

        --expected_doc;
        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        ASSERT_EQ(actual_value, EOFMAX);
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        irs::doc_id_t expected_value = 0;
        for (; it->next(); ) {
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str = std::to_string(expected_value);
          if (expected_value % 2) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          ++expected_doc;
          ++expected_value;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
        ASSERT_EQ(MAX_DOCS, expected_value);
      }
    }
  }

  void read_write_doc_attributes_sparse_variable_length() {
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

      auto inserter = [&inserted, &field](const irs::index_writer::document& doc) {
        if (field.value % 2 ) {
          doc.insert(irs::action::store, field);
          ++inserted;
        }
        return ++field.value < MAX_DOCS;
      };

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      writer->insert(inserter); // insert MAX_DOCS documents
      writer->commit();
    }

    // check inserted values:
    // - visit (not cached)
    // - random read (not cached)
    // - random read (cached)
    // - visit (cached)
    // - iterate (cached)
    {
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; it->next(); ) {
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; it->next(); ) {
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; it->next(); ) {
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
      auto reader = ir::directory_reader::open(this->dir(), this->codec());
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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(&actual_value, &(it->seek(expected_doc)));
          auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          ASSERT_EQ(&actual_value, &(it->seek(expected_value))); // seek before the existing key (value should remain the same)
          actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }

        ASSERT_EQ(EOFMAX, it->seek(expected_doc));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_EQ(EOFMAX, it->seek(MAX_DOCS + 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(inserted, docs);
      }

      // seek over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(&actual_value, &(it->seek(expected_value)));
          auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          ASSERT_EQ(&actual_value, &(it->seek(expected_doc))); // seek to the existing key (value should remain the same)
          actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());
          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }

        ASSERT_EQ(EOFMAX, it->seek(expected_doc));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_EQ(EOFMAX, it->seek(MAX_DOCS + 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
        ASSERT_EQ(inserted, docs);
      }

      // seek to the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;

        ASSERT_EQ(&actual_value, &(it->seek(expected_doc)));
        const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }
        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value_str, actual_value_str);

        expected_doc += 2;
        expected_value += 2;
        ++docs;

        for (; it->next(); ) {
          const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_value_str);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
        ASSERT_EQ(inserted, docs);
      }

      // seek before the begin + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;

        ASSERT_EQ(&actual_value, &(it->seek(expected_doc-1)));
        const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }
        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value_str, actual_value_str);

        expected_doc += 2;
        expected_value += 2;
        ++docs;

        for (; it->next(); ) {
          const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_value_str);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
        ASSERT_EQ(inserted, docs);
      }

      // seek to the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        auto expected_doc = MAX_DOCS;
        auto expected_value = MAX_DOCS-1;
        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        it->seek(expected_doc);
        const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());
        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value_str, actual_value_str);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek to before the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        auto expected_value = MAX_DOCS-1;
        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        it->seek(expected_value);
        const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());
        ASSERT_EQ(MAX_DOCS, actual_value.first);
        ASSERT_EQ(expected_value_str, actual_value_str);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek to after the end + next
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS+1));
        ASSERT_EQ(EOFMAX, actual_value);

        // can't seek backwards
        ASSERT_EQ(&actual_value, &it->seek(MAX_DOCS-1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
      }

      // seek + next(x5)
      {
        const size_t steps_forward = 5;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;

        for (;;) {
          ASSERT_EQ(&actual_value, &it->seek(expected_doc));

          if (EOFMAX == actual_value) {
            break;
          }

          auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_value_str);

          ++docs;

          auto next_expected_doc = expected_doc + 2;
          auto next_expected_value = expected_value + 2;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

            auto next_expected_value_str  = std::to_string(next_expected_value);
            if (next_expected_value % 3) {
              next_expected_value_str.append(column_name.c_str(), column_name.size());
            }

            ASSERT_EQ(next_expected_doc, actual_value.first);
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            // can't seek backwards
            ASSERT_EQ(&actual_value, &it->seek(expected_doc));
            ASSERT_EQ(next_expected_doc, actual_value.first);
            ASSERT_EQ(next_expected_value_str, actual_value_str);

            next_expected_doc += 2;
            next_expected_value += 2;
            ++docs;
          }

          expected_doc = next_expected_doc;
          expected_value = next_expected_value;
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
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

          auto& actual_value = it->value();
          ASSERT_EQ(INVALID, actual_value);

          ASSERT_EQ(&actual_value, &it->seek(expected_doc));

          auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_value_str);

          ++docs;

          auto next_expected_doc = expected_doc + 2;
          auto next_expected_value = expected_value + 2;
          for (auto i = 0; i < steps_forward && it->next(); ++i) {
            actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

            auto next_expected_value_str  = std::to_string(next_expected_value);
            if (next_expected_value % 3) {
              next_expected_value_str.append(column_name.c_str(), column_name.size());
            }

            ASSERT_EQ(next_expected_doc, actual_value.first);
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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        expected_doc = min_doc;
        expected_value = expected_doc - 1;
        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());
        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }
        ASSERT_EQ(min_doc, actual_value.first);
        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 2;
        auto next_expected_value = expected_value + 2;
        for (auto i = 0; i < steps_forward; ++i) {
          ASSERT_TRUE(it->next());
          actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto next_expected_value_str  = std::to_string(next_expected_value);
          if (next_expected_value % 3) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, actual_value.first);
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

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = MAX_DOCS;
        irs::doc_id_t expected_value = expected_doc - 1;

        ASSERT_EQ(&actual_value, &it->seek(expected_doc));

        auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, actual_value.first);
        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 2;
        auto next_expected_value = expected_value + 2;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto next_expected_value_str  = std::to_string(next_expected_value);
          if (next_expected_value % 3) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, actual_value.first);
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          next_expected_doc += 2;
          next_expected_value += 2;
        }

        expected_doc -= 2;
        ASSERT_EQ(&actual_value, &it->seek(expected_doc));
        ASSERT_EQ(actual_value, EOFMAX);
      }

      // seek over column (cached)
      {
        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(INVALID, actual_value);

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; expected_doc <= MAX_DOCS; ) {
          ASSERT_EQ(&actual_value, &(it->seek(expected_doc-1)));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }

        ASSERT_EQ(EOFMAX, it->seek(expected_doc));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_EQ(EOFMAX, it->seek(MAX_DOCS + 1));
        ASSERT_EQ(EOFMAX, actual_value);

        ASSERT_FALSE(it->next());
        ASSERT_EQ(EOFMAX, actual_value);
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        irs::doc_id_t expected_doc = 2;
        irs::doc_id_t expected_value = 1;
        size_t docs = 0;
        for (; it->next(); ) {
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          auto expected_value_str  = std::to_string(expected_value);
          if (expected_value % 3) {
            expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(expected_doc, actual_value.first);
          ASSERT_EQ(expected_value_str, actual_str_value);

          expected_doc += 2;
          expected_value += 2;
          ++docs;
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);
      ASSERT_NE(nullptr, writer);

      auto inserter = [&field, &names](irs::index_writer::document& doc) {
        for (auto& name : names) {
          field.name_ = name;
          doc.insert(irs::action::index, field);
        }
        return false; // break the loop
      };

      ASSERT_TRUE(writer->insert(inserter));

      writer->commit();
    }

    // iterate over fields
    {
      auto reader = ir::directory_reader::open(dir(), codec());
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
      auto reader = ir::directory_reader::open(dir(), codec());
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
      auto reader = ir::directory_reader::open(dir(), codec());
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
      auto reader = ir::directory_reader::open(dir(), codec());
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

      auto reader = ir::directory_reader::open(dir(), codec());
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

      auto reader = ir::directory_reader::open(dir(), codec());
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
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);
      ASSERT_NE(nullptr, writer);

      auto inserter = [&field, &names](irs::index_writer::document& doc) {
        for (auto& name : names) {
          field.name_ = name;
          doc.insert(irs::action::store, field);
        }
        return false; // break the loop
      };

      ASSERT_TRUE(writer->insert(inserter));

      writer->commit();
    }

    // iterate over attributes
    {
      auto reader = ir::directory_reader::open(dir(), codec());
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
      auto reader = ir::directory_reader::open(dir(), codec());
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
      auto reader = ir::directory_reader::open(dir(), codec());
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
      auto reader = ir::directory_reader::open(dir(), codec());
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

      auto reader = ir::directory_reader::open(dir(), codec());
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

      auto reader = ir::directory_reader::open(dir(), codec());
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
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);

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
      auto reader = ir::directory_reader::open(dir(), codec());
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
          { 1, "A" }, { 2, "B" }, { 3, "C" }, { 4, "D" }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_value.first, actual_value.first);
          ASSERT_EQ(expected_value.second, actual_str_value);
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
          { 1, "abcd" }, { 4, "abcde" }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_value.first, actual_value.first);
          ASSERT_EQ(expected_value.second, actual_str_value);
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
        ASSERT_EQ(i, expected_values.size());
      }
    }

    // check inserted values:
    // - iterate (not cached)
    // - random read (cached)
    // - iterate (cached)
    {
      auto reader = ir::directory_reader::open(dir(), codec());
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
          { 1, "A" }, { 2, "B" }, { 3, "C" }, { 4, "D" }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_value.first, actual_value.first);
          ASSERT_EQ(expected_value.second, actual_str_value);
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
          { 1, "A" }, { 2, "B" }, { 3, "C" }, { 4, "D" }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_value.first, actual_value.first);
          ASSERT_EQ(expected_value.second, actual_str_value);
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
        ASSERT_EQ(i, expected_values.size());
      }

      {
        // iterate over 'prefix' column (not cached)
        auto column = segment.column_reader("prefix");
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
          { 1, "abcd" }, { 4, "abcde" }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_value.first, actual_value.first);
          ASSERT_EQ(expected_value.second, actual_str_value);
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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

        auto& actual_value = it->value();
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

        std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
          { 1, "abcd" }, { 4, "abcde" }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.second.c_str());

          ASSERT_EQ(expected_value.first, actual_value.first);
          ASSERT_EQ(expected_value.second, actual_str_value);
        }
        ASSERT_FALSE(it->next());
        ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
        ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
        ASSERT_EQ(i, expected_values.size());
      }
    }
  }

  void read_write_doc_attributes_big() {
    struct csv_doc_template_t: public tests::delim_doc_generator::doc_template {
      virtual void init() {
        clear();
        reserve(2);
        insert(std::make_shared<tests::templates::string_field>("id"));
        insert(std::make_shared<tests::templates::string_field>("label"));
      }

      virtual void value(size_t idx, const std::string& value) {
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
    tests::delim_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template, ',');
    size_t docs_count = 0;

    // write attributes 
    {
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);

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
      auto reader = ir::directory_reader::open(dir());
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
          ir::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id] (ir::doc_id_t id, const irs::bytes_ref& in) {
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

          ir::doc_id_t id = 0;
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
          ir::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id] (ir::doc_id_t id, const irs::bytes_ref& in) {
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
          ir::doc_id_t expected_id = 0;

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& actual_value = it->value();
          ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
          ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

          for (; it->next(); ) {
            ++expected_id;

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);

            const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

            ASSERT_EQ(expected_id, actual_value.first);
            ASSERT_EQ(field->value(), actual_value_str);
          }
          ASSERT_FALSE(it->next());
          ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
          ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
          ir::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id] (ir::doc_id_t id, const irs::bytes_ref& in) {
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

          ir::doc_id_t id = 0;
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
          ir::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id] (ir::doc_id_t id, const irs::bytes_ref& in) {
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
          ir::doc_id_t expected_id = 0;

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& actual_value = it->value();
          ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
          ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

          for (; it->next(); ) {
            ++expected_id;

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);

            const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

            ASSERT_EQ(expected_id, actual_value.first);
            ASSERT_EQ(field->value(), actual_value_str);
          }
          ASSERT_FALSE(it->next());
          ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
          ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
      auto reader = ir::directory_reader::open(dir());
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
          ir::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id] (ir::doc_id_t id, const irs::bytes_ref& in) {
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
          ir::doc_id_t expected_id = 0;

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& actual_value = it->value();
          ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
          ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

          for (; it->next(); ) {
            ++expected_id;

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);

            const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

            ASSERT_EQ(expected_id, actual_value.first);
            ASSERT_EQ(field->value(), actual_value_str);
          }
          ASSERT_FALSE(it->next());
          ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
          ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
          ASSERT_EQ(docs_count, expected_id);
        }

        // random access
        {
          const tests::document* doc = nullptr;
          irs::bytes_ref actual_value;
          auto column = segment.column_reader(meta->id);
          ASSERT_NE(nullptr, column);
          auto reader = column->values();

          ir::doc_id_t id = 0;
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
          ir::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id] (ir::doc_id_t id, const irs::bytes_ref& in) {
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
          ir::doc_id_t expected_id = 0;

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& actual_value = it->value();
          ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
          ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

          for (; it->next(); ) {
            ++expected_id;

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);

            const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

            ASSERT_EQ(expected_id, actual_value.first);
            ASSERT_EQ(field->value(), actual_value_str);
          }
          ASSERT_FALSE(it->next());
          ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
          ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
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
          ir::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id] (ir::doc_id_t id, const irs::bytes_ref& in) {
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
          ir::doc_id_t expected_id = 0;

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& actual_value = it->value();
          ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
          ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

          for (; it->next(); ) {
            ++expected_id;

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);

            const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

            ASSERT_EQ(expected_id, actual_value.first);
            ASSERT_EQ(field->value(), actual_value_str);
          }
          ASSERT_FALSE(it->next());
          ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
          ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
          ASSERT_EQ(docs_count, expected_id);
        }

        // random access
        {
          const tests::document* doc = nullptr;

          irs::bytes_ref actual_value;
          auto column = segment.column_reader(meta->id);
          ASSERT_NE(nullptr, column);
          auto reader = column->values();

          ir::doc_id_t id = 0;
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
          ir::doc_id_t expected_id = 0;
          auto visitor = [&gen, &column_name, &expected_id] (ir::doc_id_t id, const irs::bytes_ref& in) {
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
          ir::doc_id_t expected_id = 0;

          auto column = segment.column_reader(column_name);
          ASSERT_NE(nullptr, column);
          auto it = column->iterator();
          ASSERT_NE(nullptr, it);

          auto& actual_value = it->value();
          ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::invalid(), actual_value.first);
          ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);

          for (; it->next(); ) {
            ++expected_id;

            auto* doc = gen.next();
            auto* field = doc->stored.get<tests::templates::string_field>(column_name);
            ASSERT_NE(nullptr, field);

            const auto actual_value_str = irs::to_string<irs::string_ref>(actual_value.second.c_str());

            ASSERT_EQ(expected_id, actual_value.first);
            ASSERT_EQ(field->value(), actual_value_str);
          }
          ASSERT_FALSE(it->next());
          ASSERT_EQ(ir::type_limits<ir::type_t::doc_id_t>::eof(), actual_value.first);
          ASSERT_EQ(ir::bytes_ref::nil, actual_value.second);
          ASSERT_EQ(docs_count, expected_id);
        }
      }
    }
  }

  void insert_doc_with_null_empty_term() {
    class field {
      public:
      field(std::string&& name, const ir::string_ref& value)
        : name_(std::move(name)),
        value_(value) {}
      field(field&& other) NOEXCEPT
        : stream_(std::move(other.stream_)),
          name_(std::move(other.name_)),
          value_(std::move(other.value_)) {
      }
      ir::string_ref name() const { return name_; }
      float_t boost() const { return 1.f; }
      ir::token_stream& get_tokens() const {
        stream_.reset(value_);
        return stream_;
      }
      const ir::flags& features() const {
        return ir::flags::empty_instance();
      }

      private:
      mutable ir::string_token_stream stream_;
      std::string name_;
      ir::string_ref value_;
    }; // field

    // write docs with empty terms
    {
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);
      // doc0: empty, nullptr
      {
        std::vector<field> doc;
        doc.emplace_back(std::string("name"), ir::string_ref("", 0));
        doc.emplace_back(std::string("name"), ir::string_ref::nil);
        ASSERT_TRUE(insert(*writer, doc.begin(), doc.end()));
      }
      // doc1: nullptr, empty, nullptr
      {
        std::vector<field> doc;
        doc.emplace_back(std::string("name1"), ir::string_ref::nil);
        doc.emplace_back(std::string("name1"), ir::string_ref("", 0));
        doc.emplace_back(std::string("name"), ir::string_ref::nil);
        ASSERT_TRUE(insert(*writer, doc.begin(), doc.end()));
      }
      writer->commit();
    }

    // check fields with empty terms
    {
      auto reader = ir::directory_reader::open(dir());
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
      indexed_and_stored_field(std::string&& name, const ir::string_ref& value, bool stored_valid = true, bool indexed_valid = true)
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
      ir::string_ref name() const { return name_; }
      float_t boost() const { return 1.f; }
      ir::token_stream& get_tokens() const {
        stream_.reset(value_);
        return stream_;
      }
      const ir::flags& features() const {
        return features_;
      }
      bool write(ir::data_output& out) const NOEXCEPT {
        irs::write_string(out, value_);
        return stored_valid_;
      }

     private:
      ir::flags features_;
      mutable ir::string_token_stream stream_;
      std::string name_;
      ir::string_ref value_;
      bool stored_valid_;
    }; // indexed_and_stored_field

    class indexed_field {
     public:
      indexed_field(std::string&& name, const ir::string_ref& value, bool valid = true)
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
      ir::string_ref name() const { return name_; }
      float_t boost() const { return 1.f; }
      ir::token_stream& get_tokens() const {
        stream_.reset(value_);
        return stream_;
      }
      const ir::flags& features() const {
        return features_;
      }

     private:
      irs::flags features_;
      mutable ir::string_token_stream stream_;
      std::string name_;
      ir::string_ref value_;
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
    auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);

    size_t i = 0;
    const size_t max = 8;
    bool states[max];
    std::fill(std::begin(states), std::end(states), true);

    auto bulk_inserter = [&i, &max, &states](irs::index_writer::document& doc) mutable {
      auto& state = states[i];
      switch (i) {
        case 0: { // doc0
          indexed_field indexed("indexed", "doc0");
          state &= doc.insert(irs::action::index, indexed);
          stored_field stored("stored", "doc0");
          state &= doc.insert(irs::action::store, stored);
          indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc0");
          state &= doc.insert(irs::action::index_store, indexed_and_stored);
        } break;
        case 1: { // doc1
          // indexed and stored fields can be indexed/stored only
          indexed_and_stored_field indexed("indexed", "doc1");
          state &= doc.insert(irs::action::index, indexed);
          indexed_and_stored_field stored("stored", "doc1");
          state &= doc.insert(irs::action::store, stored);
        } break;
        case 2: { // doc2 (will be dropped since it contains invalid stored field)
          indexed_and_stored_field indexed("indexed", "doc2");
          state &= doc.insert(irs::action::index, indexed);
          stored_field stored("stored", "doc2", false);
          state &= doc.insert(irs::action::store, stored);
        } break;
        case 3: { // doc3 (will be dropped since it contains invalid indexed field)
          indexed_field indexed("indexed", "doc3", false);
          state &= doc.insert(irs::action::index, indexed);
          stored_field stored("stored", "doc3");
          state &= doc.insert(irs::action::store, stored);
        } break;
        case 4: { // doc4 (will be dropped since it contains invalid indexed and stored field)
          indexed_and_stored_field indexed_and_stored("indexed", "doc4", false, false);
          state &= doc.insert(irs::action::index_store, indexed_and_stored);
          stored_field stored("stored", "doc4");
          state &= doc.insert(irs::action::store, stored);
        } break;
        case 5: { // doc5 (will be dropped since it contains failed stored field)
          indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc5", false); // will fail on store, but will pass on index
          state &= doc.insert(irs::action::index_store, indexed_and_stored);
          stored_field stored("stored", "doc5");
          state &= doc.insert(irs::action::store, stored);
        } break;
        case 6: { // doc6 (will be dropped since it contains failed indexed field)
          indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc6", true, false); // will fail on index, but will pass on store
          state &= doc.insert(irs::action::index_store, indexed_and_stored);
          stored_field stored("stored", "doc6");
          state &= doc.insert(irs::action::store, stored);
        } break;
        case 7: { // valid insertion of last doc will mark bulk insert result as valid
          indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc7", true, true); // will be indexed, and will be stored
          state &= doc.insert(irs::action::index_store, indexed_and_stored);
          stored_field stored("stored", "doc7");
          state &= doc.insert(irs::action::store, stored);
        } break;
      }

      return ++i != max;
    };

    ASSERT_TRUE(writer->insert(bulk_inserter));
    ASSERT_TRUE(states[0]); // successfully inserted
    ASSERT_TRUE(states[1]); // successfully inserted
    ASSERT_FALSE(states[2]); // skipped
    ASSERT_FALSE(states[3]); // skipped
    ASSERT_FALSE(states[4]); // skipped
    ASSERT_FALSE(states[5]); // skipped
    ASSERT_FALSE(states[6]); // skipped
    ASSERT_TRUE(states[7]); // successfully inserted

    writer->commit();

    // check index
    {
      auto reader = ir::directory_reader::open(dir());
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

      override_sync_directory(ir::directory& impl, sync_f&& sync)
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
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);

      writer->commit();
    }

    // error while commiting index (during sync in index_meta_writer)
    {
      override_sync_directory override_dir(
        dir(), [this] (const ir::string_ref& name) {
          throw ir::io_error();
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

      auto writer = ir::index_writer::make(override_dir, codec(), ir::OM_APPEND);

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
      ASSERT_THROW(writer->commit(), ir::illegal_state);
    }

    // error while commiting index (during sync in index_writer)
    {
      override_sync_directory override_dir(
        dir(), [this] (const ir::string_ref& name) {
        if (ir::starts_with(name, "_")) {
          throw ir::io_error();
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

      auto writer = ir::index_writer::make(override_dir, codec(), ir::OM_APPEND);

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
      ASSERT_THROW(writer->commit(), ir::io_error);
    }

    // check index, it should be empty 
    {
      auto reader = ir::directory_reader::open(dir(), codec());
      ASSERT_EQ(0, reader.live_docs_count());
      ASSERT_EQ(0, reader.docs_count());
      ASSERT_EQ(0, reader.size());
      ASSERT_EQ(reader.begin(), reader.end());
    }
  }
}; // index_test_case

class memory_test_case_base : public index_test_case_base {
protected:
  virtual ir::directory* get_directory() override {
    return new ir::memory_directory();
  }

  virtual ir::format::ptr get_codec() override {
    return ir::formats::get("1_0");
  }
}; // memory_test_case_base

class fs_test_case_base : public index_test_case_base { 
protected:
  virtual void SetUp() override {
    index_test_case_base::SetUp();
    MSVC_ONLY(_setmaxstdio(2048)); // workaround for error: EMFILE - Too many open files
  }

  virtual ir::directory* get_directory() override {
    const fs::path dir = fs::path( test_dir() ).append( "index" );
    return new iresearch::fs_directory(dir.string());
  }

  virtual ir::format::ptr get_codec() override {
    return ir::formats::get("1_0");
  }
}; // fs_test_case_base

class mmap_test_case_base : public index_test_case_base {
protected:
  virtual void SetUp() override {
    index_test_case_base::SetUp();
    MSVC_ONLY(_setmaxstdio(2048)); // workaround for error: EMFILE - Too many open files
  }

  virtual ir::directory* get_directory() override {
    const fs::path dir = fs::path( test_dir() ).append( "index" );
    return new iresearch::mmap_directory(dir.string());
  }

  virtual ir::format::ptr get_codec() override {
    return ir::formats::get("1_0");
  }
}; // fs_test_case_base

namespace cases {

template<typename Base>
class tfidf : public Base {
 public:
  using Base::assert_index;
  using Base::get_codec;
  using Base::get_directory;

  void assert_index(size_t skip = 0) const {
    assert_index(ir::flags(), skip);
    assert_index(ir::flags{ ir::document::type() }, skip);
    assert_index(ir::flags{ ir::document::type(), ir::frequency::type() }, skip);
    assert_index(ir::flags{ ir::document::type(), ir::frequency::type(), ir::position::type() }, skip);
    assert_index(ir::flags{ ir::document::type(), ir::frequency::type(), ir::position::type(), ir::offset::type() }, skip);
    assert_index(ir::flags{ ir::document::type(), ir::frequency::type(), ir::position::type(), ir::payload::type() }, skip);
    assert_index(ir::flags{ ir::document::type(), ir::frequency::type(), ir::position::type(), ir::payload::type(), ir::offset::type() }, skip);
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
  read_write_doc_attributes_sparse_variable_length();
  read_write_doc_attributes_sparse_mask();
  read_write_doc_attributes_dense_variable_length();
  read_write_doc_attributes_dense_fixed_length();
  read_write_doc_attributes_dense_mask();
  read_write_doc_attributes_big();
  read_write_doc_attributes();
  read_empty_doc_attributes();
}

TEST_F(memory_index_test, clear_writer) {
  clear_writer();
}

TEST_F(memory_index_test, open_writer) {
  open_writer_check_lock();
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
        ir::string_ref(name),
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
      writer->remove(std::move(query_doc1.filter));
    });

    thread0.join();
    thread1.join();
    thread2.join();
    writer->commit();

    std::unordered_set<irs::string_ref> expected = { "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "~", "!", "@", "#", "$", "%" };
    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_TRUE(reader.size() == 1 || reader.size() == 2); // can be 1 if thread0 finishes before thread1 starts
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

TEST_F(memory_index_test, doc_removal) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        ir::string_ref(name),
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
    writer->remove(*(query_doc1.filter.get()));
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
    writer->remove(std::move(query_doc1.filter));
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
    writer->remove(std::shared_ptr<iresearch::filter>(std::move(query_doc1.filter)));
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
    writer->remove(std::move(query_doc2.filter)); // not present yet
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
    writer->remove(std::move(query_doc1.filter));
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
    writer->remove(std::move(query_doc3.filter));
    writer->commit(); // document mask with 'doc3' created
    writer->remove(std::move(query_doc2.filter));
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
    writer->remove(std::move(query_doc1_doc2.filter));
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
    writer->remove(std::move(query_doc2.filter));
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
    writer->remove(std::move(query_doc1_doc3.filter));
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
    writer->remove(std::move(query_doc4.filter));
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
    writer->remove(std::move(query_doc3_doc7.filter));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc8->indexed.begin(), doc8->indexed.end(),
      doc8->stored.begin(), doc8->stored.end()
    )); // H
    ASSERT_TRUE(insert(*writer,
      doc9->indexed.begin(), doc9->indexed.end(),
      doc9->stored.begin(), doc9->stored.end()
    )); // I
    writer->remove(std::move(query_doc2_doc6_doc9.filter));
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
        ir::string_ref(name),
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
    writer->remove(*(query_doc2.filter)); // remove no longer existent
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
    writer->remove(*(query_doc2.filter)); // remove no longer existent
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
    writer->remove(*(query_doc2.filter));
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
    writer->remove(*(query_doc2.filter));
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
    writer->remove(*(query_doc2.filter));
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
    writer->remove(*(query_doc2.filter));
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
}

TEST_F(memory_index_test, import_reader) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        ir::string_ref(name),
        data.str
      ));
    }
  });

  irs::bytes_ref actual_value;

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  // add a reader with 1 full segment
  {
    iresearch::memory_directory data_dir;
    auto data_writer = iresearch::index_writer::make(data_dir, codec(), iresearch::OM_CREATE);
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
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
    data_writer->remove(std::move(query_doc1.filter));
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
    data_writer->remove(std::move(query_doc2_doc3.filter));
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
    data_writer->remove(std::move(query_doc4.filter));
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
    writer->remove(std::move(query_doc2.filter)); // should not match any documents
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

TEST_F(memory_index_test, profile_bulk_index_singlethread_full_mt) {
  profile_bulk_index(0, 0, 0, 0);
}

TEST_F(memory_index_test, profile_bulk_index_singlethread_batched_mt) {
  profile_bulk_index(0, 0, 0, 10000);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_cleanup_mt) {
  profile_bulk_index_dedicated_cleanup(16, 10000, 100);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_consolidate_mt) {
  // a lot of threads cause a lot of contention for the segment pool
  // small consolidate_interval causes too many policies to be added and slows down test
  profile_bulk_index_dedicated_consolidate(8, 10000, 5000);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_dedicated_commit_mt) {
  profile_bulk_index_dedicated_commit(16, 1, 1000);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_full_mt) {
  profile_bulk_index(16, 0, 0, 0);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_batched_mt) {
  profile_bulk_index(16, 0, 0, 10000);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_import_full_mt) {
  profile_bulk_index(12, 4, 0, 0);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_import_batched_mt) {
  profile_bulk_index(12, 4, 0, 10000);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_import_update_full_mt) {
  profile_bulk_index(9, 7, 5, 0); // 5 does not divide evenly into 9 or 7
}

TEST_F(memory_index_test, profile_bulk_index_multithread_import_update_batched_mt) {
  profile_bulk_index(9, 7, 5, 10000); // 5 does not divide evenly into 9 or 7
}

TEST_F(memory_index_test, profile_bulk_index_multithread_update_full_mt) {
  profile_bulk_index(16, 0, 5, 0); // 5 does not divide evenly into 16
}

TEST_F(memory_index_test, profile_bulk_index_multithread_update_batched_mt) {
  profile_bulk_index(16, 0, 5, 10000); // 5 does not divide evenly into 16
}

TEST_F(memory_index_test, refresh_reader) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        ir::string_ref(name),
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
    auto writer = open_writer(ir::OPEN_MODE::OM_APPEND);
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());

    writer->remove(std::move(query_doc2.filter));
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
    auto writer = open_writer(ir::OPEN_MODE::OM_APPEND);

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
    auto writer = open_writer(ir::OPEN_MODE::OM_APPEND);
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());

    writer->remove(std::move(query_doc1.filter));
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
    auto merge_all = [](const iresearch::directory& dir, const iresearch::index_meta& meta)->iresearch::index_writer::consolidation_acceptor_t {
      return [](const iresearch::segment_meta& meta)->bool { return true; };
    };

    writer->consolidate(merge_all, true);
    writer->commit();
  }
}

TEST_F(memory_index_test, segment_consolidate) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          ir::string_ref(name),
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

  auto always_merge = [](const iresearch::directory& dir, const iresearch::index_meta& meta)->iresearch::index_writer::consolidation_acceptor_t {
    return [](const iresearch::segment_meta& meta)->bool { return true; };
  };
  auto all_features = ir::flags{ ir::document::type(), ir::frequency::type(), ir::position::type(), ir::payload::type(), ir::offset::type() };

  // remove empty new segment
  {
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto writer = open_writer();

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->remove(std::move(query_doc1.filter));
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
    writer->remove(std::move(query_doc1.filter));
    writer->commit();

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(0, reader.size());
  }

  // remove empty old, defragment new (deferred)
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
    writer->remove(std::move(query_doc1_doc2.filter));
    writer->consolidate(always_merge, false);
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

  // remove empty old, defragment new (immediate)
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
    writer->remove(std::move(query_doc1_doc2.filter));
    writer->commit();
    writer->consolidate(always_merge, true);
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

  // remove empty old, defragment old (deferred)
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
    writer->remove(std::move(query_doc1_doc2.filter));
    writer->consolidate(always_merge, false);
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

  // remove empty old, defragment old (immediate)
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
    writer->remove(std::move(query_doc1_doc2.filter));
    writer->commit();
    writer->consolidate(always_merge, true);
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

  // do defragment old segment with uncommited removal (i.e. do not consider uncomitted removals)
  {
    auto merge_if_masked = [](const iresearch::directory& dir, const iresearch::index_meta& meta)->iresearch::index_writer::consolidation_acceptor_t {
      return [&dir](const iresearch::segment_meta& meta)->bool { 
        bool seen;
        return meta.codec->get_document_mask_reader()->prepare(dir, meta, &seen) && seen;
      };
    };
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
    writer->remove(std::move(query_doc1.filter));
    writer->consolidate(merge_if_masked, false);
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
    auto merge_if_masked = [](const iresearch::directory& dir, const iresearch::index_meta& meta)->iresearch::index_writer::consolidation_acceptor_t {
      return [&dir](const iresearch::segment_meta& meta)->bool { 
        bool seen;
        return meta.codec->get_document_mask_reader()->prepare(dir, meta, &seen) && seen;
      };
    };
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
    writer->remove(std::move(query_doc1.filter));
    writer->consolidate(merge_if_masked, true);
    writer->commit();

    {
      auto reader = iresearch::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = reader[0]; // assume 0 is id of first/only segment
      ASSERT_EQ(2, segment.docs_count()); // total count of documents
    }

    writer->consolidate(merge_if_masked, true); // previous removal now committed and considered
    writer->commit();

    {
      auto reader = iresearch::directory_reader::open(dir(), codec());
      ASSERT_EQ(1, reader.size());
      auto& segment = reader[0]; // assume 0 is id of first/only segment
      ASSERT_EQ(1, segment.docs_count()); // total count of documents
    }
  }

  // merge new+old segment (defragment deferred)
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
    writer->remove(std::move(query_doc1_doc3.filter));
    writer->consolidate(always_merge, false);
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

  // merge new+old segment (defragment immediate)
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
    writer->remove(std::move(query_doc1_doc3.filter));
    writer->commit();
    writer->consolidate(always_merge, true);
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

  // merge old+old segment (defragment deferred)
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
    writer->remove(std::move(query_doc1_doc3.filter));
    writer->consolidate(always_merge, false);
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

  // merge old+old segment (defragment immediate)
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
    writer->remove(std::move(query_doc1_doc3.filter));
    writer->commit();
    writer->consolidate(always_merge, true);
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

  // merge old+old+old segment (defragment deferred)
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
    writer->remove(std::move(query_doc1_doc3_doc5.filter));
    writer->consolidate(always_merge, false);
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

  // merge old+old+old segment (defragment immediate)
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
    writer->remove(std::move(query_doc1_doc3_doc5.filter));
    writer->commit();
    writer->consolidate(always_merge, true);
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

  // merge two segments with different fields (defragment deferred)
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
            ir::string_ref(name),
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
    writer->consolidate(always_merge, false);
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

  // merge two segments with different fields (defragment immediate)
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
            ir::string_ref(name),
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
    writer->consolidate(std::move(always_merge), true);
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
        ir::string_ref(name),
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
    writer->consolidate(iresearch::index_utils::consolidate_bytes(1), true); // value garanteeing merge
    writer->commit();

    std::unordered_set<irs::string_ref> expectedName = { "A", "B", "C", "D", "E" };

    auto reader = iresearch::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
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
    writer->consolidate(iresearch::index_utils::consolidate_bytes(0), true); // value garanteeing non-merge
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
    writer->consolidate(iresearch::index_utils::consolidate_bytes_accum(1), true); // value garanteeing merge
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
    writer->consolidate(iresearch::index_utils::consolidate_bytes_accum(0), true); // value garanteeing non-merge
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
    auto query_doc2_doc3 = iresearch::iql::query_builder().build("name==B||name==C", std::locale::classic());
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
    writer->remove(std::move(query_doc2_doc3.filter));
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));
    writer->commit();
    writer->consolidate(iresearch::index_utils::consolidate_count(1), true); // value garanteeing merge
    writer->commit();

    std::unordered_set<irs::string_ref> expectedName = { "A", "D", "E" };
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
    writer->remove(std::move(query_doc2_doc3_doc4.filter));
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));
    writer->commit();
    writer->consolidate(iresearch::index_utils::consolidate_count(0), true); // value garanteeing non-merge
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
    writer->remove(std::move(query_doc2_doc4.filter));
    writer->commit();
    writer->consolidate(iresearch::index_utils::consolidate_fill(1), true); // value garanteeing merge
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
    writer->remove(std::move(query_doc2_doc4.filter));
    writer->commit();
    writer->consolidate(iresearch::index_utils::consolidate_fill(0), true); // value garanteeing non-merge
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
  read_write_doc_attributes_sparse_variable_length();
  read_write_doc_attributes_sparse_mask();
  read_write_doc_attributes_dense_variable_length();
  read_write_doc_attributes_dense_fixed_length();
  read_write_doc_attributes_dense_mask();
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
  auto writer = open_writer();

  ASSERT_TRUE(insert(*writer,
    doc->indexed.begin(), doc->indexed.end(),
    doc->stored.begin(), doc->stored.end()
  ));
  writer->commit();
  writer->close();

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

TEST_F(fs_index_test, profile_bulk_index_singlethread_full_mt) {
  profile_bulk_index(0, 0, 0, 0);
}

TEST_F(fs_index_test, profile_bulk_index_singlethread_batched_mt) {
  profile_bulk_index(0, 0, 0, 10000);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_cleanup_mt) {
  profile_bulk_index_dedicated_cleanup(16, 10000, 100);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_consolidate_mt) {
  // a lot of threads cause a lot of contention for the segment pool
  // small consolidate_interval causes too many policies to be added and slows down test
  profile_bulk_index_dedicated_consolidate(8, 10000, 5000);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_dedicated_commit_mt) {
  profile_bulk_index_dedicated_commit(16, 1, 1000);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_full_mt) {
  profile_bulk_index(16, 0, 0, 0);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_batched_mt) {
  profile_bulk_index(16, 0, 0, 10000);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_import_full_mt) {
  profile_bulk_index(12, 4, 0, 0);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_import_batched_mt) {
  profile_bulk_index(12, 4, 0, 10000);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_import_update_full_mt) {
  profile_bulk_index(9, 7, 5, 0); // 5 does not divide evenly into 9 or 7
}

TEST_F(fs_index_test, profile_bulk_index_multithread_import_update_batched_mt) {
  profile_bulk_index(9, 7, 5, 10000); // 5 does not divide evenly into 9 or 7
}

TEST_F(fs_index_test, profile_bulk_index_multithread_update_full_mt) {
  profile_bulk_index(16, 0, 5, 0); // 5 does not divide evenly into 16
}

TEST_F(fs_index_test, profile_bulk_index_multithread_update_batched_mt) {
  profile_bulk_index(16, 0, 5, 10000); // 5 does not divide evenly into 16
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
  read_write_doc_attributes_sparse_variable_length();
  read_write_doc_attributes_sparse_mask();
  read_write_doc_attributes_dense_variable_length();
  read_write_doc_attributes_dense_fixed_length();
  read_write_doc_attributes_dense_mask();
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
  auto writer = open_writer();

  ASSERT_TRUE(insert(*writer,
    doc->indexed.begin(), doc->indexed.end(),
    doc->stored.begin(), doc->stored.end()
  ));
  writer->commit();
  writer->close();

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

TEST_F(mmap_index_test, profile_bulk_index_singlethread_full_mt) {
  profile_bulk_index(0, 0, 0, 0);
}

TEST_F(mmap_index_test, profile_bulk_index_singlethread_batched_mt) {
  profile_bulk_index(0, 0, 0, 10000);
}

TEST_F(mmap_index_test, profile_bulk_index_multithread_cleanup_mt) {
  profile_bulk_index_dedicated_cleanup(16, 10000, 100);
}

TEST_F(mmap_index_test, profile_bulk_index_multithread_consolidate_mt) {
  // a lot of threads cause a lot of contention for the segment pool
  // small consolidate_interval causes too many policies to be added and slows down test
  profile_bulk_index_dedicated_consolidate(8, 10000, 5000);
}

TEST_F(mmap_index_test, profile_bulk_index_multithread_dedicated_commit_mt) {
  profile_bulk_index_dedicated_commit(16, 1, 1000);
}

TEST_F(mmap_index_test, profile_bulk_index_multithread_full_mt) {
  profile_bulk_index(16, 0, 0, 0);
}

TEST_F(mmap_index_test, profile_bulk_index_multithread_batched_mt) {
  profile_bulk_index(16, 0, 0, 10000);
}

TEST_F(mmap_index_test, profile_bulk_index_multithread_import_full_mt) {
  profile_bulk_index(12, 4, 0, 0);
}

TEST_F(mmap_index_test, profile_bulk_index_multithread_import_batched_mt) {
  profile_bulk_index(12, 4, 0, 10000);
}

TEST_F(mmap_index_test, profile_bulk_index_multithread_import_update_full_mt) {
  profile_bulk_index(9, 7, 5, 0); // 5 does not divide evenly into 9 or 7
}

TEST_F(mmap_index_test, profile_bulk_index_multithread_import_update_batched_mt) {
  profile_bulk_index(9, 7, 5, 10000); // 5 does not divide evenly into 9 or 7
}

TEST_F(mmap_index_test, profile_bulk_index_multithread_update_full_mt) {
  profile_bulk_index(16, 0, 5, 0); // 5 does not divide evenly into 16
}

TEST_F(mmap_index_test, profile_bulk_index_multithread_update_batched_mt) {
  profile_bulk_index(16, 0, 5, 10000); // 5 does not divide evenly into 16
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------