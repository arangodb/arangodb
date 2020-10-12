////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/memory.h"
#include "Logger/LogAppender.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <velocypack/StringRef.h>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#if ARANGODB_ENABLE_BACKTRACE
#include <sstream>
#ifdef _WIN32
#include <DbgHelp.h>
#else
#include <cxxabi.h>
#include <execinfo.h>
#endif
#endif
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
  WRITE_LOCKER(writeLocker, ::failurePointsLock);

  if (::failurePoints.emplace(value).second) {
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

/// @brief appends a backtrace to the string provided
void TRI_GetBacktrace(std::string& btstr) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#if ARANGODB_ENABLE_BACKTRACE
#ifdef _WIN32
  void* stack[100];
  HANDLE process = GetCurrentProcess();

  SymInitialize(process, nullptr, true);

  unsigned short frames = CaptureStackBackTrace(0, 100, stack, nullptr);
  SYMBOL_INFO* symbol =
      (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);

  if (symbol == nullptr) {
    // cannot allocate memory
    return;
  }

  symbol->MaxNameLen = 255;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  IMAGEHLP_LINE64 line;
  line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
  char address[64];
  std::string fn;
  std::string err;
  DWORD dwDisplacement;
  for (unsigned int i = 0; i < frames; i++) {
    DWORD lineNumber;
    fn.clear();
    SymFromAddr(process, (DWORD64)stack[i], nullptr, symbol);
    if (SymGetLineFromAddr64(process, (DWORD64)stack[i], &dwDisplacement, &line)) {
      // SymGetLineFromAddr64 returned success
      if (line.FileName != nullptr) {
        fn = line.FileName;
      }
      lineNumber = line.LineNumber;
      err.clear();
    } else {
      lineNumber = -1;
      DWORD error = GetLastError();
      err = std::string("SymGetLineFromAddr64 returned error: ") + std::to_string(error);
    }
    snprintf(address, sizeof(address), "0x%0X", (unsigned int)symbol->Address);
    btstr += std::to_string(frames - i - 1) + std::string(": ") + symbol->Name +
             std::string(" [") + address + std::string("] ") + fn +
             std::string(":") + std::to_string(lineNumber) + err +
             std::string("\n");
  }

  TRI_Free(symbol);

#else
  void* stack_frames[50];

  size_t size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
  char** strings = backtrace_symbols(stack_frames, size);

  for (size_t i = 0; i < size; i++) {
    std::stringstream ss;
    if (strings != nullptr) {
      char *mangled_name = nullptr, *offset_begin = nullptr, *offset_end = nullptr;

      // find parentheses and +address offset surrounding mangled name
      for (char* p = strings[i]; *p; ++p) {
        if (*p == '(') {
          mangled_name = p;
        } else if (*p == '+') {
          offset_begin = p;
        } else if (*p == ')') {
          offset_end = p;
          break;
        }
      }
      if (mangled_name && offset_begin && offset_end && mangled_name < offset_begin) {
        *mangled_name++ = '\0';
        *offset_begin++ = '\0';
        *offset_end++ = '\0';
        int status = 0;
        char* demangled_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);

        if (demangled_name != nullptr) {
          if (status == 0) {
            ss << stack_frames[i];
            btstr += strings[i] + std::string("() [") + ss.str() +
                     std::string("] ") + demangled_name + '\n';
          } else {
            btstr += strings[i];
            btstr += '\n';
          }
          TRI_Free(demangled_name);
        }
      } else {
        btstr += strings[i];
        btstr += '\n';
      }
    } else {
      ss << stack_frames[i];
      btstr += ss.str() + '\n';
    }
  }
  if (strings != nullptr) {
    TRI_Free(strings);
  }
#endif
#endif
#endif
}

/// @brief prints a backtrace on stderr
void TRI_PrintBacktrace() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#if ARANGODB_ENABLE_BACKTRACE
  if (LogAppender::allowStdLogging()) {
    std::string out;
    TRI_GetBacktrace(out);
    fprintf(stderr, "%s", out.c_str());
  }
#endif
#if TRI_HAVE_PSTACK
  char buf[64];
  snprintf(buf, sizeof(buf), "/usr/bin/pstack %i", getpid());
  system(buf);
#endif
#endif
}

/// @brief logs a backtrace in log level warning
void TRI_LogBacktrace() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#if ARANGODB_ENABLE_BACKTRACE
  std::string bt;
  TRI_GetBacktrace(bt);
  if (!bt.empty()) {
    LOG_TOPIC("3945a", WARN, arangodb::Logger::FIXME) << bt;
  }
#endif
#endif
}

template<> char const conpar<true>::open = '{';
template<> char const conpar<true>::close = '}';
template<> char const conpar<false>::open = '[';
template<> char const conpar<false>::close = ']';
