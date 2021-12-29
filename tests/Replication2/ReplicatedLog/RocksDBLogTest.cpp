////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <rocksdb/db.h>

#include <gtest/gtest.h>

#include <Basics/Exceptions.h>
#include <Basics/RocksDBUtils.h>
#include <Basics/files.h>
#include <RocksDBEngine/RocksDBFormat.h>
#include <RocksDBEngine/RocksDBPersistedLog.h>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

struct RocksDBLogTest : testing::Test {
 protected:
  static void SetUpTestCase() {
    rocksutils::setRocksDBKeyFormatEndianess(RocksDBEndianness::Little);
    _path = "rocksdb-log-test";
    rocksdb::Options opts;
    opts.create_if_missing = true;
    auto s = rocksdb::DB::Open(opts, _path, &_db);
    if (!s.ok()) {
      auto res = rocksutils::convertStatus(s);
      THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
    }

    struct SyncExecutor : RocksDBLogPersistor::Executor {
      void operator()(
          fu2::unique_function<void() noexcept> f) noexcept override {
        std::move(f).operator()();
      }
    };

    _persistor = std::make_shared<RocksDBLogPersistor>(
        _db->DefaultColumnFamily(), _db, std::make_shared<SyncExecutor>());
  }

  static void TearDownTestCase() {
    delete _db;
    _db = nullptr;
    TRI_RemoveDirectory(_path.c_str());
  }

  auto createLog(LogId id) -> std::unique_ptr<RocksDBPersistedLog> {
    if (id > _maxLogId) {
      _maxLogId = id;
    }

    return std::make_unique<RocksDBPersistedLog>(id, id.id(), _persistor);
  }

  auto createUniqueLog() -> std::unique_ptr<RocksDBPersistedLog> {
    return createLog(LogId{_maxLogId.id() + 1});
  }

  static LogId _maxLogId;
  static std::string _path;
  static rocksdb::DB* _db;
  static std::shared_ptr<RocksDBLogPersistor> _persistor;
};

LogId RocksDBLogTest::_maxLogId = LogId{0};
std::string RocksDBLogTest::_path = {};
rocksdb::DB* RocksDBLogTest::_db = nullptr;
std::shared_ptr<RocksDBLogPersistor> RocksDBLogTest::_persistor = nullptr;

template<typename I>
struct SimpleIterator : PersistedLogIterator {
  SimpleIterator(I begin, I end) : current(begin), end(end) {}
  ~SimpleIterator() override = default;

  auto next() -> std::optional<PersistingLogEntry> override {
    if (current == end) {
      return std::nullopt;
    }

    return *(current++);
  }

  I current, end;
};

template<typename C, typename Iter = typename C::const_iterator>
auto make_iterator(C const& c) -> std::shared_ptr<SimpleIterator<Iter>> {
  return std::make_shared<SimpleIterator<Iter>>(c.begin(), c.end());
}

auto make_iterator(std::initializer_list<PersistingLogEntry> c)
    -> std::shared_ptr<
        SimpleIterator<std::initializer_list<PersistingLogEntry>::iterator>> {
  return std::make_shared<
      SimpleIterator<std::initializer_list<PersistingLogEntry>::iterator>>(
      c.begin(), c.end());
}

TEST_F(RocksDBLogTest, insert_iterate) {
  auto log = createUniqueLog();

  {
    auto entries = std::vector{
        PersistingLogEntry{LogTerm{1}, LogIndex{1},
                           LogPayload::createFromString("first")},
        PersistingLogEntry{LogTerm{1}, LogIndex{2},
                           LogPayload::createFromString("second")},
        PersistingLogEntry{LogTerm{2}, LogIndex{3},
                           LogPayload::createFromString("third")},
        PersistingLogEntry{LogTerm{2}, LogIndex{1000},
                           LogPayload::createFromString("thousand")},
    };
    auto iter = make_iterator(entries);

    auto res = log->insert(*iter, {});
    ASSERT_TRUE(res.ok());
  }

  {
    auto entry = std::optional<PersistingLogEntry>{};
    auto iter = log->read(LogIndex{1});

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 1);
    ASSERT_EQ(entry->logTerm().value, 1);
    ASSERT_EQ(entry->logPayload(), LogPayload::createFromString("first"));

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 2);
    ASSERT_EQ(entry->logTerm().value, 1);
    ASSERT_EQ(entry->logPayload(), LogPayload::createFromString("second"));

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 3);
    ASSERT_EQ(entry->logTerm().value, 2);
    ASSERT_EQ(entry->logPayload(), LogPayload::createFromString("third"));

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 1000);
    ASSERT_EQ(entry->logTerm().value, 2);
    ASSERT_EQ(entry->logPayload(), LogPayload::createFromString("thousand"));

    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}

