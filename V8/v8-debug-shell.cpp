////////////////////////////////////////////////////////////////////////////////
/// @brief V8 debug shell
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "V8/v8-globals.h"

#include <stdio.h>

#include <BasicsC/files.h>
#include <BasicsC/logging.h>
#include <BasicsC/strings.h>
#include <Rest/Initialise.h>
#include <VocBase/document-collection.h>
#include <VocBase/vocbase.h>

#include "v8-actions.h"
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

  V8LineEditor* console = new V8LineEditor();

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
  TRI_InitV8Utils(context, ".");
  TRI_InitV8Shell(context);

  bool ok;

  ok = TRI_LoadJavaScriptFile(context, "js/modules.js");

  if (! ok) {
    LOG_ERROR("cannot load file 'js/modules.js'");
  }

  ok = TRI_LoadJavaScriptFile(context, "js/shell.js");

  if (! ok) {
    LOG_ERROR("cannot load file 'js/shell.js'");
  }

  ok = TRI_LoadJavaScriptFile(context, "js/actions.js");

  if (! ok) {
    LOG_ERROR("cannot load file 'js/actions.js'");
  }

  int result = RunMain(context, argc, argv);
  if (RunShellFlag) {
    RunShell(context);
  }

  context->Exit();
  context.Dispose();
  v8::V8::Dispose();

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
