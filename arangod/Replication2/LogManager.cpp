////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include "LogManager.h"

#include <utility>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/flex_vector.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

using namespace arangodb::replication2;

#if 0
void LogManager::markLogAsDirty(std::shared_ptr<LogInterface> log) {
  std::unique_lock guard(_mutex);
  _logs = _logs.push_back(std::move(log));
  if (!workerActive) {
    workerActive = true;
    _executor->operator()([this]{
      workerEntryPoint();
    });
  }
}
#endif

auto LogManagerProxy::appendEntries(AppendEntriesRequest request)
    -> arangodb::futures::Future<AppendEntriesResult> {
  return _manager->appendEntries(std::move(request), _logId);
}

auto LogManagerProxy::participantId() const noexcept -> ParticipantId {
  return _id;
}

LogManagerProxy::LogManagerProxy(const LogId& logId, ParticipantId id,
                                 std::shared_ptr<LogManager> manager)
    : _logId(logId), _id(std::move(id)), _manager(manager) {}

LogManager::LogManager(std::shared_ptr<LogWorkerExecutor> executor)
    : _executor(std::move(executor)) {}

auto LogManager::appendEntries(AppendEntriesRequest request, LogId logId)
    -> arangodb::futures::Future<AppendEntriesResult> {
  std::unique_lock guard(_mutex);
  auto f = _requests.emplace_back(std::move(request), logId).promise.getFuture();
  if (!_isWorkerActive) {
    _isWorkerActive = true;
    _executor->operator()([this, self = shared_from_this()]{
      workerEntryPoint();
    });
  }
  return f;
}

void LogManager::workerEntryPoint() {

  while(true) {
    RequestVector request;
    {
      std::unique_lock guard(_mutex);
      if (_requests.empty()) {
        _isWorkerActive = false;
        return;
      }
      std::swap(request, _requests);
    }

    for (auto&& [request, promise, logId] : request) {

      struct MyLogIterator : LogIterator {
        auto next() -> std::optional<LogEntry> override {
          if (_iter == _entries.end()) {
            return std::nullopt;
          }

          return *_iter++;
        }

        explicit MyLogIterator(immer::flex_vector<LogEntry> entries) : _entries(std::move(entries)), _iter(_entries.begin()) {}
       private:
        immer::flex_vector<LogEntry> _entries;
        immer::flex_vector<LogEntry>::iterator _iter;
      };

      auto persistedLog = getPersistedLogById(logId);
      auto res = persistedLog->insert(std::make_shared<MyLogIterator>(std::move(request.entries)));

      AppendEntriesResult result;
      result.success = res.ok();
      result.logTerm = request.leaderTerm;
      _executor->operator()([p = std::move(promise), result]() mutable {
        p.setValue(result);
      });
    }
  }
}
