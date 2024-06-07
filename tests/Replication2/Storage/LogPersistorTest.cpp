////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include <gmock/gmock.h>

#include "Basics/ResultT.h"
#include "Futures/Future.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/InMemoryLogEntry.h"
#include "Replication2/ReplicatedLog/LogEntry.h"
#include "Replication2/ReplicatedLog/LogMetaPayload.h"
#include "Replication2/ReplicatedLog/PersistedLogEntry.h"
#include "Replication2/Storage/WAL/FileHeader.h"
#include "Replication2/Storage/WAL/LogPersistor.h"
#include "Replication2/Storage/WAL/Record.h"

#include "Replication2/Storage/InMemoryLogFile.h"
#include "Replication2/Storage/StreamReader.h"
#include "Replication2/Storage/MockFileManager.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"

#include <absl/crc/crc32c.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <immer/flex_vector_transient.hpp>
#include <ios>
#include <memory>
#include <variant>

using namespace ::testing;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

namespace {
auto makeNormalLogEntry(std::uint64_t term, std::uint64_t index,
                        std::string payload) {
  return InMemoryLogEntry{LogEntry{LogTerm{term}, LogIndex{index},
                                   LogPayload::createFromString(payload)}};
}

auto makeMetaLogEntry(std::uint64_t term, std::uint64_t index,
                      LogMetaPayload payload) {
  return InMemoryLogEntry{LogEntry{LogTerm{term}, LogIndex{index}, payload}};
}

auto paddedPayloadSize(std::size_t payloadSize) -> std::size_t {
  // we intentionally use a different implementation to calculate the padded
  // size to implicitly test the other paddedPayloadSize implementation
  using arangodb::replication2::storage::wal::Record;
  return ((payloadSize + Record::alignment - 1) / 8) * 8;
}

}  // namespace

namespace arangodb::replication2::storage::wal::test {

struct LogPersistorTest : ::testing::Test {
  void SetUp() override {
    auto file = std::make_unique<InMemoryFileWriter>(buffer);
    EXPECT_CALL(*fileManager, createWriter(_))
        .WillOnce(Return(std::move(file)));
    persistor = std::make_unique<LogPersistor>(LogId{42}, fileManager);
  }

  void insertEntries() {
    log = InMemoryLog{}.append({
        makeNormalLogEntry(1, 1, "blubb"),
        makeNormalLogEntry(1, 2, "dummyPayload"),
        makeMetaLogEntry(
            1, 3,
            LogMetaPayload::withPing(
                "message",
                // the timepoint is serialized as seconds, so we must avoid
                // sub-second precision for simple equality comparison
                LogMetaPayload::Ping::clock::time_point{
                    std::chrono::milliseconds(123000)})),
        makeNormalLogEntry(1, 4, "entry with somewhat larger payload"),
        makeNormalLogEntry(2, 5, "foobar"),
    });

    auto res =
        persistor->insert(log.getLogIterator(), LogPersistor::WriteOptions{})
            .waitAndGet();
    ASSERT_TRUE(res.ok());
    EXPECT_EQ(res.get(), 5);
  }

  void checkFileHeader(StreamReader& reader) {
    auto header = reader.read<FileHeader>();
    EXPECT_EQ(wMagicFileType, header.magic);
    EXPECT_EQ(wCurrentVersion, header.version);
  }

  void checkLogEntry(StreamReader& reader, LogIndex idx, LogTerm term,
                     RecordType type,
                     std::variant<LogPayload, LogMetaPayload> payload) {
    auto* data = reader.data();
    auto dataSize = reader.size();

    VPackBuilder builder;
    VPackSlice payloadSlice;
    if (std::holds_alternative<LogPayload>(payload)) {
      auto const& p = get<LogPayload>(payload);
      payloadSlice = p.slice();
    } else {
      auto const& p = get<LogMetaPayload>(payload);
      p.toVelocyPack(builder);
      payloadSlice = builder.slice();
    }

    auto expectedSize = sizeof(Record::CompressedHeader) +
                        ::paddedPayloadSize(payloadSlice.byteSize())  // payload
                        + sizeof(Record::Footer);
    ASSERT_EQ(dataSize, expectedSize);

    auto compressedHeader = reader.read<Record::CompressedHeader>();
    auto header = Record::Header{compressedHeader};

    EXPECT_EQ(idx.value, header.index) << "Log index mismatch";
    EXPECT_EQ(term.value, header.term) << "Log term mismatch";
    EXPECT_EQ(type, header.type) << "Entry type mismatch";
    EXPECT_EQ(payloadSlice.byteSize(), header.payloadSize) << "size mismatch";

    EXPECT_TRUE(memcmp(reader.data(), payloadSlice.getDataPtr(),
                       payloadSlice.byteSize()) == 0)
        << "Payload mismatch";

    auto paddedSize = ::paddedPayloadSize(header.payloadSize);
    reader.skip(paddedSize);

    auto footer = reader.read<Record::Footer>();

    auto expectedCrc = static_cast<std::uint32_t>(
        absl::ComputeCrc32c({reinterpret_cast<char const*>(data),
                             sizeof(Record::CompressedHeader) + paddedSize}));
    EXPECT_EQ(expectedCrc, footer.crc32);
    EXPECT_EQ(expectedSize, footer.size);
  }

