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

#include "tests_shared.hpp" 
#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"
#include "document/field.hpp"
#include "iql/query_builder.hpp"
#include "formats/formats_10.hpp"
#include "search/filter.hpp"
#include "search/term_filter.hpp"
#include "store/fs_directory.hpp"
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
  DECLARE_FACTORY_DEFAULT();

  incompatible_attribute() NOEXCEPT;

  virtual void clear() override { }
};

REGISTER_ATTRIBUTE(incompatible_attribute);
DEFINE_ATTRIBUTE_TYPE(incompatible_attribute);
DEFINE_FACTORY_DEFAULT(incompatible_attribute);

incompatible_attribute::incompatible_attribute() NOEXCEPT
  : irs::attribute(incompatible_attribute::type()) {
}

struct directory_mock : public ir::directory {
  directory_mock(ir::directory& impl) : impl_(impl) {}
  using directory::attributes;
  virtual iresearch::attributes& attributes() NOEXCEPT override {
    return impl_.attributes();
  }
  virtual void close() NOEXCEPT override {
    impl_.close();
  }
  virtual ir::index_output::ptr create(
    const std::string& name
  ) NOEXCEPT override {
    return impl_.create(name);
  }
  virtual bool exists(
    bool& result, const std::string& name
  ) const NOEXCEPT override {
    return impl_.exists(result, name);
  }
  virtual bool length(
    uint64_t& result, const std::string& name
  ) const NOEXCEPT override {
    return impl_.length(result, name);
  }
  virtual bool visit(const ir::directory::visitor_f& visitor) const override {
    return impl_.visit(visitor);
  }
  virtual ir::index_lock::ptr make_lock(
    const std::string& name
  ) NOEXCEPT override {
    return impl_.make_lock(name);
  }
  virtual bool mtime(
    std::time_t& result, const std::string& name
  ) const NOEXCEPT override {
    return impl_.mtime(result, name);
  }
  virtual ir::index_input::ptr open(
    const std::string& name
  ) const NOEXCEPT override {
    return impl_.open(name);
  }
  virtual bool remove(const std::string& name) NOEXCEPT override {
    return impl_.remove(name);
  }
  virtual bool rename(
    const std::string& src, const std::string& dst
  ) NOEXCEPT override {
    return impl_.rename(src, dst);
  }
  virtual bool sync(const std::string& name) NOEXCEPT override {
    return impl_.sync(name);
  }

 private:
  ir::directory& impl_;
}; // directory_mock 

