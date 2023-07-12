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
#include "Replication2/Storage/WAL/Entry.h"
#include "Replication2/Storage/WAL/IFileReader.h"

namespace arangodb::replication2::storage::wal {

auto seekLogIndexForward(IFileReader& reader, LogIndex index)
    -> ResultT<Entry::CompressedHeader> {
  auto pos = reader.position();

  Entry::CompressedHeader compressedHeader;
  while (reader.read(compressedHeader)) {
    auto header = Entry::Header::fromCompressed(compressedHeader);
    if (header.index >= index.value) {
      reader.seek(pos);  // reset to start of entry
      return ResultT<Entry::CompressedHeader>::success(compressedHeader);
    }
    pos += sizeof(Entry::CompressedHeader) +
           Entry::paddedPayloadSize(header.size) + sizeof(Entry::Footer);
    reader.seek(pos);
  }
  // TODO - use proper error code
  return ResultT<Entry::CompressedHeader>::error(TRI_ERROR_INTERNAL,
                                                 "log index not found");
}

auto seekLogIndexBackward(IFileReader& reader, LogIndex index)
    -> ResultT<Entry::CompressedHeader> {
  using RT = ResultT<Entry::CompressedHeader>;

  if (reader.size() <= sizeof(Entry::Footer)) {
    return RT::error(TRI_ERROR_INTERNAL, "log is empty");
  }

  Entry::Footer footer;
  Entry::CompressedHeader compressedHeader;
  auto pos = reader.position();
  if (pos < sizeof(Entry::Footer)) {
    // TODO - file is corrupt - write log message
    return RT::error(TRI_ERROR_INTERNAL, "log is corrupt");
  }
  reader.seek(pos - sizeof(Entry::Footer));

  while (reader.read(footer)) {
    if (footer.size % 8 != 0) {
      // TODO - file is corrupt - write log message
      return RT::error(TRI_ERROR_INTERNAL, "log is corrupt");
    }
    pos -= footer.size;
    reader.seek(pos);
    reader.read(compressedHeader);
    auto idx = Entry::Header::fromCompressed(compressedHeader).index;
    if (idx == index.value) {
      // reset reader to start of entry
      reader.seek(pos);
      return RT::success(compressedHeader);
    } else if (idx < index.value) {
      return RT::error(
          TRI_ERROR_INTERNAL,
          "found index lower than start index while searching backwards");
    } else if (pos <= sizeof(Entry::Footer)) {
      // TODO - file is corrupt - write log message
      return RT::error(TRI_ERROR_INTERNAL, "log is corrupt");
    }
    reader.seek(pos - sizeof(Entry::Footer));
  }
  return RT::error(TRI_ERROR_INTERNAL, "log index not found");
}

}  // namespace arangodb::replication2::storage::wal