////////////////////////////////////////////////////////////////////////////////
/// @brief MRuby shell
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

#include <v8.h>

#include <stdio.h>
#include <fstream>

#include "build.h"

#include "BasicsC/csv.h"

#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "BasicsC/files.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "Logger/Logger.h"
#include "MRuby/MRLineEditor.h"
#include "MRuby/mr-utils.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

extern "C" {
#include "mruby.h"
#include "mruby/proc.h"
#include "mruby/data.h"
#include "mruby/variable.h"
#include "compile.h"
}

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief connection default values
////////////////////////////////////////////////////////////////////////////////

// static string DEFAULT_SERVER_NAME = "localhost";
// static int    DEFAULT_SERVER_PORT = 8529;
// static double DEFAULT_REQUEST_TIMEOUT = 10.0;
// static size_t DEFAULT_RETRIES = 5;
// static double DEFAULT_CONNECTION_TIMEOUT = 1.0;

////////////////////////////////////////////////////////////////////////////////
/// @brief server address
////////////////////////////////////////////////////////////////////////////////

static string ServerAddress = "127.0.0.1:8529";

////////////////////////////////////////////////////////////////////////////////
/// @brief the output pager
////////////////////////////////////////////////////////////////////////////////

static string OutputPager = "less -X -R -F -L";

////////////////////////////////////////////////////////////////////////////////
/// @brief the pager FILE 
////////////////////////////////////////////////////////////////////////////////

static FILE* PAGER = stdout;

////////////////////////////////////////////////////////////////////////////////
/// @brief use pager
////////////////////////////////////////////////////////////////////////////////

static bool UsePager = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief deactivate colors
////////////////////////////////////////////////////////////////////////////////

static bool NoColors = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief use pretty print
////////////////////////////////////////////////////////////////////////////////

static bool PrettyPrint = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief disable auto completion
////////////////////////////////////////////////////////////////////////////////

static bool NoAutoComplete = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief adding colors for output
////////////////////////////////////////////////////////////////////////////////

static char DEF_RED[6]         = "\x1b[31m";
// static char DEF_BOLD_RED[8]    = "\x1b[1;31m";
static char DEF_GREEN[6]       = "\x1b[32m";
// static char DEF_BOLD_GREEN[8]  = "\x1b[1;32m";
// static char DEF_BLUE[6]        = "\x1b[34m";
// static char DEF_BOLD_BLUE[8]   = "\x1b[1;34m";
// static char DEF_YELLOW[8]      = "\x1b[1;33m";
// static char DEF_WHITE[6]       = "\x1b[37m";
// static char DEF_BOLD_WHITE[8]  = "\x1b[1;37m";
// static char DEF_BLACK[6]       = "\x1b[30m";
// static char DEF_BOLD_BLACK[8]  = "\x1b[1;39m";
// static char DEF_BLINK[5]       = "\x1b[5m";
// static char DEF_BRIGHT[5]      = "\x1b[1m";
static char DEF_RESET[5]       = "\x1b[0m";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              JavaScript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief print to pager
////////////////////////////////////////////////////////////////////////////////