namespace templates {

// ----------------------------------------------------------------------------
// --SECTION--                               token_stream_payload implemntation
// ----------------------------------------------------------------------------

token_stream_payload::token_stream_payload(ir::token_stream* impl)
  : impl_(impl) {
    assert(impl_);
    ir::attributes& attrs = const_cast<ir::attributes&>(impl_->attributes());
    term_ = attrs.get<ir::term_attribute>();
    assert(term_);
    pay_ = attrs.add<ir::payload>();
}

bool token_stream_payload::next() {
  if (impl_->next()) {
    pay_->value = term_->value();
    return true;
  }
  pay_->value = ir::bytes_ref::nil;
  return false;
}

// ----------------------------------------------------------------------------
// --SECTION--                                       string_field implemntation
// ----------------------------------------------------------------------------

string_field::string_field(const ir::string_ref& name) {
  this->name(name);
}

string_field::string_field(
    const ir::string_ref& name, 
    const ir::string_ref& value) 
  : value_(value) {
  this->name(name);
}

const ir::flags& string_field::features() const {
  static ir::flags features{ ir::frequency::type(), ir::position::type() };
  return features;
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

//////////////////////////////////////////////////////////////////////////////
/// @class europarl_doc_template
/// @brief document template for europarl.subset.text
//////////////////////////////////////////////////////////////////////////////
class europarl_doc_template : public delim_doc_generator::doc_template {
 public:
  typedef templates::text_field<ir::string_ref> text_field;

  virtual void init() {
    clear();
    indexed.push_back(std::make_shared<tests::templates::string_field>("title"));
    indexed.push_back(std::make_shared<text_field>("title_anl", false));
    indexed.push_back(std::make_shared<text_field>("title_anl_pay", true));
    indexed.push_back(std::make_shared<text_field>("body_anl", false));
    indexed.push_back(std::make_shared<text_field>("body_anl_pay", true));
    {
      insert(std::make_shared<tests::long_field>());
      auto& field = static_cast<tests::long_field&>(indexed.back());
      field.name(ir::string_ref("date"));
    }
    insert(std::make_shared<tests::templates::string_field>("datestr"));
    insert(std::make_shared<tests::templates::string_field>("body"));
    {
      insert(std::make_shared<tests::int_field>());
      auto& field = static_cast<tests::int_field&>(indexed.back());
      field.name(ir::string_ref("id"));
    }
    insert(std::make_shared<string_field>("idstr"));
  }

  virtual void value(size_t idx, const std::string& value) {
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

  virtual void end() {
    ++idval_;
    indexed.get<tests::int_field>("id")->value(idval_);
    indexed.get<tests::templates::string_field>("idstr")->value(std::to_string(idval_));
  }

  virtual void reset() {
    idval_ = 0;
  }

 private:
  std::string title_; // current title
  std::string body_; // current body
  ir::doc_id_t idval_ = 0;
}; // europarl_doc_template

} // templates

void generic_json_field_factory(
    tests::document& doc,
    const std::string& name,
    const tests::json::json_value& data) {
  if (data.quoted) {
    doc.insert(std::make_shared<templates::string_field>(
      ir::string_ref(name),
      ir::string_ref(data.value)
    ));
  } else if ("null" == data.value) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(ir::null_token_stream::value_null());
  } else if ("true" == data.value) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(ir::boolean_token_stream::value_true());
  } else if ("false" == data.value) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(ir::boolean_token_stream::value_true());
  } else {
    char* czSuffix;
    double dValue = strtod(data.value.c_str(), &czSuffix);

    // 'value' can be interpreted as a double
    if (!czSuffix[0]) {
      doc.insert(std::make_shared<tests::double_field>());
      auto& field = (doc.indexed.end() - 1).as<tests::double_field>();
      field.name(iresearch::string_ref(name));
      field.value(dValue);
    }
  }
}

void payloaded_json_field_factory(
    tests::document& doc,
    const std::string& name,
    const tests::json::json_value& data) {
  typedef templates::text_field<std::string> text_field;

  if (data.quoted) {
    // analyzed && pyaloaded
    doc.indexed.push_back(std::make_shared<text_field>(
      std::string(name.c_str()) + "_anl_pay",
      ir::string_ref(data.value),
      true
    ));

    // analyzed field
    doc.indexed.push_back(std::make_shared<text_field>(
      std::string(name.c_str()) + "_anl",
      ir::string_ref(data.value)
    ));

    // not analyzed field
    doc.insert(std::make_shared<templates::string_field>(
      ir::string_ref(name),
      ir::string_ref(data.value)
    ));
  } else if ("null" == data.value) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(ir::null_token_stream::value_null());
  } else if ("true" == data.value) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(ir::boolean_token_stream::value_true());
  } else if ("false" == data.value) {
    doc.insert(std::make_shared<tests::binary_field>());
    auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
    field.name(iresearch::string_ref(name));
    field.value(ir::boolean_token_stream::value_false());
  } else {
    char* czSuffix;
    double dValue = strtod(data.value.c_str(), &czSuffix);
    if (!czSuffix[0]) {
      // 'value' can be interpreted as a double
      doc.insert(std::make_shared<tests::double_field>());
      auto& field = (doc.indexed.end() - 1).as<tests::double_field>();
      field.name(iresearch::string_ref(name));
      field.value(dValue);
    }
  }
}

class index_test_case_base : public tests::index_test_base {
 public:
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

                auto& actual_offs = actual_pos->get<irs::offset>();
                auto& expected_offs = expected_pos->get<irs::offset>();
                ASSERT_FALSE(!actual_offs);
                ASSERT_FALSE(!expected_offs);

