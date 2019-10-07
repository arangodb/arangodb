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

#include "assert_format.hpp"
#include "index_tests.hpp"
#include "tests_shared.hpp"
#include "index/transaction_store.hpp"
#include "iql/query_builder.hpp"
#include "search/term_filter.hpp"
#include "store/memory_directory.hpp"
#include "utils/index_utils.hpp"

NS_LOCAL

void add_segment(
    tests::index_segment& segment,
    irs::store_writer& writer,
    tests::doc_generator_base& gen
) {
  const tests::document* src;

  while ((src = gen.next())) {
    auto inserter = [&src](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(src->indexed.begin(), src->indexed.end());
      doc.insert<irs::Action::STORE>(src->stored.begin(), src->stored.end());
      return false; // break the loop
    };

    segment.add(src->indexed.begin(), src->indexed.end());
    ASSERT_TRUE(writer.insert(inserter));
  }
}

void add_segment(
    tests::index_t& expected,
    irs::transaction_store& store,
    tests::doc_generator_base& gen
) {
  irs::store_writer writer(store);

  expected.emplace_back();
  add_segment(expected.back(), writer, gen);
  ASSERT_TRUE(writer.commit());
}

void assert_index(
    const tests::index_t& expected,
    const irs::index_reader& actual,
    size_t skip = 0
) {
  tests::assert_index(expected, actual, irs::flags(), skip);
  tests::assert_index(expected, actual, irs::flags{ irs::document::type() }, skip);
  tests::assert_index(expected, actual, irs::flags{ irs::document::type(), irs::frequency::type() }, skip);
  tests::assert_index(expected, actual, irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type() }, skip);
  tests::assert_index(expected, actual, irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::offset::type() }, skip);
  tests::assert_index(expected, actual, irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::payload::type() }, skip);
  tests::assert_index(expected, actual, irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::offset::type(), irs::payload::type() }, skip);
}

struct incompatible_test_attribute: irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
  incompatible_test_attribute() NOEXCEPT {}
};

REGISTER_ATTRIBUTE(incompatible_test_attribute);
DEFINE_ATTRIBUTE_TYPE(incompatible_test_attribute)

class transaction_store_tests: public test_base {
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right before each test).

    test_base::SetUp();
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the destructor).

    test_base::TearDown();
  }

 public:
  void profile_bulk_index(
      irs::transaction_store& store,
      size_t num_insert_threads,
      size_t num_update_threads,
      size_t batch_size,
      bool skip_docs_count_check = false // sued by profile_bulk_index_dedicated_flush(...)
  ) {
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

    irs::timer_utils::init_stats(true);

    std::atomic<size_t> parsed_docs_count(0);
    auto thread_count = (std::max)((size_t)1, num_insert_threads);
    auto total_threads = thread_count + num_update_threads;
    irs::async_utils::thread_pool thread_pool(total_threads, total_threads);
    size_t update_skip = (std::min)((size_t)10000, (batch_size / thread_count / 3) * 3); // round to modulo prime number (ensure first update thread is garanteed to match at least 1 record, e.g. for batch 10000, 16 insert threads -> commit every 10000/16=625 inserts, therefore update_skip must be less than this number)
    size_t writer_batch_size = batch_size ? batch_size : (std::numeric_limits<size_t>::max)();
    std::mutex mutex;
    std::mutex commit_mutex;
    std::atomic<bool> start_update(false);

    {
      std::lock_guard<std::mutex> lock(mutex);

      // register insertion jobs
      for (size_t i = 0; i < thread_count; ++i) {
        thread_pool.run([&mutex, &commit_mutex, &store, thread_count, i, writer_batch_size, &parsed_docs_count, &start_update]()->void {
          {
            // wait for all threads to be registered
            std::lock_guard<std::mutex> lock(mutex);
          }

          irs::store_writer writer(store);
          csv_doc_template_t csv_doc_template;
          auto csv_doc_inserter = [&csv_doc_template](irs::store_writer::document& doc)->bool {
            doc.insert<irs::Action::INDEX>(csv_doc_template.indexed.begin(), csv_doc_template.indexed.end());
            doc.insert<irs::Action::STORE>(csv_doc_template.stored.begin(), csv_doc_template.stored.end());
            return false;
          };
          tests::csv_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template);
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
              ASSERT_TRUE(writer.insert(csv_doc_inserter));
            }

            if (count >= writer_batch_size) {
              TRY_SCOPED_LOCK_NAMED(commit_mutex, commit_lock);

              // break commit chains by skipping commit if one is already in progress
              if (!commit_lock) {
                continue;
              }

              {
                REGISTER_TIMER_NAMED_DETAILED("commit");
                ASSERT_TRUE(writer.commit());
              }

              count = 0;
              start_update = true;
            }
          }

          {
            REGISTER_TIMER_NAMED_DETAILED("commit");
            ASSERT_TRUE(writer.commit());
          }

          start_update = true;
        });
      }

      // register update jobs
      for (size_t i = 0; i < num_update_threads; ++i) {
        thread_pool.run([&mutex, &commit_mutex, &store, num_update_threads, i, update_skip, writer_batch_size, &start_update]()->void {
          {
            // wait for all threads to be registered
            std::lock_guard<std::mutex> lock(mutex);
          }

          // without this updates happen before insertions had a chance to commit
          while(!start_update) {
            std::this_thread::yield(); // allow insertions to take place
          }

          irs::store_writer writer(store);
          csv_doc_template_t csv_doc_template;
          tests::csv_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template);
          size_t doc_skip = update_skip; // update every 'skip' records
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
              auto filter = irs::by_term::make();
              auto key_field = csv_doc_template.indexed.begin()->name();
              auto key_term = csv_doc_template.indexed.get<tests::templates::string_field>(key_field)->value();
              auto value_field = (++(csv_doc_template.indexed.begin()))->name();
              auto value_term = csv_doc_template.indexed.get<tests::templates::string_field>(value_field)->value();
              std::string updated_term(value_term.c_str(), value_term.size());

              static_cast<irs::by_term&>(*filter).field(key_field).term(key_term);
              updated_term.append(value_term.c_str(), value_term.size()); // double up term
              csv_doc_template.indexed.get<tests::templates::string_field>(value_field)->value(updated_term);
              csv_doc_template.insert(std::make_shared<tests::templates::string_field>("updated"));

              REGISTER_TIMER_NAMED_DETAILED("update");
              ASSERT_TRUE(writer.update(
                std::move(filter),
                [&csv_doc_template](irs::store_writer::document& doc)->bool {
                  doc.insert<irs::Action::INDEX>(csv_doc_template.indexed.begin(), csv_doc_template.indexed.end());
                  doc.insert<irs::Action::STORE>(csv_doc_template.stored.begin(), csv_doc_template.stored.end());
                  return false;
                }
              ));
            }

            if (count >= writer_batch_size) {
              TRY_SCOPED_LOCK_NAMED(commit_mutex, commit_lock);

              // break commit chains by skipping commit if one is already in progress
              if (!commit_lock) {
                continue;
              }

              {
                REGISTER_TIMER_NAMED_DETAILED("commit");
                ASSERT_TRUE(writer.commit());
              }

              count = 0;
            }
          }

          {
            REGISTER_TIMER_NAMED_DETAILED("commit");
            ASSERT_TRUE(writer.commit());
          }
        });
      }
    }

    thread_pool.stop();

    auto path = test_dir();

    path /= "profile_bulk_index.log";

    std::ofstream out(path.native());

    irs::timer_utils::flush_stats(out);
    out.close();
    std::cout << "Path to timing log: " << path.utf8_absolute() << std::endl;

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());

    size_t indexed_docs_count = reader.live_docs_count();
    size_t updated_docs_count = 0;
    auto updated_visitor = [&updated_docs_count](irs::doc_id_t, const irs::bytes_ref&)->bool {
      ++updated_docs_count;
      return true;
    };
    const auto* column = reader.begin()->column_reader("updated");

    if (column) {
      // field insterted by updater threads
      column->visit(updated_visitor);
    }

    ASSERT_TRUE(skip_docs_count_check || parsed_docs_count == indexed_docs_count);
    ASSERT_TRUE(!num_update_threads || column); // at least some updates took place if update enabled
    ASSERT_TRUE(!num_update_threads || updated_docs_count); // at least some updates took place if update enabled
  }

  void profile_bulk_index_dedicated_flush(
      irs::transaction_store& store,
      size_t num_threads,
      size_t batch_size,
      size_t flush_interval
) {
    auto always_merge = [](
      irs::bitvector& candidates, const irs::directory& dir, const irs::index_meta& meta
    )->void {
      for (size_t i = meta.size(); i; candidates.set(--i)); // merge every segment
    };
    auto codec = irs::formats::get("1_0");
    irs::memory_directory dir;
    auto dir_writer = irs::index_writer::make(dir, codec, irs::OM_CREATE);
    std::atomic<bool> working(true);
    irs::async_utils::thread_pool thread_pool(2, 2);

    thread_pool.run([flush_interval, &store, &working, &dir_writer]()->void {
      while (working.load()) {
        auto flushed = store.flush();
        ASSERT_TRUE(flushed && dir_writer->import(flushed));
        std::this_thread::sleep_for(std::chrono::milliseconds(flush_interval));
      }
    });

    {
      auto finalizer = irs::make_finally([&working]()->void{working = false;});
      profile_bulk_index(store, num_threads, 0, batch_size, true);
    }

    thread_pool.stop();

    // flush any remaining docs
    {
      auto flushed = store.flush();

      ASSERT_TRUE(flushed && dir_writer->import(flushed));
      dir_writer->commit();
      ASSERT_TRUE(dir_writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));
      dir_writer->commit();
    }

    struct dummy_doc_template_t: public tests::csv_doc_generator::doc_template {
      virtual void init() {}
      virtual void value(size_t idx, const irs::string_ref& value) {};
    };
    dummy_doc_template_t dummy_doc_template;
    tests::csv_doc_generator gen(resource("simple_two_column.csv"), dummy_doc_template);
    size_t docs_count = 0;

    // determine total number of docs in source data
    while (gen.next()) {
      ++docs_count;
    }

    auto reader = irs::directory_reader::open(dir);
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(docs_count, reader[0].docs_count());
  }
};

