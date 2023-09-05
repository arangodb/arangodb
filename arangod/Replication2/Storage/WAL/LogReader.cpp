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

#include "LogReader.h"

#include "Basics/Exceptions.h"
#include "Basics/ResultT.h"
#include "Basics/voc-errors.h"
#include "Replication2/Storage/WAL/IFileReader.h"
#include "Replication2/Storage/WAL/FileHeader.h"
#include "Replication2/Storage/WAL/Record.h"

#include <string>

namespace arangodb::replication2::storage::wal {

namespace {
constexpr auto minFileSize =
    sizeof(FileHeader) + sizeof(Record::Header) + sizeof(Record::Footer);
}

LogReader::LogReader(std::unique_ptr<IFileReader> reader)
    : _reader(std::move(reader)) {
  FileHeader header;
  if (!_reader->read(header)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "failed to read header from log file " + reader->path());
  }
  if (header.magic != wMagicFileType) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "invalid file type in log file " + reader->path());
  }
  if (header.version != wCurrentVersion) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "invalid file version in file " + reader->path());
  }
  _firstEntry = _reader->position();
}

void LogReader::seek(std::uint64_t pos) {
  pos = std::max(pos, _firstEntry);
  _reader->seek(pos);
}

auto LogReader::position() const -> std::uint64_t {
  return _reader->position();
}

auto LogReader::size() const -> std::uint64_t { return _reader->size(); }

auto LogReader::seekLogIndexForward(LogIndex index)
    -> ResultT<Record::CompressedHeader> {
  auto pos = _reader->position();

  Record::CompressedHeader header;
  while (_reader->read(header)) {
    if (header.index >= index.value) {
      _reader->seek(pos);  // reset to start of entry
      return ResultT<Record::CompressedHeader>::success(header);
    }
    pos += sizeof(Record::CompressedHeader) +
           Record::paddedPayloadSize(header.payloadSize) +
           sizeof(Record::Footer);
    _reader->seek(pos);
  }
  // TODO - use proper error code
  return ResultT<Record::CompressedHeader>::error(TRI_ERROR_INTERNAL,
                                                  "log index not found");
}

auto LogReader::seekLogIndexBackward(LogIndex index)
    -> ResultT<Record::CompressedHeader> {
  using RT = ResultT<Record::CompressedHeader>;

  if (_reader->size() <= minFileSize) {
    return RT::error(TRI_ERROR_INTERNAL, "log is empty");
  }

  Record::Footer footer;
  Record::CompressedHeader compressedHeader;
  auto pos = _reader->position();
  ADB_PROD_ASSERT(pos % 8 == 0)
      << "file " << _reader->path() << " - pos: " << pos;
  if (pos < sizeof(Record::Footer)) {
    // TODO - file is corrupt - write log message
    return RT::error(TRI_ERROR_INTERNAL, "log is corrupt");
  }
  _reader->seek(pos - sizeof(Record::Footer));

  while (_reader->read(footer)) {
    // calculate the max entry size based on our current position
    auto maxEntrySize = pos - sizeof(FileHeader) - sizeof(Record::Header);
    ADB_PROD_ASSERT(footer.size % 8 != 0 || footer.size > maxEntrySize)
        << "file " << _reader->path() << " - pos: " << pos
        << "; footer.size: " << footer.size
        << "; maxEntrySize: " << maxEntrySize;
    pos -= footer.size;
    // seek to beginning of entry
    _reader->seek(pos);
    auto res = _reader->read(compressedHeader);
    TRI_ASSERT(res);

    auto idx = compressedHeader.index;
    if (idx == index.value) {
      ADB_PROD_ASSERT(Record::paddedPayloadSize(compressedHeader.payloadSize) +
                          sizeof(compressedHeader) + sizeof(footer) ==
                      footer.size)
          << "file " << _reader->path() << " - footer.size: " << footer.size
          << "; payloadSize: " << compressedHeader.payloadSize;
      // reset reader to start of entry
      _reader->seek(pos);
      return RT::success(compressedHeader);
    } else if (idx < index.value) {
      return RT::error(
          TRI_ERROR_INTERNAL,
          "found index lower than start index while searching backwards");
    } else if (pos <= minFileSize) {
      // TODO - file is corrupt - write log message
      return RT::error(TRI_ERROR_INTERNAL, "log is corrupt");
    }
    _reader->seek(pos - sizeof(Record::Footer));
  }
  return RT::error(TRI_ERROR_INTERNAL, "log index not found");
}