                auto& actual_pay = actual_pos->get<irs::payload>();
                auto& expected_pay = expected_pos->get<irs::payload>();
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

  void profile_bulk_index(size_t num_insert_threads, size_t num_import_threads, size_t num_update_threads, size_t batch_size, ir::index_writer::ptr writer = nullptr, std::atomic<size_t>* commit_count = nullptr) {
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

          while (import_again.load()) {
            import_docs_count += import_reader.docs_count();

            {
              REGISTER_TIMER_NAMED_DETAILED("import");
              writer->import(import_reader);
            }

            ++writer_import_count;
            std::this_thread::sleep_for(std::chrono::milliseconds(import_interval));
          }
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
          tests::delim_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template, ',');size_t doc_skip = update_skip;
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
      reader[i].visit("same", imported_visitor); // field present in all docs from simple_sequential.json
      reader[i].visit("updated", updated_visitor); // field insterted by updater threads
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
      [] (tests::document& doc, const std::string& name, const tests::json::json_value& data) {
        if (data.quoted) {
          doc.insert(std::make_shared<templates::string_field>(
            ir::string_ref(name),
            ir::string_ref(data.value)
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
          auto values = segment.values("name");
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
          auto values = segment.values("name");
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
      auto values = segment.values("name");
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
          auto values = segment.values("name");
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

      const auto thread_count = 10;
      std::vector<int> results(thread_count, 0);
      std::vector<std::thread> pool;

      for (size_t i = 0; i < thread_count; ++i) {
        auto& result = results[i];
        pool.emplace_back(std::thread([&result, &read_columns] () {
          result = static_cast<int>(read_columns());
        }));
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
      while (doc = gen.next()) {
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

        return segment.visit(meta->id, visitor);
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

        auto reader = segment.values(meta->id);
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

      const auto thread_count = 5;
      std::vector<int> results(thread_count, 0);
      std::vector<std::thread> pool;

      const iresearch::string_ref id_column = "id";
      const iresearch::string_ref label_column = "label";
      auto i = 0;
      for (auto max = thread_count/2; i < max; ++i) {
        auto& result = results[i];
        auto& column_name = i % 2 ? id_column : label_column;
        pool.emplace_back(std::thread([&result, &visit_column, column_name] () {
          result = static_cast<int>(visit_column(column_name));
        }));
      }

      ir::doc_id_t skip = 0;
      for (; i < thread_count; ++i) {
        auto& result = results[i];
        auto& column_name = i % 2 ? id_column : label_column;
        pool.emplace_back(std::thread([&result, &read_column_offset, column_name, skip] () {
          result = static_cast<int>(read_column_offset(column_name, skip));
        }));
        skip += 10000;
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

    // read attributes
    {
      irs::bytes_ref value;
      auto column = segment.values("name");
      ASSERT_FALSE(column(0, value));
      ASSERT_FALSE(column(1, value));
      ASSERT_FALSE(column(2, value));
      ASSERT_FALSE(column(3, value));
    }
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
          doc.insert<irs::Action::STORE>(field);
        }
        return ++docs_count < MAX_DOCS;
      };

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      writer->insert(inserter); // insert MAX_DOCS/2 documents
      writer->commit();
    }

    // check inserted values
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

        ASSERT_TRUE(segment.visit(column_name, visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }

      // read values
      {
        irs::columnstore_reader::values_reader_f values = segment.values(column_name);
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

      // visit values
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

        ASSERT_TRUE(segment.visit(column_name, visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
      }
    }
  }

  void read_write_doc_attributes_dense_mask() {
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
        doc.insert<irs::Action::STORE>(field);
        return ++docs_count < MAX_DOCS;
      };

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      writer->insert(inserter); // insert MAX_DOCS documents
      writer->commit();
    }

    // check inserted values
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

        ASSERT_TRUE(segment.visit(column_name, visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }

      // read values
      {
        irs::columnstore_reader::values_reader_f values = segment.values(column_name);
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

        ASSERT_TRUE(segment.visit(column_name, visitor));
        ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
      }
    }
  }

  void read_write_doc_attributes_fixed_length() {
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
        doc.insert<irs::Action::STORE>(field);
        return ++field.value < MAX_DOCS;
      };

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      writer->insert(inserter); // insert MAX_DOCS documents
      writer->commit();
    }

    // check inserted values
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

        ASSERT_TRUE(segment.visit(column_name, visitor));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        irs::columnstore_reader::values_reader_f values = segment.values(column_name);

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

        ASSERT_TRUE(segment.visit(column_name, visitor));
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
        doc.insert<irs::Action::STORE>(field);
        return ++field.value < MAX_DOCS;
      };

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      writer->insert(inserter); // insert MAX_DOCS documents
      writer->commit();
    }