#if 0
static void InternalPrint (const char *format, const char *str = 0) {
  if (str) {
    fprintf(PAGER, format, str);    
  }
  else {
    fprintf(PAGER, "%s", format);    
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief starts pager
////////////////////////////////////////////////////////////////////////////////

#if 0
static void StartPager () {
  if (! UsePager || OutputPager == "" || OutputPager == "stdout") {
    PAGER = stdout;
    return;
  }
  
  PAGER = popen(OutputPager.c_str(), "w");

  if (PAGER == 0) {
    printf("popen() failed! defaulting PAGER to stdout!\n");
    PAGER = stdout;
    UsePager = false;
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stops pager
////////////////////////////////////////////////////////////////////////////////

#if 0
static void StopPager () {
  if (PAGER != stdout) {
    pclose(PAGER);
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief splits the address
////////////////////////////////////////////////////////////////////////////////

#if 0
static bool SplitServerAdress (string const& definition, string& address, int& port) {
  if (definition.empty()) {
    return false;
  }

  if (definition[0] == '[') {
    // ipv6 address
    size_t find = definition.find("]:", 1);

    if (find != string::npos && find + 2 < definition.size()) {
      address = definition.substr(1, find - 1);
      port = triagens::basics::StringUtils::uint32(definition.substr(find + 2));
      return true;
    }

  }

  int n = triagens::basics::StringUtils::numEntries(definition, ":");

  if (n == 1) {
    address = "";
    port = triagens::basics::StringUtils::uint32(definition);
    return true;
  }
  else if (n == 2) {
    address = triagens::basics::StringUtils::entry(1, definition, ":");
    port = triagens::basics::StringUtils::int32(triagens::basics::StringUtils::entry(2, definition, ":"));
    return true;
  }
  else {
    return false;
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions (int argc, char* argv[]) {
  string level = "info";

  ProgramOptionsDescription description("STANDARD options");

  ProgramOptionsDescription hidden("HIDDEN options");

  hidden
    ("colors", "activate color support")
    ("no-pretty-print", "disable pretty printting")          
    ("auto-complete", "enable auto completion, use no-auto-complete to disable")
  ;

  description
    ("help,h", "help message")
    ("log.level,l", &level,  "log level")
    ("server", &ServerAddress, "server address and port")
    // ("startup.directory", &StartupPath, "startup paths containing the JavaScript files; multiple directories can be separated by cola")
    // ("startup.modules-path", &StartupModules, "one or more directories separated by cola")
    ("pager", &OutputPager, "output pager")
    ("use-pager", "use pager")
    ("pretty-print", "pretty print values")          
    ("no-colors", "deactivate color support")
    ("no-auto-complete", "disable auto completion")
    // ("unit-tests", &UnitTests, "do not start as shell, run unit tests instead")
    (hidden, true)
  ;

  ProgramOptions options;

  if (! options.parse(description, argc, argv)) {
    cerr << options.lastError() << "\n";
    exit(EXIT_FAILURE);
  }

  // check for help
  set<string> help = options.needHelp("help");

  if (! help.empty()) {
    cout << description.usage(help) << endl;
    exit(EXIT_SUCCESS);
  }

  // set the logging
  TRI_SetLogLevelLogging(level.c_str());
  TRI_CreateLogAppenderFile("-");

  // set colors
  if (options.has("colors")) {
    NoColors = false;
  }

  if (options.has("no-colors")) {
    NoColors = true;
  }

  if (options.has("auto-complete")) {
    NoAutoComplete = false;
  }

  if (options.has("no-auto-complete")) {
    NoAutoComplete = true;
  }

  if (options.has("pretty-print")) {
    PrettyPrint = true;
  }

  if (options.has("no-pretty-print")) {
    PrettyPrint = false;
  }

  if (options.has("use-pager")) {
    UsePager = true;
  }
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief executes the shell
////////////////////////////////////////////////////////////////////////////////

static void RunShell (mrb_state* mrb) {
  MRLineEditor* console = new MRLineEditor(mrb, ".avocado-mrb");

  console->open(! NoAutoComplete);

  while (true) {
    char* input = console->prompt("avocirb> ");

    if (input == 0) {
      break;
    }

    if (*input == '\0') {
      TRI_FreeString(input);
      continue;
    }

    console->addHistory(input);

    struct mrb_parser_state* p = mrb_parse_nstring(mrb, input, strlen(input));
    TRI_FreeString(input);

    if (p == 0 || p->tree == 0 || 0 < p->nerr) {
      cout << "UPPS!\n";
      continue;
    }

    int n = mrb_generate_code(mrb, p->tree);

    if (n < 0) {
      cout << "UPPS: " << n << " returned by mrb_generate_code\n";
      continue;
    }

    mrb_value result = mrb_run(mrb, mrb_proc_new(mrb, mrb->irep[n]), mrb_nil_value());

    if (mrb->exc) {
      cout << "Caught exception:\n";
      mrb_funcall(mrb, mrb_nil_value(), "p", 1, mrb_obj_value(mrb->exc));
      mrb->exc = 0;
    }
#if 0
    else {
      mrb_funcall(mrb, mrb_nil_value(), "p", 1, result);
    }
#endif
  }

  console->close();
  printf("\nBye Bye! Auf Wiedersehen! さようなら\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  TRIAGENS_C_INITIALISE;
  TRI_InitialiseLogging(false);
  int ret = EXIT_SUCCESS;

  // parse the program options
  ParseProgramOptions(argc, argv);

  // http://www.network-science.de/ascii/   Font: ogre
  if (NoColors) {
    printf("                        _      _      \n");
    printf("   __ ___   _____   ___(_)_ __| |__   \n");
    printf("  / _` \\ \\ / / _ \\ / __| | '__| '_ \\  \n");
    printf(" | (_| |\\ V / (_) | (__| | |  | |_) | \n");
    printf("  \\__,_| \\_/ \\___/ \\___|_|_|  |_.__/  \n");
    printf("                                      \n");
  }
  else {
    printf("%s                       %s _      _      %s\n", DEF_GREEN, DEF_RED, DEF_RESET);
    printf("%s   __ ___   _____   ___%s(_)_ __| |__   %s\n", DEF_GREEN, DEF_RED, DEF_RESET);
    printf("%s  / _` \\ \\ / / _ \\ / __%s| | '__| '_ \\  %s\n", DEF_GREEN, DEF_RED, DEF_RESET);
    printf("%s | (_| |\\ V / (_) | (__%s| | |  | |_) | %s\n", DEF_GREEN, DEF_RED, DEF_RESET);
    printf("%s  \\__,_| \\_/ \\___/ \\___%s|_|_|  |_.__/  %s\n", DEF_GREEN, DEF_RED, DEF_RESET);
    printf("%s                       %s               %s\n", DEF_GREEN, DEF_RED, DEF_RESET);
  }
  printf("Welcome to avocirb %s. Copyright (c) 2012 triAGENS GmbH.\n", TRIAGENS_VERSION);

#ifdef TRI_V8_VERSION
  printf("Using MRUBY %s engine. Copyright (c) 2012 mruby developers.\n", TRI_MRUBY_VERSION);
#endif

#ifdef TRI_READLINE_VERSION
  printf("Using READLINE %s.\n", TRI_READLINE_VERSION);
#endif

  printf("\n");

  if (UsePager) {
    printf("Using pager '%s' for output buffering.\n", OutputPager.c_str());    
  }
 
  if (PrettyPrint) {
    printf("Pretty print values.\n");    
  }

  // create a new ruby shell
  mrb_state* mrb = mrb_open();

  TRI_InitMRUtils(mrb);

  RunShell(mrb);

  // calling dispose in V8 3.10.x causes a segfault. the v8 docs says its not necessary to call it upon program termination
  // v8::V8::Dispose();

  return ret;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
