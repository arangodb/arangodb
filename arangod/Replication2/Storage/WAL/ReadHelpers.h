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

#pragma once

#include "Basics/ResultT.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/PersistedLogEntry.h"
#include "Replication2/Storage/WAL/Entry.h"

namespace arangodb::replication2::storage::wal {

struct IFileReader;

// Seek to the entry with the specified index in the file, starting from the
// current position of the reader, either forward or backward
// In case of success, the reader is positioned at the start of the matching
// entry.
auto seekLogIndexForward(IFileReader& reader, LogIndex index)
    -> ResultT<Entry::CompressedHeader>;
auto seekLogIndexBackward(IFileReader& reader, LogIndex index)
    -> ResultT<Entry::CompressedHeader>;

// read the next entry, starting from the current position of the reader
auto readLogEntry(IFileReader& reader) -> ResultT<PersistedLogEntry>;

}  // namespace arangodb::replication2::storage::wal