////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "CurrentWatcher.h"

#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"

using namespace arangodb;

void CurrentWatcher::reserve(size_t expectedSize) {
  _callbacks.reserve(expectedSize);
  _results.doUnderLock(
      [&expectedSize](auto& lists) { lists.reserve(expectedSize); });
}

void CurrentWatcher::addWatchPath(
    std::string path, std::string identifier,
    std::function<bool(velocypack::Slice)> callback) {
  // There cannot be any results while we are still adding new paths to watch
  TRI_ASSERT(_results.doUnderLock([](auto& lists) { return lists.empty(); }));
  _callbacks.emplace_back(std::move(path), std::move(identifier),
                          std::move(callback));
  _expectedResults++;
}

void CurrentWatcher::addReport(std::string identifier, Result result) {
  _results.doUnderLock([identifier = std::move(identifier),
                        result = std::move(result)](auto& lists) mutable {
    if (lists.find(identifier) == lists.end()) {
      lists.emplace(std::move(identifier), std::move(result));
    }
  });
}

bool CurrentWatcher::hasReported(std::string const& identifier) const {
  return _results.doUnderLock([&identifier](auto& lists) {
    return lists.find(identifier) != lists.end();
  });
}

bool CurrentWatcher::haveAllReported() const {
  return _results.doUnderLock([expected = _expectedResults](auto& lists) {
    return lists.size() == expected;
  });
}

std::vector<std::tuple<std::string, std::string,
                       std::function<bool(velocypack::Slice)>>> const&
CurrentWatcher::getCallbackInfos() const {
  return _callbacks;
}

std::optional<Result> CurrentWatcher::getResultIfAllReported() const {
  return _results.doUnderLock([expected = _expectedResults](
                                  auto const& lists) -> std::optional<Result> {
    for (auto const& [_, result] : lists) {
      // Test if there is an error reported
      if (!result.ok()) {
        return result;
      }
    }
    // If we get here, all reports are OK.
    // let's check if we have all reports
    if (lists.size() == expected) {
      // We have all, complete the operation, no error
      return TRI_ERROR_NO_ERROR;
    }
    // Not yet complete, cannot report a Result.
    return std::nullopt;
  });
}

void CurrentWatcher::clearCallbacks() { _callbacks.clear(); }
