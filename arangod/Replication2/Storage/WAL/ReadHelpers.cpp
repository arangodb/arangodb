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

#include "ReadHelpers.h"

#include "Basics/ResultT.h"
#include "Basics/voc-errors.h"
#include "Replication2/Storage/WAL/IFileReader.h"
#include "Replication2/Storage/WAL/Record.h"

namespace arangodb::replication2::storage::wal {

auto seekLogIndexForward(IFileReader& reader, LogIndex index)
    -> ResultT<Record::CompressedHeader> {
  auto pos = reader.position();

  Record::CompressedHeader header;
  while (reader.read(header)) {
    if (header.index() >= index.value) {
      reader.seek(pos);  // reset to start of entry
      return ResultT<Record::CompressedHeader>::success(header);
    }
    pos += sizeof(Record::CompressedHeader) +
           Record::paddedPayloadSize(header.size) + sizeof(Record::Footer);
    reader.seek(pos);
  }
  // TODO - use proper error code
  return ResultT<Record::CompressedHeader>::error(TRI_ERROR_INTERNAL,
                                                  "log index not found");
}

auto seekLogIndexBackward(IFileReader& reader, LogIndex index)
    -> ResultT<Record::CompressedHeader> {
  using RT = ResultT<Record::CompressedHeader>;

  if (reader.size() <= sizeof(Record::Footer)) {
    return RT::error(TRI_ERROR_INTERNAL, "log is empty");
  }

  Record::Footer footer;
  Record::CompressedHeader compressedHeader;
  auto pos = reader.position();
  if (pos < sizeof(Record::Footer)) {
    // TODO - file is corrupt - write log message
    return RT::error(TRI_ERROR_INTERNAL, "log is corrupt");
  }
  reader.seek(pos - sizeof(Record::Footer));

  while (reader.read(footer)) {
    if (footer.size % 8 != 0) {
      // TODO - file is corrupt - write log message
      return RT::error(TRI_ERROR_INTERNAL, "log is corrupt");
    }
    pos -= footer.size;
    reader.seek(pos);
    reader.read(compressedHeader);
    auto idx = compressedHeader.index();
    if (idx == index.value) {
      // reset reader to start of entry
      reader.seek(pos);
      return RT::success(compressedHeader);
    } else if (idx < index.value) {
      return RT::error(
          TRI_ERROR_INTERNAL,
          "found index lower than start index while searching backwards");
    } else if (pos <= sizeof(Record::Footer)) {
      // TODO - file is corrupt - write log message
      return RT::error(TRI_ERROR_INTERNAL, "log is corrupt");
    }
    reader.seek(pos - sizeof(Record::Footer));
  }
  return RT::error(TRI_ERROR_INTERNAL, "log index not found");
}

auto getFirstRecordHeader(IFileReader& reader)
    -> ResultT<Record::CompressedHeader> {
  reader.seek(0);
  Record::CompressedHeader header;
  if (!reader.read(header)) {
    return ResultT<Record::CompressedHeader>::error(TRI_ERROR_INTERNAL,
                                                    "failed to read header");
  }
  return ResultT<Record::CompressedHeader>::success(header);
}

auto getLastRecordHeader(IFileReader& reader)
    -> ResultT<Record::CompressedHeader> {
  auto fileSize = reader.size();
  if (fileSize <= sizeof(Record::Footer)) {
    return ResultT<Record::CompressedHeader>::error(TRI_ERROR_INTERNAL,
                                                    "log is too small");
  }
  reader.seek(fileSize - sizeof(Record::Footer));
  Record::Footer footer;
  if (!reader.read(footer)) {
    return ResultT<Record::CompressedHeader>::error(TRI_ERROR_INTERNAL,
                                                    "failed to read footer");
  }
  if (fileSize < footer.size) {
    return ResultT<Record::CompressedHeader>::error(TRI_ERROR_INTERNAL,
                                                    "log is corrupt");
  }
  reader.seek(fileSize - footer.size);
  Record::CompressedHeader header;
  if (!reader.read(header)) {
    return ResultT<Record::CompressedHeader>::error(TRI_ERROR_INTERNAL,
                                                    "failed to read header");
  }
  return ResultT<Record::CompressedHeader>::success(header);
}

auto readLogEntry(IFileReader& reader) -> ResultT<PersistedLogEntry> {
  using RT = ResultT<PersistedLogEntry>;

  // TODO - use proper error codes

  auto startPos = reader.position();

  Record::CompressedHeader compressedHeader;
  if (!reader.read(compressedHeader)) {
    return RT::error(TRI_ERROR_INTERNAL, "failed to read header");
  }

  auto header = Record::Header{compressedHeader};

  auto paddedSize = Record::paddedPayloadSize(header.size);
  velocypack::UInt8Buffer buffer(paddedSize);
  if (reader.read(buffer.data(), paddedSize) != paddedSize) {
    return RT::error(TRI_ERROR_INTERNAL, "failed to read payload");
  }
  buffer.resetTo(header.size);

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
  if (!reader.read(footer)) {
    return RT::error(TRI_ERROR_INTERNAL, "failed to read footer");
  }
  TRI_ASSERT(footer.size % 8 == 0);
  TRI_ASSERT(footer.size == reader.position() - startPos);

  auto pos = IteratorPosition::withFileOffset(entry.logIndex(), startPos);

  return PersistedLogEntry(std::move(entry), pos);
}

}  // namespace arangodb::replication2::storage::wal