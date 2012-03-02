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

using namespace v8;

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::v8client;
using namespace triagens::avocado;

#include "js/bootstrap/js-print.h"
#include "js/bootstrap/js-modules.h"
#include "js/client/js-client.h"

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
/// @brief startup JavaScript files
////////////////////////////////////////////////////////////////////////////////

static JSLoader StartupLoader;

////////////////////////////////////////////////////////////////////////////////
/// @brief server address
////////////////////////////////////////////////////////////////////////////////

static string ServerAddress = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection* clientConnection = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief object template for the initial connection
////////////////////////////////////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> ConnectionTempl;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

bool splitServerAdress (string const& definition, string& address, int& port) {
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

  description
    ("help,h", "help message")
    ("log.level,l", &level,  "log level")
    ("server", &ServerAddress, "server address and port")
    ("startup", &StartupPath, "startup path containing the JavaScript files")
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

static Handle<Value> ClientConnection_ConstructorCallback(v8::Arguments const& argv) {
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

static Handle<Value> ClientConnection_httpGet(v8::Arguments const& argv) {
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

static Handle<Value> ClientConnection_httpDelete(v8::Arguments const& argv) {
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

static Handle<Value> ClientConnection_httpPost(v8::Arguments const& argv) {
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

static Handle<Value> ClientConnection_httpPut(v8::Arguments const& argv) {
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

static Handle<Value> ClientConnection_lastHttpReturnCode(v8::Arguments const& argv) {
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

static Handle<Value> ClientConnection_lastErrorMessage(v8::Arguments const& argv) {
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

static Handle<Value> ClientConnection_isConnected(v8::Arguments const& argv) {
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

  console->open();

  while (true) {
    while (!V8::IdleNotification()) {
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
    
    console->addHistory(input);
    
    HandleScope scope;

    TRI_ExecuteStringVocBase(context, String::New(input), name, true, true);
    TRI_FreeString(input);
  }

  console->close();
  
  printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adding colors for output
////////////////////////////////////////////////////////////////////////////////

static char DEF_RED[6]         = { 0x1B, '[',           '3', '1', 'm', 0 };
static char DEF_BOLD_RED[8]    = { 0x1B, '[', '1', ';', '3', '1', 'm', 0 };
static char DEF_GREEN[6]       = { 0x1B, '[',           '3', '2', 'm', 0 };
static char DEF_BOLD_GREEN[8]  = { 0x1B, '[', '1', ';', '3', '2', 'm', 0 };
static char DEF_BLUE[6]        = { 0x1B, '[',           '3', '4', 'm', 0 };
static char DEF_BOLD_BLUE[8]   = { 0x1B, '[', '1', ';', '3', '4', 'm', 0 };
static char DEF_YELLOW[8]      = { 0x1B, '[', '1', ';', '3', '3', 'm', 0 };
static char DEF_WHITE[6]       = { 0x1B, '[',           '3', '7', 'm', 0 };
static char DEF_BOLD_WHITE[8]  = { 0x1B, '[', '1', ';', '3', '7', 'm', 0 };
static char DEF_BLACK[6]       = { 0x1B, '[',           '3', '0', 'm', 0 };
static char DEF_BOLD_BLACK[8]  = { 0x1B, '[', '1', ';', '3', '0', 'm', 0 };  
static char DEF_BLINK[5]       = { 0x1B, '[',                '5', 'm', 0 };
static char DEF_BRIGHT[5]      = { 0x1B, '[',                '1', 'm', 0 };
static char DEF_RESET[5]       = { 0x1B, '[',                '0', 'm', 0 };

static void addColors (v8::Handle<v8::Context> context) {  
  context->Global()->Set(v8::String::New("COLOR_RED"), v8::String::New(DEF_RED, 5));
  context->Global()->Set(v8::String::New("COLOR_BOLD_RED"), v8::String::New(DEF_BOLD_RED, 8));
  context->Global()->Set(v8::String::New("COLOR_GREEN"), v8::String::New(DEF_GREEN, 5));
  context->Global()->Set(v8::String::New("COLOR_BOLD_GREEN"), v8::String::New(DEF_BOLD_GREEN, 8));
  context->Global()->Set(v8::String::New("COLOR_BLUE"), v8::String::New(DEF_BLUE, 5));
  context->Global()->Set(v8::String::New("COLOR_BOLD_BLUE"), v8::String::New(DEF_BOLD_BLUE, 8));
  context->Global()->Set(v8::String::New("COLOR_WHITE"), v8::String::New(DEF_WHITE, 5));
  context->Global()->Set(v8::String::New("COLOR_YELLOW"), v8::String::New(DEF_YELLOW, 5));
  context->Global()->Set(v8::String::New("COLOR_BOLD_WHITE"), v8::String::New(DEF_BOLD_WHITE, 7));
  context->Global()->Set(v8::String::New("COLOR_BLACK"), v8::String::New(DEF_BLACK, 5));
  context->Global()->Set(v8::String::New("COLOR_BOLD_BLACK"), v8::String::New(DEF_BOLD_BLACK, 8));
  context->Global()->Set(v8::String::New("COLOR_BLINK"), v8::String::New(DEF_BLINK, 4));
  context->Global()->Set(v8::String::New("COLOR_BRIGHT"), v8::String::New(DEF_BRIGHT, 4));
  context->Global()->Set(v8::String::New("COLOR_OUTPUT"), v8::String::New(DEF_BRIGHT, 4));
  context->Global()->Set(v8::String::New("COLOR_OUTPUT_RESET"), v8::String::New(DEF_RESET, 4));    
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

  // parse the program options
  ParseProgramOptions(argc, argv);

  RunShellFlag = (argc == 1);

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

  TRI_InitV8Utils(context, ".");
  TRI_InitV8Shell(context);

  // processComandLineArguments(argc, argv);
  if (! splitServerAdress(ServerAddress, DEFAULT_SERVER_NAME, DEFAULT_SERVER_PORT)) {
    printf("Could not spilt %s.\n", ServerAddress.c_str());                  
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
  v8::Handle<v8::FunctionTemplate> connection_templ = FunctionTemplate::New();
  connection_templ->SetClassName(String::New("AvocadoConnection"));
  v8::Handle<v8::ObjectTemplate> connection_proto = connection_templ->PrototypeTemplate();
  connection_proto->Set("get", FunctionTemplate::New(ClientConnection_httpGet));
  connection_proto->Set("post", FunctionTemplate::New(ClientConnection_httpPost));
  connection_proto->Set("delete", FunctionTemplate::New(ClientConnection_httpDelete));
  connection_proto->Set("put", FunctionTemplate::New(ClientConnection_httpPut));
  connection_proto->Set("lastHttpReturnCode", FunctionTemplate::New(ClientConnection_lastHttpReturnCode));
  connection_proto->Set("lastErrorMessage", FunctionTemplate::New(ClientConnection_lastErrorMessage));
  connection_proto->Set("isConnected", FunctionTemplate::New(ClientConnection_isConnected));
  connection_proto->SetCallAsFunctionHandler(ClientConnection_ConstructorCallback);    
  Handle<ObjectTemplate> connection_inst = connection_templ->InstanceTemplate();
  connection_inst->SetInternalFieldCount(2);    
  context->Global()->Set(v8::String::New("AvocadoConnection"), connection_proto->NewInstance());    
  ConnectionTempl = v8::Persistent<v8::ObjectTemplate>::New(connection_inst);
    
  // http://www.network-science.de/ascii/   Font: ogre
  printf("%c[%dm                           _     %c[%dm\n", 0x1B, 31, 0x1B, 0);
  printf("%c[%dm  __ ___   _____   ___ %c[%dm___| |__  %c[%dm\n", 0x1B, 32, 0x1B, 31, 0x1B, 0);
  printf("%c[%dm / _` \\ \\ / / _ \\ / __%c[%dm/ __| '_ \\ %c[%dm\n", 0x1B, 32, 0x1B, 31, 0x1B, 0);
  printf("%c[%dm| (_| |\\ V / (_) | (__%c[%dm\\__ \\ | | |%c[%dm\n", 0x1B, 32, 0x1B, 31, 0x1B, 0);
  printf("%c[%dm \\__,_| \\_/ \\___/ \\___|%c[%dm___/_| |_|%c[%dm\n\n", 0x1B, 32, 0x1B, 31, 0x1B, 0);
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
 
    // add the client connection to the context:
    context->Global()->Set(v8::String::New("avocado"), wrapV8ClientConnection(clientConnection));

    addColors(context);
    
    // load java script from js/bootstrap/*.h files
    if (StartupPath.empty()) {
      StartupLoader.defineScript("bootstrap/modules.js", JS_bootstrap_modules);
      StartupLoader.defineScript("bootstrap/print.js", JS_bootstrap_print);
      StartupLoader.defineScript("client/client.js", JS_client_client);
    }
    else {
      LOGGER_DEBUG << "using JavaScript startup files at '" << StartupPath << "'";
      StartupLoader.setDirectory(StartupPath);
    }

    // load all init files
    char const* files[] = {
      "bootstrap/modules.js",
      "bootstrap/print.js",
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
