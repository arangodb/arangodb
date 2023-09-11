////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "LogPersistor.h"

#include <absl/crc/crc32c.h>
#include <type_traits>

#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "Basics/voc-errors.h"
#include "Futures/Future.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "Replication2/Storage/IteratorPosition.h"
#include "Replication2/Storage/WAL/Buffer.h"
#include "Replication2/Storage/WAL/FileHeader.h"
#include "Replication2/Storage/WAL/FileIterator.h"
#include "Replication2/Storage/WAL/IFileManager.h"
#include "Replication2/Storage/WAL/IFileReader.h"
#include "Replication2/Storage/WAL/IFileWriter.h"
#include "Replication2/Storage/WAL/LogReader.h"
#include "Replication2/Storage/WAL/Record.h"
#include "velocypack/Builder.h"

namespace arangodb::replication2::storage::wal {

namespace {
std::string activeLogFileName = "_current.log";
}

std::ostream& operator<<(std::ostream& s, LogPersistor::LogFile const& f) {
  return s << f.filename << " [" << f.first << " - " << f.last << "]";
}

LogPersistor::LogPersistor(LogId logId,
                           std::shared_ptr<IFileManager> fileManager,
                           Options options)
    : _logId(logId), _fileManager(std::move(fileManager)), _options(options) {
  LOG_TOPIC("a5ceb", TRACE, Logger::REPLICATED_WAL)
      << "Creating LogPersistor for log " << logId;

  // TODO - implement segmented logs
  loadFileSet();
  validateFileSet();
  createActiveLogFile();
}

void LogPersistor::loadFileSet() {
  for (auto& file : _fileManager->listFiles()) {
    try {
      LogReader reader(_fileManager->createReader(file));
      auto res = reader.getFirstRecordHeader();
      if (res.fail()) {
        LOG_TOPIC("b0f4c", WARN, Logger::REPLICATED_WAL)
            << "Ignoring file " << file << " in log " << _logId
            << " because we failed to read the first record: "
            << res.errorMessage();
        continue;
      }
      auto first = res.get();

      res = reader.getLastRecordHeader();
      if (res.fail()) {
        LOG_TOPIC("5b8d6", WARN, Logger::REPLICATED_WAL)
            << "Ignoring file " << file << " in log " << _logId
            << "because we failed to read the last record: "
            << res.errorMessage();
        continue;
      }
      auto last = res.get();

      _fileSet.insert(LogFile{
          .filename = file,
          .first = TermIndexPair(LogTerm{first.term()}, LogIndex{first.index}),
          .last = TermIndexPair(LogTerm{last.term()}, LogIndex{last.index})});
    } catch (std::exception const& ex) {
      LOG_TOPIC("4550a", WARN, Logger::REPLICATED_WAL)
          << "Ignoring file " << file << " in log " << _logId << ": "
          << ex.what();
    }
  }
}

void LogPersistor::validateFileSet() {
  for (auto it = _fileSet.begin(); it != _fileSet.end();) {
    auto next = std::next(it);
    if (next != _fileSet.end()) {
      if (it->last.index + 1 != next->first.index) {
        LOG_TOPIC("a9e3c", ERR, Logger::REPLICATED_WAL)
            << "Found a gap in the file set of log " << _logId << " - "
            << " file " << it->filename << " ends at log index " << it->last
            << " and file " << next->filename << " starts at log index"
            << next->first;
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_REPLICATION_REPLICATED_WAL_ERROR,
            "Found a gap in the file set of log " + to_string(_logId));
      }
    }
    it = next;
  }
}

void LogPersistor::createActiveLogFile() {
  _activeFile = _fileManager->createWriter(activeLogFileName);
  auto fileReader = _activeFile->getReader();
  if (fileReader->size() > sizeof(FileHeader)) {
    LogReader logReader(std::move(fileReader));
    auto res = logReader.getLastRecordHeader();
    if (res.fail()) {
      LOG_TOPIC("940c2", ERR, Logger::REPLICATED_WAL)
          << "Failed to read last record from " << fileReader->path();
      THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
    }
    auto header = res.get();
    _lastWrittenEntry =
        TermIndexPair(LogTerm{header.term()}, LogIndex{header.index});
  } else {
    // file must either be completely empty (newly created) or contain only the
    // FileHeader
    ADB_PROD_ASSERT(fileReader->size() == 0 ||
                    fileReader->size() == sizeof(FileHeader));
    if (fileReader->size() == 0) {
      FileHeader header = {.magic = wMagicFileType, .version = wCurrentVersion};
      auto res = _activeFile->append(header);
      if (res.fail()) {
        LOG_TOPIC("f219e", ERR, Logger::REPLICATED_WAL)
            << "Failed to write file header to " << _activeFile->path();
        THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
      }
    }

    if (!_fileSet.empty()) {
      _lastWrittenEntry = _fileSet.rbegin()->last;
    }
  }
}

auto LogPersistor::getIterator(IteratorPosition position)
    -> std::unique_ptr<PersistedLogIterator> {
  LOG_TOPIC("a6986", TRACE, Logger::REPLICATED_WAL)
      << "Creating iterator for index " << position.index() << " at offset "
      << position.fileOffset() << " in file " << _activeFile->path();
  return std::make_unique<FileIterator>(position, _activeFile->getReader());
}

