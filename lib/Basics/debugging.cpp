////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "debugging.h"

#include "Basics/CrashHandler.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/WriteLocker.h"
#include "Logger/LogAppender.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

using namespace arangodb;

#ifdef ARANGODB_ENABLE_FAILURE_TESTS

namespace {
std::atomic<bool> hasFailurePoints{false};

/// @brief a read-write lock for thread-safe access to the failure points set
arangodb::basics::ReadWriteLock failurePointsLock;

/// @brief a global set containing the currently registered failure points
std::set<std::string, std::less<>> failurePoints;
}  // namespace

/// @brief intentionally cause a segmentation violation or other failures
/// this is used for crash and recovery tests
void TRI_TerminateDebugging(std::string_view message) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  CrashHandler::setHardKill();

  // there are some reserved crash messages we use in testing the
  // crash handler
  if (message == "CRASH-HANDLER-TEST-ABORT") {
    // intentionally crashes the program!
    std::abort();
  } else if (message == "CRASH-HANDLER-TEST-TERMINATE") {
    // intentionally crashes the program!
    std::terminate();
  } else if (message == "CRASH-HANDLER-TEST-TERMINATE-ACTIVE") {
    // intentionally crashes the program!
    // note: when using ASan/UBSan, this actually does not crash
    // the program but continues.
    auto f = []() noexcept {
      // intentionally crashes the program!
      // cppcheck-suppress *
      return std::string(nullptr);
    };
    f();
    // we will get here at least with ASan/UBSan.
    std::terminate();
  } else if (message == "CRASH-HANDLER-TEST-SEGFAULT") {
    std::unique_ptr<int> x;
    // intentionally crashes the program!
    // cppcheck-suppress *
    int a = *x;
    // cppcheck-suppress *
    *x = 2;
    TRI_ASSERT(a == 1);
  } else if (message == "CRASH-HANDLER-TEST-ASSERT") {
    int a = 1;
    // intentionally crashes the program!
    TRI_ASSERT(a == 2);
  }

#endif

  // intentional crash - no need for a backtrace here
  CrashHandler::disableBacktraces();
  CrashHandler::crash(message);
}

/// @brief check whether we should fail at a specific failure point
bool TRI_ShouldFailDebugging(std::string_view value) noexcept {
  if (::hasFailurePoints.load(std::memory_order_relaxed)) {
    READ_LOCKER(readLocker, ::failurePointsLock);
    return ::failurePoints.find(value) != ::failurePoints.end();
  }
  return false;
}

/// @brief add a failure point
void TRI_AddFailurePointDebugging(std::string_view value) {
  bool added = false;
  {
    WRITE_LOCKER(writeLocker, ::failurePointsLock);
    added = ::failurePoints.emplace(value).second;
    ::hasFailurePoints.store(true, std::memory_order_relaxed);
  }

  if (added) {
    LOG_TOPIC("d8a5f", WARN, arangodb::Logger::FIXME)
        << "activating intentional failure point '" << value
        << "'. the server will misbehave!";
  }
}

/// @brief remove a failure point
void TRI_RemoveFailurePointDebugging(std::string_view value) {
  size_t numRemoved = 0;
  {
    WRITE_LOCKER(writeLocker, ::failurePointsLock);
    numRemoved = ::failurePoints.erase(std::string(value));
    if (::failurePoints.size() == 0) {
      ::hasFailurePoints.store(false, std::memory_order_relaxed);
    }
  }

  if (numRemoved > 0) {
    LOG_TOPIC("5aacb", INFO, arangodb::Logger::FIXME)
        << "cleared failure point " << value;
  }
}

/// @brief clear all failure points
void TRI_ClearFailurePointsDebugging() noexcept {
  size_t numExisting = 0;
  {
    WRITE_LOCKER(writeLocker, ::failurePointsLock);
    numExisting = ::failurePoints.size();
    ::failurePoints.clear();
    ::hasFailurePoints.store(false, std::memory_order_relaxed);
  }

  if (numExisting > 0) {
    LOG_TOPIC("ea4e7", INFO, arangodb::Logger::FIXME)
        << "cleared " << numExisting << " failure point(s)";
  }
}

/// @brief return all currently set failure points
void TRI_GetFailurePointsDebugging(arangodb::velocypack::Builder& builder) {
  builder.openArray();
  {
    READ_LOCKER(readLocker, ::failurePointsLock);
    for (auto const& it : ::failurePoints) {
      builder.add(arangodb::velocypack::Value(it));
    }
  }
  builder.close();
}
#endif

template<>
char const conpar<true>::open = '{';
template<>
char const conpar<true>::close = '}';
template<>
char const conpar<false>::open = '[';
template<>
char const conpar<false>::close = ']';
