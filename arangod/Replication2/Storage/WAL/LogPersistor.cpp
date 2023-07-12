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
#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "Basics/voc-errors.h"
#include "Futures/Future.h"
#include "Logger/LogMacros.h"
#include "Replication2/Storage/IteratorPosition.h"
#include "Replication2/Storage/WAL/Buffer.h"
#include "Replication2/Storage/WAL/Entry.h"
#include "Replication2/Storage/WAL/EntryType.h"
#include "Replication2/Storage/WAL/FileIterator.h"
#include "Replication2/Storage/WAL/IFileReader.h"
#include "Replication2/Storage/WAL/IFileWriter.h"
#include "Replication2/Storage/WAL/IWalManager.h"
#include "Replication2/Storage/WAL/ReadHelpers.h"
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
    Entry::Header header;
    header.index = entry.logIndex().value;
    header.term = entry.logTerm().value;
    header.type = EntryType::wNormal;

    auto* payload = entry.logPayload();
    TRI_ASSERT(payload != nullptr);
    header.size = payload->byteSize();
    _buffer.append(header.compress());
    _buffer.append(payload->slice().getDataPtr(), payload->byteSize());
    return header.size;
  }

  std::uint32_t writeMetaEntry(LogEntry const& entry) {
    TRI_ASSERT(entry.hasMeta());
    Entry::Header header;
    header.index = entry.logIndex().value;
    header.term = entry.logTerm().value;
    header.type = EntryType::wMeta;
    header.size = 0;  // we initialize this to zero so the compiler is happy,
                      // but will update it later

    // the meta entry is directly encoded into the buffer, so we have to write
    // the header beforehand and update the size afterwards
    _buffer.append(header.compress());
    auto pos = _buffer.size() - sizeof(Entry::CompressedHeader::size);

    auto* payload = entry.meta();
    TRI_ASSERT(payload != nullptr);
    VPackBuilder builder(_buffer.buffer());
    payload->toVelocyPack(builder);
    header.size = _buffer.size() - pos - sizeof(Entry::CompressedHeader::size);

    // reset to the saved position so we can write the actual size
    _buffer.resetTo(pos);
    _buffer.append(
        static_cast<decltype(Entry::CompressedHeader::size)>(header.size));
    _buffer.advance(header.size);
    return header.size;
  }

  void writePaddingBytes(
      std::uint32_t payloadSize) {  // write zeroed out padding bytes
    auto numPaddingBytes = Entry::paddedPayloadSize(payloadSize) - payloadSize;
    TRI_ASSERT(numPaddingBytes < 8);
    std::uint64_t const zero = 0;
    _buffer.append(&zero, numPaddingBytes);
  }

  void writeFooter(std::size_t startPos) {
    auto size = _buffer.size() - startPos;
    Entry::Footer footer{
        .crc32 = static_cast<std::uint32_t>(absl::ComputeCrc32c(
            {reinterpret_cast<char const*>(_buffer.data() + startPos), size})),
        .size = static_cast<std::uint32_t>(size + sizeof(Entry::Footer))};
    TRI_ASSERT(footer.size % 8 == 0);
    _buffer.append(footer);
  }

  Buffer _buffer;
};

}  // namespace

LogPersistor::LogPersistor(LogId logId, std::shared_ptr<IWalManager> walManager)
    : _logId(logId),
      _walManager(std::move(walManager)),
      _fileWriter(_walManager->openLog(_logId)) {
  ;
}

auto LogPersistor::getIterator(IteratorPosition position)
    -> std::unique_ptr<PersistedLogIterator> {
  return std::make_unique<FileIterator>(position, _fileWriter->getReader());
}

auto LogPersistor::insert(std::unique_ptr<LogIterator> iter,
                          WriteOptions const& writeOptions)
    -> futures::Future<ResultT<SequenceNumber>> {
  BufferWriter bufferWriter;

  SequenceNumber seq = 0;  // TODO - what if iterator is empty?
  auto lastWrittenEntry = _lastWrittenEntry;
  while (auto e = iter->next()) {
    auto& entry = e.value();
    if (lastWrittenEntry) {
      if (entry.logIndex() != lastWrittenEntry->index + 1) {
        LOG_DEVEL << "attempting to write log entry with idx "
                  << entry.logIndex() << " after " << lastWrittenEntry->index;
        return ResultT<SequenceNumber>::error(
            TRI_ERROR_INTERNAL, "Log entries must be written in order");
      }
      TRI_ASSERT(entry.logTerm() >= lastWrittenEntry->term);
      // TODO - can we make a similar check for the term?
    }

    seq = entry.logIndex().value;

    bufferWriter.appendEntry(entry);

    lastWrittenEntry = entry.logTermIndexPair();
  }

  auto& buffer = bufferWriter.buffer();
  auto res = _fileWriter->append(
      {reinterpret_cast<char const*>(buffer.data()), buffer.size()});
  if (res.fail()) {
    return res;
  }
  _lastWrittenEntry = lastWrittenEntry;

  if (writeOptions.waitForSync) {
    _fileWriter->sync();
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
    -> futures::Future<ResultT<SequenceNumber>> {
  auto reader = _fileWriter->getReader();
  reader->seek(reader->size());

  auto res = seekLogIndexBackward(*reader, start);
  if (res.fail()) {
    return res.result();
  }

  auto header = Entry::Header::fromCompressed(res.get());
  TRI_ASSERT(header.index == start.value);

  auto pos = reader->position();
  // TODO - refactor and check that we actually have one more entry to read!
  Entry::Footer footer;
  reader->seek(pos - sizeof(Entry::Footer));
  reader->read(footer);
  reader->seek(pos - footer.size);

  Entry::CompressedHeader compressedHeader;
  reader->read(compressedHeader);
  header = Entry::Header::fromCompressed(compressedHeader);

  _lastWrittenEntry =
      TermIndexPair(LogTerm{header.term}, LogIndex{header.index});

  _fileWriter->truncate(pos);
  return ResultT<SequenceNumber>::success(start.value);
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

auto LogPersistor::drop() -> Result { return _walManager->dropLog(_logId); }

}  // namespace arangodb::replication2::storage::wal
