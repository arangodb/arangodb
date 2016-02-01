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

#include <iostream>

#include "ApplicationServer/ApplicationServer.h"
#include "Basics/Logger.h"
#include "Basics/tri-strings.h"
#include "Basics/MutexLocker.h"
#include "Rest/Version.h"
#include "VocBase/vocbase.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include <v8.h>

using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief the line editor object for use in debugging
////////////////////////////////////////////////////////////////////////////////

V8LineEditor* ConsoleThread::serverConsole = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex for console access
////////////////////////////////////////////////////////////////////////////////

Mutex ConsoleThread::serverConsoleMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a console thread
////////////////////////////////////////////////////////////////////////////////

ConsoleThread::ConsoleThread(ApplicationServer* applicationServer,
                             ApplicationV8* applicationV8,
                             TRI_vocbase_t* vocbase)
    : Thread("Console"),
      _applicationServer(applicationServer),
      _applicationV8(applicationV8),
      _context(nullptr),
      _vocbase(vocbase),
      _done(0),
      _userAborted(false) {
  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a console thread
////////////////////////////////////////////////////////////////////////////////

ConsoleThread::~ConsoleThread() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the thread
////////////////////////////////////////////////////////////////////////////////

void ConsoleThread::run() {
  usleep(100 * 1000);

  // enter V8 context
  _context = _applicationV8->enterContext(_vocbase, true);

  // work
  try {
    inner();
  } catch (char const*) {
  } catch (...) {
    _applicationV8->exitContext(_context);
    _done = true;
    _applicationServer->beginShutdown();

    throw;
  }

  // exit context
  _applicationV8->exitContext(_context);
  _done = true;
  _applicationServer->beginShutdown();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inner thread loop - this handles all the user inputs
////////////////////////////////////////////////////////////////////////////////

void ConsoleThread::inner() {
  v8::Isolate* isolate = _context->isolate;
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

    while (!_userAborted) {
      if (nrCommands >= gcInterval) {
        TRI_RunGarbageCollectionV8(isolate, 0.5);
        nrCommands = 0;
      }

      std::string input;
      bool eof;

      {
        MUTEX_LOCKER(mutexLocker, serverConsoleMutex);
        input = console.prompt("arangod> ", "arangod", eof);
      }

      if (eof) {
        _userAborted = true;
      }

      if (_userAborted) {
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

        if (_userAborted) {
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
  throw "user aborted";
}
