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

#include "FileIterator.h"

#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Replication2/Storage/WAL/IFileReader.h"
#include "Replication2/Storage/WAL/Record.h"

#include "velocypack/Buffer.h"

namespace arangodb::replication2::storage::wal {

FileIterator::FileIterator(IteratorPosition position,
                           std::unique_ptr<IFileReader> reader)
    : _reader(std::move(reader)) {
  _reader.seek(position.fileOffset());
  if (position.index().value != 0) {
    moveToFirstEntry(position);
  }
}

void FileIterator::moveToFirstEntry(IteratorPosition position) {
  auto res = _reader.seekLogIndexForward(position.index());
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res.result());
  }

  TRI_ASSERT(res.get().index >= position.index().value);
}

auto FileIterator::next() -> std::optional<PersistedLogEntry> {
  auto res = _reader.readNextLogEntry();
  if (res.fail()) {
    if (res.is(TRI_ERROR_END_OF_FILE)) {
      return std::nullopt;
    }
    THROW_ARANGO_EXCEPTION(res.result());
  }

  return {std::move(res.get())};
}

}  // namespace arangodb::replication2::storage::wal
