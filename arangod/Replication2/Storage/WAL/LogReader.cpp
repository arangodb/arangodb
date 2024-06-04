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

#include "LogReader.h"

#include "Basics/Exceptions.h"
#include "Basics/ResultT.h"
#include "Basics/ScopeGuard.h"
#include "Basics/voc-errors.h"

#include "Replication2/Storage/WAL/IFileReader.h"
#include "Replication2/Storage/WAL/FileHeader.h"
#include "Replication2/Storage/WAL/Record.h"

#include <absl/crc/crc32c.h>
#include <velocypack/Validator.h>

#include <string>

namespace arangodb::replication2::storage::wal {

namespace {
constexpr auto minFileSize =
    sizeof(FileHeader) + sizeof(Record::Header) + sizeof(Record::Footer);
}

LogReader::LogReader(std::unique_ptr<IFileReader> reader)
    : _reader(std::move(reader)) {
  FileHeader header;
  auto res = _reader->read(header);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_REPLICATED_WAL_ERROR,
                                   "failed to read header from log file " +
                                       _reader->path() + " - " +
                                       std::string(res.errorMessage()));
  }
  if (header.magic != wMagicFileType) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_REPLICATION_REPLICATED_WAL_INVALID_FILE,
        "invalid file type in log file " + _reader->path());
  }
  if (header.version != wCurrentVersion) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_REPLICATION_REPLICATED_WAL_INVALID_FILE,
        "invalid file version in file " + _reader->path());
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
  while (true) {
    auto res = _reader->read(header);
    if (res.fail()) {
      if (res.is(TRI_ERROR_END_OF_FILE)) {
        return ResultT<Record::CompressedHeader>::error(TRI_ERROR_END_OF_FILE,
                                                        "log index not found");
      }
      return ResultT<Record::CompressedHeader>::error(
          TRI_ERROR_REPLICATION_REPLICATED_WAL_ERROR, res.errorMessage());
    }
    if (header.index >= index.value) {
      _reader->seek(pos);  // reset to start of entry
      return ResultT<Record::CompressedHeader>::success(header);
    }
    pos += sizeof(Record::CompressedHeader) +
           Record::paddedPayloadSize(header.payloadSize) +
           sizeof(Record::Footer);
    _reader->seek(pos);
  }
}

auto LogReader::seekLogIndexBackward(LogIndex index)
    -> ResultT<Record::CompressedHeader> {
  using RT = ResultT<Record::CompressedHeader>;

  if (_reader->size() <= minFileSize) {
    return RT::error(TRI_ERROR_REPLICATION_REPLICATED_WAL_CORRUPT,
                     "log file " + _reader->path() + " is empty");
  }

  Record::Footer footer;
  Record::CompressedHeader compressedHeader;
  auto pos = _reader->position();
  ADB_PROD_ASSERT(pos % Record::alignment == 0)
      << "file " << _reader->path() << " - pos: " << pos;
  if (pos < minFileSize) {
    return RT::error(TRI_ERROR_REPLICATION_REPLICATED_WAL_CORRUPT,
                     "found corrupt log while searching backwards for index " +
                         to_string(index) + " - cannot read record from file " +
                         _reader->path() + " at position " +
                         std::to_string(pos) +
                         " because it is too small for a single record");
  }
  _reader->seek(pos - sizeof(Record::Footer));

  while (true) {
    auto res = _reader->read(footer);
    ADB_PROD_ASSERT(res.ok())
        << "failed to read footer - " << res.errorMessage();

    // calculate the max entry size based on our current position
    auto maxEntrySize = pos - sizeof(FileHeader);
    ADB_PROD_ASSERT(footer.size % Record::alignment == 0 &&
                    footer.size <= maxEntrySize)
        << "file " << _reader->path() << " - pos: " << pos
        << "; footer.size: " << footer.size
        << "; maxEntrySize: " << maxEntrySize;
    pos -= footer.size;
    // seek to beginning of entry
    _reader->seek(pos);
    res = _reader->read(compressedHeader);
    ADB_PROD_ASSERT(res.ok())
        << "failed to read header - " << res.errorMessage();

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
      return RT::error(TRI_ERROR_REPLICATION_REPLICATED_WAL_CORRUPT,
                       "found index (" + std::to_string(idx) +
                           ") lower than start index (" + to_string(index) +
                           ") while searching backwards");
    } else if (pos <= minFileSize) {
      return RT::error(
          TRI_ERROR_REPLICATION_REPLICATED_WAL_CORRUPT,
          "found corrupt log while searching backwards for index " +
              to_string(index) + " - cannot read record from file " +
              _reader->path() + " at position " + std::to_string(pos) +
              " because it is less than the minimum file size");
    }
    _reader->seek(pos - sizeof(Record::Footer));
  }
  ADB_UNREACHABLE;
}