NS_END

// ----------------------------------------------------------------------------
// --SECTION--      tests for transaction_store based on memory_directory tests
// ----------------------------------------------------------------------------

TEST_F(transaction_store_tests, arango_demo_docs) {
  tests::index_t expected;
  irs::transaction_store store;

  {
    tests::json_doc_generator gen(
      resource("arango_demo.json"), &tests::generic_json_field_factory
    );

    add_segment(expected, store, gen);
  }

  assert_index(expected, store.reader());
}

TEST_F(transaction_store_tests, check_fields_order) {
  irs::transaction_store store;
  std::vector<irs::string_ref> names {
   "06D36", "0OY4F", "1DTSP", "1KCSY", "2NGZD", "3ME9S", "4UIR7",
   "68QRT", "6XTTH", "7NDWJ", "9QXBA", "A8MSE", "CNH1B", "I4EWS",
   "JXQKH", "KPQ7R", "LK1MG", "M47KP", "NWCBQ", "OEKKW", "RI1QG",
   "TD7H7", "U56E5", "UKETS", "UZWN7", "V4DLA", "W54FF", "Z4K42",
   "ZKQCU", "ZPNXJ"
  };

  ASSERT_TRUE(std::is_sorted(names.begin(), names.end()));

  struct {
    irs::string_ref name_;
    mutable irs::string_token_stream stream_;
    const irs::string_ref& name() const { return name_; }
    float_t boost() const { return 1.f; }
    const irs::flags& features() const { return irs::flags::empty_instance(); }
    irs::token_stream& get_tokens() const {
      stream_.reset(name_);
      return stream_;
    }
  } field;

  // insert attributes
  {
    irs::store_writer writer(store);
    auto inserter = [&field, &names](irs::store_writer::document& doc)->bool {
      for (auto& name: names) {
        field.name_ = name;
        doc.insert<irs::Action::INDEX>(field);
      }
      return false; // break the loop
    };

    ASSERT_TRUE(writer.insert(inserter));
    ASSERT_TRUE(writer.commit());
  }

  // iterate over fields
  {
    auto reader = store.reader();
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
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    auto actual = segment.fields();

    for (auto expected = names.begin(), prev = expected; expected != names.end();) {
      ASSERT_TRUE(actual->seek(*expected));
      ASSERT_EQ(*expected, actual->value().meta().name);

      if (prev != expected) {
        ASSERT_TRUE(actual->seek(*prev)); // can seek backwards
        ASSERT_EQ(*prev, actual->value().meta().name);
      }

      // seek to the same value
      ASSERT_TRUE(actual->seek(*expected));
      ASSERT_EQ(*expected, actual->value().meta().name);

      prev = expected;
      ++expected;
    }
    ASSERT_FALSE(actual->next()); // reached the end
    ASSERT_TRUE(actual->seek(names.front())); // can seek backwards
    ASSERT_TRUE(actual->next());
  }

  // seek before the first element
  {
    auto reader = store.reader();
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
        ASSERT_TRUE(actual->seek(*prev)); // can seek backwards
        ASSERT_EQ(*prev, actual->value().meta().name);
      }

      // seek to the same value
      ASSERT_TRUE(actual->seek(*expected));
      ASSERT_EQ(*expected, actual->value().meta().name);

      prev = expected;
      ++expected;
    }
    ASSERT_FALSE(actual->next()); // reached the end
    ASSERT_TRUE(actual->seek(names.front())); // can seek backwards
    ASSERT_TRUE(actual->next());
  }

  // seek after the last element
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    auto actual = segment.fields();

    const auto key = irs::string_ref("~");
    ASSERT_TRUE(key > names.back());
    ASSERT_FALSE(actual->seek(key));
    ASSERT_FALSE(actual->next()); // reached the end
    ASSERT_TRUE(actual->seek(names.front())); // can seek backwards
  }

  // seek in between
  {
    std::vector<std::pair<irs::string_ref, irs::string_ref>> seeks {
      { "0B", names[1] }, { names[1], names[1] }, { "0", names[0] },
      { "D", names[13] }, { names[13], names[13] }, { names[12], names[12] },
      { "P", names[20] }, { "Z", names[27] }
    };

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    auto actual = segment.fields();

    for (auto& seek: seeks) {
      auto& key = seek.first;
      auto& expected = seek.second;

      ASSERT_TRUE(actual->seek(key));
      ASSERT_EQ(expected, actual->value().meta().name);
    }

    const auto key = irs::string_ref("~");
    ASSERT_TRUE(key > names.back());
    ASSERT_FALSE(actual->seek(key));
    ASSERT_FALSE(actual->next()); // reached the end
    ASSERT_TRUE(actual->seek(names.front())); // can seek backwards
  }

  // seek in between + next
  {
    std::vector<std::pair<irs::string_ref, size_t>> seeks {
      { "0B", 1 },  { "D", 13 }, { "O", 19 }, { "P", 20 }, { "Z", 27 }
    };

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());

    for (auto& seek: seeks) {
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
      ASSERT_TRUE(actual->seek(names.front())); // can seek backwards
    }
  }
}

TEST_F(transaction_store_tests, check_attributes_order) {
  irs::transaction_store store;
  std::vector<irs::string_ref> names {
   "06D36", "0OY4F", "1DTSP", "1KCSY", "2NGZD", "3ME9S", "4UIR7",
   "68QRT", "6XTTH", "7NDWJ", "9QXBA", "A8MSE", "CNH1B", "I4EWS",
   "JXQKH", "KPQ7R", "LK1MG", "M47KP", "NWCBQ", "OEKKW", "RI1QG",
   "TD7H7", "U56E5", "UKETS", "UZWN7", "V4DLA", "W54FF", "Z4K42",
   "ZKQCU", "ZPNXJ"
  };

  ASSERT_TRUE(std::is_sorted(names.begin(), names.end()));

  struct {
    irs::string_ref name_;
    const irs::string_ref& name() const { return name_; }
    bool write(irs::data_output&) const NOEXCEPT { return true; }
  } field;

  // insert attributes
  {
    irs::store_writer writer(store);
    auto inserter = [&field, &names](irs::store_writer::document& doc) {
      for (auto& name : names) {
        field.name_ = name;
        doc.insert<irs::Action::STORE>(field);
      }
      return false; // break the loop
    };

    ASSERT_TRUE(writer.insert(inserter));
    ASSERT_TRUE(writer.commit());
  }

  // iterate over attributes
  {
    auto reader = store.reader();
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
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    auto actual = segment.columns();

    for (auto expected = names.begin(), prev = expected; expected != names.end();) {
      ASSERT_TRUE(actual->seek(*expected));
      ASSERT_EQ(*expected, actual->value().name);

      if (prev != expected) {
        ASSERT_TRUE(actual->seek(*prev)); // can seek backwards
        ASSERT_EQ(*prev, actual->value().name);
      }

      // seek to the same value
      ASSERT_TRUE(actual->seek(*expected));
      ASSERT_EQ(*expected, actual->value().name);

      prev = expected;
      ++expected;
    }
    ASSERT_FALSE(actual->next()); // reached the end
    ASSERT_TRUE(actual->seek(names.front())); // can seek backwards
    ASSERT_TRUE(actual->next());
  }

  // seek before the first element
  {
    auto reader = store.reader();
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
        ASSERT_TRUE(actual->seek(*prev)); // can seek backwards
        ASSERT_EQ(*prev, actual->value().name);
      }

      // seek to the same value
      ASSERT_TRUE(actual->seek(*expected));
      ASSERT_EQ(*expected, actual->value().name);

      prev = expected;
      ++expected;
    }
    ASSERT_FALSE(actual->next()); // reached the end
    ASSERT_TRUE(actual->seek(names.front())); // can seek backwards
    ASSERT_TRUE(actual->next());
  }

  // seek after the last element
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    auto actual = segment.columns();

    const auto key = irs::string_ref("~");
    ASSERT_TRUE(key > names.back());
    ASSERT_FALSE(actual->seek(key));
    ASSERT_FALSE(actual->next()); // reached the end
    ASSERT_TRUE(actual->seek(names.front())); // can seek backwards
  }

  // seek in between
  {
    std::vector<std::pair<irs::string_ref, irs::string_ref>> seeks {
      { "0B", names[1] }, { names[1], names[1] }, { "0", names[0] },
      { "D", names[13] }, { names[13], names[13] }, { names[12], names[12] },
      { "P", names[20] }, { "Z", names[27] }
    };

    auto reader = store.reader();
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
    ASSERT_TRUE(actual->seek(names.front())); // can seek backwards
  }

  // seek in between + next
  {
    std::vector<std::pair<irs::string_ref, size_t>> seeks {
      { "0B", 1 },  { "D", 13 }, { "O", 19 }, { "P", 20 }, { "Z", 27 }
    };

    auto reader = store.reader();
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
      ASSERT_TRUE(actual->seek(names.front())); // can't seek backwards
    }
  }
}

