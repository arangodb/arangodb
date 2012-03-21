////////////////////////////////////////////////////////////////////////////////
/// @brief V8 shell
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

#include "build.h"

#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "BasicsC/files.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "Logger/Logger.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "V8/JSLoader.h"
#include "V8/v8-conv.h"
#include "V8/v8-line-editor.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8Client/V8ClientConnection.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::v8client;
using namespace triagens::avocado;

#include "js/common/bootstrap/js-print.h"
#include "js/common/bootstrap/js-modules.h"
#include "js/common/bootstrap/js-errors.h"
#include "js/client/js-client.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief connection default values
////////////////////////////////////////////////////////////////////////////////

static string DEFAULT_SERVER_NAME = "localhost";
static int    DEFAULT_SERVER_PORT = 8529;
static double DEFAULT_REQUEST_TIMEOUT = 10.0;
static size_t DEFAULT_RETRIES = 5;
static double DEFAULT_CONNECTION_TIMEOUT = 1.0;

////////////////////////////////////////////////////////////////////////////////
/// @brief path for JavaScript bootstrap files
////////////////////////////////////////////////////////////////////////////////

static string StartupPath = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief path for JavaScript modules files
////////////////////////////////////////////////////////////////////////////////

static string StartupModules = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief startup JavaScript files
////////////////////////////////////////////////////////////////////////////////

static JSLoader StartupLoader;

////////////////////////////////////////////////////////////////////////////////
/// @brief server address
////////////////////////////////////////////////////////////////////////////////

static string ServerAddress = "127.0.0.1:8529";

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection* clientConnection = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief object template for the initial connection
////////////////////////////////////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> ConnectionTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief the output pager
////////////////////////////////////////////////////////////////////////////////

static string OutputPager = "less -X -R -F -L";

////////////////////////////////////////////////////////////////////////////////
/// @brief the pager FILE 
////////////////////////////////////////////////////////////////////////////////

static FILE *PAGER = stdout;

////////////////////////////////////////////////////////////////////////////////
/// @brief use pager
////////////////////////////////////////////////////////////////////////////////

static bool usePager = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief deactivate colors
////////////////////////////////////////////////////////////////////////////////

static bool noColors = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief use pretty print
////////////////////////////////////////////////////////////////////////////////

static bool prettyPrint = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief disable auto completion
////////////////////////////////////////////////////////////////////////////////

static bool noAutoComplete = false;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              JavaScript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief print to pager
////////////////////////////////////////////////////////////////////////////////

