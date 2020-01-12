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

#include <dirent.h>
#include <thread>

#ifndef _WIN32
#include <execinfo.h>
#endif

#include "CrashHandlerFeature.h"
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
}

/// @brief logs the occurrence of a signal to the logfile.
/// does not allocate any memory, so should be safe to call even
/// in SIGSEGV context with a broken heap etc.
/// assumes that the buffer pointed to by s has enough space to
/// hold the thread id, the thread name and the signal name
/// (256 bytes should be more than enough)
size_t buildLogMessage(char* s, int signal) {
  char* p = s;
  appendNullTerminatedString("thread ", p);
  size_t len = arangodb::basics::StringUtils::itoa(uint64_t(arangodb::Thread::currentThreadNumber()), p);
  p += len;
  
  appendNullTerminatedString(", tid ", p);
  len = arangodb::basics::StringUtils::itoa(uint64_t(gettid()), p);
  p += len;

  char const* name = arangodb::Thread::currentThreadName();
  if (gettid() == getpid()) {
    name = "main";
  }
  if (name != nullptr && *name != '\0') {
    appendNullTerminatedString(" [", p);
    appendNullTerminatedString(name, p);
    appendNullTerminatedString("]", p);
  }
  appendNullTerminatedString(" caught unexpected signal ", p);
  len = arangodb::basics::StringUtils::itoa(uint64_t(signal), p);
  p += len;
  appendNullTerminatedString(" (", p);
  appendNullTerminatedString(arangodb::signals::name(signal), p);
  appendNullTerminatedString(")", p);
  appendNullTerminatedString(". attempting stack trace dump...", p);
  
  return p - s;
}

void crashHandler(int signal, siginfo_t* info, void*) {
  char buffer[256];
  memset(&buffer[0], 0, sizeof(buffer));

  size_t length = buildLogMessage(&buffer[0], signal);
  LOG_TOPIC("a7902", ERR, arangodb::Logger::FIXME) << arangodb::Logger::CHARS(&buffer[0], length);
  arangodb::Logger::flush();

  void* traces[64];
  int size = backtrace(traces, sizeof(traces) / sizeof(traces[0]));
  char** stack = backtrace_symbols(traces, size); 
  if (stack != nullptr) {
    int skip = 2;
    for (int i = skip /* ignore first frames */; i < size; ++i) {
      LOG_TOPIC("308c2", WARN, arangodb::Logger::FIXME) << "- #" << (i - skip) << ' ' << stack[i];
    }
    free(stack);
  }
  
  arangodb::Logger::flush();
  arangodb::Logger::shutdown();

  // restore default signal action, so that we can write a core dump and crash "properly"
  struct sigaction act;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND;
  act.sa_handler = SIG_DFL;
  sigaction(signal, &act, nullptr);

  // resend signal to ourselves to invoke default action for the signal
  kill(getpid(), signal);
}
} // namespace
#endif

namespace arangodb {

CrashHandlerFeature::CrashHandlerFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "CrashHandler") {
  setOptional(true);
  startsAfter<LoggerFeature>();
}

void CrashHandlerFeature::prepare() {
#ifndef _WIN32
  // install signal handler
  struct sigaction act;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
  act.sa_sigaction = crashHandler;
  sigaction(SIGSEGV, &act,nullptr);
  sigaction(SIGBUS, &act, nullptr);
  sigaction(SIGILL, &act, nullptr);
  sigaction(SIGFPE, &act, nullptr);
#endif
}

}  // namespace arangodb
