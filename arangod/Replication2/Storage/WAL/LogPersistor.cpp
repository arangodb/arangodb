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

struct BufferWriter {
  Buffer const& buffer() const { return _buffer; }

  void appendEntry(LogEntry const& entry) {
    auto startPos = _buffer.size();

    auto payloadSize = [&]() {
      if (entry.hasPayload()) {
        return writeNormalEntry(entry);
      } else {
        return writeMetaEntry(entry);
      }
    }();

    // we want every thing to be 8 byte aligned, so we round size to the next
    // greater multiple of 8
    writePaddingBytes(payloadSize);
    writeFooter(startPos);
    TRI_ASSERT(_buffer.size() % 8 == 0);
  }

 private:
  std::uint32_t writeNormalEntry(LogEntry const& entry) {
    TRI_ASSERT(entry.hasPayload());
    Record::Header header;
    header.index = entry.logIndex().value;
    header.term = entry.logTerm().value;
    header.type = RecordType::wNormal;

    auto* payload = entry.logPayload();
    TRI_ASSERT(payload != nullptr);
    header.payloadSize = payload->byteSize();
    _buffer.append(Record::CompressedHeader{header});
    _buffer.append(payload->slice().getDataPtr(), payload->byteSize());
    return header.payloadSize;
  }

  std::uint32_t writeMetaEntry(LogEntry const& entry) {
    TRI_ASSERT(entry.hasMeta());
    Record::Header header;
    header.index = entry.logIndex().value;
    header.term = entry.logTerm().value;
    header.type = RecordType::wMeta;
    header.payloadSize = 0;  // we initialize this to zero so the compiler is
                             // happy, but will update it later

    // the meta entry is directly encoded into the buffer, so we have to write
    // the header beforehand and update the size afterwards
    _buffer.append(Record::CompressedHeader{header});
    auto pos = _buffer.size() - sizeof(Record::CompressedHeader::payloadSize);

    auto* payload = entry.meta();
    TRI_ASSERT(payload != nullptr);
    VPackBuilder builder(_buffer.buffer());
    payload->toVelocyPack(builder);
    header.payloadSize =
        _buffer.size() - pos - sizeof(Record::CompressedHeader::payloadSize);

    // reset to the saved position so we can write the actual size
    _buffer.resetTo(pos);
    _buffer.append(static_cast<decltype(Record::CompressedHeader::payloadSize)>(
        header.payloadSize));
    _buffer.advance(header.payloadSize);
    return header.payloadSize;
  }

  void writePaddingBytes(
      std::uint32_t payloadSize) {  // write zeroed out padding bytes
    auto numPaddingBytes = Record::paddedPayloadSize(payloadSize) - payloadSize;
    TRI_ASSERT(numPaddingBytes < 8);
    std::uint64_t const zero = 0;
    _buffer.append(&zero, numPaddingBytes);
  }

  void writeFooter(std::size_t startPos) {
    auto size = _buffer.size() - startPos;
    Record::Footer footer{
        .crc32 = static_cast<std::uint32_t>(absl::ComputeCrc32c(
            {reinterpret_cast<char const*>(_buffer.data() + startPos), size})),
        .size = static_cast<std::uint32_t>(size + sizeof(Record::Footer))};
    TRI_ASSERT(footer.size % 8 == 0);
    _buffer.append(footer);
  }

  Buffer _buffer;
};

}  // namespace

LogPersistor::LogPersistor(LogId logId,
                           std::shared_ptr<IFileManager> fileManager)
    : _logId(logId), _fileManager(std::move(fileManager)) {
  // TODO - implement segmented logs
  auto filename = std::to_string(logId.id()) + ".log";
  _fileSet.emplace(LogFile{.filename = filename});
  _activeFile = _fileManager->createWriter(filename);
  auto fileReader = _activeFile->getReader();
  if (fileReader->size() == 0) {
    FileHeader header = {.magic = wMagicFileType, .version = wCurrentVersion};
    auto res = _activeFile->append(header);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
    }
  } else if (fileReader->size() > sizeof(FileHeader)) {
    LogReader logReader(std::move(fileReader));
    auto res = logReader.getLastRecordHeader();
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
    }
    auto header = res.get();
    _lastWrittenEntry =
        TermIndexPair(LogTerm{header.term()}, LogIndex{header.index});
  }
}

auto LogPersistor::getIterator(IteratorPosition position)
    -> std::unique_ptr<PersistedLogIterator> {
  return std::make_unique<FileIterator>(position, _activeFile->getReader());
}

auto LogPersistor::insert(std::unique_ptr<LogIterator> iter,
                          WriteOptions const& writeOptions)
    -> futures::Future<ResultT<SequenceNumber>> {
  BufferWriter bufferWriter;

  SequenceNumber seq = 0;  // TODO - what if iterator is empty?
  auto lastWrittenEntry = _lastWrittenEntry;
  while (auto e = iter->next()) {
    auto& entry = e.value();
    if (lastWrittenEntry.has_value()) {
      ADB_PROD_ASSERT(entry.logIndex() == lastWrittenEntry->index + 1 &&
                      entry.logTerm() >= lastWrittenEntry->term)
          << "attempting to write log entry " << entry.logTermIndexPair()
          << " after " << lastWrittenEntry.value();
    }

    bufferWriter.appendEntry(entry);

    seq = entry.logIndex().value;
    lastWrittenEntry = entry.logTermIndexPair();
  }

  auto& buffer = bufferWriter.buffer();
  auto res = _activeFile->append(
      {reinterpret_cast<char const*>(buffer.data()), buffer.size()});
  if (res.fail()) {
    return res;
  }
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
  ADB_PROD_ASSERT(start.value > 0);

  LogReader reader(_activeFile->getReader());
  reader.seek(reader.size());

  // we seek the predecessor of start, because we want to get its term
  auto res = reader.seekLogIndexBackward(start.saturatedDecrement());
  if (res.fail()) {
    return res.result();
  }

  auto header = res.get();
  reader.skipEntry();

  TRI_ASSERT(header.index + 1 == start.value);

  _lastWrittenEntry =
      TermIndexPair(LogTerm{header.term()}, LogIndex{header.index});

  _activeFile->truncate(reader.position());
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
  // TODO - error handling
  _activeFile.reset();
  _fileManager->removeAll();
  return Result{};
}

}  // namespace arangodb::replication2::storage::wal