    // check inserted values
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

        ASSERT_TRUE(segment.visit(column_name, visitor));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        irs::columnstore_reader::values_reader_f values = segment.values(column_name);

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

        ASSERT_TRUE(segment.visit(column_name, visitor));
      }
    }
  }

  void read_write_doc_attributes_sparse_variable_length() {
    static const irs::doc_id_t MAX_DOCS = 1500;
    static const iresearch::string_ref column_name = "id";

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

      auto inserter = [&field](const irs::index_writer::document& doc) {
        if (field.value % 2 ) {
          doc.insert<irs::Action::STORE>(field);
        }
        return ++field.value < MAX_DOCS;
      };

      auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
      writer->insert(inserter); // insert MAX_DOCS documents
      writer->commit();
    }

    // check inserted values
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

        ASSERT_TRUE(segment.visit(column_name, visitor));
      }

      // read values
      {
        irs::bytes_ref actual_value;
        irs::columnstore_reader::values_reader_f values = segment.values(column_name);

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

        ASSERT_TRUE(segment.visit(column_name, visitor));
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

    auto reader = ir::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());

    // read attribute from invalid column
    {
      irs::bytes_ref actual_value;
      auto value_reader = segment.values("invalid_column");
      ASSERT_FALSE(value_reader(0, actual_value));
      ASSERT_FALSE(value_reader(1, actual_value));
      ASSERT_FALSE(value_reader(2, actual_value));
      ASSERT_FALSE(value_reader(3, actual_value));
      ASSERT_FALSE(value_reader(4, actual_value));
    }

    // read attributes from 'name' column (dense)
    {
      irs::bytes_ref actual_value;
      auto value_reader = segment.values("name");
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

    // read attributes from 'prefix' column (sparse)
    {
      irs::bytes_ref actual_value;
      auto value_reader = segment.values("prefix");
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

    // write attributes 
    {
      auto writer = ir::index_writer::make(dir(), codec(), ir::OM_CREATE);

      const tests::document* doc;
      while (doc = gen.next()) {
        ASSERT_TRUE(insert(*writer, doc->indexed.end(), doc->indexed.end(), doc->stored.begin(), doc->stored.end()));
      }
      writer->commit();
    }

    // read attributes
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

          ASSERT_TRUE(segment.visit(meta->id, visitor));
        }

        // random access
        {
          const tests::document* doc = nullptr;
          irs::bytes_ref actual_value;
          auto reader = segment.values(meta->id);

          ir::doc_id_t id = 0;
          gen.reset();
          while (doc = gen.next()) {
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

          ASSERT_TRUE(segment.visit(meta->id, visitor));
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

          ASSERT_TRUE(segment.visit(meta->id, visitor));
        }

        // random access
        {
          const tests::document* doc = nullptr;

          irs::bytes_ref actual_value;
          auto reader = segment.values(meta->id);

          ir::doc_id_t id = 0;
          while (doc = gen.next()) {
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

          ASSERT_TRUE(segment.visit(meta->id, visitor));
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
    const size_t max = 7;
    bool states[max];
    std::fill(std::begin(states), std::end(states), true);

    auto bulk_inserter = [&i, &max, &states](irs::index_writer::document& doc) mutable {
      auto& state = states[i];
      switch (i) {
        case 0: { // doc0
          indexed_field indexed("indexed", "doc0");
          state &= doc.insert<irs::Action::INDEX>(indexed);
          stored_field stored("stored", "doc0");
          state &= doc.insert<irs::Action::STORE>(stored);
          indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc0");
          state &= doc.insert<irs::Action::INDEX_STORE>(indexed_and_stored);
        } break;
        case 1: { // doc1
          // indexed and stored fields can be indexed/stored only
          indexed_and_stored_field indexed("indexed", "doc1");
          state &= doc.insert<irs::Action::INDEX>(indexed);
          indexed_and_stored_field stored("stored", "doc1");
          state &= doc.insert<irs::Action::STORE>(stored);
        } break;
        case 2: { // doc2 (will be dropped since it contains invalid stored field)
          indexed_and_stored_field indexed("indexed", "doc2");
          state &= doc.insert<irs::Action::INDEX>(indexed);
          stored_field stored("stored", "doc2", false);
          state &= doc.insert<irs::Action::STORE>(stored);
        } break;
        case 3: { // doc3 (will be dropped since it contains invalid indexed field)
          indexed_field indexed("indexed", "doc3", false);
          state &= doc.insert<irs::Action::INDEX>(indexed);
          stored_field stored("stored", "doc3");
          state &= doc.insert<irs::Action::STORE>(stored);
        } break;
        case 4: { // doc4 (will be dropped since it contains invalid indexed and stored field)
          indexed_and_stored_field indexed_and_stored("indexed", "doc4", false, false);
          state &= doc.insert<irs::Action::INDEX_STORE>(indexed_and_stored);
          stored_field stored("stored", "doc4");
          state &= doc.insert<irs::Action::STORE>(stored);
        } break;
        case 5: { // doc5
          indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc5", false); // will not be stored, but will be indexed
          state &= doc.insert<irs::Action::INDEX_STORE>(indexed_and_stored);
          stored_field stored("stored", "doc5");
          state &= doc.insert<irs::Action::STORE>(stored);
        } break;
        case 6: { // doc6
          indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc6", true, false); // will not be indexed, but will be stored
          state &= doc.insert<irs::Action::INDEX_STORE>(indexed_and_stored);
          stored_field stored("stored", "doc6");
          state &= doc.insert<irs::Action::STORE>(stored);
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
    ASSERT_TRUE(states[5]); // successfully inserted
    ASSERT_TRUE(states[6]); // successfully inserted

    writer->commit();

    // check index
    {
      auto reader = ir::directory_reader::open(dir());
      ASSERT_EQ(1, reader.size());
      auto& segment = reader[0];
      ASSERT_EQ(7, reader->docs_count()); // we have 7 documents in total
      ASSERT_EQ(4, reader->live_docs_count()); // 3 of which marked as deleted

      std::unordered_set<std::string> expected_values { "doc0", "doc1", "doc5", "doc6" };
      std::unordered_set<std::string> actual_values;

      auto visitor = [&actual_values](irs::doc_id_t doc, const irs::bytes_ref& value) {
        return true;
      };

      irs::bytes_ref value;
      auto column = segment.values("stored");
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
  virtual void SetUp() {
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

TEST_F(memory_index_test, read_write_doc_attributes) {
  read_write_doc_attributes_sparse_variable_length();
  read_write_doc_attributes_dense_variable_length();
  read_write_doc_attributes_sparse_mask();
  read_write_doc_attributes_dense_mask();
  read_write_doc_attributes_fixed_length();
  read_write_doc_attributes_big();
  read_write_doc_attributes();
  read_empty_doc_attributes();
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

TEST_F(memory_index_test, concurrent_read_column) {
  concurrent_read_single_column_smoke();
  concurrent_read_multiple_columns();
}

TEST_F(memory_index_test, concurrent_read_index) {
  concurrent_read_index();
}

TEST_F(memory_index_test, concurrent_add) {
  tests::json_doc_generator gen(resource("simple_sequential.json"), &tests::generic_json_field_factory);
  std::vector<const tests::document*> docs;

  for (const tests::document* doc; (doc = gen.next()) != nullptr; docs.emplace_back(doc));

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

TEST_F(memory_index_test, concurrent_add_remove) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json::json_value& data) {
    if (data.quoted) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        ir::string_ref(name),
        ir::string_ref(data.value))
      );
    }
  });
  std::vector<const tests::document*> docs;
  std::atomic<bool> first_doc(false);

  for (const tests::document* doc; (doc = gen.next()) != nullptr; docs.emplace_back(doc));

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
      auto values = segment.values("name");
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
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
    [] (tests::document& doc, const std::string& name, const tests::json::json_value& data) {
    if (data.quoted) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        ir::string_ref(name),
        ir::string_ref(data.value)
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
      auto values = segment.values("name");
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

    {
      auto& segment = reader[1]; // assume 1 is id of new segment
      auto values = segment.values("name");
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
      auto values = segment.values("name");
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

    {
      auto& segment = reader[1]; // assume 1 is id of new segment
      auto values = segment.values("name");
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
      auto values = segment.values("name");
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

    {
      auto& segment = reader[1]; // assume 1 is id of old segment
      auto values = segment.values("name");
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("E", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc5
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[2]; // assume 2 is id of new segment
      auto values = segment.values("name");
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
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
    [] (tests::document& doc, const std::string& name, const tests::json::json_value& data) {
    if (data.quoted) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        ir::string_ref(name),
        ir::string_ref(data.value)
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
      auto values = segment.values("name");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(iresearch::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto& segment = reader[1]; // assume 1 is id of new segment
      auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
      auto values = segment.values("name");
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

    {
      auto& segment = reader[1]; // assume 1 is id of new segment
      auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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

TEST_F(memory_index_test, import_reader) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json::json_value& data) {
    if (data.quoted) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        ir::string_ref(name),
        ir::string_ref(data.value)
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
      auto values = segment.values("name");
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
      auto values = segment.values("name");
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

TEST_F(memory_index_test, profile_bulk_index_singlethread_full) {
  profile_bulk_index(0, 0, 0, 0);
}

TEST_F(memory_index_test, profile_bulk_index_singlethread_batched) {
  profile_bulk_index(0, 0, 0, 10000);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_cleanup) {
  profile_bulk_index_dedicated_cleanup(16, 10000, 100);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_consolidate) {
  // a lot of threads cause a lot of contention for the segment pool
  // small consolidate_interval causes too many policies to be added and slows down test
  profile_bulk_index_dedicated_consolidate(8, 10000, 5000);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_dedicated_commit) {
  profile_bulk_index_dedicated_commit(16, 1, 1000);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_full) {
  profile_bulk_index(16, 0, 0, 0);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_batched) {
  profile_bulk_index(16, 0, 0, 10000);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_import_full) {
  profile_bulk_index(12, 4, 0, 0);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_import_batched) {
  profile_bulk_index(12, 4, 0, 10000);
}

TEST_F(memory_index_test, profile_bulk_index_multithread_import_update_full) {
  profile_bulk_index(9, 7, 5, 0); // 5 does not divide evenly into 9 or 7
}

TEST_F(memory_index_test, profile_bulk_index_multithread_import_update_batched) {
  profile_bulk_index(9, 7, 5, 10000); // 5 does not divide evenly into 9 or 7
}

TEST_F(memory_index_test, profile_bulk_index_multithread_update_full) {
  profile_bulk_index(16, 0, 5, 0); // 5 does not divide evenly into 16
}

TEST_F(memory_index_test, profile_bulk_index_multithread_update_batched) {
  profile_bulk_index(16, 0, 5, 10000); // 5 does not divide evenly into 16
}

TEST_F(memory_index_test, refresh_reader) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json::json_value& data) {
    if (data.quoted) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        ir::string_ref(name),
        ir::string_ref(data.value)
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
    auto values = segment.values("name");
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
      auto values = segment.values("name");
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
      auto values = segment.values("name");
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
    auto values = segment.values("name");
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(iresearch::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());

    reader = reader.reopen();
    ASSERT_EQ(2, reader.size());

    {
      auto& segment = reader[0]; // assume 0 is id of first segment
      auto values = segment.values("name");
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

    {
      auto& segment = reader[1]; // assume 1 is id of second segment
      auto values = segment.values("name");
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
      auto values = segment.values("name");
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

    {
      auto& segment = reader[1]; // assume 1 is id of second segment
      auto values = segment.values("name");
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

    reader = reader.reopen();
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of second segment
    auto values = segment.values("name");
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
    [] (tests::document& doc, const std::string& name, const tests::json::json_value& data) {
      if (data.quoted) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          ir::string_ref(name),
          ir::string_ref(data.value)
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
    auto values = segment.values("name");
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
      [] (tests::document& doc, const std::string& name, const tests::json::json_value& data) {
        if (data.quoted) {
          doc.insert(std::make_shared<tests::templates::string_field>(
            ir::string_ref(name),
            ir::string_ref(data.value)
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
    auto values = segment.values("name");
    auto upper_case_values = segment.values("NAME");

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
      [] (tests::document& doc, const std::string& name, const tests::json::json_value& data) {
        if (data.quoted) {
          doc.insert(std::make_shared<tests::templates::string_field>(
            ir::string_ref(name),
            ir::string_ref(data.value)
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
    auto values = segment.values("name");
    auto upper_case_values = segment.values("NAME");

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
    [] (tests::document& doc, const std::string& name, const tests::json::json_value& data) {
    if (data.quoted) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        ir::string_ref(name),
        ir::string_ref(data.value)
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
    auto values = segment.values("name");
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

      auto values = segment.values("name");
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

      auto values = segment.values("name");
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

    auto values = segment.values("name");
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

      auto values = segment.values("name");
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

      auto values = segment.values("name");
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

    auto values = segment.values("name");
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

      auto values = segment.values("name");
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

      auto values = segment.values("name");
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

    auto values = segment.values("name");
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

      auto values = segment.values("name");
      for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
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

      auto values = segment.values("name");
      for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
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

TEST_F(fs_index_test, open_writer) {
  open_writer_check_lock();
}

TEST_F(fs_index_test, read_write_doc_attributes) {
  read_write_doc_attributes_sparse_variable_length();
  read_write_doc_attributes_dense_variable_length();
  read_write_doc_attributes_sparse_mask();
  read_write_doc_attributes_dense_mask();
  read_write_doc_attributes_fixed_length();
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

TEST_F(fs_index_test, concurrent_read_column) {
  concurrent_read_single_column_smoke();
  concurrent_read_multiple_columns();
}

TEST_F(fs_index_test, concurrent_read_index) {
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

TEST_F(fs_index_test, profile_bulk_index_singlethread_full) {
  profile_bulk_index(0, 0, 0, 0);
}

TEST_F(fs_index_test, profile_bulk_index_singlethread_batched) {
  profile_bulk_index(0, 0, 0, 10000);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_cleanup) {
  profile_bulk_index_dedicated_cleanup(16, 10000, 100);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_consolidate) {
  // a lot of threads cause a lot of contention for the segment pool
  // small consolidate_interval causes too many policies to be added and slows down test
  profile_bulk_index_dedicated_consolidate(8, 10000, 5000);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_dedicated_commit) {
  profile_bulk_index_dedicated_commit(16, 1, 1000);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_full) {
  profile_bulk_index(16, 0, 0, 0);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_batched) {
  profile_bulk_index(16, 0, 0, 10000);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_import_full) {
  profile_bulk_index(12, 4, 0, 0);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_import_batched) {
  profile_bulk_index(12, 4, 0, 10000);
}

TEST_F(fs_index_test, profile_bulk_index_multithread_import_update_full) {
  profile_bulk_index(9, 7, 5, 0); // 5 does not divide evenly into 9 or 7
}

TEST_F(fs_index_test, profile_bulk_index_multithread_import_update_batched) {
  profile_bulk_index(9, 7, 5, 10000); // 5 does not divide evenly into 9 or 7
}

TEST_F(fs_index_test, profile_bulk_index_multithread_update_full) {
  profile_bulk_index(16, 0, 5, 0); // 5 does not divide evenly into 16
}

TEST_F(fs_index_test, profile_bulk_index_multithread_update_batched) {
  profile_bulk_index(16, 0, 5, 10000); // 5 does not divide evenly into 16
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
