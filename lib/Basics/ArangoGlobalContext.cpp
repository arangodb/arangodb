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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ArangoGlobalContext.h"

#include "Basics/files.h"
#include "Logger/LogAppender.h"
#include "Rest/InitializeRest.h"

using namespace arangodb;

#ifndef _WIN32
static void ReopenLog(int) { LogAppender::reopen(); }
#endif

static void AbortHandler(int signum) {
  TRI_PrintBacktrace();
#ifdef _WIN32
  exit(255 + signum);
#else
  signal(signum, SIG_DFL);
  kill(getpid(), signum);
#endif
}

ArangoGlobalContext* ArangoGlobalContext::CONTEXT = nullptr;

ArangoGlobalContext::ArangoGlobalContext(int argc, char* argv[])
    : _binaryName(TRI_BinaryName(argv[0])), _ret(EXIT_FAILURE) {
  ADB_WindowsEntryFunction();
  TRIAGENS_REST_INITIALIZE();
  CONTEXT = this;
}

ArangoGlobalContext::~ArangoGlobalContext() {
  CONTEXT = nullptr;

#ifndef _WIN32
  signal(SIGHUP, SIG_IGN);
#endif

  TRIAGENS_REST_SHUTDOWN;
  ADB_WindowsExitFunction(_ret, nullptr);
}

int ArangoGlobalContext::exit(int ret) {
  _ret = ret;
  return _ret;
}

void ArangoGlobalContext::installHup() {
#ifndef _WIN32
  signal(SIGHUP, ReopenLog);
#endif
}

void ArangoGlobalContext::installSegv() {
  signal(SIGSEGV, AbortHandler);
}

void ArangoGlobalContext::maskAllSignals() {
#ifdef TRI_HAVE_POSIX_THREADS
  sigset_t all;
  sigfillset(&all);
  pthread_sigmask(SIG_SETMASK, &all, 0);
#endif
}
