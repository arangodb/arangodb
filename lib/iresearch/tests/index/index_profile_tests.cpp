////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include <thread>

#include "index_tests.hpp"
#include "search/term_filter.hpp"
#include "store/fs_directory.hpp"
#include "store/memory_directory.hpp"
#include "store/mmap_directory.hpp"
#include "utils/file_utils.hpp"
#include "utils/index_utils.hpp"

namespace {
bool visit(const irs::column_reader& reader,
           const std::function<bool(irs::doc_id_t, irs::bytes_view)>& visitor) {
  auto it = reader.iterator(irs::ColumnHint::kConsolidation);

  irs::payload dummy;
  auto* doc = irs::get<irs::document>(*it);
  if (!doc) {
    return false;
  }
  auto* payload = irs::get<irs::payload>(*it);
  if (!payload) {
    payload = &dummy;
  }

  while (it->next()) {
    if (!visitor(doc->value, payload->value)) {
      return false;
    }
  }

  return true;
}
}  // namespace

class index_profile_test_case : public tests::index_test_base {
 private:
  std::atomic_uint64_t tick_{irs::writer_limits::kMinTick + 1};
  std::mutex commit_mutex;
  bool onTick_{false};

  void TransactionTick(irs::IndexWriter::Transaction& trx) {
    if (onTick_) {
      trx.RegisterFlush();
      trx.Commit(tick_.fetch_add(2) + 2);
    } else {
      trx.Commit();
    }
  }

  uint64_t CommitTick() noexcept {
    if (onTick_) {
      return tick_.load();
    }
    return irs::writer_limits::kMaxTick;
  }

 public:
  void SetOnTick(bool value) noexcept { onTick_ = value; }

  void profile_bulk_index(size_t num_insert_threads, size_t num_import_threads,
                          size_t num_update_threads, size_t batch_size,
                          irs::IndexWriter::ptr writer = nullptr,
                          std::atomic<size_t>* commit_count = nullptr) {
    struct csv_doc_template_t : public tests::csv_doc_generator::doc_template {
      std::vector<std::shared_ptr<tests::string_field>> fields;

      csv_doc_template_t() {
        fields.emplace_back(std::make_shared<tests::string_field>("id"));
        fields.emplace_back(std::make_shared<tests::string_field>("label"));
        reserve(fields.size());
      }

      virtual void init() {
        clear();

        for (auto& field : fields) {
          field->value(std::string_view{""});
          insert(field);
        }
      }

      virtual void value(size_t idx, const std::string_view& value) {
        IRS_ASSERT(idx < fields.size());
        fields[idx]->value(value);
      }
    };

    irs::timer_utils::init_stats(true);

    std::atomic<bool> import_again(true);
    irs::memory_directory import_dir;
    std::atomic<size_t> import_docs_count(0);
    size_t import_interval = 10000;
    irs::DirectoryReader import_reader;
    std::atomic<size_t> parsed_docs_count(0);
    size_t update_skip = 1000;
    size_t writer_batch_size =
      batch_size ? batch_size : (std::numeric_limits<size_t>::max)();
    std::atomic<size_t> local_writer_commit_count(0);
    std::atomic<size_t>& writer_commit_count =
      commit_count ? *commit_count : local_writer_commit_count;
    std::atomic<size_t> writer_import_count(0);
    auto thread_count = (std::max)((size_t)1, num_insert_threads);
    auto total_threads = thread_count + num_import_threads + num_update_threads;
    irs::async_utils::ThreadPool<> thread_pool(total_threads);
    std::mutex mutex;

    if (!writer) {
      irs::IndexWriterOptions options;
      // match original implementation or may run out of file handles
      // (e.g. MacOS/Travis)
      options.segment_count_max = 8;
      writer = open_writer(irs::OM_CREATE, options);
    }

    // initialize reader data source for import threads
    if (num_import_threads != 0) {
      auto import_writer =
        irs::IndexWriter::Make(import_dir, codec(), irs::OM_CREATE);

      {
        REGISTER_TIMER_NAMED_DETAILED("init - setup");
        tests::json_doc_generator import_gen{
          resource("simple_sequential.json"),
          &tests::generic_json_field_factory};

        for (const tests::document* doc; (doc = import_gen.next());) {
          REGISTER_TIMER_NAMED_DETAILED("init - insert");
          auto ctx = import_writer->GetBatch();
          {
            auto d = ctx.Insert();
            EXPECT_TRUE(d.Insert<irs::Action::INDEX>(doc->indexed.begin(),
                                                     doc->indexed.end()));
            EXPECT_TRUE(d.Insert<irs::Action::STORE>(doc->stored.begin(),
                                                     doc->stored.end()));
          }
          TransactionTick(ctx);
        }
      }

      {
        std::unique_lock commit_lock{commit_mutex};
        REGISTER_TIMER_NAMED_DETAILED("init - commit");
        import_writer->Commit({.tick = CommitTick()});
      }

      REGISTER_TIMER_NAMED_DETAILED("init - open");
      import_reader = irs::DirectoryReader(import_dir);
    }

    {
      std::lock_guard<std::mutex> lock(mutex);

      // register insertion jobs
      for (size_t i = 0; i < thread_count; ++i) {
        thread_pool.run([&mutex, &writer, thread_count, i, writer_batch_size,
                         &parsed_docs_count, &writer_commit_count,
                         &import_again, this] {
          {
            // wait for all threads to be registered
            std::lock_guard<std::mutex> lock(mutex);
          }

          csv_doc_template_t csv_doc_template;
          tests::csv_doc_generator gen(resource("simple_two_column.csv"),
                                       csv_doc_template);
          size_t gen_skip = i;

          for (size_t count = 0;; ++count) {
            // assume docs generated in same order and skip docs not meant for
            // this thread
            if (gen_skip--) {
              if (!gen.skip()) {
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
              auto ctx = writer->GetBatch();
              {
                auto d = ctx.Insert();
                EXPECT_TRUE(
                  d.Insert<irs::Action::INDEX>(csv_doc_template.indexed.begin(),
                                               csv_doc_template.indexed.end()));
                EXPECT_TRUE(
                  d.Insert<irs::Action::STORE>(csv_doc_template.stored.begin(),
                                               csv_doc_template.stored.end()));
              }
              TransactionTick(ctx);
            }

            if (count >= writer_batch_size) {
              std::unique_lock commit_lock{commit_mutex, std::try_to_lock};

              // break commit chains by skipping commit if one is already in
              // progress
              if (!commit_lock) {
                continue;
              }

              {
                REGISTER_TIMER_NAMED_DETAILED("commit");
                writer->Commit({.tick = CommitTick()});
              }

              count = 0;
              ++writer_commit_count;
            }
          }

          {
            std::unique_lock commit_lock{commit_mutex};
            REGISTER_TIMER_NAMED_DETAILED("commit");
            writer->Commit({.tick = CommitTick()});
          }

          ++writer_commit_count;
          import_again.store(false);  // stop any import threads, on completion
                                      // of any insert thread
        });
      }

      // register import jobs
      for (size_t i = 0; i < num_import_threads; ++i) {
        thread_pool.run([&mutex, &writer, import_reader, &import_docs_count,
                         &import_again, &import_interval,
                         &writer_import_count]() -> void {
          {
            // wait for all threads to be registered
            std::lock_guard<std::mutex> lock(mutex);
          }

          // ensure there will be at least 1 commit if scheduled
          do {
            import_docs_count += import_reader.docs_count();

            {
              REGISTER_TIMER_NAMED_DETAILED("import");
              writer->Import(import_reader);
            }

            ++writer_import_count;
            std::this_thread::sleep_for(
              std::chrono::milliseconds(import_interval));
          } while (import_again.load());
        });
      }

      // register update jobs
      for (size_t i = 0; i < num_update_threads; ++i) {
        thread_pool.run([&mutex, &writer, num_update_threads, i, update_skip,
                         writer_batch_size, &writer_commit_count, this] {
          {
            // wait for all threads to be registered
            std::lock_guard<std::mutex> lock(mutex);
          }

          csv_doc_template_t csv_doc_template;
          tests::csv_doc_generator gen(resource("simple_two_column.csv"),
                                       csv_doc_template);
          size_t doc_skip = update_skip;
          size_t gen_skip = i;

          for (size_t count = 0;; ++count) {
            // assume docs generated in same order and skip docs not meant for
            // this thread
            if (gen_skip--) {
              if (!gen.skip()) {
                break;
              }

              continue;
            }

            gen_skip = num_update_threads - 1;

            if (doc_skip--) {
              continue;  // skip docs originally meant to be updated by this
                         // thread
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
              irs::filter::ptr filter = std::make_unique<irs::by_term>();
              auto key_field = csv_doc_template.indexed.begin()->name();
              auto key_term =
                csv_doc_template.indexed.get<tests::string_field>(key_field)
                  ->value();
              auto value_field = (++(csv_doc_template.indexed.begin()))->name();
              auto value_term =
                csv_doc_template.indexed.get<tests::string_field>(value_field)
                  ->value();
              std::string updated_term(value_term.data(), value_term.size());

              auto& filter_impl = static_cast<irs::by_term&>(*filter);
              *filter_impl.mutable_field() = key_field;
              filter_impl.mutable_options()->term =
                irs::ViewCast<irs::byte_type>(key_term);
              // double up term
              updated_term.append(value_term.data(), value_term.size());
              csv_doc_template.indexed.get<tests::string_field>(value_field)
                ->value(updated_term);
              csv_doc_template.insert(
                std::make_shared<tests::string_field>("updated"));

              REGISTER_TIMER_NAMED_DETAILED("update");
              {
                auto ctx = writer->GetBatch();
                {
                  auto d = ctx.Replace(std::move(filter));
                  EXPECT_TRUE(d.Insert<irs::Action::INDEX>(
                    csv_doc_template.indexed.begin(),
                    csv_doc_template.indexed.end()));
                  EXPECT_TRUE(d.Insert<irs::Action::STORE>(
                    csv_doc_template.stored.begin(),
                    csv_doc_template.stored.end()));
                }
                TransactionTick(ctx);
              }
            }

            if (count >= writer_batch_size) {
              std::unique_lock commit_lock{commit_mutex, std::try_to_lock};

              // break commit chains by skipping commit if one is already in
              // progress
              if (!commit_lock) {
                continue;
              }

              {
                REGISTER_TIMER_NAMED_DETAILED("commit");
                writer->Commit({.tick = CommitTick()});
              }

              count = 0;
              ++writer_commit_count;
            }
          }

          {
            std::unique_lock commit_lock{commit_mutex};
            REGISTER_TIMER_NAMED_DETAILED("commit");
            writer->Commit({.tick = CommitTick()});
          }

          ++writer_commit_count;
        });
      }
    }

    thread_pool.stop();

    // ensure all data have been committed
    {
      std::unique_lock commit_lock{commit_mutex};
      writer->Commit({.tick = CommitTick()});
      EXPECT_FALSE(writer->Commit());
    }

    auto path = test_dir();

    path /= "profile_bulk_index.log";

    std::ofstream out(path.native());

    irs::timer_utils::flush_stats(out);

    out.close();

    irs::file_utils::ensure_absolute(path);
    std::cout << "Path to timing log: " << path.string() << std::endl;

    auto reader = irs::DirectoryReader(dir(), codec());
    // not all commits might produce a new segment,
    ASSERT_LE(1, reader.size());
    // some might merge with concurrent commits
    ASSERT_TRUE(writer_commit_count * thread_count + writer_import_count >=
                reader.size());  // worst case each thread is concurrently
                                 // populating its own segment for every commit

    size_t indexed_docs_count = 0;
    size_t imported_docs_count = 0;
    size_t updated_docs_count = 0;
    auto imported_visitor = [&imported_docs_count](
                              irs::doc_id_t, const irs::bytes_view&) -> bool {
      ++imported_docs_count;
      return true;
    };
    auto updated_visitor = [&updated_docs_count](
                             irs::doc_id_t, const irs::bytes_view&) -> bool {
      ++updated_docs_count;
      return true;
    };

    for (size_t i = 0, count = reader.size(); i < count; ++i) {
      indexed_docs_count += reader[i].live_docs_count();

      const auto* column = reader[i].column("same");
      if (column) {
        // field present in all docs from simple_sequential.json
        visit(*column, imported_visitor);
      }

      column = reader[i].column("updated");
      if (column) {
        // field inserted by updater threads
        visit(*column, updated_visitor);
      }
    }

    EXPECT_EQ(parsed_docs_count + imported_docs_count, indexed_docs_count)
      << parsed_docs_count << " " << imported_docs_count;
    EXPECT_EQ(imported_docs_count, import_docs_count);
    // at least some imports took place if import enabled
    EXPECT_TRUE(imported_docs_count != 0 || num_import_threads == 0);
    // at least some updates took place if update enabled
    EXPECT_TRUE(updated_docs_count != 0 || num_update_threads == 0);
  }

  void profile_bulk_index_dedicated_cleanup(size_t num_threads,
                                            size_t batch_size,
                                            size_t cleanup_interval) {
    auto* directory = &dir();
    std::atomic<bool> working(true);
    irs::async_utils::ThreadPool<> thread_pool(1);

    thread_pool.run([cleanup_interval, directory, &working]() -> void {
      while (working.load()) {
        irs::directory_cleaner::clean(*directory);
        std::this_thread::sleep_for(
          std::chrono::milliseconds(cleanup_interval));
      }
    });

    {
      irs::Finally finalizer = [&working]() noexcept { working = false; };
      profile_bulk_index(num_threads, 0, 0, batch_size);
    }

    thread_pool.stop();
  }

  void profile_bulk_index_dedicated_commit(size_t insert_threads,
                                           size_t commit_threads,
                                           size_t commit_interval) {
    irs::IndexWriterOptions options;
    std::atomic<bool> working(true);
    std::atomic<size_t> writer_commit_count(0);

    commit_threads = (std::max)(size_t(1), commit_threads);
    options.segment_count_max = 8;  // match original implementation or may run
                                    // out of file handles (e.g. MacOS/Travis)

    irs::async_utils::ThreadPool<> thread_pool(commit_threads);
    auto writer = open_writer(irs::OM_CREATE, options);

    for (size_t i = 0; i < commit_threads; ++i) {
      thread_pool.run(
        [commit_interval, &working, &writer, &writer_commit_count, this] {
          while (working.load()) {
            {
              std::unique_lock commit_lock{commit_mutex};
              REGISTER_TIMER_NAMED_DETAILED("commit");
              writer->Commit({.tick = CommitTick()});
            }
            ++writer_commit_count;
            std::this_thread::sleep_for(
              std::chrono::milliseconds(commit_interval));
          }
        });
    }

    {
      irs::Finally finalizer = [&working]() noexcept { working = false; };
      profile_bulk_index(insert_threads, 0, 0, 0, writer, &writer_commit_count);
    }

    thread_pool.stop();
  }

  void profile_bulk_index_dedicated_consolidate(size_t num_threads,
                                                size_t batch_size,
                                                size_t consolidate_interval) {
    const auto policy =
      irs::index_utils::MakePolicy(irs::index_utils::ConsolidateCount());
    irs::IndexWriterOptions options;
    std::atomic<bool> working(true);
    irs::async_utils::ThreadPool<> thread_pool(2);

    options.segment_count_max = 8;  // match original implementation or may run
                                    // out of file handles (e.g. MacOS/Travis)

    auto writer = open_writer(irs::OM_CREATE, options);

    thread_pool.run(
      [consolidate_interval, &working, &writer, &policy]() -> void {
        while (working.load()) {
          writer->Consolidate(policy);
          std::this_thread::sleep_for(
            std::chrono::milliseconds(consolidate_interval));
        }
      });

    {
      irs::Finally finalizer = [&working]() noexcept { working = false; };
      profile_bulk_index(num_threads, 0, 0, batch_size, writer);
    }

    thread_pool.stop();
    // ensure there are no consolidation-pending segments
    // left in 'consolidating_segments_' before applying the final consolidation
    {
      std::unique_lock commit_lock{commit_mutex};
      writer->Commit({.tick = CommitTick()});
      EXPECT_FALSE(writer->Commit());
    }
    ASSERT_TRUE(writer->Consolidate(policy));
    {
      std::unique_lock commit_lock{commit_mutex};
      writer->Commit({.tick = CommitTick()});
      EXPECT_FALSE(writer->Commit());
    }

    struct dummy_doc_template_t
      : public tests::csv_doc_generator::doc_template {
      virtual void init() {}
      virtual void value(size_t, const std::string_view&) {}
    };
    dummy_doc_template_t dummy_doc_template;
    tests::csv_doc_generator gen(resource("simple_two_column.csv"),
                                 dummy_doc_template);
    size_t docs_count = 0;

    // determine total number of docs in source data
    while (gen.next()) {
      ++docs_count;
    }

    auto reader = irs::DirectoryReader(dir(), codec());
    ASSERT_EQ(1, reader.size());
    ASSERT_EQ(docs_count, reader[0].docs_count());
  }
};

TEST_P(index_profile_test_case, profile_bulk_index_singlethread_full_mt) {
  profile_bulk_index(0, 0, 0, 0);
}

TEST_P(index_profile_test_case, profile_bulk_index_singlethread_batched_mt) {
  profile_bulk_index(0, 0, 0, 10000);
}

TEST_P(index_profile_test_case, profile_bulk_index_multithread_cleanup_mt) {
  tests::dir_param_f factory;
  std::tie(factory, std::ignore) = GetParam();

  profile_bulk_index_dedicated_cleanup(16, 10000, 100);
}

TEST_P(index_profile_test_case, profile_bulk_index_multithread_consolidate_mt) {
  // a lot of threads cause a lot of contention for the segment pool
  profile_bulk_index_dedicated_consolidate(8, 10000, 500);
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_dedicated_commit_mt) {
  profile_bulk_index_dedicated_commit(16, 1, 1000);
}

TEST_P(index_profile_test_case, profile_bulk_index_multithread_full_mt) {
  profile_bulk_index(16, 0, 0, 0);
}

TEST_P(index_profile_test_case, profile_bulk_index_multithread_batched_mt) {
  profile_bulk_index(16, 0, 0, 10000);
}

TEST_P(index_profile_test_case, profile_bulk_index_multithread_import_full_mt) {
  profile_bulk_index(12, 4, 0, 0);
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_import_batched_mt) {
  profile_bulk_index(12, 4, 0, 10000);
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_import_update_full_mt) {
  profile_bulk_index(9, 7, 5, 0);  // 5 does not divide evenly into 9 or 7
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_import_update_batched_mt) {
  profile_bulk_index(9, 7, 5, 10000);  // 5 does not divide evenly into 9 or 7
}

TEST_P(index_profile_test_case, profile_bulk_index_multithread_update_full_mt) {
  profile_bulk_index(16, 0, 5, 0);  // 5 does not divide evenly into 16
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_update_batched_mt) {
  profile_bulk_index(16, 0, 5, 10000);  // 5 does not divide evenly into 16
}

TEST_P(index_profile_test_case, profile_bulk_index_singlethread_full_mt_tick) {
  SetOnTick(true);
  profile_bulk_index(0, 0, 0, 0);
}

TEST_P(index_profile_test_case,
       profile_bulk_index_singlethread_batched_mt_tick) {
  SetOnTick(true);
  profile_bulk_index(0, 0, 0, 10000);
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_cleanup_mt_tick) {
  SetOnTick(true);
  tests::dir_param_f factory;
  std::tie(factory, std::ignore) = GetParam();

  profile_bulk_index_dedicated_cleanup(16, 10000, 100);
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_consolidate_mt_tick) {
  SetOnTick(true);
  // a lot of threads cause a lot of contention for the segment pool
  profile_bulk_index_dedicated_consolidate(8, 10000, 500);
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_dedicated_commit_mt_tick) {
  SetOnTick(true);
  profile_bulk_index_dedicated_commit(16, 1, 1000);
}

TEST_P(index_profile_test_case, profile_bulk_index_multithread_full_mt_tick) {
  SetOnTick(true);
  profile_bulk_index(16, 0, 0, 0);
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_batched_mt_tick) {
  SetOnTick(true);
  profile_bulk_index(16, 0, 0, 10000);
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_import_full_mt_tick) {
  SetOnTick(true);
  profile_bulk_index(12, 4, 0, 0);
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_import_batched_mt_tick) {
  SetOnTick(true);
  profile_bulk_index(12, 4, 0, 10000);
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_import_update_full_mt_tick) {
  SetOnTick(true);
  profile_bulk_index(9, 7, 5, 0);  // 5 does not divide evenly into 9 or 7
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_import_update_batched_mt_tick) {
  SetOnTick(true);
  profile_bulk_index(9, 7, 5, 10000);  // 5 does not divide evenly into 9 or 7
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_update_full_mt_tick) {
  SetOnTick(true);
  profile_bulk_index(16, 0, 5, 0);  // 5 does not divide evenly into 16
}

TEST_P(index_profile_test_case,
       profile_bulk_index_multithread_update_batched_mt_tick) {
  SetOnTick(true);
  profile_bulk_index(16, 0, 5, 10000);  // 5 does not divide evenly into 16
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(
  index_profile_test, index_profile_test_case,
  ::testing::Combine(::testing::ValuesIn(kTestDirs),
                     ::testing::Values("1_0", "1_2", "1_3", "1_4", "1_5")),
  index_profile_test_case::to_string);