auto LogReader::getFirstRecordHeader() -> ResultT<Record::CompressedHeader> {
  _reader->seek(_firstEntry);
  Record::CompressedHeader header;
  if (!_reader->read(header)) {
    return ResultT<Record::CompressedHeader>::error(TRI_ERROR_INTERNAL,
                                                    "failed to read header");
  }
  return ResultT<Record::CompressedHeader>::success(header);
}

auto LogReader::getLastRecordHeader() -> ResultT<Record::CompressedHeader> {
  auto fileSize = _reader->size();
  if (fileSize <= minFileSize) {
    return ResultT<Record::CompressedHeader>::error(TRI_ERROR_INTERNAL,
                                                    "log is too small");
  }
  _reader->seek(fileSize - sizeof(Record::Footer));
  Record::Footer footer;
  if (!_reader->read(footer)) {
    return ResultT<Record::CompressedHeader>::error(TRI_ERROR_INTERNAL,
                                                    "failed to read footer");
  }
  ADB_PROD_ASSERT(footer.size % 8 != 0 || footer.size > fileSize - minFileSize)
      << "file " << _reader->path() << " - footer.size: " << footer.size
      << "; fileSize: " << fileSize;
  _reader->seek(fileSize - footer.size);
  Record::CompressedHeader header;
  if (!_reader->read(header)) {
    return ResultT<Record::CompressedHeader>::error(TRI_ERROR_INTERNAL,
                                                    "failed to read header");
  }
  return ResultT<Record::CompressedHeader>::success(header);
}

auto LogReader::readNextLogEntry() -> ResultT<PersistedLogEntry> {
  using RT = ResultT<PersistedLogEntry>;

  // TODO - use proper error codes

  auto startPos = _reader->position();

  Record::CompressedHeader compressedHeader;
  if (!_reader->read(compressedHeader)) {
    return RT::error(TRI_ERROR_INTERNAL, "failed to read header");
  }

  auto header = Record::Header{compressedHeader};

  auto paddedSize = Record::paddedPayloadSize(header.payloadSize);
  velocypack::UInt8Buffer buffer(paddedSize);
  if (_reader->read(buffer.data(), paddedSize) != paddedSize) {
    return RT::error(TRI_ERROR_INTERNAL, "failed to read payload");
  }
  buffer.resetTo(header.payloadSize);

  auto entry = [&]() -> LogEntry {
    if (header.type == RecordType::wMeta) {
      auto payload =
          LogMetaPayload::fromVelocyPack(velocypack::Slice(buffer.data()));
      return LogEntry(LogTerm(header.term), LogIndex(header.index),
                      std::move(payload));
    } else {
      LogPayload payload(std::move(buffer));
      return LogEntry(LogTerm(header.term), LogIndex(header.index),
                      std::move(payload));
    }
  }();

  Record::Footer footer;
  if (!_reader->read(footer)) {
    return RT::error(TRI_ERROR_INTERNAL, "failed to read footer");
  }
  // TODO - verify crc32
  ADB_PROD_ASSERT(footer.size % 8 == 0 &&
                  footer.size == _reader->position() - startPos)
      << "file " << _reader->path() << " - footer.size: " << footer.size
      << "; pos: " << _reader->position() << " startPos: " << startPos;

  auto pos = IteratorPosition::withFileOffset(entry.logIndex(), startPos);

  return PersistedLogEntry(std::move(entry), pos);
}

}  // namespace arangodb::replication2::storage::wal