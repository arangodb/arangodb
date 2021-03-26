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

#include <Basics/RocksDBUtils.h>
#include <Basics/files.h>
#include <Replication2/InMemoryLog.h>
#include <RocksDBEngine/RocksDBFormat.h>
#include <RocksDBEngine/RocksDBLog.h>

using namespace arangodb;
using namespace arangodb::replication2;

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
  }

  static void TearDownTestCase() {
    delete _db;
    _db = nullptr;
    TRI_RemoveDirectory(_path.c_str());
  }

  auto createLog(LogId id) const -> std::unique_ptr<RocksDBLog> {
    if (id > _maxLogId) {
      _maxLogId = id;
    }

    return std::make_unique<RocksDBLog>(id, _db->DefaultColumnFamily(), _db, id.id());
  }

  auto createUniqueLog() const -> std::unique_ptr<RocksDBLog> {
    return createLog(LogId{_maxLogId.id() + 1});
  }

  static LogId _maxLogId;
  static std::string _path;
  static rocksdb::DB* _db;
};

LogId RocksDBLogTest::_maxLogId = LogId{0};
std::string RocksDBLogTest::_path = {};
rocksdb::DB* RocksDBLogTest::_db = nullptr;

template <typename I>
struct SimpleIterator : LogIterator {
  SimpleIterator(I begin, I end) : current(begin), end(end) {}
  ~SimpleIterator() override = default;

  auto next() -> std::optional<LogEntry> override {
    if (current == end) {
      return std::nullopt;
    }

    return *(current++);
  }

  I current, end;
};

template <typename C, typename Iter = typename C::const_iterator>
auto make_iterator(C const& c) -> std::shared_ptr<SimpleIterator<Iter>> {
  return std::make_shared<SimpleIterator<Iter>>(c.begin(), c.end());
}

auto make_iterator(std::initializer_list<LogEntry> c)
    -> std::shared_ptr<SimpleIterator<std::initializer_list<LogEntry>::iterator>> {
  return std::make_shared<SimpleIterator<std::initializer_list<LogEntry>::iterator>>(
      c.begin(), c.end());
}

TEST_F(RocksDBLogTest, insert_iterate) {
  auto log = createUniqueLog();

  {
    auto entries = std::vector{
        LogEntry{LogTerm{1}, LogIndex{1}, LogPayload{"first"}},
        LogEntry{LogTerm{1}, LogIndex{2}, LogPayload{"second"}},
        LogEntry{LogTerm{2}, LogIndex{3}, LogPayload{"third"}},
        LogEntry{LogTerm{2}, LogIndex{1000}, LogPayload{"thousand"}},
    };
    auto iter = make_iterator(entries);

    auto res = log->insert(iter);
    ASSERT_TRUE(res.ok());
  }

  {
    auto entry = std::optional<LogEntry>{};
    auto iter = log->read(LogIndex{1});

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 1);
    ASSERT_EQ(entry->logTerm().value, 1);
    ASSERT_EQ(entry->logPayload().dummy, "first");

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 2);
    ASSERT_EQ(entry->logTerm().value, 1);
    ASSERT_EQ(entry->logPayload().dummy, "second");

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 3);
    ASSERT_EQ(entry->logTerm().value, 2);
    ASSERT_EQ(entry->logPayload().dummy, "third");

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 1000);
    ASSERT_EQ(entry->logTerm().value, 2);
    ASSERT_EQ(entry->logPayload().dummy, "thousand");

    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}

TEST_F(RocksDBLogTest, insert_remove_iterate) {
  auto log = createUniqueLog();

  {
    auto entries = std::vector{
        LogEntry{LogTerm{1}, LogIndex{1}, LogPayload{"first"}},
        LogEntry{LogTerm{1}, LogIndex{2}, LogPayload{"second"}},
        LogEntry{LogTerm{2}, LogIndex{3}, LogPayload{"third"}},
        LogEntry{LogTerm{2}, LogIndex{1000}, LogPayload{"thousand"}},
    };
    auto iter = make_iterator(entries);

    auto res = log->insert(iter);
    ASSERT_TRUE(res.ok());
  }

  {
    auto s = log->remove(LogIndex{1000});
    ASSERT_TRUE(s.ok());
  }

  {
    auto entry = std::optional<LogEntry>{};
    auto iter = log->read(LogIndex{1});

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 1000);
    ASSERT_EQ(entry->logTerm().value, 2);
    ASSERT_EQ(entry->logPayload().dummy, "thousand");

    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}


TEST_F(RocksDBLogTest, insert_iterate_remove_iterate) {
  auto log = createUniqueLog();

  {
    auto entries = std::vector{
        LogEntry{LogTerm{1}, LogIndex{1}, LogPayload{"first"}},
        LogEntry{LogTerm{1}, LogIndex{2}, LogPayload{"second"}},
        LogEntry{LogTerm{2}, LogIndex{3}, LogPayload{"third"}},
        LogEntry{LogTerm{2}, LogIndex{1000}, LogPayload{"thousand"}},
    };
    auto iter = make_iterator(entries);

    auto res = log->insert(iter);
    ASSERT_TRUE(res.ok());
  }

  auto iter = log->read(LogIndex{1});

  {
    auto s = log->remove(LogIndex{1000});
    ASSERT_TRUE(s.ok());
  }

  {
    auto entry = std::optional<LogEntry>{};

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 1);
    ASSERT_EQ(entry->logTerm().value, 1);
    ASSERT_EQ(entry->logPayload().dummy, "first");

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 2);
    ASSERT_EQ(entry->logTerm().value, 1);
    ASSERT_EQ(entry->logPayload().dummy, "second");

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 3);
    ASSERT_EQ(entry->logTerm().value, 2);
    ASSERT_EQ(entry->logPayload().dummy, "third");

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    ASSERT_EQ(entry->logIndex().value, 1000);
    ASSERT_EQ(entry->logTerm().value, 2);
    ASSERT_EQ(entry->logPayload().dummy, "thousand");
    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}
