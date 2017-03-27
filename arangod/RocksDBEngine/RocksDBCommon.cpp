////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBCommon.h"

using namespace arangodb::rocksdb;

arangodb::Result convertStatus(::rocksdb::Status const& status, StatusHint hint) {
  switch (status.code()) {
    case rocksdb::Status::Code::kOk:
      return {TRI_ERROR_NO_ERROR};
    case rocksdb::Status::Code::kNotFound:
      switch (hint) {
        case StatusHint::collection:
          return {TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, status.ToString()};
        case StatusHint::database:
          return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, status.ToString()};
        case StatusHint::document:
          return {TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, status.ToString()};
        case StatusHint::index:
          return {TRI_ERROR_ARANGO_INDEX_NOT_FOUND, status.ToString()};
        case StatusHint::view:
          return {TRI_ERROR_ARANGO_VIEW_NOT_FOUND, status.ToString()};
        default:
          return {TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, status.ToString()};
      }
    case rocksdb::Status::Code::kCorruption:
      return {TRI_ERROR_ARANGO_CORRUPTED_DATAFILE, status.ToString()};
    case rocksdb::Status::Code::kNotSupported:
      return {TRI_ERROR_ILLEGAL_OPTION, status.ToString()};
    case rocksdb::Status::Code::kInvalidArgument:
      return {TRI_ERROR_BAD_PARAMETER, status.ToString()};
    case rocksdb::Status::Code::kIOError:
      return {TRI_ERROR_ARANGO_IO_ERROR, status.ToString()};
    case rocksdb::Status::Code::kMergeInProgress:
      return {TRI_ERROR_ARANGO_MERGE_IN_PROGRESS, status.ToString()};
    case rocksdb::Status::Code::kIncomplete:
      return {TRI_ERROR_NO_ERROR, status.ToString()};
    case rocksdb::Status::Code::kShutdownInProgress:
      return {TRI_ERROR_SHUTTING_DOWN, status.ToString()};
    case rocksdb::Status::Code::kTimedOut:
      return {TRI_ERROR_LOCK_TIMEOUT, status.ToString()};
    case rocksdb::Status::Code::kAborted:
      return {TRI_ERROR_TRANSACTION_ABORTED, status.ToString()};
    case rocksdb::Status::Code::kBusy:
      return {TRI_ERROR_ARANGO_BUSY, status.ToString()};
    case rocksdb::Status::Code::kExpired:
      return {TRI_ERROR_INTERNAL, "key expired; TTL was set in error"};
    case rocksdb::Status::Code::kTryAgain:
      return {TRI_ERROR_ARANGO_TRY_AGAIN, status.ToString()};
    default:
      return {TRI_ERROR_INTERNAL, "unknown RocksDB status code"};
  }
}

uint64_t uint64FromPersistent(char const* p) {
  uint64_t value = 0;
  uint64_t x = 0;
  char const* end = p + sizeof(uint64_t);
  do {
    value += static_cast<uint64_t>(*p++) << x;
    x += 8;
  } while (p < end);
  return value;
}

void uint64ToPersistent(char* p, uint64_t value) {
  char* end = p + sizeof(uint64_t);
  do {
    *p++ = static_cast<uint8_t>(value & 0xff);
    value >>= 8;
  } while (p < end);
}

void uint64ToPersistent(std::string& p, uint64_t value) {
  size_t len = 0;
  do {
    p.push_back(static_cast<char>(value & 0xff));
    value >>= 8;
  } while (++len < sizeof(uint64_t));
}