auto LogReader::getFirstRecordHeader() -> ResultT<Record::CompressedHeader> {
  auto pos = _reader->position();
  auto guard = scopeGuard([&]() noexcept { _reader->seek(pos); });
  _reader->seek(_firstEntry);
  Record::CompressedHeader header;
  auto res = _reader->read(header);
  if (res.fail()) {
    return ResultT<Record::CompressedHeader>::error(
        TRI_ERROR_REPLICATION_REPLICATED_WAL_ERROR,
        "failed to read header: " + std::string(res.errorMessage()));
  }
  return ResultT<Record::CompressedHeader>::success(header);
}

auto LogReader::getLastRecordHeader() -> ResultT<Record::CompressedHeader> {
  auto pos = _reader->position();
  auto guard = scopeGuard([&]() noexcept { _reader->seek(pos); });
  auto fileSize = _reader->size();
  if (fileSize <= minFileSize) {
    return ResultT<Record::CompressedHeader>::error(
        TRI_ERROR_REPLICATION_REPLICATED_WAL_CORRUPT, "log is too small");
  }
  _reader->seek(fileSize - sizeof(Record::Footer));
  Record::Footer footer;
  auto res = _reader->read(footer);
  if (res.fail()) {
    return ResultT<Record::CompressedHeader>::error(
        TRI_ERROR_REPLICATION_REPLICATED_WAL_ERROR,
        "failed to read footer: " + std::string(res.errorMessage()));
  }
  ADB_PROD_ASSERT(footer.size % Record::alignment == 0 &&
                  footer.size <= fileSize - sizeof(FileHeader))
      << "file " << _reader->path() << " - footer.size: " << footer.size
      << "; fileSize: " << fileSize;
  _reader->seek(fileSize - footer.size);
  Record::CompressedHeader header;
  res = _reader->read(header);
  if (res.fail()) {
    return ResultT<Record::CompressedHeader>::error(
        TRI_ERROR_REPLICATION_REPLICATED_WAL_ERROR,
        "failed to read header" + std::string(res.errorMessage()));
  }
  return ResultT<Record::CompressedHeader>::success(header);
}

auto LogReader::readNextLogEntry() -> ResultT<PersistedLogEntry> {
  using RT = ResultT<PersistedLogEntry>;

  auto startPos = _reader->position();

  Record::CompressedHeader compressedHeader;
  auto res = _reader->read(compressedHeader);
  if (res.fail()) {
    auto error = res.is(TRI_ERROR_END_OF_FILE)
                     ? TRI_ERROR_END_OF_FILE
                     : TRI_ERROR_REPLICATION_REPLICATED_WAL_ERROR;
    return RT::error(
        error, "failed to read header - " + std::string(res.errorMessage()));
  }

  auto header = Record::Header{compressedHeader};

  auto paddedSize = Record::paddedPayloadSize(header.payloadSize);
  velocypack::UInt8Buffer buffer(paddedSize);
  res = _reader->read(buffer.data(), paddedSize);
  if (res.fail()) {
    return RT::error(
        TRI_ERROR_REPLICATION_REPLICATED_WAL_ERROR,
        "failed to read payload - " + std::string(res.errorMessage()));
  }
  buffer.resetTo(header.payloadSize);

  auto payloadCrc =
      absl::ComputeCrc32c({reinterpret_cast<char const*>(&compressedHeader),
                           sizeof(compressedHeader)});
  payloadCrc = absl::ExtendCrc32c(
      payloadCrc, {reinterpret_cast<char const*>(buffer.data()), paddedSize});

  auto entry = [&]() -> LogEntry {
    if (header.type == RecordType::wMeta) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      velocypack::Validator validator;
      try {
        validator.validate(buffer.data(), buffer.size(), true);
      } catch (std::exception const& ex) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_FAILED,
            "error processing velocypack data for log index " +
                std::to_string(header.index) + " at position " +
                std::to_string(startPos) + " from input file '" +
                _reader->path() + ": " + ex.what());
      }
#endif
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
  res = _reader->read(footer);
  if (res.fail()) {
    return RT::error(
        TRI_ERROR_REPLICATION_REPLICATED_WAL_ERROR,
        "failed to read footer - " + std::string(res.errorMessage()));
  }

  ADB_PROD_ASSERT(static_cast<std::uint32_t>(payloadCrc) == footer.crc32)
      << "crc mismatch for log index " << header.index << " at offset "
      << startPos << " - file: " << _reader->path()
      << "; calculated crc: " << payloadCrc << "; stored crc: " << footer.crc32;

  ADB_PROD_ASSERT(footer.size % Record::alignment == 0 &&
                  footer.size == _reader->position() - startPos)
      << "file " << _reader->path() << " - footer.size: " << footer.size
      << "; pos: " << _reader->position() << " startPos: " << startPos;

  auto pos = IteratorPosition::withFileOffset(entry.logIndex(), startPos);

  return PersistedLogEntry(std::move(entry), pos);
}

void LogReader::skipEntry() {
  auto pos = _reader->position();
  Record::CompressedHeader header;
  auto res = _reader->read(header);
  ADB_PROD_ASSERT(res.ok()) << "failed to read header - " << res.errorMessage();
  pos += sizeof(Record::CompressedHeader) +
         Record::paddedPayloadSize(header.payloadSize) + sizeof(Record::Footer);
  _reader->seek(pos);
}

}  // namespace arangodb::replication2::storage::wal