  void skip(std::unique_ptr<LogIterator>& iter, size_t num) {
    for (size_t i = 0; i < num; ++i) {
      auto v = iter->next();
      ASSERT_TRUE(v.has_value());
    }
  }

  void checkIterators(std::unique_ptr<PersistedLogIterator> actualIter,
                      std::unique_ptr<LogIterator> expectedIter,
                      size_t expectedSize) {
    ASSERT_NE(nullptr, actualIter);
    ASSERT_NE(nullptr, expectedIter);

    auto actual = actualIter->next();
    auto expected = expectedIter->next();
    size_t count = 0;
    while (expected.has_value()) {
      ASSERT_TRUE(actual.has_value());
      EXPECT_EQ(expected->logIndex(), actual->entry().logIndex());
      EXPECT_EQ(expected->logTerm(), actual->entry().logTerm());
      if (expected->hasPayload()) {
        ASSERT_TRUE(actual->entry().hasPayload());
        auto* expectedPayload = expected->logPayload();
        ASSERT_NE(nullptr, expectedPayload);
        auto* actualPayload = actual->entry().logPayload();
        ASSERT_NE(nullptr, actualPayload);
        ASSERT_EQ(expectedPayload->byteSize(), actualPayload->byteSize());
        ASSERT_TRUE(
            expectedPayload->slice().binaryEquals(actualPayload->slice()));
      } else {
        ASSERT_TRUE(expected->hasMeta());
        ASSERT_TRUE(actual->entry().hasMeta());
        auto* expectedPayload = expected->meta();
        ASSERT_NE(nullptr, expectedPayload);
        auto* actualPayload = actual->entry().meta();
        ASSERT_NE(nullptr, actualPayload);
        EXPECT_EQ(*expectedPayload, *actualPayload);
      }
      ++count;

      actual = actualIter->next();
      expected = expectedIter->next();
    }
    EXPECT_FALSE(actual.has_value());
    EXPECT_EQ(expectedSize, count);
  }

  std::string buffer;
  std::shared_ptr<MockFileManager> fileManager{
      std::make_shared<MockFileManager>()};

