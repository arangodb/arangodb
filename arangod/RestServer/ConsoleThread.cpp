////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Basics/application-exit.h"
#include "Basics/tri-strings.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/Version.h"
#include "V8/JavaScriptSecurityContext.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/vocbase.h"

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
struct UserAbortedException {};
}  // namespace

V8LineEditor* ConsoleThread::serverConsole = nullptr;
std::mutex ConsoleThread::serverConsoleMutex;

ConsoleThread::ConsoleThread(Server& applicationServer, TRI_vocbase_t* vocbase)
    : ServerThread<ArangodServer>(applicationServer, "Console"),
      _vocbase(vocbase),
      _userAborted(false) {}

ConsoleThread::~ConsoleThread() { shutdown(); }

void ConsoleThread::run() {
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  bool v8Enabled = server().hasFeature<V8DealerFeature>() &&
                   server().isEnabled<V8DealerFeature>() &&
                   server().getFeature<V8DealerFeature>().isEnabled();
  if (!v8Enabled) {
    LOG_TOPIC("4a00f", FATAL, arangodb::Logger::FIXME)
        << "V8 engine is not enabled";
    FATAL_ERROR_EXIT();
  }

  // work
  try {
    // enter V8 context
    JavaScriptSecurityContext securityContext =
        JavaScriptSecurityContext::createAdminScriptContext();
    V8ExecutorGuard guard(_vocbase, securityContext);

    inner(guard);
  } catch (UserAbortedException const&) {
    LOG_TOPIC("6e7fd", TRACE, arangodb::Logger::FIXME) << "user aborted";
  } catch (...) {
    _server.beginShutdown();
    throw;
  }

  // exit context
  _server.beginShutdown();
}

void ConsoleThread::inner(V8ExecutorGuard& guard) {
  // flush all log output before we print the console prompt
  Logger::flush();

  // run the shell
  std::cout << "arangod console (" << rest::Version::getVerboseVersionString()
            << ")" << std::endl;
  std::cout << "Copyright (c) ArangoDB GmbH" << std::endl;

  guard.runInContext([this](v8::Isolate* isolate) -> Result {
    v8::HandleScope globalScope(isolate);

    // .............................................................................
    // run console
    // .............................................................................

    uint64_t const gcInterval = 10;
    uint64_t nrCommands = 0;

    // read and eval .arangod.rc from home directory if it exists
    std::string_view startupScript = R"SCRIPT(
start_pretty_print(true);
start_color_print('arangodb', true);

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

    TRI_ExecuteJavaScriptString(isolate, startupScript, "startup", false);

    // allow SIGINT in this particular thread... otherwise we cannot CTRL-C the
    // console
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    if (pthread_sigmask(SIG_UNBLOCK, &set, nullptr) < 0) {
      LOG_TOPIC("62022", ERR, arangodb::Logger::FIXME)
          << "unable to install signal handler";
    }

    v8::Handle<v8::Context> context = isolate->GetCurrentContext();
    V8LineEditor console(isolate, context, ".arangod.history");

    console.open(true);

    {
      std::lock_guard mutexLocker{serverConsoleMutex};
      serverConsole = &console;
    }

    bool lastEmpty = false;

    while (!isStopping() && !_userAborted.load()) {
      if (nrCommands >= gcInterval ||
          V8PlatformFeature::isOutOfMemory(isolate)) {
        TRI_RunGarbageCollectionV8(isolate, 0.5);
        nrCommands = 0;

        // needs to be reset after the garbage collection
        V8PlatformFeature::resetOutOfMemory(isolate);
      }

      std::string input;
      ShellBase::EofType eof;

      isolate->CancelTerminateExecution();

      {
        std::lock_guard mutexLocker{serverConsoleMutex};
        input = console.prompt("arangod> ", "arangod>", eof);
      }

      if (eof == ShellBase::EOF_FORCE_ABORT ||
          (eof == ShellBase::EOF_ABORT && lastEmpty)) {
        _userAborted.store(true);
      }

      if (_userAborted.load()) {
        break;
      }

      if (input.empty()) {
        lastEmpty = true;
        continue;
      }
      lastEmpty = false;

      nrCommands++;
      console.addHistory(input);

      {
        v8::TryCatch tryCatch(isolate);
        v8::HandleScope scope(isolate);

        console.setExecutingCommand(true);
        TRI_ExecuteJavaScriptString(
            isolate, input, std::string_view(TRI_V8_SHELL_COMMAND_NAME), true);
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
      std::lock_guard mutexLocker{serverConsoleMutex};
      serverConsole = nullptr;
    }

    return {};
  });

  throw UserAbortedException{};
}
