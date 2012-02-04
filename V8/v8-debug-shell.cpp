////////////////////////////////////////////////////////////////////////////////
/// @brief V8 debug shell
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "V8/v8-globals.h"

#include <stdio.h>

#include "BasicsC/files.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "Rest/Initialise.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"

#include "v8-actions.h"
#include "v8-line-editor.h"
#include "v8-shell.h"
#include "v8-utils.h"
#include "v8-vocbase.h"

using namespace v8;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief need to run the shell
////////////////////////////////////////////////////////////////////////////////

static bool RunShellFlag = false;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the shell
////////////////////////////////////////////////////////////////////////////////

static void RunShell (v8::Handle<v8::Context> context) {
  v8::Context::Scope contextScope(context);
  v8::Local<v8::String> name(v8::String::New("(shell)"));

  V8LineEditor* console = new V8LineEditor(context, ".avoc");

  console->open();

  while (true) {
    while(! V8::IdleNotification()) {
    }

    char* input = console->prompt("avocado> ");

    if (input == 0) {
      break;
    }

    if (*input == '\0') {
      continue;
    }

    console->addHistory(input);

    HandleScope scope;

    TRI_ExecuteStringVocBase(context, String::New(input), name, true, true);
    TRI_FreeString(input);
  }

  printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief proceses the command line arguments
////////////////////////////////////////////////////////////////////////////////

static int RunMain (v8::Handle<v8::Context> context, int argc, char* argv[]) {
  HandleScope scope;

  for (int i = 1;  i < argc;  ++i) {
    const char* str = argv[i];

    if (strcmp(str, "--shell") == 0) {
      RunShellFlag = true;
    }
    else if (strncmp(str, "--", 2) == 0) {
      printf("Warning: unknown flag %s.\n", str);
    }
    else {
      v8::Handle<v8::String> filename = v8::String::New(str);
      char* content = TRI_SlurpFile(str);

      if (content == 0) {
        printf("Error reading '%s'\n", str);
        continue;
      }

      v8::Handle<v8::String> source = v8::String::New(content);
      TRI_FreeString(content);

      bool ok = TRI_ExecuteStringVocBase(context, source, filename, false, true);

      if (! ok) {
        return 1;
      }
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  TRIAGENS_REST_INITIALISE;

  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);

  RunShellFlag = (argc == 1);

  TRI_InitialiseLogging(false);
  TRI_InitialiseVocBase();

  TRI_SetLogLevelLogging("info");
  TRI_CreateLogAppenderFile("-");

  char const* path = "/tmp/vocbase";
  TRI_vocbase_t* VocBase;
  VocBase = TRI_OpenVocBase(path);

  if (VocBase == 0) {
    printf("cannot open database '%s'\n", path);
    exit(EXIT_FAILURE);
  }

  v8::HandleScope handle_scope;

  // create the global template
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

  // create the context
  v8::Persistent<v8::Context> context = v8::Context::New(0, global);

  if (context.IsEmpty()) {
    printf("cannot initialize V8 engine\n");
    exit(EXIT_FAILURE);
  }

  context->Enter();

  TRI_InitV8VocBridge(context, VocBase);
  TRI_InitV8Actions(context);
  TRI_InitV8Conversions(context);
  TRI_InitV8Utils(context, ".");
  TRI_InitV8Shell(context);

  char const* files[] = {
    "js/bootstrap/modules.js",
    "js/bootstrap/print.js",
    "js/modules/shell.js",
    "js/modules/json.js"
  };

  for (size_t i = 0;   i < sizeof(files) / sizeof(files[0]);  ++i) {
    bool ok;

    ok = TRI_LoadJavaScriptFile(context, files[i]);

    if (! ok) {
      LOG_ERROR("cannot load file '%s'", files[i]);
    }
  }

  RunMain(context, argc, argv);

  if (RunShellFlag) {
    RunShell(context);
  }

  context->Exit();
  context.Dispose();
  v8::V8::Dispose();

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
