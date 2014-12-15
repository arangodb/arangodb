////////////////////////////////////////////////////////////////////////////////
/// @brief console thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ConsoleThread.h"

#include "ApplicationServer/ApplicationServer.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Rest/Version.h"
#include "VocBase/vocbase.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include <v8.h>

using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                               class ConsoleThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a console thread
////////////////////////////////////////////////////////////////////////////////

ConsoleThread::ConsoleThread (ApplicationServer* applicationServer,
                              ApplicationV8* applicationV8,
                              TRI_vocbase_t* vocbase)
  : Thread("console"),
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

ConsoleThread::~ConsoleThread () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the thread
////////////////////////////////////////////////////////////////////////////////

void ConsoleThread::run () {
  usleep(100000);

  // enter V8 context
  _context = _applicationV8->enterContext("STANDARD", _vocbase, true, true);

  try {
    inner();
  }
  catch (const char*) {
  }
  catch (...) {
    _applicationV8->exitContext(_context);
    _done = 1;
    _applicationServer->beginShutdown();

    throw;
  }

  _applicationV8->exitContext(_context);
  _done = 1;
  _applicationServer->beginShutdown();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief inner thread loop - this handles all the user inputs
////////////////////////////////////////////////////////////////////////////////

void ConsoleThread::inner () {
  v8::Isolate* isolate = v8::Isolate::GetCurrent(); // TODO: this may go away.
  v8::HandleScope globalScope(isolate);

  // run the shell
  std::cout << "ArangoDB JavaScript emergency console (" << rest::Version::getVerboseVersionString() << ")" << std::endl;

  v8::Local<v8::String> name(TRI_V8_ASCII_STRING("(arango)"));

  auto localContext = v8::Local<v8::Context>::New(isolate, _context->_context);
  localContext->Enter();
  v8::Context::Scope contextScope(localContext);

  // .............................................................................
  // run console
  // .............................................................................

  const uint64_t gcInterval = 10;
  uint64_t nrCommands = 0;

  const std::string pretty = "start_pretty_print();";

  TRI_ExecuteJavaScriptString(isolate,
                              localContext,
                              TRI_V8_STD_STRING(pretty),
                              TRI_V8_ASCII_STRING("(internal)"),
                              false);

  V8LineEditor console(localContext, ".arangod.history");

  console.open(true);

  while (! _userAborted) {
    if (nrCommands >= gcInterval) {
      isolate->LowMemoryNotification();
      while (! isolate->IdleNotification(1000)) {
      }

      nrCommands = 0;
    }

    char* input = console.prompt("arangod> ");

    if (_userAborted) {
      if (input != nullptr) {
        TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);
      }
      throw "user aborted";
    }

    if (input == nullptr) {
      _userAborted = true;

      // this will be caught by "run"
      throw "user aborted";
    }

    if (*input == '\0') {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);
      continue;
    }

    nrCommands++;
    console.addHistory(input);

    v8::TryCatch tryCatch;
    v8::HandleScope scope(isolate);

    TRI_ExecuteJavaScriptString(isolate, localContext, TRI_V8_STRING(input), name, true);
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);

    if (tryCatch.HasCaught()) {
      std::cout << TRI_StringifyV8Exception(isolate, &tryCatch);
    }
  }

  throw "user aborted";
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