TEST_F(transaction_store_tests, read_write_doc_attributes) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  // write documents
  {
    irs::store_writer writer(store);

    // fields + attributes
    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc4](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
      doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // check inserted values:
  // - random read
  // - iterate
  {
    auto reader = store.reader();
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

    // read attributes from 'name' column
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

    // iterate over 'name' column
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

    // read attributes from 'prefix' column
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

    // iterate over 'prefix' column
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
  // - random read
  // - iterate
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());

    // read attribute from invalid column
    {
      ASSERT_EQ(nullptr, segment.column_reader("invalid_column"));
    }

    // read attributes from 'name' column
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

    // iterate over 'name' column
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

    // read attributes from 'prefix' column
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

    // iterate over 'prefix' column
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

TEST_F(transaction_store_tests, read_write_doc_attributes_big) {
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

  irs::transaction_store store;
  csv_doc_template_t csv_doc_template;
  tests::csv_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template);
  size_t docs_count = 0;

  // write attributes
  {
    irs::store_writer writer(store);
    const tests::document* src;

    while ((src = gen.next())) {
      auto inserter = [&src](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(src->indexed.begin(), src->indexed.end());
        doc.insert<irs::Action::STORE>(src->stored.begin(), src->stored.end());
        return false; // break the loop
      };

      ASSERT_TRUE(writer.insert(inserter));
      ++docs_count;
    }

    ASSERT_TRUE(writer.commit());
  }

  // check inserted values:
  // - random read
  // - visit
  // - iterate
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
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
      const irs::string_ref column_name = "id";
      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

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

      // visit column
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

      // iterate over column
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
          ASSERT_TRUE(payload->next());
          ++expected_id;

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);
          ASSERT_NE(nullptr, field);

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
      const irs::string_ref column_name = "label";
      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

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

      // visit column
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

      // iterate over 'label' column
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
          ASSERT_TRUE(payload->next());
          ++expected_id;

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);
          ASSERT_NE(nullptr, field);

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
  // - random read
  // - visit
  // - iterate
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
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
      const irs::string_ref column_name = "id";
      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

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

      // visit column
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

      // iterate over column
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
          ASSERT_TRUE(payload->next());
          ++expected_id;

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);
          ASSERT_NE(nullptr, field);

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
      const irs::string_ref column_name = "label";
      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

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

      // visit column
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

      // iterate over 'label' column
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
          ASSERT_TRUE(payload->next());
          ++expected_id;

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);
          ASSERT_NE(nullptr, field);

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