static void internalPrint (const char *format, const char *str = 0) {
  if (str) {
    fprintf(PAGER, format, str);    
  }
  else {
    fprintf(PAGER, "%s", format);    
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs the arguments
///
/// @FUN{internal.output(@FA{string1}, @FA{string2}, @FA{string3}, ...)}
///
/// Outputs the arguments to standard output.
///
/// @verbinclude fluent39
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_PagerOutput (v8::Arguments const& argv) {
  for (int i = 0; i < argv.Length(); i++) {
    v8::HandleScope scope;

    // extract the next argument
    v8::Handle<v8::Value> val = argv[i];

    string str = TRI_ObjectToString(val);

    internalPrint(str.c_str());
  }

  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the output pager
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_StartOutputPager (v8::Arguments const& argv) {
  if (usePager) {
    internalPrint("Using pager already.\n");        
  }
  else {
    usePager = true;
    internalPrint("Using pager '%s' for output buffering.\n", OutputPager.c_str());    
  }
  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the output pager
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_StopOutputPager (v8::Arguments const& argv) {
  if (usePager) {
    internalPrint("Stopping pager.\n");
  }
  else {
    internalPrint("Pager not running.\n");    
  }
  usePager = false;
  return v8::Undefined();
}

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
/// @brief starts pager
////////////////////////////////////////////////////////////////////////////////

static void StartPager () {
  if (!usePager || OutputPager == "" || OutputPager == "stdout") {
    PAGER= stdout;
    return;
  }
  
  if (!(PAGER = popen(OutputPager.c_str(), "w"))) {
    printf("popen() failed! defaulting PAGER to stdout!\n");
    PAGER= stdout;
    usePager = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops pager
////////////////////////////////////////////////////////////////////////////////

static void StopPager ()
{
  if (PAGER != stdout) {
    pclose(PAGER);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief splits the address
////////////////////////////////////////////////////////////////////////////////

static bool splitServerAdress (string const& definition, string& address, int& port) {
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
    ("startup.directory", &StartupPath, "startup paths containing the JavaScript files; multiple directories can be separated by cola")
    ("startup.modules-path", &StartupModules, "one or more directories separated by cola")
    ("pager", &OutputPager, "output pager")
    ("use-pager", "use pager")
    ("pretty-print", "pretty print values")          
    ("no-colors", "deactivate color support")
    ("no-auto-complete", "disable auto completion")
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
    noColors = false;
  }

  if (options.has("no-colors")) {
    noColors = true;
  }

  if (options.has("auto-complete")) {
    noAutoComplete = false;
  }

  if (options.has("no-auto-complete")) {
    noAutoComplete = true;
  }

  if (options.has("pretty-print")) {
    prettyPrint = true;
  }

  if (options.has("no-pretty-print")) {
    prettyPrint = false;
  }

  if (options.has("use-pager")) {
    usePager = true;
  }

  // set V8 options
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief copy v8::Object to std::map<string, string>
////////////////////////////////////////////////////////////////////////////////

static void objectToMap (map<string, string>& myMap, v8::Handle<v8::Value> val) {
  v8::Handle<v8::Object> v8Headers = val.As<v8::Object> ();
  if (v8Headers->IsObject()) {
    v8::Handle<v8::Array> props = v8Headers->GetPropertyNames();
    for (uint32_t i = 0; i < props->Length(); i++) {
      v8::Handle<v8::Value> key = props->Get(v8::Integer::New(i));
      myMap[TRI_ObjectToString(key)] = TRI_ObjectToString(v8Headers->Get(key));
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection class
////////////////////////////////////////////////////////////////////////////////

enum WRAP_SLOTS {SLOT_CLASS_TYPE = 0, SLOT_CLASS = 1};
enum WRAP_CLASS_TYPES {WRAP_TYPE_CONNECTION = 1};

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries (call the destructor here)
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_DestructorCallback (v8::Persistent<v8::Value> object, void* parameter) {
  V8ClientConnection* client = (V8ClientConnection*) parameter;
  delete(client);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wrap V8ClientConnection in a v8::Object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> wrapV8ClientConnection (V8ClientConnection* connection) {
  v8::Handle<v8::Object> result = ConnectionTempl->NewInstance();
  v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(connection));
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRAP_TYPE_CONNECTION));
  result->SetInternalField(SLOT_CLASS, persistent);
  persistent.MakeWeak(connection, ClientConnection_DestructorCallback);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unwraps a C++ class given a v8::Object
////////////////////////////////////////////////////////////////////////////////

template<class T>
static T* UnwrapClass (v8::Handle<v8::Object> obj, int32_t type) {
  if (obj->InternalFieldCount() <= SLOT_CLASS) {
    return 0;
  }

  if (obj->GetInternalField(SLOT_CLASS_TYPE)->Int32Value() != type) {
    return 0;
  }

  return static_cast<T*>(v8::Handle<v8::External>::Cast(obj->GetInternalField(SLOT_CLASS))->Value());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection constructor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_ConstructorCallback(v8::Arguments const& argv) {
  v8::HandleScope scope;

  string server = DEFAULT_SERVER_NAME;
  int    port = DEFAULT_SERVER_PORT;
  double requestTimeout = DEFAULT_REQUEST_TIMEOUT;
  size_t retries = DEFAULT_RETRIES;
  double connectionTimeout = DEFAULT_CONNECTION_TIMEOUT;

  if (argv.Length() > 0 && argv[0]->IsString()) {
    string definition = TRI_ObjectToString(argv[0]);
    
    if (!splitServerAdress(definition, server, port)) {
      string errorMessage = "error in '" + definition + "'";
      return scope.Close(v8::ThrowException(v8::String::New(errorMessage.c_str())));      
    }    
    
  }
  
  V8ClientConnection* connection = new V8ClientConnection(server, port, requestTimeout, retries, connectionTimeout);
  
  if (connection->isConnected()) {
    printf("Connected to Avocado DB %s:%d Version %s\n", 
            connection->getHostname().c_str(), 
            connection->getPort(), 
            connection->getVersion().c_str());    
  }
  else {
    string errorMessage = "Could not connect. Error message: " + connection->getErrorMessage();
    delete(connection);
    return scope.Close(v8::ThrowException(v8::String::New(errorMessage.c_str())));
  }

  return scope.Close(wrapV8ClientConnection(connection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "httpGet"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_httpGet(v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  // check params
  if (argv.Length() < 1 || argv.Length() > 2 || !argv[0]->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: get(<url>[, <headers>])")));
  }

  v8::String::Utf8Value url(argv[0]);

  // check header fields
  map<string, string> headerFields;
  if (argv.Length() > 1) {
    objectToMap(headerFields, argv[1]);
  }

  return scope.Close(connection->getData(*url, headerFields));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "httpDelete"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_httpDelete(v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  // check params
  if (argv.Length() < 1 || argv.Length() > 2 || !argv[0]->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: delete(<url>[, <headers>])")));
  }

  v8::String::Utf8Value url(argv[0]);

  // check header fields
  map<string, string> headerFields;
  if (argv.Length() > 1) {
    objectToMap(headerFields, argv[1]);
  }

  return scope.Close(connection->deleteData(*url, headerFields));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "httpPost"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_httpPost(v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  // check params
  if (argv.Length() < 2 || argv.Length() > 3 || !argv[0]->IsString() || !argv[1]->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: post(<url>, <body>[, <headers>])")));
  }

  v8::String::Utf8Value url(argv[0]);
  v8::String::Utf8Value body(argv[1]);

  // check header fields
  map<string, string> headerFields;
  if (argv.Length() > 2) {
    objectToMap(headerFields, argv[2]);
  }

  return scope.Close(connection->postData(*url, *body, headerFields));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "httpPut"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_httpPut(v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  // check params
  if (argv.Length() < 2 || argv.Length() > 3 || !argv[0]->IsString() || !argv[1]->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: put(<url>, <body>[, <headers>])")));
  }

  v8::String::Utf8Value url(argv[0]);
  v8::String::Utf8Value body(argv[1]);

  // check header fields
  map<string, string> headerFields;
  if (argv.Length() > 2) {
    objectToMap(headerFields, argv[2]);
  }

  return scope.Close(connection->putData(*url, *body, headerFields));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "lastError"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_lastHttpReturnCode(v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  // check params
  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: lastHttpReturnCode()")));
  }

  return scope.Close(v8::Integer::New(connection->getLastHttpReturnCode()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "lastErrorMessage"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_lastErrorMessage(v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  // check params
  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: lastErrorMessage()")));
  }

  return scope.Close(v8::String::New(connection->getErrorMessage().c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "isConnected"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_isConnected(v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: isConnected()")));
  }

  return scope.Close(v8::Boolean::New(connection->isConnected()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "isConnected"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_toString(v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: toString()")));
  }
  
  string result = "[object AvocadoConnection:" 
          + connection->getHostname()
          + ":"
          + triagens::basics::StringUtils::itoa(connection->getPort())
          + ","
          + connection->getVersion();
          
  if (connection->isConnected()) {
    result += ",connected]";
  }
  else {
    result += "]";    
  }
  
  return scope.Close(v8::String::New(result.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "isConnected"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_getVersion(v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: getVersion()")));
  }

  return scope.Close(v8::String::New(connection->getVersion().c_str()));
}




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

  V8LineEditor* console = new V8LineEditor(context, ".avocsh");

  console->open(!noAutoComplete);

  while (true) {
    while (! v8::V8::IdleNotification()) {
    }

    char* input = console->prompt("avocsh> ");

    if (input == 0) {
      break;
    }

    if (*input == '\0') {
      continue;
    }

    string i = triagens::basics::StringUtils::trim(input);
    if (i == "exit" || i == "quit") {
      break;
    }

    if (i == "help") {
      TRI_FreeString(input);
      input = TRI_DuplicateString("help()");
    }
    
    console->addHistory(input);
    
    v8::HandleScope scope;
    v8::TryCatch tryCatch;
    
    StartPager();

    TRI_ExecuteStringVocBase(context, v8::String::New(input), name, true);
    TRI_FreeString(input);

    if (tryCatch.HasCaught()) {
      cout << TRI_StringifyV8Exception(&tryCatch);
    }

    StopPager();
  }

  console->close();
  
  printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adding colors for output
////////////////////////////////////////////////////////////////////////////////

static char DEF_RED[6]         = "\x1b[31m";
static char DEF_BOLD_RED[8]    = "\x1b[1;31m";
static char DEF_GREEN[6]       = "\x1b[32m";
static char DEF_BOLD_GREEN[8]  = "\x1b[1;32m";
static char DEF_BLUE[6]        = "\x1b[34m";
static char DEF_BOLD_BLUE[8]   = "\x1b[1;34m";
static char DEF_YELLOW[8]      = "\x1b[1;33m";
static char DEF_WHITE[6]       = "\x1b[37m";
static char DEF_BOLD_WHITE[8]  = "\x1b[1;37m";
static char DEF_BLACK[6]       = "\x1b[30m";
static char DEF_BOLD_BLACK[8]  = "\x1b[1;39m";
static char DEF_BLINK[5]       = "\x1b[5m";
static char DEF_BRIGHT[5]      = "\x1b[1m";
static char DEF_RESET[5]       = "\x1b[0m";

static void addColors (v8::Handle<v8::Context> context) {  
  context->Global()->Set(v8::String::New("COLOR_RED"), v8::String::New(DEF_RED, 5),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("COLOR_BOLD_RED"), v8::String::New(DEF_BOLD_RED, 8),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("COLOR_GREEN"), v8::String::New(DEF_GREEN, 5),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("COLOR_BOLD_GREEN"), v8::String::New(DEF_BOLD_GREEN, 8),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("COLOR_BLUE"), v8::String::New(DEF_BLUE, 5),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("COLOR_BOLD_BLUE"), v8::String::New(DEF_BOLD_BLUE, 8),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("COLOR_WHITE"), v8::String::New(DEF_WHITE, 5),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("COLOR_YELLOW"), v8::String::New(DEF_YELLOW, 5),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("COLOR_BOLD_WHITE"), v8::String::New(DEF_BOLD_WHITE, 7),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("COLOR_BLACK"), v8::String::New(DEF_BLACK, 5),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("COLOR_BOLD_BLACK"), v8::String::New(DEF_BOLD_BLACK, 8),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("COLOR_BLINK"), v8::String::New(DEF_BLINK, 4),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("COLOR_BRIGHT"), v8::String::New(DEF_BRIGHT, 4),
                         v8::ReadOnly);
  if (!noColors) {
    context->Global()->Set(v8::String::New("COLOR_OUTPUT"), v8::String::New(DEF_BRIGHT, 4));
  }
  context->Global()->Set(v8::String::New("COLOR_OUTPUT_RESET"), v8::String::New(DEF_RESET, 4),
                         v8::ReadOnly);    
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
  TRIAGENS_C_INITIALISE;
  TRI_InitialiseLogging(false);

  // .............................................................................
  // use relative system paths
  // .............................................................................

  {
    char* binaryPath = TRI_LocateBinaryPath(argv[0]);

#ifdef TRI_ENABLE_RELATIVE_SYSTEM
  
    StartupModules = string(binaryPath) + "/../share/avocado/js/client/modules"
             + ";" + string(binaryPath) + "/../share/avocado/js/common/modules";

#else

  // .............................................................................
  // use relative development paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_DEVEL

#ifdef TRI_STARTUP_MODULES_PATH
    StartupModules = TRI_STARTUP_MODULES_PATH;
#else
    StartupModules = string(binaryPath) + "/js/client/modules"
             + ";" + string(binaryPath) + "/js/common/modules";
#endif

#else

  // .............................................................................
  // use absolute paths
  // .............................................................................

#ifdef _PKGDATADIR_

    StartupModules = string(_PKGDATADIR_) + "/js/client/modules"
             + ";" + string(_PKGDATADIR_) + "/js/common/modules";

#endif

#endif
#endif

    TRI_FreeString(binaryPath);
  }

  // parse the program options
  ParseProgramOptions(argc, argv);

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

  // add function SYS_OUTPUT to use pager
  context->Global()->Set(v8::String::New("TRI_SYS_OUTPUT"),
                         v8::FunctionTemplate::New(JS_PagerOutput)->GetFunction(),
                         v8::ReadOnly);
  
  
  TRI_InitV8Utils(context, StartupModules);
  
  TRI_InitV8Shell(context);

  // processComandLineArguments(argc, argv);
  if (! splitServerAdress(ServerAddress, DEFAULT_SERVER_NAME, DEFAULT_SERVER_PORT)) {
    if (ServerAddress.length()) {
      printf("Could not split %s.\n", ServerAddress.c_str());                  
    }
  }
        
  
  clientConnection = new V8ClientConnection(
          DEFAULT_SERVER_NAME, 
          DEFAULT_SERVER_PORT, 
          DEFAULT_REQUEST_TIMEOUT, 
          DEFAULT_RETRIES, 
          DEFAULT_CONNECTION_TIMEOUT);
  
  // .............................................................................
  // define AvocadoConnection class
  // .............................................................................  
  v8::Handle<v8::FunctionTemplate> connection_templ = v8::FunctionTemplate::New();
  connection_templ->SetClassName(v8::String::New("AvocadoConnection"));
  v8::Handle<v8::ObjectTemplate> connection_proto = connection_templ->PrototypeTemplate();
  connection_proto->Set("get", v8::FunctionTemplate::New(ClientConnection_httpGet));
  connection_proto->Set("post", v8::FunctionTemplate::New(ClientConnection_httpPost));
  connection_proto->Set("delete", v8::FunctionTemplate::New(ClientConnection_httpDelete));
  connection_proto->Set("put", v8::FunctionTemplate::New(ClientConnection_httpPut));
  connection_proto->Set("lastHttpReturnCode", v8::FunctionTemplate::New(ClientConnection_lastHttpReturnCode));
  connection_proto->Set("lastErrorMessage", v8::FunctionTemplate::New(ClientConnection_lastErrorMessage));
  connection_proto->Set("isConnected", v8::FunctionTemplate::New(ClientConnection_isConnected));
  connection_proto->Set("toString", v8::FunctionTemplate::New(ClientConnection_toString));
  connection_proto->Set("getVersion", v8::FunctionTemplate::New(ClientConnection_getVersion));
  connection_proto->SetCallAsFunctionHandler(ClientConnection_ConstructorCallback);    
  v8::Handle<v8::ObjectTemplate> connection_inst = connection_templ->InstanceTemplate();
  connection_inst->SetInternalFieldCount(2);    
  context->Global()->Set(v8::String::New("AvocadoConnection"), connection_proto->NewInstance());    
  ConnectionTempl = v8::Persistent<v8::ObjectTemplate>::New(connection_inst);
    
  context->Global()->Set(v8::String::New("SYS_START_PAGER"),
                         v8::FunctionTemplate::New(JS_StartOutputPager)->GetFunction(),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("SYS_STOP_PAGER"),
                         v8::FunctionTemplate::New(JS_StopOutputPager)->GetFunction(),
                         v8::ReadOnly);
  

  
  // http://www.network-science.de/ascii/   Font: ogre
  if (noColors) {
    printf("                        "      "    _         \n");
    printf("   __ ___   _____   ___ "      "___| |__      \n");
    printf("  / _` \\ \\ / / _ \\ / __"    "/ __| '_ \\   \n");
    printf(" | (_| |\\ V / (_) | (__"      "\\__ \\ | | | \n");
    printf("  \\__,_| \\_/ \\___/ \\___"   "|___/_| |_|   \n\n");        
  }
  else {
    printf(        "                        "      "\x1b[31m    _         \x1b[0m\n");
    printf("\x1b[32m   __ ___   _____   ___ "      "\x1b[31m___| |__      \x1b[0m\n");
    printf("\x1b[32m  / _` \\ \\ / / _ \\ / __"    "\x1b[31m/ __| '_ \\   \x1b[0m\n");
    printf("\x1b[32m | (_| |\\ V / (_) | (__"      "\x1b[31m\\__ \\ | | | \x1b[0m\n");
    printf("\x1b[32m  \\__,_| \\_/ \\___/ \\___"   "\x1b[31m|___/_| |_|   \x1b[0m\n\n");    
  }
  printf("Welcome to avocsh %s. Copyright (c) 2012 triAGENS GmbH.\n", TRIAGENS_VERSION);

#ifdef TRI_V8_VERSION
  printf("Using Google V8 " TRI_V8_VERSION " JavaScript engine.\n");
#else
  printf("Using Google V8 JavaScript engine.\n\n");
#endif
  
#ifdef TRI_READLINE_VERSION
  printf("Using READLINE " TRI_READLINE_VERSION ".\n\n");
#endif

  if (clientConnection->isConnected()) {
    printf("Connected to Avocado DB %s:%d Version %s\n", 
            clientConnection->getHostname().c_str(), 
            clientConnection->getPort(), 
            clientConnection->getVersion().c_str());
    
    if (usePager) {
      printf("Using pager '%s' for output buffering.\n", OutputPager.c_str());    
    }
 
    // add the client connection to the context:
    context->Global()->Set(v8::String::New("avocado"), 
                         wrapV8ClientConnection(clientConnection),
                         v8::ReadOnly);
    
    if (prettyPrint) {
      printf("Pretty print values.\n");    
    }

    // set pretty print default: (used in print.js)
    context->Global()->Set(v8::String::New("PRETTY_PRINT"), v8::Boolean::New(prettyPrint));

    // add colors for print.js
    addColors(context);
    
    // load java script from js/bootstrap/*.h files
    if (StartupPath.empty()) {
      StartupLoader.defineScript("common/bootstrap/modules.js", JS_common_bootstrap_modules);
      StartupLoader.defineScript("common/bootstrap/print.js", JS_common_bootstrap_print);
      StartupLoader.defineScript("common/bootstrap/errors.js", JS_common_bootstrap_errors);
      StartupLoader.defineScript("client/client.js", JS_client_client);
    }
    else {
      LOGGER_DEBUG << "using JavaScript startup files at '" << StartupPath << "'";
      StartupLoader.setDirectory(StartupPath);
    }

    // load all init files
    char const* files[] = {
      "common/bootstrap/modules.js",
      "common/bootstrap/print.js",
      "common/bootstrap/errors.js",
      "client/client.js"
    };

    for (size_t i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
      bool ok = StartupLoader.loadScript(context, files[i]);
      
      if (ok) {
        LOGGER_TRACE << "loaded json file '" << files[i] << "'";
      }
      else {
        LOGGER_ERROR << "cannot load json file '" << files[i] << "'";
        exit(EXIT_FAILURE);
      }
    }
    
    RunShell(context);
  }
  else {
    printf("Could not connect to server %s:%d\n", DEFAULT_SERVER_NAME.c_str(), DEFAULT_SERVER_PORT);
    printf("Error message '%s'\n", clientConnection->getErrorMessage().c_str());
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