auto LogPersistor::insert(std::unique_ptr<LogIterator> iter,
                          WriteOptions const& writeOptions)
    -> futures::Future<ResultT<SequenceNumber>> {
  Buffer buffer;
  EntryWriter entryWriter(buffer);

  SequenceNumber seq = 0;  // TODO - what if iterator is empty?
  auto lastWrittenEntry = _lastWrittenEntry;
  unsigned cnt = 0;
  while (auto e = iter->next()) {
    auto& entry = e.value();
    if (lastWrittenEntry.has_value()) {
      ADB_PROD_ASSERT(entry.logIndex() == lastWrittenEntry->index + 1 &&
                      entry.logTerm() >= lastWrittenEntry->term)
          << "attempting to write log entry " << entry.logTermIndexPair()
          << " after " << lastWrittenEntry.value();
    }

    ++cnt;
    entryWriter.appendEntry(entry);

    seq = entry.logIndex().value;
    lastWrittenEntry = entry.logTermIndexPair();
  }

  auto res = _activeFile->append(
      {reinterpret_cast<char const*>(buffer.data()), buffer.size()});
  if (res.fail()) {
    LOG_TOPIC("89261", ERR, Logger::REPLICATED_WAL)
        << "Failed to write " << cnt << " entries (" << buffer.size()
        << " bytes) to file " << _activeFile->path()
        << "; last written entry is "
        << (_lastWrittenEntry.has_value() ? to_string(_lastWrittenEntry.value())
                                          : "<na>");

    return res;
  }
  LOG_TOPIC("6fbfd", TRACE, Logger::REPLICATED_WAL)
      << "Wrote " << cnt << " entries (" << buffer.size() << " bytes) to file "
      << _activeFile->path() << "; last written entry is "
      << (lastWrittenEntry.has_value() ? to_string(lastWrittenEntry.value())
                                       : "<na>");

  _lastWrittenEntry = lastWrittenEntry;

  if (writeOptions.waitForSync) {
    _activeFile->sync();
  }

  return ResultT<SequenceNumber>::success(seq);
}

auto LogPersistor::removeFront(LogIndex stop, WriteOptions const&)
    -> futures::Future<ResultT<SequenceNumber>> {
  // TODO
  LOG_DEVEL << "LogPersistor::removeFront not implemented";
  return ResultT<SequenceNumber>::success(0);
}

auto LogPersistor::removeBack(LogIndex start, WriteOptions const&)
    -> futures::Future<ResultT<SequenceNumber>> try {
  LOG_TOPIC("2545c", INFO, Logger::REPLICATED_WAL)
      << "Removing entries from back starting at " << start << " from log "
      << _logId;
  ADB_PROD_ASSERT(start.value > 0);

  LogReader reader(_activeFile->getReader());
  reader.seek(reader.size());

  // we seek the predecessor of start, because we want to get its term
  auto lookupIndex = start.saturatedDecrement();
  auto res = reader.seekLogIndexBackward(lookupIndex);
  if (res.fail()) {
    LOG_TOPIC("93e92", ERR, Logger::REPLICATED_WAL)
        << "Failed to located entry with index " << lookupIndex << " in log "
        << _logId << ": " << res.errorMessage();
    return res.result();
  }

  auto header = res.get();
  TRI_ASSERT(header.index + 1 == start.value);
  // we located the predecessor, now we skip over it so we find the offset at
  // which we need to truncate
  reader.skipEntry();

  _lastWrittenEntry =
      TermIndexPair(LogTerm{header.term()}, LogIndex{header.index});

  auto newSize = reader.position();
  LOG_TOPIC("a1db0", INFO, Logger::REPLICATED_WAL)
      << "Truncating file " << _activeFile->path() << " at " << newSize;

  _activeFile->truncate(newSize);
  return ResultT<SequenceNumber>::success(start.value);
} catch (basics::Exception& ex) {
  LOG_TOPIC("7741d", ERR, Logger::REPLICATED_WAL)
      << "Failed to remove entries from back of file " << _activeFile->path()
      << ": " << ex.message();
  return ResultT<SequenceNumber>::error(ex.code(), ex.message());
} catch (std::exception& ex) {
  LOG_TOPIC("08da6", FATAL, Logger::REPLICATED_WAL)
      << "Failed to remove entries from back of file " << _activeFile->path()
      << ": " << ex.what();
  return ResultT<SequenceNumber>::error(
      TRI_ERROR_REPLICATION_REPLICATED_WAL_ERROR, ex.what());
}

auto LogPersistor::getLogId() -> LogId { return _logId; }

auto LogPersistor::waitForSync(SequenceNumber) -> futures::Future<Result> {
  // TODO
  return Result{};
}

// waits for all ongoing requests to be done
void LogPersistor::waitForCompletion() noexcept {
  // TODO
}

auto LogPersistor::compact() -> Result {
  // nothing to do here - this function should be removed from the interface
  return Result{};
}

auto LogPersistor::drop() -> Result {
  LOG_TOPIC("8fb77", INFO, Logger::REPLICATED_WAL)
      << "Dropping LogPersistor for log " << _logId;
  // TODO - error handling
  _activeFile.reset();
  _fileManager->removeAll();
  return Result{};
}

}  // namespace arangodb::replication2::storage::wal