  InMemoryLog log;
  std::unique_ptr<LogPersistor> persistor;
};

TEST_F(LogPersistorTest, drop_calls_file_manager_removeAll) {
  EXPECT_CALL(*fileManager, removeAll).Times(1);
  persistor->drop();
  Mock::VerifyAndClearExpectations(fileManager.get());
}

TEST_F(LogPersistorTest, insert_normal_payload) {
  auto payload = LogPayload::createFromString("foobar");
  log = InMemoryLog{}.append(
      {InMemoryLogEntry{LogEntry{LogTerm{1}, LogIndex{100}, payload}}});

  auto res =
      persistor->insert(log.getLogIterator(), LogPersistor::WriteOptions{})
          .waitAndGet();
  ASSERT_TRUE(res.ok());
  EXPECT_EQ(res.get(), 100);

  StreamReader reader{buffer.data(), buffer.size()};
  checkFileHeader(reader);
  checkLogEntry(reader, LogIndex{100}, LogTerm{1}, RecordType::wNormal,
                payload);
}

TEST_F(LogPersistorTest, insert_meta_payload) {
  LogMetaPayload::Ping::clock::time_point tp{};
  auto payload = LogMetaPayload::withPing("message", tp);
  log = InMemoryLog{}.append(
      {InMemoryLogEntry{LogEntry{LogTerm{1}, LogIndex{100}, payload}}});

  auto res =
      persistor->insert(log.getLogIterator(), LogPersistor::WriteOptions{})
          .waitAndGet();
  ASSERT_TRUE(res.ok());
  EXPECT_EQ(res.get(), 100);

  StreamReader reader{buffer.data(), buffer.size()};
  checkFileHeader(reader);
  checkLogEntry(reader, LogIndex{100}, LogTerm{1}, RecordType::wMeta, payload);
}

TEST_F(LogPersistorTest, getIterator) {
  insertEntries();
  auto iter =
      persistor->getIterator(IteratorPosition::fromLogIndex(LogIndex{0}));
  ASSERT_NE(nullptr, iter);

  auto logIter = log.getLogIterator();
  checkIterators(std::move(iter), std::move(logIter), 5);
}

TEST_F(LogPersistorTest, getIterator_seeks_to_log_index) {
  insertEntries();
  auto iter =
      persistor->getIterator(IteratorPosition::fromLogIndex(LogIndex{3}));

  auto logIter = log.getLogIterator();
  skip(logIter, 2);

  checkIterators(std::move(iter), std::move(logIter), 3);
}

TEST_F(LogPersistorTest,
       getIterator_for_position_from_returned_entry_seeks_to_same_entry) {
  insertEntries();
  auto iter =
      persistor->getIterator(IteratorPosition::fromLogIndex(LogIndex{3}));

  ASSERT_NE(nullptr, iter);
  auto entry = iter->next();
  {
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(LogIndex{3}, entry->entry().logIndex());
    EXPECT_EQ(LogTerm{1}, entry->entry().logTerm());
  }
  iter = persistor->getIterator(entry->position());
  ASSERT_NE(nullptr, iter);
  entry = iter->next();
  {
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(LogIndex{3}, entry->entry().logIndex());
    EXPECT_EQ(LogTerm{1}, entry->entry().logTerm());
  }
}

TEST_F(LogPersistorTest, removeBack) {
  insertEntries();

  auto res = persistor->removeBack(LogIndex{3}, {}).waitAndGet();
  ASSERT_TRUE(res.ok()) << res.errorMessage();

  auto iter =
      persistor->getIterator(IteratorPosition::fromLogIndex(LogIndex{0}));
  ASSERT_NE(nullptr, iter);
  auto entry = iter->next();
  {
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(LogIndex{1}, entry->entry().logIndex());
    EXPECT_EQ(LogTerm{1}, entry->entry().logTerm());
  }

  entry = iter->next();
  {
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(LogIndex{2}, entry->entry().logIndex());
    EXPECT_EQ(LogTerm{1}, entry->entry().logTerm());
  }

  entry = iter->next();
  EXPECT_FALSE(entry.has_value());

  {
    auto log = InMemoryLog{}.append({
        makeNormalLogEntry(2, 3, "override"),
    });
    auto res =
        persistor->insert(log.getLogIterator(), LogPersistor::WriteOptions{})
            .waitAndGet();
    ASSERT_TRUE(res.ok()) << res.errorMessage();
    EXPECT_EQ(res.get(), 3);
  }
}

TEST_F(LogPersistorTest, removeBack_fails_no_matching_entry_found) {
  auto res = persistor->removeBack(LogIndex{2}, {}).waitAndGet();
  ASSERT_TRUE(res.fail());
  EXPECT_EQ("log file in-memory file is empty", res.errorMessage());
}

TEST_F(LogPersistorTest, removeBack_fails_if_log_file_corrupt) {
  // we simulate a corrupt log file by writing some garbage in the memory buffer
  buffer = "xxxxyyyyzzzz";
  FileHeader header = {.magic = wMagicFileType, .version = wCurrentVersion};
  TRI_ASSERT(buffer.size() > sizeof(header));
  std::memcpy(buffer.data(), &header, sizeof(header));

  auto res = persistor->removeBack(LogIndex{2}, {}).waitAndGet();
  ASSERT_TRUE(res.fail());
}

TEST_F(LogPersistorTest, removeBack_fails_if_start_index_too_small) {
  {
    auto log = InMemoryLog{}.append({
        makeNormalLogEntry(1, 4, "blubb"),
        makeNormalLogEntry(1, 5, "dummyPayload"),
        makeNormalLogEntry(1, 6, "foobar"),
    });
    auto res =
        persistor->insert(log.getLogIterator(), LogPersistor::WriteOptions{})
            .waitAndGet();
    ASSERT_TRUE(res.ok());
    EXPECT_EQ(res.get(), 6);
  }

  auto res = persistor->removeBack(LogIndex{2}, {}).waitAndGet();
  ASSERT_TRUE(res.fail());
}

TEST_F(LogPersistorTest, removeBack_fails_if_start_index_too_large) {
  insertEntries();

  auto res = persistor->removeBack(LogIndex{8}, {}).waitAndGet();
  ASSERT_TRUE(res.fail());
  EXPECT_EQ(
      "found index (5) lower than start index (7) while searching backwards",
      res.errorMessage());
}

}  // namespace arangodb::replication2::storage::wal::test
