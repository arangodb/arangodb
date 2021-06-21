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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>

#include "ArangoGlobalContext.h"

#include "Basics/debugging.h"
#include "Basics/operating-system.h"
#include "Basics/process-utils.h"

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif

#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Logger/LogAppender.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Random/RandomGenerator.h"
#include "Rest/Version.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#else
inline void ADB_WindowsEntryFunction() {}
inline void ADB_WindowsExitFunction(int, void*) {}
#endif

#if (_MSC_VER >= 1)
// Disable a warning caused by the call to ADB_WindowsExitFunction() in
// ~ArangoGlobalContext().
#pragma warning(push)
#pragma warning(disable : 4722)  // destructor never returns, potential memory leak
#endif

using namespace arangodb;
using namespace arangodb::basics;

namespace {

#ifndef _WIN32
static void ReopenLog(int) { LogAppender::reopen(); }
#endif
}  // namespace

ArangoGlobalContext* ArangoGlobalContext::CONTEXT = nullptr;

ArangoGlobalContext::ArangoGlobalContext(int /*argc*/, char* argv[], char const* installDirectory)
    : _binaryName(TRI_BinaryName(argv[0])),
      _binaryPath(TRI_LocateBinaryPath(argv[0])),
      _runRoot(TRI_GetInstallRoot(TRI_LocateBinaryPath(argv[0]), installDirectory)),
      _ret(EXIT_FAILURE) {
#ifndef _WIN32
#ifndef __APPLE__
#ifndef __GLIBC__
  // Increase default stack size for libmusl:
  pthread_attr_t a;
  pthread_attr_init(&a);
  pthread_attr_setstacksize(&a, 8 * 1024 * 1024);  // 8MB as in glibc
  pthread_attr_setguardsize(&a, 4096);             // one page
  pthread_setattr_default_np(&a);
#endif
#endif
#endif

  ADB_WindowsEntryFunction();

  // global initialization
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);

  arangodb::rest::Version::initialize();
  VelocyPackHelper::initialize();

  CONTEXT = this;
}

ArangoGlobalContext::~ArangoGlobalContext() {
  CONTEXT = nullptr;

#ifndef _WIN32
  signal(SIGHUP, SIG_IGN);
#endif

  RandomGenerator::shutdown();
  TRI_ShutdownProcess();

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

void ArangoGlobalContext::normalizePath(std::vector<std::string>& paths,
                                        char const* whichPath, bool fatal) {
  for (auto& path : paths) {
    normalizePath(path, whichPath, fatal);
  }
}

void ArangoGlobalContext::normalizePath(std::string& path, char const* whichPath, bool fatal) {
  StringUtils::rTrimInPlace(path, TRI_DIR_SEPARATOR_STR);

  arangodb::basics::FileUtils::normalizePath(path);
  if (!arangodb::basics::FileUtils::exists(path)) {
    std::string directory = arangodb::basics::FileUtils::buildFilename(_runRoot, path);
    if (!arangodb::basics::FileUtils::exists(directory)) {
      if (!fatal) {
        return;
      }
      LOG_TOPIC("3537a", FATAL, arangodb::Logger::FIXME)
          << "failed to locate " << whichPath << " directory, its neither available in '"
          << path << "' nor in '" << directory << "'";
      FATAL_ERROR_EXIT();
    }
    arangodb::basics::FileUtils::normalizePath(directory);
    path = directory;
  } else {
    if (!TRI_PathIsAbsolute(path)) {
      arangodb::basics::FileUtils::makePathAbsolute(path);
    }
  }
}

#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif
