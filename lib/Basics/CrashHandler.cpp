////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/operating-system.h"

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>

#ifndef _WIN32
#include <execinfo.h>
#endif

#ifdef __linux__
#include <syscall.h>
#endif

#include "CrashHandler.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/signals.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#ifndef _WIN32
namespace {
/// @brief appends null-terminated string src to dst,
/// advances dst pointer by length of src
void appendNullTerminatedString(char const* src, char*& dst) {
  size_t len = strlen(src);
  memcpy(static_cast<void*>(dst), src, len);
  dst += len;
  *dst = '\0';
}

/// @brief logs the occurrence of a signal to the logfile.
/// does not allocate any memory, so should be safe to call even
/// in SIGSEGV context with a broken heap etc.
/// assumes that the buffer pointed to by s has enough space to
/// hold the thread id, the thread name and the signal name
/// (512 bytes should be more than enough)
size_t buildLogMessage(char* s, int signal, siginfo_t const* info, int stackSize) {
  // build a crash message
  char* p = s;
  appendNullTerminatedString("thread ", p);
  p += arangodb::basics::StringUtils::itoa(uint64_t(arangodb::Thread::currentThreadNumber()), p);
  
#ifdef __linux__
  // gettid() would be nice to use, unfortunately it is only available
  // in _very_ recent versions of glibc and current not available in libmusl.
  // thus we use syscall here. evil, but works
  uint64_t threadId = static_cast<uint64_t>(syscall(__NR_gettid));
  appendNullTerminatedString(", tid ", p);
  p += arangodb::basics::StringUtils::itoa(threadId, p);

  char const* name = arangodb::Thread::currentThreadName();
  if (threadId == static_cast<uint64_t>(::getpid())) {
    name = "main";
  }
#else
  char const* name = nullptr;
#endif
  if (name != nullptr && *name != '\0') {
    appendNullTerminatedString(" [", p);
    appendNullTerminatedString(name, p);
    appendNullTerminatedString("]", p);
  }
  appendNullTerminatedString(" caught unexpected signal ", p);
  p += arangodb::basics::StringUtils::itoa(uint64_t(signal), p);
  appendNullTerminatedString(" (", p);
  appendNullTerminatedString(arangodb::signals::name(signal), p);
  appendNullTerminatedString(")", p);
  
  if (signal == SIGSEGV || signal == SIGBUS) {
    // dump address that was accessed when the failure occurred (this is very likely
    // a nullptr)
    appendNullTerminatedString(" accessing address 0x", p);
    char chars[] = "0123456789abcdef";
    unsigned char const* x = reinterpret_cast<unsigned char const*>(info->si_addr);
    unsigned char const* s = reinterpret_cast<unsigned char const*>(&x);
    unsigned char const* e = s + sizeof(unsigned char const*);

    while (--e >= s) {
      unsigned char c = *e;
      *p++ = chars[c >> 4U];
      *p++ = chars[c & 0xfU];
    }
  }

  appendNullTerminatedString(". displaying ", p);
  p += arangodb::basics::StringUtils::itoa(uint64_t(stackSize), p);
  
  appendNullTerminatedString(" stack frame(s). use addr2line to resolve addresses!", p);
  
  return p - s;
}

void crashHandler(int signal, siginfo_t* info, void*) {
  // restore default signal action, so that we can write a core dump and crash "properly"
  struct sigaction act;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND;
  act.sa_handler = SIG_DFL;
  sigaction(signal, &act, nullptr);
  
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // prints a backtrace just in maintainer mode, and only if ARANGODB_ENABLE_BACKTRACE
  // is defined. Will do nothing in production
  TRI_PrintBacktrace();
#endif

  try {
    // buffer for constructing temporary log messages (to avoid malloc as much as possible)
    char buffer[512];
    memset(&buffer[0], 0, sizeof(buffer));

    // acquire backtrace
    static constexpr int maxFrames = 100;
    void* traces[maxFrames];
    int skipFrames = 2;
    int numFrames = backtrace(traces, maxFrames);
    
    char* p = &buffer[0];
    size_t length = buildLogMessage(p, signal, info, numFrames - skipFrames);
    // note: LOG_TOPIC() will allocate memory
    LOG_TOPIC("a7902", ERR, arangodb::Logger::CRASH) << arangodb::Logger::CHARS(&buffer[0], length);
    arangodb::Logger::flush();

    // note: backtrace_symbols() will allocate memory
    char** stack = backtrace_symbols(traces, numFrames); 
    if (stack != nullptr) {
      for (int i = skipFrames /* ignore first frames */; i < numFrames; ++i) {
        char* p = &buffer[0];
        appendNullTerminatedString("- frame #", p);
        p += arangodb::basics::StringUtils::itoa(uint64_t(i), p);
        appendNullTerminatedString(": ", p);
        if (strlen(stack[i]) <= 256) {
          appendNullTerminatedString(stack[i], p);

          LOG_TOPIC("308c2", INFO, arangodb::Logger::CRASH) << arangodb::Logger::CHARS(&buffer[0], p - &buffer[0]);
        }
      }
      free(stack);
    }
    
    arangodb::Logger::flush();
    arangodb::Logger::shutdown();
  } catch (...) {
    // we better not throw an exception from inside a signal handler
  }

#ifdef _WIN32
  exit(255 + signal);
#else
  // resend signal to ourselves to invoke default action for the signal (e.g. coredump)
  ::kill(::getpid(), signal);
#endif
}
} // namespace
#endif

namespace arangodb {

void CrashHandler::installCrashHandler() {
#ifndef _WIN32
  // install signal handlers
  struct sigaction act;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
  act.sa_sigaction = crashHandler;
  sigaction(SIGSEGV, &act,nullptr);
  sigaction(SIGBUS, &act, nullptr);
  sigaction(SIGILL, &act, nullptr);
  sigaction(SIGFPE, &act, nullptr);
  sigaction(SIGABRT, &act,nullptr);
#endif
}

}  // namespace arangodb
