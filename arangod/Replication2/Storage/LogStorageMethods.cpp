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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "LogStorageMethods.h"

#include <Basics/Result.h>
#include <Basics/ResultT.h>
#include <Futures/Future.h>

#include "Replication2/ReplicatedLog/LogEntry.h"
#include "Replication2/Storage/IteratorPosition.h"

namespace arangodb::replication2::storage {

LogStorageMethods::LogStorageMethods(
    std::unique_ptr<ILogPersistor> logPersistor,
    std::unique_ptr<IStatePersistor> statePersistor)
    : _logPersistor(std::move(logPersistor)),
      _statePersistor(std::move(statePersistor)) {}

Result LogStorageMethods::updateMetadata(storage::PersistedStateInfo info) {
  return _statePersistor->updateMetadata(info);
}

ResultT<PersistedStateInfo> LogStorageMethods::readMetadata() {
  return _statePersistor->readMetadata();
}

std::unique_ptr<PersistedLogIterator> LogStorageMethods::getIterator(
    IteratorPosition position) {
  return _logPersistor->getIterator(position);
}

auto LogStorageMethods::removeFront(LogIndex stop, WriteOptions const& opts)
    -> futures::Future<ResultT<SequenceNumber>> {
  return _logPersistor->removeFront(stop, opts);
}

auto LogStorageMethods::removeBack(LogIndex start, WriteOptions const& opts)
    -> futures::Future<ResultT<SequenceNumber>> {
  return _logPersistor->removeBack(start, opts);
}

auto LogStorageMethods::insert(std::unique_ptr<LogIterator> iter,
                               WriteOptions const& opts)
    -> futures::Future<ResultT<SequenceNumber>> {
  return _logPersistor->insert(std::move(iter), opts);
}

LogId LogStorageMethods::getLogId() { return _logPersistor->getLogId(); }

auto LogStorageMethods::waitForSync(
    IStorageEngineMethods::SequenceNumber number) -> futures::Future<Result> {
  return _logPersistor->waitForSync(number);
}

void LogStorageMethods::waitForCompletion() noexcept {
  _logPersistor->waitForCompletion();
}

auto LogStorageMethods::drop() -> Result {
  // TODO - can we make this atomic?
  // Probably rather tricky once we write our own WAL files
  auto res = _statePersistor->drop();
  if (res.ok()) {
    res = _logPersistor->drop();
  }
  return res;
}

auto LogStorageMethods::compact() -> Result { return _logPersistor->compact(); }

}  // namespace arangodb::replication2::storage
