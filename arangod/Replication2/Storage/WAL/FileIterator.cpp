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

#include "FileIterator.h"

#include "Basics/Exceptions.h"
#include "Replication2/Storage/WAL/Entry.h"
#include "Replication2/Storage/WAL/IFileReader.h"
#include "Replication2/Storage/WAL/ReadHelpers.h"

#include "velocypack/Buffer.h"

namespace arangodb::replication2::storage::wal {

FileIterator::FileIterator(IteratorPosition position,
                           std::unique_ptr<IFileReader> reader)
    : _pos(position), _reader(std::move(reader)) {
  _reader->seek(_pos.fileOffset());
  moveToFirstEntry();
}

void FileIterator::moveToFirstEntry() {
  auto res = seekLogIndexForward(*_reader, _pos.index());
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
  }

  TRI_ASSERT(Entry::Header::fromCompressed(res.get()).index >=
             _pos.index().value);
  _pos = IteratorPosition::withFileOffset(_pos.index(), _reader->position());
}

auto FileIterator::next() -> std::optional<PersistedLogEntry> {
  auto startPos = _reader->position();

  Entry::CompressedHeader compressedHeader;
  if (!_reader->read(compressedHeader)) {
    return std::nullopt;
  }

  auto header = Entry::Header::fromCompressed(compressedHeader);

  auto paddedSize = Entry::paddedPayloadSize(header.size);
  velocypack::UInt8Buffer buffer(paddedSize);
  if (_reader->read(buffer.data(), paddedSize) != paddedSize) {
    return std::nullopt;
  }
  buffer.resetTo(header.size);

  auto entry = [&]() -> LogEntry {
    if (header.type == EntryType::wMeta) {
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

  Entry::Footer footer;
  if (!_reader->read(footer)) {
    return std::nullopt;
  }
  TRI_ASSERT(footer.size % 8 == 0);
  TRI_ASSERT(footer.size == _reader->position() - startPos);

  _pos = IteratorPosition::withFileOffset(entry.logIndex(), startPos);

  return PersistedLogEntry(std::move(entry), _pos);
}

}  // namespace arangodb::replication2::storage::wal