TEST_F(transaction_store_tests, cleanup_store) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  // initial insert into store
  {
    irs::store_writer writer(store);
    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // make a gap in document IDs
  {
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    irs::store_writer writer(store);
    writer.remove(*query_doc2.filter);
    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // check documents
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(4, reader.docs_count()); // +1 for invalid doc
    ASSERT_EQ(2, reader.live_docs_count());
    irs::bytes_ref actual_value;
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  store.cleanup(); // release unused records

  // make a gap in document IDs (repeat above, but new doc placed in gap)
  {
    auto query_doc3 = irs::iql::query_builder().build("name==C", std::locale::classic());
    irs::store_writer writer(store);
    writer.remove(*query_doc3.filter);
    ASSERT_TRUE(writer.insert([doc4](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
      doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // check documents
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(3, reader.docs_count()); // +1 for invalid doc
    ASSERT_EQ(2, reader.live_docs_count());
    irs::bytes_ref actual_value;
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(transaction_store_tests, clear_store) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();

  // initial insert into store
  {
    irs::store_writer writer(store);
    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // check documents
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(2, reader.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader.live_docs_count());
    irs::bytes_ref actual_value;
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  auto query_doc3 = irs::iql::query_builder().build("name==C", std::locale::classic());
  irs::store_writer writer0(store); // writer for generation #0

  // modifications that should not apply because of call to clear()
  {
    ASSERT_TRUE(writer0.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    writer0.remove(*query_doc3.filter);
  }

  store.clear();

  // check documents (empty)
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(0, reader.docs_count());
    ASSERT_EQ(0, reader.live_docs_count());
  }

  // add documents for generation one (matching removals from #0)
  {
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // check documents
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(4, reader.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader.live_docs_count());
    irs::bytes_ref actual_value;
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  ASSERT_FALSE(writer0.commit());

  // check documents
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(4, reader.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader.live_docs_count());
    irs::bytes_ref actual_value;
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // test with same midifications that did not apply before
  {
    irs::store_writer writer(store); // writer for generation #0

    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    writer.remove(std::move(query_doc3.filter));
    ASSERT_TRUE(writer.commit());
  }

  // check documents
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(5, reader.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader.live_docs_count());
    irs::bytes_ref actual_value;
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(transaction_store_tests, flush_store) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();

  // initial insert into store
  {
    irs::store_writer writer(store);
    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  auto reader0 = store.reader();

  // check documents
  {
    ASSERT_EQ(1, reader0.size());
    ASSERT_EQ(2, reader0.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader0.live_docs_count());
    irs::bytes_ref actual_value;
    auto& segment = *(reader0.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  auto reader1 = store.flush();

  // initial insert into store
  {
    irs::store_writer writer(store);
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // check documents
  {
    ASSERT_EQ(1, reader1.size());
    ASSERT_EQ(2, reader1.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader1.live_docs_count());
    irs::bytes_ref actual_value;
    auto& segment = *(reader1.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  // check documents (reader0 after reopen)
  {
    auto reader = reader0.reopen();
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(3, reader.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader.live_docs_count());
    irs::bytes_ref actual_value;
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  // check documents (reader1 after reopen)
  {
    auto reader = reader1.reopen();
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(3, reader.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader.live_docs_count());
    irs::bytes_ref actual_value;
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(transaction_store_tests, read_empty_doc_attributes) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  // write documents without attributes
  {
    irs::store_writer writer(store);

    // fields only
    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc4](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  auto reader = store.reader();
  ASSERT_EQ(1, reader.size());
  auto& segment = *(reader.begin());

  const auto* column = segment.column_reader("name");
  ASSERT_EQ(nullptr, column);
}

TEST_F(transaction_store_tests, read_multipart_doc_attributes) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();
  tests::document const* doc5 = gen.next();

  struct data_field: public tests::field_base {
    irs::bytes_ref data_;
    mutable irs::string_token_stream stream_;
    data_field(const irs::bytes_ref& data): data_(data) { name("testcol"); }
    virtual irs::token_stream& get_tokens() const override { return stream_; }
    virtual bool write(irs::data_output& out) const override {
      out.write_bytes(data_.c_str(), data_.size());
      return true;
    }
  };

  const_cast<tests::document*>(doc1)->stored.push_back(std::make_shared<data_field>(irs::ref_cast<irs::byte_type>(irs::string_ref("abc"))));

  const_cast<tests::document*>(doc2)->stored.push_back(std::make_shared<data_field>(irs::ref_cast<irs::byte_type>(irs::string_ref("abcd"))));
  const_cast<tests::document*>(doc2)->stored.push_back(std::make_shared<data_field>(irs::ref_cast<irs::byte_type>(irs::string_ref("efg"))));
  const_cast<tests::document*>(doc2)->stored.push_back(std::make_shared<data_field>(irs::ref_cast<irs::byte_type>(irs::string_ref("hijkl"))));

  const_cast<tests::document*>(doc3)->stored.push_back(std::make_shared<data_field>(irs::ref_cast<irs::byte_type>(irs::string_ref("abc"))));
  const_cast<tests::document*>(doc3)->stored.push_back(std::make_shared<data_field>(irs::ref_cast<irs::byte_type>(irs::string_ref("defgh"))));
  const_cast<tests::document*>(doc3)->stored.push_back(std::make_shared<data_field>(irs::ref_cast<irs::byte_type>(irs::string_ref("ijlkmn"))));

  const_cast<tests::document*>(doc4)->stored.push_back(std::make_shared<data_field>(irs::ref_cast<irs::byte_type>(irs::string_ref("abcdef"))));
  const_cast<tests::document*>(doc4)->stored.push_back(std::make_shared<data_field>(irs::ref_cast<irs::byte_type>(irs::string_ref("ghij"))));
  const_cast<tests::document*>(doc4)->stored.push_back(std::make_shared<data_field>(irs::ref_cast<irs::byte_type>(irs::string_ref("klmnop"))));
  const_cast<tests::document*>(doc4)->stored.push_back(std::make_shared<data_field>(irs::ref_cast<irs::byte_type>(irs::string_ref("qrstuv"))));
  const_cast<tests::document*>(doc4)->stored.push_back(std::make_shared<data_field>(irs::ref_cast<irs::byte_type>(irs::string_ref("wxyz"))));

  // no 'data_field' for 'doc5'

  // write documents with multipart attributes
  {
    irs::store_writer writer(store);

    // fields only
    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc4](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
      doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc5](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc5->indexed.begin(), doc5->indexed.end());
      doc.insert<irs::Action::STORE>(doc5->stored.begin(), doc5->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  auto reader = store.reader();
  ASSERT_EQ(1, reader.size());
  auto& segment = *(reader.begin());

  // check number of values in the column
  {
    const auto* column = segment.column_reader("testcol");
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(4, column->size());
  }

  // read attributes from 'testcol' column
  {
    irs::bytes_ref actual_value;

    const auto* column = segment.column_reader("testcol");
    ASSERT_NE(nullptr, column);
    auto value_reader = column->values();

    ASSERT_TRUE(value_reader(2, actual_value));
    ASSERT_EQ("abcdefghijkl", irs::ref_cast<char>(actual_value)); // 'testcol' value in doc2
    ASSERT_TRUE(value_reader(4, actual_value));
    ASSERT_EQ("abcdefghijklmnopqrstuvwxyz", irs::ref_cast<char>(actual_value)); // 'testcol' value in doc4
    ASSERT_TRUE(value_reader(1, actual_value));
    ASSERT_EQ("abc", irs::ref_cast<char>(actual_value)); // 'testcol' value in doc1
    ASSERT_TRUE(value_reader(3, actual_value));
    ASSERT_EQ("abcdefghijlkmn", irs::ref_cast<char>(actual_value)); // 'testcol' value in doc3
    ASSERT_FALSE(value_reader(5, actual_value)); // invalid document id
    ASSERT_EQ("abcdefghijlkmn", irs::ref_cast<char>(actual_value)); // same as 'testcol' value in doc3
  }

  // iterate over 'testcol' column
  {
    auto column = segment.column_reader("testcol");
    ASSERT_NE(nullptr, column);
    auto it = column->iterator();
    ASSERT_NE(nullptr, it);

    auto& payload = it->attributes().get<irs::payload_iterator>();
    ASSERT_FALSE(!payload);
    ASSERT_FALSE(payload->next());
    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
    ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

    std::vector<std::pair<irs::doc_id_t, std::vector<irs::string_ref>>> expected_values = {
      { 1, { "abc" } },
      { 2, { "abcd", "efg", "hijkl" } },
      { 3, { "abc", "defgh", "ijlkmn" } },
      { 4, { "abcdef", "ghij", "klmnop", "qrstuv", "wxyz" } },
    };
    size_t i = 0;

    for (; it->next(); ++i) {
      const auto& expected_value = expected_values[i];

      for (auto& expected_item: expected_value.second) {
        ASSERT_TRUE(payload->next());
        const auto actual_str_value = irs::ref_cast<char>(payload->value());
        ASSERT_EQ(expected_item, actual_str_value);
      }

      ASSERT_FALSE(payload->next());
      ASSERT_EQ(expected_value.first, it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
    }

    ASSERT_FALSE(it->next());
    ASSERT_FALSE(payload->next());
    ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
    ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
    ASSERT_EQ(expected_values.size(), i);
  }

  // visit over 'testcol' column
  {
    std::map<irs::doc_id_t, std::vector<irs::string_ref>> expected = {
      { 1, { "abc" } },
      { 2, { "abcd", "efg", "hijkl" } },
      { 3, { "abc", "defgh", "ijlkmn" } },
      { 4, { "abcdef", "ghij", "klmnop", "qrstuv", "wxyz" } },
    };
    auto visitor = [&expected](
        irs::doc_id_t doc_id, const irs::bytes_ref& value
    )->bool {
      auto itr = expected.find(doc_id);

      if (itr == expected.end()) {
        return false;
      }

      auto& values = itr->second;

      if (values.empty()) {
        return false;
      }

      if (value != irs::ref_cast<irs::byte_type>(values.front())) {
        return false;
      }

      values.erase(values.begin());

      return true;
    };
    auto column = segment.column_reader("testcol");
    ASSERT_NE(nullptr, column);
    ASSERT_TRUE(column->visit(visitor));

    for (auto& entry: expected) {
      ASSERT_TRUE(entry.second.empty());
    }
  }
}

TEST_F(transaction_store_tests, open_writer) {
  irs::transaction_store store;

  // open multiple writers
  {
    irs::store_writer writer0(store);
    irs::store_writer writer1(store);
  }

  // open multiple writers with commit
  {
    irs::store_writer writer0(store);
    ASSERT_TRUE(writer0.commit());
    irs::store_writer writer1(store);
  }
}

TEST_F(transaction_store_tests, check_writer_open_modes) {
  irs::transaction_store store;

  // create empty index
  {
    irs::store_writer writer(store);
    ASSERT_TRUE(writer.commit());
  }

  // read empty index, it should not fail
  {
    auto reader = store.reader();
    ASSERT_EQ(0, reader.live_docs_count());
    ASSERT_EQ(0, reader.docs_count());
    ASSERT_EQ(1, reader.size());
    ASSERT_NE(reader.begin(), reader.end());
  }

  // append to index
  {
    irs::store_writer writer(store);
    tests::json_doc_generator gen(
      resource("simple_sequential.json"), &tests::generic_json_field_factory
    );
    tests::document const* doc1 = gen.next();
    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // read index
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.live_docs_count());
    ASSERT_EQ(2, reader.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader.size());
    ASSERT_NE(reader.begin(), reader.end());
  }
}

TEST_F(transaction_store_tests, writer_transaction_isolation) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (tests::json_doc_generator::ValueType::STRING == data.vt) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name), data.str
        ));
      }
    }
  );
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();

  irs::store_writer writer0(store); // start transaction #0
  ASSERT_TRUE(writer0.insert([doc1](irs::store_writer::document& doc)->bool {
    doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
    doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
    return false;
  }));

  irs::store_writer writer1(store); // start transaction #1
  ASSERT_TRUE(writer1.insert([doc2](irs::store_writer::document& doc)->bool {
    doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
    doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
    return false;
  }));
  ASSERT_TRUE(writer1.commit()); // finish transaction #1

  // check index, 1 document in 1 segment
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.live_docs_count());
    ASSERT_EQ(3, reader.docs_count());  // +1 for invalid doc
    ASSERT_EQ(1, reader.size());
    ASSERT_NE(reader.begin(), reader.end());
  }

  // check documents
  {
    auto reader = store.reader();
    irs::bytes_ref actual_value;

    {
      auto& segment = *(reader.begin());
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_FALSE(docsItr->next());
    }
  }

  ASSERT_TRUE(writer0.commit()); // transaction #0

  // check index, 2 documents in 1 segment
  {
    auto reader = store.reader();
    ASSERT_EQ(2, reader.live_docs_count());
    ASSERT_EQ(3, reader.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader.size());
    ASSERT_NE(reader.begin(), reader.end());
  }

  // check documents
  {
    auto reader = store.reader();
    irs::bytes_ref actual_value;

    {
      auto& segment = *(reader.begin());
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST_F(transaction_store_tests, writer_bulk_insert) {
  class indexed_and_stored_field {
   public:
    indexed_and_stored_field(std::string&& name, const irs::string_ref& value, bool stored_valid = true, bool indexed_valid = true)
      : name_(std::move(name)), value_(value), stored_valid_(stored_valid) {
      if (!indexed_valid) {
        features_.add<incompatible_test_attribute>();
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
    const irs::flags& features() const { return features_; }
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
        features_.add<incompatible_test_attribute>();
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
    const irs::flags& features() const { return features_; }

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

  size_t i = 0;
  size_t max = 8;
  bool states[8]; // max
  std::fill(std::begin(states), std::end(states), true);

  auto bulk_inserter = [&i, &max, &states](irs::store_writer::document& doc)->bool {
    auto& state = states[i];
    switch (i) {
      case 0: { // doc0
        indexed_field indexed("indexed", "doc0");
        state &= doc.insert<irs::Action::INDEX>(indexed);
        stored_field stored("stored", "doc0");
        state &= doc.insert<irs::Action::STORE>(stored);
        indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc0");
        state &= doc.insert<irs::Action::INDEX | irs::Action::STORE>(indexed_and_stored);
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
        state &= doc.insert<irs::Action::INDEX | irs::Action::STORE>(indexed_and_stored);
        stored_field stored("stored", "doc4");
        state &= doc.insert<irs::Action::STORE>(stored);
      } break;
      case 5: { // doc5 (will be dropped since it contains failed stored field)
        indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc5", false); // will fail on store, but will pass on index
        state &= doc.insert<irs::Action::INDEX | irs::Action::STORE>(indexed_and_stored);
        stored_field stored("stored", "doc5");
        state &= doc.insert<irs::Action::STORE>(stored);
      } break;
      case 6: { // doc6 (will be dropped since it contains failed indexed field)
        indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc6", true, false); // will fail on index, but will pass on store
        state &= doc.insert<irs::Action::INDEX | irs::Action::STORE>(indexed_and_stored);
        stored_field stored("stored", "doc6");
        state &= doc.insert<irs::Action::STORE>(stored);
      } break;
      case 7: { // valid insertion of last doc will mark bulk insert result as valid
        indexed_and_stored_field indexed_and_stored("indexed_and_stored", "doc7", true, true); // will be indexed, and will be stored
        state &= doc.insert<irs::Action::INDEX | irs::Action::STORE>(indexed_and_stored);
        stored_field stored("stored", "doc7");
        state &= doc.insert<irs::Action::STORE>(stored);
      } break;
    }

    return ++i != max;
  };

  {
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_FALSE(writer.insert(bulk_inserter));
    ASSERT_TRUE(states[0]); // successfully inserted
    ASSERT_TRUE(states[1]); // successfully inserted
    ASSERT_FALSE(states[2]); // skipped
    ASSERT_TRUE(states[3]); // skipped (not reached in bulk operation)
    ASSERT_TRUE(states[4]); // skipped (not reached in bulk operation)
    ASSERT_TRUE(states[5]); // skipped (not reached in bulk operation)
    ASSERT_TRUE(states[6]); // skipped (not reached in bulk operation)
    ASSERT_TRUE(states[7]); // skipped (not reached in bulk operation)
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(0, reader->docs_count()); // we have 3 documents in total (skipped docs have released their reservations)
    ASSERT_EQ(0, reader->live_docs_count()); // no docs were added

    // insert the 7th valid doc
    i = 7;
    max = 8;
    ASSERT_TRUE(writer.insert(bulk_inserter));
    ASSERT_TRUE(states[7]); // successfully inserted
    ASSERT_TRUE(writer.commit());

    reader = reader.reopen();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    ASSERT_EQ(5, reader->docs_count()); // we have 4 documents +1 for invalid doc in total (3 docs from failed batch have released their reservations)
    ASSERT_EQ(1, reader->live_docs_count()); // 1 document was added

    std::unordered_set<std::string> expected_values { "doc7" };
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

  irs::transaction_store store;
  irs::store_writer writer(store);

  i = 0;
  max = 2;
  std::fill(std::begin(states), std::end(states), true);
  ASSERT_TRUE(writer.insert(bulk_inserter));
  ASSERT_TRUE(states[0]); // successfully inserted
  ASSERT_TRUE(states[1]); // successfully inserted
  ASSERT_TRUE(states[2]); // skipped (not reached in bulk operation)

  ASSERT_TRUE(writer.commit());

  // check index
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    ASSERT_EQ(3, reader->docs_count()); // we have 2 documents +1 for invalid doc in total
    ASSERT_EQ(2, reader->live_docs_count()); // all (2) docs were added

    std::unordered_set<std::string> expected_values { "doc0", "doc1" };
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

TEST_F(transaction_store_tests, writer_begin_rollback) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );

  irs::bytes_ref actual_value;

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();

  // rolled back transaction
  {
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
  }

  {
    auto reader = store.reader();
    ASSERT_EQ(0, reader.docs_count());
  }

  // partially rolled back transaction
  {
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));

    ASSERT_TRUE(writer.commit());

    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
  }

  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(transaction_store_tests, insert_null_empty_term) {
  class field {
   public:
    field(std::string&& name, const irs::string_ref& value)
      : name_(std::move(name)), value_(value) {}
    field(field&& other) NOEXCEPT
      : stream_(std::move(other.stream_)),
        name_(std::move(other.name_)),
        value_(std::move(other.value_)) {}
    float_t boost() const { return 1.f; }
    const irs::flags& features() const { return irs::flags::empty_instance(); }
    irs::token_stream& get_tokens() const {
      stream_.reset(value_);
      return stream_;
    }
    irs::string_ref name() const { return name_; }


   private:
    mutable irs::string_token_stream stream_;
    std::string name_;
    irs::string_ref value_;
  }; // field

  irs::transaction_store store;

  // write docs with empty terms
  {
    irs::store_writer writer(store);

    // doc0: empty, nullptr
    std::vector<field> doc0;
    doc0.emplace_back(std::string("name"), irs::string_ref("", 0));
    doc0.emplace_back(std::string("name"), irs::string_ref::NIL);
    ASSERT_TRUE(writer.insert([&doc0](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc0.begin(), doc0.end());
      return false;
    }));

    // doc1: nullptr, empty, nullptr
    std::vector<field> doc1;
    doc1.emplace_back(std::string("name1"), irs::string_ref::NIL);
    doc1.emplace_back(std::string("name1"), irs::string_ref("", 0));
    doc1.emplace_back(std::string("name"), irs::string_ref::NIL);
    ASSERT_TRUE(writer.insert([&doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1.begin(), doc1.end());
      return false;
    }));

    ASSERT_TRUE(writer.commit());
  }

  // check fields with empty terms
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());

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

TEST_F(transaction_store_tests, europarl_docs) {
  tests::index_t expected;
  irs::transaction_store store;

  {
    tests::templates::europarl_doc_template doc;
    tests::delim_doc_generator gen(resource("europarl.subset.txt"), doc);

    add_segment(expected, store, gen);
  }

  assert_index(expected, store.reader());
}

TEST_F(transaction_store_tests, monarch_eco_onthology) {
  tests::index_t expected;
  irs::transaction_store store;

  {
    tests::json_doc_generator gen(
      resource("ECO_Monarch.json"), &tests::payloaded_json_field_factory
    );

    add_segment(expected, store, gen);
  }

  assert_index(expected, store.reader());
}

TEST_F(transaction_store_tests, concurrent_read_single_column_smoke_mt) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );
  std::vector<const tests::document*> expected_docs;

  // write some data into columnstore
  {
    irs::store_writer writer(store);

    for (auto* src = gen.next(); src; src = gen.next()) {
      ASSERT_TRUE(writer.insert([src](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(src->indexed.begin(), src->indexed.end());
        doc.insert<irs::Action::STORE>(src->stored.begin(), src->stored.end());
        return false;
      }));
      expected_docs.push_back(src);
    }

    ASSERT_TRUE(writer.commit());
  }

  auto reader = store.reader();
  auto read_columns = [&expected_docs, &reader]() {
    size_t i = 0;
    irs::bytes_ref actual_value;
    for (auto& segment: reader) {
      auto* column = segment.column_reader("name");
      if (!column) {
        return false;
      }
      auto values = column->values();
      const auto docs = segment.docs_count();
      for (irs::doc_id_t doc = (irs::type_limits<irs::type_t::doc_id_t>::min)(), end = segment.docs_count(); doc < end; ++doc) {
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

  for (auto& thread: pool) {
    thread.join();
  }

  ASSERT_TRUE(std::all_of(
    results.begin(), results.end(), [](int res) { return 1 == res; }
  ));
}

TEST_F(transaction_store_tests, concurrent_read_multiple_columns_mt) {
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

  irs::transaction_store store;

  // write columns
  {
    csv_doc_template_t csv_doc_template;
    tests::csv_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template);
    irs::store_writer writer(store);
    const tests::document* src;

    while ((src = gen.next())) {
      ASSERT_TRUE(writer.insert([src](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(src->indexed.begin(), src->indexed.end());
        doc.insert<irs::Action::STORE>(src->stored.begin(), src->stored.end());
        return false;
      }));
    }

    ASSERT_TRUE(writer.commit());
  }

  auto reader = store.reader();
  ASSERT_EQ(1, reader.size());
  auto& segment = *(reader.begin());

  // read columns
  {
    auto visit_column = [&segment](const irs::string_ref& column_name) {
      auto* meta = segment.column(column_name);
      if (!meta) {
        return false;
      }

      irs::doc_id_t expected_id = 0;
      csv_doc_template_t csv_doc_template;
      tests::csv_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template);
      auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& actual_value) {
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

    auto read_column_offset = [&segment](const irs::string_ref& column_name, irs::doc_id_t offset) {
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

    const irs::string_ref id_column = "id";
    const irs::string_ref label_column = "label";

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
      results.begin(), results.end(), [](int res) { return 1 == res; }
    ));
  }
}

TEST_F(transaction_store_tests, concurrent_read_index_mt) {
  tests::index_t expected_index;
  irs::transaction_store store;

  // write test docs
  {
    tests::json_doc_generator gen(
      resource("simple_single_column_multi_term.json"),
      &tests::payloaded_json_field_factory
    );

    add_segment(expected_index, store, gen);
  }

  auto actual_reader = store.reader();
  ASSERT_FALSE(!actual_reader);
  ASSERT_EQ(1, actual_reader.size());
  ASSERT_EQ(expected_index.size(), actual_reader.size());

  size_t thread_count = 16; // arbitrary value > 1
  std::vector<tests::field_reader> expected_segments;
  std::vector<const irs::term_reader*> expected_terms; // used to validate terms
  std::vector<irs::seek_term_iterator::ptr> expected_term_itrs; // used to validate docs

  auto& actual_segment = *(actual_reader.begin());
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

TEST_F(transaction_store_tests, concurrent_add_mt) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );
  std::vector<const tests::document*> docs;

  for (const tests::document* doc; (doc = gen.next()) != nullptr; docs.emplace_back(doc)) {}

  {
    std::thread thread0([&store, docs](){
      irs::store_writer writer(store);
      size_t records = 0;

      for (size_t i = 0, count = docs.size(); i < count; i += 2) {
        auto* src = docs[i];

        ASSERT_TRUE(writer.insert([src](irs::store_writer::document& doc)->bool {
          doc.insert<irs::Action::INDEX>(src->indexed.begin(), src->indexed.end());
          doc.insert<irs::Action::STORE>(src->stored.begin(), src->stored.end());
          return false;
        }));

        if (!(++records % 4)) {
          ASSERT_TRUE(writer.commit());
        }
      }

      ASSERT_TRUE(writer.commit());
    });
    std::thread thread1([&store, docs](){
      irs::store_writer writer(store);
      size_t records = 0;

      for (size_t i = 1, count = docs.size(); i < count; i += 2) {
        auto* src = docs[i];

        ASSERT_TRUE(writer.insert([src](irs::store_writer::document& doc)->bool {
          doc.insert<irs::Action::INDEX>(src->indexed.begin(), src->indexed.end());
          doc.insert<irs::Action::STORE>(src->stored.begin(), src->stored.end());
          return false;
        }));

        if (!(++records % 4)) {
          ASSERT_TRUE(writer.commit());
        }
      }

      ASSERT_TRUE(writer.commit());
    });

    thread0.join();
    thread1.join();

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(docs.size() + 1, reader.docs_count()); // +1 for invalid doc
  }
}

TEST_F(transaction_store_tests, concurrent_add_flush_mt) {
  auto codec = irs::formats::get("1_0");
  irs::memory_directory dir;
  auto dir_writer = irs::index_writer::make(dir, codec, irs::OM_CREATE);
  std::atomic<bool> done(false);
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );
  std::vector<const tests::document*> docs;

  for (const tests::document* doc; (doc = gen.next()) != nullptr; docs.emplace_back(doc)) {}

  {
    std::thread thread0a([&store, docs]()->void{
      irs::store_writer writer(store);
      size_t records = 0;

      for (size_t i = 0, count = docs.size(); i < count; i += 2) {
        auto* src = docs[i];

        ASSERT_TRUE(writer.insert([src](irs::store_writer::document& doc)->bool {
          doc.insert<irs::Action::INDEX>(src->indexed.begin(), src->indexed.end());
          doc.insert<irs::Action::STORE>(src->stored.begin(), src->stored.end());
          return false;
        }));

        if (!(++records % 4)) {
          ASSERT_TRUE(writer.commit());
        }
      }

      ASSERT_TRUE(writer.commit());
    });
    std::thread thread1a([&store, docs]()->void{
      irs::store_writer writer(store);
      size_t records = 0;

      for (size_t i = 1, count = docs.size(); i < count; i += 2) {
        auto* src = docs[i];

        ASSERT_TRUE(writer.insert([src](irs::store_writer::document& doc)->bool {
          doc.insert<irs::Action::INDEX>(src->indexed.begin(), src->indexed.end());
          doc.insert<irs::Action::STORE>(src->stored.begin(), src->stored.end());
          return false;
        }));

        if (!(++records % 4)) {
          ASSERT_TRUE(writer.commit());
        }
      }

      ASSERT_TRUE(writer.commit());
    });
    std::thread thread0f([&dir_writer, &store, &done]()->void{
      while(!done) {
        auto flushed = store.flush();
        ASSERT_TRUE(flushed && dir_writer->import(flushed));
      }
    });
    std::thread thread1f([&dir_writer, &store, &done]()->void{
      while(!done) {
        auto flushed = store.flush();
        ASSERT_TRUE(flushed && dir_writer->import(flushed));
      }
    });

    thread0a.join();
    thread1a.join();
    done = true;
    thread0f.join();
    thread1f.join();
  }

  // last flush
  {
    auto flushed = store.flush();

    ASSERT_TRUE(flushed && dir_writer->import(flushed));
    dir_writer->commit();
    ASSERT_TRUE(dir_writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));
    dir_writer->commit();
  }

  irs::bytes_ref actual_value;
  std::unordered_set<irs::string_ref> expected = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "~", "!", "@", "#", "$", "%" };
  auto reader = irs::directory_reader::open(dir);
  ASSERT_EQ(1, reader.size());
  auto& segment = reader[0]; // assume 0 is id of first/only segment
  const auto* column = segment.column_reader("name");
  ASSERT_NE(nullptr, column);
  auto values = column->values();
  ASSERT_EQ(32, segment.docs_count()); // total count of documents
  auto terms = segment.field("same");
  ASSERT_NE(nullptr, terms);
  auto termItr = terms->iterator();
  ASSERT_TRUE(termItr->next());
  auto docsItr = termItr->postings(irs::flags());
  while(docsItr->next()) {
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ(1, expected.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
  }

  ASSERT_TRUE(expected.empty());
}

TEST_F(transaction_store_tests, concurrent_add_remove_mt) {
  irs::transaction_store store;
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
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());

    std::thread thread0([&store, docs, &first_doc]() {
      irs::store_writer writer(store);
      size_t records = 0;
      auto* src = docs[0];
      ASSERT_TRUE(writer.insert([src](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(src->indexed.begin(), src->indexed.end());
        doc.insert<irs::Action::STORE>(src->stored.begin(), src->stored.end());
        return false;
      }));
      ASSERT_TRUE(writer.commit());
      first_doc = true;

      for (size_t i = 2, count = docs.size(); i < count; i += 2) { // skip first doc
        src = docs[i];
        ASSERT_TRUE(writer.insert([src](irs::store_writer::document& doc)->bool {
          doc.insert<irs::Action::INDEX>(src->indexed.begin(), src->indexed.end());
          doc.insert<irs::Action::STORE>(src->stored.begin(), src->stored.end());
          return false;
        }));

        if (!(++records % 4)) {
          ASSERT_TRUE(writer.commit());
        }
      }

      ASSERT_TRUE(writer.commit());
    });
    std::thread thread1([&store, docs](){
      irs::store_writer writer(store);
      size_t records = 0;

      for (size_t i = 1, count = docs.size(); i < count; i += 2) {
        auto* src = docs[i];
        ASSERT_TRUE(writer.insert([src](irs::store_writer::document& doc)->bool {
          doc.insert<irs::Action::INDEX>(src->indexed.begin(), src->indexed.end());
          doc.insert<irs::Action::STORE>(src->stored.begin(), src->stored.end());
          return false;
        }));

        if (!(++records % 4)) {
          ASSERT_TRUE(writer.commit());
        }
      }

      ASSERT_TRUE(writer.commit());
    });
    std::thread thread2([&store, &query_doc1, &first_doc](){
      irs::store_writer writer(store);
      while(!first_doc); // busy-wait until first document loaded
      writer.remove(std::move(query_doc1.filter));
      ASSERT_TRUE(writer.commit());
    });

    thread0.join();
    thread1.join();
    thread2.join();

    std::unordered_set<irs::string_ref> expected = { "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "~", "!", "@", "#", "$", "%" };
    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(docs.size() - 1, reader.live_docs_count());
    ASSERT_EQ(docs.size() + 1, reader.docs_count()); // +1 for invalid doc

    irs::bytes_ref actual_value;
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(irs::flags()));
    while(docsItr->next()) {
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ(1, expected.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    }

    ASSERT_TRUE(expected.empty());
  }
}

TEST_F(transaction_store_tests, doc_removal) {
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
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add + remove 1st (as reference)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    writer.remove(*(query_doc1.filter.get()));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add + remove 1st (as unique_ptr)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    writer.remove(std::move(query_doc1.filter));
    writer.remove(std::unique_ptr<irs::filter>(nullptr)); // test nullptr filter ignored
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add + remove 1st (as shared_ptr)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    writer.remove(std::shared_ptr<irs::filter>(std::move(query_doc1.filter)));
    writer.remove(std::shared_ptr<irs::filter>(nullptr)); // test nullptr filter ignored
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: remove + add
  {
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    writer.remove(std::move(query_doc2.filter)); // not present yet
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
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
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    writer.remove(std::move(query_doc1.filter));
    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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

  // new segment: add + remove, old segment: remove
  {
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    auto query_doc3 = irs::iql::query_builder().build("name==C", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    writer.remove(std::move(query_doc3.filter));
    ASSERT_TRUE(writer.commit()); // document mask with 'doc3' created
    writer.remove(std::move(query_doc2.filter));
    ASSERT_TRUE(writer.commit()); // new document mask with 'doc2','doc3' created

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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

  // new segment: add + add, old segment: remove + remove + add
  {
    auto query_doc1_doc2 = irs::iql::query_builder().build("name==A||name==B", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
    writer.remove(std::move(query_doc1_doc2.filter));
    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add, old segment: remove
  {
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    writer.remove(std::move(query_doc2.filter));
    writer.remove(std::unique_ptr<irs::filter>(nullptr)); // test nullptr filter ignored
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add + remove, old segment: remove
  {
    auto query_doc1_doc3 = irs::iql::query_builder().build("name==A || name==C", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc4](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
      doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
      return false;
    }));
    writer.remove(std::move(query_doc1_doc3.filter));
    writer.remove(std::shared_ptr<irs::filter>(nullptr)); // test nullptr filter ignored
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }

  // new segment: add + remove, old segment: add + remove old-old segment: remove
  {
    auto query_doc2_doc6_doc9 = irs::iql::query_builder().build("name==B||name==F||name==I", std::locale::classic());
    auto query_doc3_doc7 = irs::iql::query_builder().build("name==C||name==G", std::locale::classic());
    auto query_doc4 = irs::iql::query_builder().build("name==D", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    })); // A
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    })); // B
    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    })); // C
    ASSERT_TRUE(writer.insert([doc4](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
      doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
      return false;
    })); // D
    writer.remove(std::move(query_doc4.filter));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.insert([doc5](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc5->indexed.begin(), doc5->indexed.end());
      doc.insert<irs::Action::STORE>(doc5->stored.begin(), doc5->stored.end());
      return false;
    })); // E
    ASSERT_TRUE(writer.insert([doc6](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc6->indexed.begin(), doc6->indexed.end());
      doc.insert<irs::Action::STORE>(doc6->stored.begin(), doc6->stored.end());
      return false;
    })); // F
    ASSERT_TRUE(writer.insert([doc7](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc7->indexed.begin(), doc7->indexed.end());
      doc.insert<irs::Action::STORE>(doc7->stored.begin(), doc7->stored.end());
      return false;
    })); // G
    writer.remove(std::move(query_doc3_doc7.filter));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.insert([doc8](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc8->indexed.begin(), doc8->indexed.end());
      doc.insert<irs::Action::STORE>(doc8->stored.begin(), doc8->stored.end());
      return false;
    })); // H
    ASSERT_TRUE(writer.insert([doc9](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc9->indexed.begin(), doc9->indexed.end());
      doc.insert<irs::Action::STORE>(doc9->stored.begin(), doc9->stored.end());
      return false;
    })); // I
    writer.remove(std::move(query_doc2_doc6_doc9.filter));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("E", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc5
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc8
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(transaction_store_tests, doc_update) {
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
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.update(
      *(query_doc1.filter.get()),
      [doc2](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
        doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment update (as unique_ptr)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.update(
      std::move(query_doc1.filter),
      [doc2](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
        doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // new segment update (as shared_ptr)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.update(
      std::shared_ptr<irs::filter>(std::move(query_doc1.filter)),
      [doc2](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
        doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // old segment update
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.update(
      std::move(query_doc1.filter),
      [doc3](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
        doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    auto terms = segment.field("same");
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = segment.mask(termItr->postings(irs::flags()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // 3x updates (same segment)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    auto query_doc3 = irs::iql::query_builder().build("name==C", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.update(
      std::move(query_doc1.filter),
      [doc2](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
        doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.update(
      std::move(query_doc2.filter),
        [doc3](irs::store_writer::document& doc)->bool {
          doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
          doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
          return false;
        }
      ));
    ASSERT_TRUE(writer.update(
      std::move(query_doc3.filter),
      [doc4](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
        doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }

  // 3x updates (different segments)
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    auto query_doc3 = irs::iql::query_builder().build("name==C", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.update(
      std::move(query_doc1.filter),
      [doc2](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
        doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.update(
      std::move(query_doc2.filter),
      [doc3](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
        doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.update(
      std::move(query_doc3.filter),
      [doc4](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
        doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }

  // no matching documnts
  {
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.update(
      std::move(query_doc2.filter),
      [doc2](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
        doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
        return false;
      }
    )); // non-existent document
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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

  // update + delete (same segment)
  {
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.update(
      *(query_doc2.filter),
      [doc3](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
        doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
        return false;
      }
    ));
    writer.remove(*(query_doc2.filter)); // remove no longer existent
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // update + delete (different segments)
  {
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.update(
      *(query_doc2.filter),
      [doc3](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
        doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.commit());
    writer.remove(*(query_doc2.filter)); // remove no longer existent
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // delete + update (same segment)
  {
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    writer.remove(*(query_doc2.filter));
    ASSERT_TRUE(writer.update(
      *(query_doc2.filter),
      [doc3](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
        doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
        return false;
      }
    )); // update no longer existent
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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

  // delete + update (different segments)
  {
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
    writer.remove(*(query_doc2.filter));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.update(
      *(query_doc2.filter),
      [doc3](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
        doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
        return false;
      }
    )); // update no longer existent
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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

  // delete + update then update (2nd - update of modified doc) (same segment)
  {
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    auto query_doc3 = irs::iql::query_builder().build("name==C", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    writer.remove(*(query_doc2.filter));
    ASSERT_TRUE(writer.update(
      *(query_doc2.filter),
      [doc3](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
        doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.update(
      *(query_doc3.filter),
      [doc4](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
        doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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

  // delete + update then update (2nd - update of modified doc) (different segments)
  {
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());
    auto query_doc3 = irs::iql::query_builder().build("name==C", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
    writer.remove(*(query_doc2.filter));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.update(
      *(query_doc2.filter),
      [doc3](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
        doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.update(
      *(query_doc3.filter),
      [doc4](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
        doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
        return false;
      }
    ));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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

  // new segment failed update (due to field features mismatch or failed serializer)
  {
    class test_field: public tests::field_base {
     public:
      irs::flags features_;
      irs::string_token_stream tokens_;
      bool write_result_;
      virtual bool write(irs::data_output& out) const override { 
        out.write_byte(1);
        return write_result_;
      }
      virtual irs::token_stream& get_tokens() const override { return const_cast<test_field*>(this)->tokens_; }
      virtual const irs::flags& features() const override { return features_; }
    };

    tests::json_doc_generator gen(resource("simple_sequential.json"), &tests::generic_json_field_factory);
    auto doc1 = gen.next();
    auto doc2 = gen.next();
    auto doc3 = gen.next();
    auto doc4 = gen.next();
    auto doc5 = gen.next();
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    irs::transaction_store store;
    irs::store_writer writer(store);
    auto test_field0 = std::make_shared<test_field>();
    auto test_field1 = std::make_shared<test_field>();
    auto test_field2 = std::make_shared<test_field>();
    auto test_field3 = std::make_shared<test_field>();
    std::string test_field_name("test_field");

    test_field0->features_.add<irs::offset>().add<irs::frequency>(); // feature superset
    test_field1->features_.add<irs::offset>(); // feature subset of 'test_field0'
    test_field2->features_.add<irs::offset>();
    test_field3->features_.add<irs::increment>();
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

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    // field features subset
    ASSERT_FALSE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    // serializer returns false
    ASSERT_FALSE(writer.insert([doc4](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
      doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
      return false;
    }));
    // field features differ for doc3, doc5 OK
    std::vector<const tests::document*> doc53 = { /*doc5,*/ doc3 };
    ASSERT_FALSE(writer.update(
      *(query_doc1.filter.get()),
      [&doc53](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc53.back()->indexed.begin(), doc53.back()->indexed.end());
        doc.insert<irs::Action::STORE>(doc53.back()->stored.begin(), doc53.back()->stored.end());
        doc53.pop_back();
        return !doc53.empty();
      }
    ));
    ASSERT_TRUE(writer.commit());

    auto reader = store.reader();
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(transaction_store_tests, profile_bulk_index_singlethread_full_mt) {
  irs::transaction_store store;

  profile_bulk_index(store, 0, 0, 0);
}

TEST_F(transaction_store_tests, profile_bulk_index_singlethread_batched_mt) {
  irs::transaction_store store;

  profile_bulk_index(store, 0, 0, 10000);
}

TEST_F(transaction_store_tests, profile_bulk_index_multithread_flush_mt) {
  irs::transaction_store store;

  profile_bulk_index_dedicated_flush(store, 8, 10000, 1000);
}

TEST_F(transaction_store_tests, profile_bulk_index_multithread_full_mt) {
  irs::transaction_store store;

  profile_bulk_index(store, 16, 0, 0);
}

TEST_F(transaction_store_tests, profile_bulk_index_multithread_batched_mt) {
  irs::transaction_store store;

  profile_bulk_index(store, 16, 0, 10000);
}

TEST_F(transaction_store_tests, profile_bulk_index_multithread_update_full_mt) {
  irs::transaction_store store;

  profile_bulk_index(store, 16, 5, 0); // 5 does not divide evenly into 16
}

TEST_F(transaction_store_tests, profile_bulk_index_multithread_update_batched_mt) {
  irs::transaction_store store;

  profile_bulk_index(store, 16, 5, 10000); // 5 does not divide evenly into 16
}

TEST_F(transaction_store_tests, refresh_reader) {
  irs::transaction_store store;
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
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // refreshable reader
  auto reader = store.reader();

  // validate state
  {
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
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
    irs::store_writer writer(store);
    auto query_doc2 = irs::iql::query_builder().build("name==B", std::locale::classic());

    writer.remove(std::move(query_doc2.filter));
    ASSERT_TRUE(writer.commit());
  }

  // validate state pre/post refresh (existing segment changed)
  {
    {
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::flags());
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
      auto& segment = *(reader.begin());
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
  }

  // modify state (2nd segment 2 docs)
  {
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc4](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
      doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // validate state pre/post refresh (new segment added)
  {
    {
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());
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
      reader = reader.reopen();
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());
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
    irs::store_writer writer(store);
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());

    writer.remove(std::move(query_doc1.filter));
    ASSERT_TRUE(writer.commit());
  }

  // validate state pre/post refresh (old segment removed)
  {
    {
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());
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
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
    }

    {
      reader = reader.reopen();
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST_F(transaction_store_tests, reuse_segment_writer) {
  tests::index_t expected;
  irs::transaction_store store;
  tests::json_doc_generator gen0(resource("arango_demo.json"), &tests::generic_json_field_factory);
  tests::json_doc_generator gen1(resource("simple_sequential.json"), &tests::generic_json_field_factory);

  // populate initial 2 very small segments
  {
    {
      gen0.reset();
      add_segment(expected, store, gen0);
    }

    {
      irs::store_writer writer(store);
      gen1.reset();
      add_segment(expected.back(), writer, gen1);
      ASSERT_TRUE(writer.commit());
    }
  }

  // populate initial small segment
  {
    irs::store_writer writer(store);
    gen0.reset();
    add_segment(expected.back(), writer, gen0);
    gen1.reset();
    add_segment(expected.back(), writer, gen1);
    ASSERT_TRUE(writer.commit());
  }

  // populate initial large segment
  {
    irs::store_writer writer(store);

    for(size_t i = 100; i > 0; --i) {
      gen0.reset();
      add_segment(expected.back(), writer, gen0);
      gen1.reset();
      add_segment(expected.back(), writer, gen1);
    }

    ASSERT_TRUE(writer.commit());
  }

  irs::store_writer writer(store);

  // populate and validate small segments with store_writer reuse
  for(size_t i = 10; i > 0; --i) {
    // add varying sized segments
    for (size_t j = 0; j < i; ++j) {
      // add test documents
      if (i%3 == 0 || i%3 == 1) {
        gen0.reset();
        add_segment(expected.back(), writer, gen0);
      }

      // add different test docs (overlap to make every 3rd segment contain docs from both sources)
      if (i%3 == 1 || i%3 == 2) {
        gen1.reset();
        add_segment(expected.back(), writer, gen1);
      }
    }

    ASSERT_TRUE(writer.commit());
  }

  assert_index(expected, store.reader());
}

TEST_F(transaction_store_tests, segment_flush) {
  auto codec = irs::formats::get("1_0");
  irs::memory_directory dir;
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

  auto always_merge = [](
    irs::bitvector& candidates, const irs::directory& dir, const irs::index_meta& meta
  )->void {
    for (size_t i = meta.size(); i; candidates.set(--i)); // merge every segment
  };
  auto all_features = irs::flags{ irs::document::type(), irs::frequency::type(), irs::position::type(), irs::payload::type(), irs::offset::type() };

  // flush empty segment
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto dir_writer = irs::index_writer::make(dir, codec, irs::OM_CREATE);
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    writer.remove(std::move(query_doc1.filter));
    ASSERT_TRUE(writer.commit());

    {
      auto flushed = store.flush();

      ASSERT_TRUE(flushed && dir_writer->import(flushed));
      dir_writer->commit();
    }

    auto reader = irs::directory_reader::open(dir);
    ASSERT_EQ(0, reader.size());
  }

  // flush non-empty segment, remove on empty store
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    auto dir_writer = irs::index_writer::make(dir, codec, irs::OM_CREATE);
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());

    {
      auto flushed = store.flush();

      ASSERT_TRUE(flushed && dir_writer->import(flushed));
      writer.remove(std::move(query_doc1.filter)); // no such document in store
      ASSERT_TRUE(writer.commit());
      flushed = store.flush();
      ASSERT_TRUE(flushed && dir_writer->import(flushed));
      dir_writer->commit();
    }

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    tests::assert_index(dir, codec, expected, all_features);

    auto reader = irs::directory_reader::open(dir);
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
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_FALSE(docsItr->next());
  }

  // flush non-empty segment, remove on non-empty store (partial remove)
  {
    auto query_doc1_doc2 = irs::iql::query_builder().build("name==A||name==B", std::locale::classic());
    auto dir_writer = irs::index_writer::make(dir, codec, irs::OM_CREATE);
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
    auto flushed = store.flush();
    ASSERT_TRUE(flushed && dir_writer->import(flushed));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    writer.remove(std::move(query_doc1_doc2.filter));
    ASSERT_TRUE(writer.commit());
    flushed = store.flush();
    ASSERT_TRUE(flushed && dir_writer->import(flushed));
    dir_writer->commit();
    ASSERT_TRUE(dir_writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));
    dir_writer->commit();

    // validate structure
    tests::index_t expected;
    expected.emplace_back();
    expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
    expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
    tests::assert_index(dir, codec, expected, all_features);

    auto reader = irs::directory_reader::open(dir);
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of first/only segment
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    ASSERT_EQ(2, segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // flush non-empty segment with uncommited insert+remove, commit remainder (partial remove)
  {
    auto query_doc1_doc2 = irs::iql::query_builder().build("name==A||name==B", std::locale::classic());
    auto dir_writer = irs::index_writer::make(dir, codec, irs::OM_CREATE);
    irs::transaction_store store;
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    writer.remove(std::move(query_doc1_doc2.filter));

    {
      auto flushed = store.flush();

      ASSERT_TRUE(flushed && dir_writer->import(flushed));
      dir_writer->commit();
    }

    {
      // validate structure
      tests::index_t expected;
      expected.emplace_back();
      expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
      tests::assert_index(dir, codec, expected, all_features);
      ASSERT_EQ(0, store.reader().docs_count());
    }

    ASSERT_TRUE(writer.commit());

    // read unflushed after flush+commit
    {
      auto reader = store.reader();
      ASSERT_EQ(1, reader.size());
      auto& segment = *(reader.begin());
      ASSERT_EQ(4, segment.docs_count()); // total count of documents +1 for invalid doc
      ASSERT_EQ(1, segment.live_docs_count()); // total count of documents
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator();
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::flags());
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }

    {
      auto flushed = store.flush();

      ASSERT_TRUE(flushed && dir_writer->import(flushed));
      dir_writer->commit();
      ASSERT_TRUE(dir_writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count())));
      dir_writer->commit();
    }

    {
      // validate structure
      tests::index_t expected;
      expected.emplace_back();
      expected.back().add(doc1->indexed.begin(), doc1->indexed.end());
      expected.back().add(doc3->indexed.begin(), doc3->indexed.end());
      tests::assert_index(dir, codec, expected, all_features);
    }

    auto reader = irs::directory_reader::open(dir);
    ASSERT_EQ(1, reader.size());
    auto& segment = reader[0]; // assume 0 is id of the first/only segment
    ASSERT_EQ(2, segment.docs_count()); // total count of documents
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::flags());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  // validate doc_id reuse after flush
  {
    auto dir_writer = irs::index_writer::make(dir, codec, irs::OM_CREATE);
    irs::transaction_store store;

    // rolled back insert
    {
      irs::store_writer writer(store);

      ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
        doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
        return false;
      }));
    }

    // commited insert (after rolled back to be highest doc_id)
    {
      irs::store_writer writer(store);

      ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
        doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
        return false;
      }));
      ASSERT_TRUE(writer.commit());
    }

    // check intial docs count in store
    {
      auto reader = store.reader();
      ASSERT_EQ(1, reader.size());
      ASSERT_EQ(3, reader.docs_count()); // +1 for invalid doc
      ASSERT_EQ(1, reader.live_docs_count());
    }

    auto flushed = store.flush();
    ASSERT_TRUE(flushed && dir_writer->import(flushed));
    store.cleanup();

    // validate doc_id count
    {
      auto reader = store.reader();
      ASSERT_EQ(1, reader.size());
      ASSERT_EQ(0, reader.docs_count());
      ASSERT_EQ(0, reader.live_docs_count());
    }

    // instert two documents again
    {
      irs::store_writer writer(store);

      ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
        doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
        return false;
      }));
      ASSERT_TRUE(writer.insert([doc4](irs::store_writer::document& doc)->bool {
        doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
        doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
        return false;
      }));
      ASSERT_TRUE(writer.commit());
    }

    // validate that flushed doc_ids get reused (total docs count does not grow)
    {
      auto reader = store.reader();
      ASSERT_EQ(1, reader.size());
      ASSERT_EQ(3, reader.docs_count()); // +1 for invalid doc (same as above)
      ASSERT_EQ(2, reader.live_docs_count());
    }
  }
}

TEST_F(transaction_store_tests, rollback_uncommited) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();

  // read empty index, it should not fail
  {
    auto reader = store.reader();
    ASSERT_EQ(0, reader.live_docs_count());
    ASSERT_EQ(0, reader.docs_count());
    ASSERT_EQ(1, reader.size());
    ASSERT_NE(reader.begin(), reader.end());
  }

  // abort valid writer
  {
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());

    // read index
    {
      auto reader = store.reader();
      ASSERT_EQ(1, reader.live_docs_count());
      ASSERT_EQ(2, reader.docs_count()); // +1 for invalid doc
      ASSERT_EQ(1, reader.size());
      ASSERT_NE(reader.begin(), reader.end());
    }

    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
  }

  // read index
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.live_docs_count());
    ASSERT_EQ(2, reader.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader.size());
    ASSERT_NE(reader.begin(), reader.end());
  }

  // abort invalid writer
  {
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
  }

  // read index
  {
    auto reader = store.reader();
    ASSERT_EQ(1, reader.live_docs_count());
    ASSERT_EQ(2, reader.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader.size());
    ASSERT_NE(reader.begin(), reader.end());
  }
}

TEST_F(transaction_store_tests, read_old_generation) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );

  irs::bytes_ref actual_value;

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  // write 1st generation
  {
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.insert([doc2](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc2->indexed.begin(), doc2->indexed.end());
      doc.insert<irs::Action::STORE>(doc2->stored.begin(), doc2->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // read 1st generation
  auto reader0 = store.reader();
  ASSERT_EQ(2, reader0.live_docs_count());
  ASSERT_EQ(3, reader0.docs_count()); // +1 for invalid doc
  ASSERT_EQ(1, reader0.size());
  ASSERT_NE(reader0.begin(), reader0.end());

  // write 2nd generation
  {
    auto query_doc1 = irs::iql::query_builder().build("name==A", std::locale::classic());
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    writer.remove(std::move(query_doc1.filter));
    ASSERT_TRUE(writer.insert([doc4](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc4->indexed.begin(), doc4->indexed.end());
      doc.insert<irs::Action::STORE>(doc4->stored.begin(), doc4->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // read 2nd generation
  auto reader1 = store.reader();
  ASSERT_EQ(3, reader1.live_docs_count());
  ASSERT_EQ(5, reader1.docs_count()); // +1 for invalid doc
  ASSERT_EQ(1, reader1.size());
  ASSERT_NE(reader1.begin(), reader1.end());

  // validate 1st generation
  {
    ASSERT_EQ(1, reader0.size());
    auto& segment = *(reader0.begin());
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
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }

  // validate 2nd generation
  {
    ASSERT_EQ(1, reader1.size());
    auto& segment = *(reader1.begin());
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
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(transaction_store_tests, read_reopen) {
  irs::transaction_store store;
  tests::json_doc_generator gen(
    resource("simple_sequential.json"), &tests::generic_json_field_factory
  );
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  // write 1st generation
  {
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc1](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc1->indexed.begin(), doc1->indexed.end());
      doc.insert<irs::Action::STORE>(doc1->stored.begin(), doc1->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  auto reader = store.reader();
  ASSERT_EQ(1, reader.live_docs_count());
  ASSERT_EQ(2, reader.docs_count()); // +1 for invalid doc
  ASSERT_EQ(1, reader.size());
  ASSERT_NE(reader.begin(), reader.end());

  // read 1st generation (via new reader)
  {
    auto reader0 = store.reader();
    ASSERT_EQ(1, reader0.live_docs_count());
    ASSERT_EQ(2, reader0.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader0.size());
    ASSERT_NE(reader0.begin(), reader0.end());
    ASSERT_NE(&*(reader.begin()), &*(reader0.begin()));
  }

  // read 1st generation (via reopen)
  {
    auto reader0 = reader.reopen();
    ASSERT_EQ(1, reader0.live_docs_count());
    ASSERT_EQ(2, reader0.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader0.size());
    ASSERT_NE(reader0.begin(), reader0.end());
    ASSERT_EQ(&*(reader.begin()), &*(reader0.begin()));
  }

  // write 2nd generation
  {
    irs::store_writer writer(store);

    ASSERT_TRUE(writer.insert([doc3](irs::store_writer::document& doc)->bool {
      doc.insert<irs::Action::INDEX>(doc3->indexed.begin(), doc3->indexed.end());
      doc.insert<irs::Action::STORE>(doc3->stored.begin(), doc3->stored.end());
      return false;
    }));
    ASSERT_TRUE(writer.commit());
  }

  // read 2nd generation (via reopen)
  {
    auto reader0 = reader.reopen();
    ASSERT_EQ(2, reader0.live_docs_count());
    ASSERT_EQ(3, reader0.docs_count()); // +1 for invalid doc
    ASSERT_EQ(1, reader0.size());
    ASSERT_NE(reader0.begin(), reader0.end());
    ASSERT_NE(&*(reader.begin()), &*(reader0.begin()));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
