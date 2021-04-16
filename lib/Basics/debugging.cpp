////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/StringRef.h>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

using namespace arangodb;

#ifdef ARANGODB_ENABLE_FAILURE_TESTS

namespace {
/// @brief custom comparer for failure points. this allows an implicit
/// conversion from char const* to arangodb::velocypack::StringRef in order to avoid memory
/// allocations for temporary string values
struct Comparer {
  using is_transparent = std::true_type;
  // implement comparison functions for various types
  inline bool operator()(arangodb::velocypack::StringRef const& lhs, std::string const& rhs) const noexcept {
    return lhs < arangodb::velocypack::StringRef(rhs);
  }
  inline bool operator()(std::string const& lhs, arangodb::velocypack::StringRef const& rhs) const noexcept {
    return arangodb::velocypack::StringRef(lhs) < rhs;
  }
  inline bool operator()(std::string const& lhs, std::string const& rhs) const noexcept {
    return lhs < rhs;
  }
};

/// @brief custom comparer for failure points. allows avoiding memory allocations
/// for temporary string objects
Comparer const comparer;

/// @brief a read-write lock for thread-safe access to the failure points set
arangodb::basics::ReadWriteLock failurePointsLock;

/// @brief a global set containing the currently registered failure points
std::set<std::string, ::Comparer> failurePoints(comparer);
}  // namespace

/// @brief intentionally cause a segmentation violation or other failures
/// this is used for crash and recovery tests
void TRI_TerminateDebugging(char const* message) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  CrashHandler::setHardKill();

  // there are some reserved crash messages we use in testing the
  // crash handler
  velocypack::StringRef const s(message);
  if (s == "CRASH-HANDLER-TEST-ABORT") {
    // intentionally crashes the program!
    std::abort();
  } else if (s == "CRASH-HANDLER-TEST-TERMINATE") {
    // intentionally crashes the program!
    std::terminate();
  } else if (s == "CRASH-HANDLER-TEST-TERMINATE-ACTIVE") {
    // intentionally crashes the program!
    // note: when using ASan/UBSan, this actually does not crash
    // the program but continues.
    auto f = []() noexcept {
      // intentionally crashes the program!
      return std::string(nullptr);
    };
    f();
    // we will get here at least with ASan/UBSan.
    std::terminate();
  } else if (s == "CRASH-HANDLER-TEST-SEGFAULT") {
    std::unique_ptr<int> x;
    // intentionally crashes the program!
    int a = *x;
    *x = 2;
    TRI_ASSERT(a == 1);
  } else if (s == "CRASH-HANDLER-TEST-ASSERT") {
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
bool TRI_ShouldFailDebugging(char const* value) {
  READ_LOCKER(readLocker, ::failurePointsLock);
  return ::failurePoints.find(arangodb::velocypack::StringRef(value)) != ::failurePoints.end();
}

/// @brief add a failure point
void TRI_AddFailurePointDebugging(char const* value) {
  bool added = false;
  {
    WRITE_LOCKER(writeLocker, ::failurePointsLock);
    added = ::failurePoints.emplace(value).second;
  }

  if (added) {
    LOG_TOPIC("d8a5f", WARN, arangodb::Logger::FIXME)
        << "activating intentional failure point '" << value
        << "'. the server will misbehave!";
  }
}

/// @brief remove a failure point
void TRI_RemoveFailurePointDebugging(char const* value) {
  WRITE_LOCKER(writeLocker, ::failurePointsLock);
  ::failurePoints.erase(std::string(value));
}

/// @brief clear all failure points
void TRI_ClearFailurePointsDebugging() {
  WRITE_LOCKER(writeLocker, ::failurePointsLock);
  ::failurePoints.clear();
}
#endif

template<> char const conpar<true>::open = '{';
template<> char const conpar<true>::close = '}';
template<> char const conpar<false>::open = '[';
template<> char const conpar<false>::close = ']';
