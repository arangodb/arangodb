////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "ConsoleThread.h"

#include <v8.h>
#include <iostream>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/MutexLocker.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "Rest/Version.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::rest;

V8LineEditor* ConsoleThread::serverConsole = nullptr;
Mutex ConsoleThread::serverConsoleMutex;

ConsoleThread::ConsoleThread(ApplicationServer* applicationServer,
                             TRI_vocbase_t* vocbase)
    : Thread("Console"),
      _applicationServer(applicationServer),
      _context(nullptr),
      _vocbase(vocbase),
      _userAborted(false) {}

ConsoleThread::~ConsoleThread() { shutdown(); }

static char const* USER_ABORTED = "user aborted";

void ConsoleThread::run() {
  usleep(100 * 1000);

  // enter V8 context
  _context = V8DealerFeature::DEALER->enterContext(_vocbase, true);

  if (_context == nullptr) {
    LOG(FATAL) << "cannot acquire V8 context";
    FATAL_ERROR_EXIT();
  }

  TRI_DEFER(V8DealerFeature::DEALER->exitContext(_context));

  // work
  try {
    inner();
  } catch (char const* error) {
    if (strcmp(error, USER_ABORTED) != 0) {
      LOG(ERR) << error;
    }
  } catch (...) {
    _applicationServer->beginShutdown();
    throw;
  }

  // exit context
  _applicationServer->beginShutdown();
}

void ConsoleThread::inner() {
  // flush all log output before we print the console prompt
  Logger::flush();

  v8::Isolate* isolate = _context->_isolate;
  v8::HandleScope globalScope(isolate);

  // run the shell
  std::cout << "arangod console (" << rest::Version::getVerboseVersionString()
            << ")" << std::endl;
  std::cout << "Copyright (c) ArangoDB GmbH" << std::endl;

  v8::Local<v8::String> name(TRI_V8_ASCII_STRING(TRI_V8_SHELL_COMMAND_NAME));

  auto localContext = v8::Local<v8::Context>::New(isolate, _context->_context);
  localContext->Enter();
  {
    v8::Context::Scope contextScope(localContext);

    // .............................................................................
    // run console
    // .............................................................................

    uint64_t const gcInterval = 10;
    uint64_t nrCommands = 0;

    // read and eval .arangod.rc from home directory if it exists
    char const* startupScript = R"SCRIPT(
start_pretty_print();

(function () {
  var __fs__ = require("fs");
  var __rcf__ = __fs__.join(__fs__.home(), ".arangod.rc");
  if (__fs__.exists(__rcf__)) {
    try {
      var __content__ = __fs__.read(__rcf__);
      eval(__content__);
    }
    catch (err) {
      require("console").log("error in rc file '%s': %s", __rcf__, String(err.stack || err));
    }
  }
})();
)SCRIPT";

    TRI_ExecuteJavaScriptString(isolate, localContext,
                                TRI_V8_ASCII_STRING(startupScript),
                                TRI_V8_ASCII_STRING("(startup)"), false);

#ifndef _WIN32
    // allow SIGINT in this particular thread... otherwise we cannot CTRL-C the
    // console
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    if (pthread_sigmask(SIG_UNBLOCK, &set, nullptr) < 0) {
      LOG(ERR) << "unable to install signal handler";
    }
#endif

    V8LineEditor console(isolate, localContext, ".arangod.history");

    console.open(true);

    {
      MUTEX_LOCKER(mutexLocker, serverConsoleMutex);
      serverConsole = &console;
    }

    while (!isStopping() && !_userAborted.load()) {
      if (nrCommands >= gcInterval ||
          V8PlatformFeature::isOutOfMemory(isolate)) {
        TRI_RunGarbageCollectionV8(isolate, 0.5);
        nrCommands = 0;

        // needs to be reset after the garbage collection
        V8PlatformFeature::resetOutOfMemory(isolate);
      }

      std::string input;
      bool eof;

      isolate->CancelTerminateExecution();

      {
        MUTEX_LOCKER(mutexLocker, serverConsoleMutex);
        input = console.prompt("arangod> ", "arangod", eof);
      }

      if (eof) {
        _userAborted.store(true);
      }

      if (_userAborted.load()) {
        break;
      }

      if (input.empty()) {
        continue;
      }

      nrCommands++;
      console.addHistory(input);

      {
        v8::TryCatch tryCatch;
        v8::HandleScope scope(isolate);

        console.setExecutingCommand(true);
        TRI_ExecuteJavaScriptString(isolate, localContext,
                                    TRI_V8_STRING(input.c_str()), name, true);
        console.setExecutingCommand(false);

        if (_userAborted.load()) {
          std::cout << "command aborted" << std::endl;
        } else if (tryCatch.HasCaught()) {
          if (!tryCatch.CanContinue() || tryCatch.HasTerminated()) {
            std::cout << "command aborted" << std::endl;
          } else {
            std::cout << TRI_StringifyV8Exception(isolate, &tryCatch);
          }
        }
      }
    }

    {
      MUTEX_LOCKER(mutexLocker, serverConsoleMutex);
      serverConsole = nullptr;
    }
  }

  localContext->Exit();
  throw USER_ABORTED;
}