TEST_F(RocksDBLogTest, insert_remove_iterate) {
  auto log = createUniqueLog();

  {
    auto entries = std::vector{
        PersistingLogEntry{LogTerm{1}, LogIndex{1},
                           LogPayload::createFromString("first")},
        PersistingLogEntry{LogTerm{1}, LogIndex{2},
                           LogPayload::createFromString("second")},
        PersistingLogEntry{LogTerm{2}, LogIndex{3},
                           LogPayload::createFromString("third")},
        PersistingLogEntry{LogTerm{2}, LogIndex{999},
                           LogPayload::createFromString("nine-nine-nine")},
        PersistingLogEntry{LogTerm{2}, LogIndex{1000},
                           LogPayload::createFromString("thousand")},
    };
    auto iter = make_iterator(entries);

    auto res = log->insert(*iter, {});
    ASSERT_TRUE(res.ok());
  }

  {
    auto s = log->removeFront(LogIndex{1000});
    ASSERT_TRUE(s.ok());
  }

  {
    auto entry = std::optional<PersistingLogEntry>{};
    auto iter = log->read(LogIndex{1});

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 1000);
    ASSERT_EQ(entry->logTerm().value, 2);
    ASSERT_EQ(entry->logPayload(), LogPayload::createFromString("thousand"));

    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}

TEST_F(RocksDBLogTest, insert_iterate_remove_iterate) {
  auto log = createUniqueLog();

  {
    auto entries = std::vector{
        PersistingLogEntry{LogTerm{1}, LogIndex{1},
                           LogPayload::createFromString("first")},
        PersistingLogEntry{LogTerm{1}, LogIndex{2},
                           LogPayload::createFromString("second")},
        PersistingLogEntry{LogTerm{2}, LogIndex{3},
                           LogPayload::createFromString("third")},
        PersistingLogEntry{LogTerm{2}, LogIndex{999},
                           LogPayload::createFromString("nine-nine-nine")},
        PersistingLogEntry{LogTerm{2}, LogIndex{1000},
                           LogPayload::createFromString("thousand")},
    };
    auto iter = make_iterator(entries);

    auto res = log->insert(*iter, {});
    ASSERT_TRUE(res.ok());
  }

  auto iter = log->read(LogIndex{1});

  {
    auto s = log->removeFront(LogIndex{1000});
    ASSERT_TRUE(s.ok());
  }

  {
    auto entry = std::optional<PersistingLogEntry>{};

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 1);
    ASSERT_EQ(entry->logTerm().value, 1);
    ASSERT_EQ(entry->logPayload(), LogPayload::createFromString("first"));

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 2);
    ASSERT_EQ(entry->logTerm().value, 1);
    ASSERT_EQ(entry->logPayload(), LogPayload::createFromString("second"));

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 3);
    ASSERT_EQ(entry->logTerm().value, 2);
    ASSERT_EQ(entry->logPayload(), LogPayload::createFromString("third"));

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 999);
    ASSERT_EQ(entry->logTerm().value, 2);
    ASSERT_EQ(entry->logPayload(),
              LogPayload::createFromString("nine-nine-nine"));

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 1000);
    ASSERT_EQ(entry->logTerm().value, 2);
    ASSERT_EQ(entry->logPayload(), LogPayload::createFromString("thousand"));
    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}
