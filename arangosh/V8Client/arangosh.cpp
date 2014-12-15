////////////////////////////////////////////////////////////////////////////////
/// @brief V8 shell
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include <v8.h>
#include <libplatform/libplatform.h>

#include "ArangoShell/ArangoClient.h"
#include "Basics/messages.h"
#include "Basics/FileUtils.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/csv.h"
#include "Basics/files.h"
#include "Basics/init.h"
#include "Basics/shell-colors.h"
#include "Basics/terminal-utils.h"
#include "Basics/tri-strings.h"
#include "Rest/Endpoint.h"
#include "Rest/InitialiseRest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "V8/JSLoader.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8Client/ImportHelper.h"
#include "V8Client/V8ClientConnection.h"

#include "3rdParty/valgrind/valgrind.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::httpclient;
using namespace triagens::v8client;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief command prompt
////////////////////////////////////////////////////////////////////////////////

static string Prompt = "arangosh [%d]> ";

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient("arangosh");

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection* ClientConnection = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief Windows console codepage
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
static int CodePage = -1;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief object template for the initial connection
////////////////////////////////////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> ConnectionTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief max size body size (used for imports)
////////////////////////////////////////////////////////////////////////////////

static uint64_t ChunkSize = 1024 * 1024 * 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief startup JavaScript files
////////////////////////////////////////////////////////////////////////////////

static JSLoader StartupLoader;

////////////////////////////////////////////////////////////////////////////////
/// @brief path for JavaScript modules files
////////////////////////////////////////////////////////////////////////////////

static string StartupModules = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief path for JavaScript files
////////////////////////////////////////////////////////////////////////////////

static string StartupPath = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief put current directory into module path
////////////////////////////////////////////////////////////////////////////////

static bool UseCurrentModulePath = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief javascript files to execute
////////////////////////////////////////////////////////////////////////////////

static vector<string> ExecuteScripts;

////////////////////////////////////////////////////////////////////////////////
/// @brief javascript string to execute
////////////////////////////////////////////////////////////////////////////////

static string ExecuteString;

////////////////////////////////////////////////////////////////////////////////
/// @brief javascript files to syntax check
////////////////////////////////////////////////////////////////////////////////

static vector<string> CheckScripts;

////////////////////////////////////////////////////////////////////////////////
/// @brief unit file test cases
////////////////////////////////////////////////////////////////////////////////

static vector<string> UnitTests;

////////////////////////////////////////////////////////////////////////////////
/// @brief files to jslint
////////////////////////////////////////////////////////////////////////////////

static vector<string> JsLint;

////////////////////////////////////////////////////////////////////////////////
/// @brief garbage collection interval
////////////////////////////////////////////////////////////////////////////////

static uint64_t GcInterval = 10;

////////////////////////////////////////////////////////////////////////////////
/// @brief console object
////////////////////////////////////////////////////////////////////////////////

static V8LineEditor* Console = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief voice mode
////////////////////////////////////////////////////////////////////////////////

static bool VoiceMode = false;

// -----------------------------------------------------------------------------
// --SECTION--                                              JavaScript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs the arguments
///
/// @FUN{internal.output(@FA{string1}, @FA{string2}, @FA{string3}, ...)}
///
/// Outputs the arguments to standard output.
///
/// @verbinclude fluent39
////////////////////////////////////////////////////////////////////////////////

static void JS_PagerOutput (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  for (int i = 0; i < args.Length(); i++) {

    // extract the next argument
    v8::Handle<v8::Value> val = args[i];

    string str = TRI_ObjectToString(val);

    BaseClient.internalPrint(str);
  }

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the output pager
////////////////////////////////////////////////////////////////////////////////

static void JS_StartOutputPager (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (BaseClient.usePager()) {
    BaseClient.internalPrint("Using pager already.\n");
  }
  else {
    BaseClient.setUsePager(true);
    BaseClient.internalPrint(string(string("Using pager ") + BaseClient.outputPager() + " for output buffering.\n"));
  }

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the output pager
////////////////////////////////////////////////////////////////////////////////

static void JS_StopOutputPager (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (BaseClient.usePager()) {
    BaseClient.internalPrint("Stopping pager.\n");
  }
  else {
    BaseClient.internalPrint("Pager not running.\n");
  }

  BaseClient.setUsePager(false);

  TRI_V8_RETURN_UNDEFINED();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   import function
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a CSV file
///
/// @FUN{importCsvFile(@FA{filename}, @FA{collection})}
///
/// Imports data of a CSV file. The data is imported to @FA{collection}.
////The seperator is @CODE{\,} and the quote is @CODE{"}.
///
/// @FUN{importCsvFile(@FA{filename}, @FA{collection}, @FA{options})}
///
/// Imports data of a CSV file. The data is imported to @FA{collection}.
////The seperator is @CODE{\,} and the quote is @CODE{"}.
////////////////////////////////////////////////////////////////////////////////

static void JS_ImportCsvFile (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("importCsvFile(<filename>, <collection>[, <options>])");
  }

  // extract the filename
  v8::String::Utf8Value filename(args[0]);

  if (*filename == 0) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be an UTF-8 filename");
  }

  v8::String::Utf8Value collection(args[1]);

  if (*collection == 0) {
    TRI_V8_THROW_TYPE_ERROR("<collection> must be an UTF-8 filename");
  }

  // extract the options
  v8::Handle<v8::String> separatorKey = TRI_V8_ASCII_STRING("separator");
  v8::Handle<v8::String> quoteKey = TRI_V8_ASCII_STRING("quote");

  string separator = ",";
  string quote = "\"";

  if (3 <= args.Length()) {
    v8::Handle<v8::Object> options = args[2]->ToObject();
    // separator
    if (options->Has(separatorKey)) {
      separator = TRI_ObjectToString(options->Get(separatorKey));

      if (separator.length() < 1) {
        TRI_V8_THROW_EXCEPTION_PARAMETER("<options>.separator must be at least one character");
      }
    }

    // quote
    if (options->Has(quoteKey)) {
      quote = TRI_ObjectToString(options->Get(quoteKey));

      if (quote.length() > 1) {
        TRI_V8_THROW_EXCEPTION_PARAMETER("<options>.quote must be at most one character");
      }
    }
  }

  ImportHelper ih(ClientConnection->getHttpClient(), ChunkSize);

  ih.setQuote(quote);
  ih.setSeparator(separator.c_str());

  string fileName = TRI_ObjectToString(args[0]);
  string collectionName = TRI_ObjectToString(args[1]);

  if (ih.importDelimited(collectionName, fileName, ImportHelper::CSV)) {
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    result->Set(TRI_V8_ASCII_STRING("lines"),   v8::Integer::New(isolate, (int32_t) ih.getReadLines()));
    result->Set(TRI_V8_ASCII_STRING("created"), v8::Integer::New(isolate, (int32_t) ih.getImportedLines()));
    result->Set(TRI_V8_ASCII_STRING("errors"),  v8::Integer::New(isolate, (int32_t) ih.getErrorLines()));
    TRI_V8_RETURN(result);
  }

  TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, ih.getErrorMessage().c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a JSON file
///
/// @FUN{importJsonFile(@FA{filename}, @FA{collection})}
///
/// Imports data of a CSV file. The data is imported to @FA{collection}.
///
////////////////////////////////////////////////////////////////////////////////

static void JS_ImportJsonFile (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("importJsonFile(<filename>, <collection>)");
  }

  // extract the filename
  v8::String::Utf8Value filename(args[0]);

  if (*filename == 0) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be an UTF-8 filename");
  }

  v8::String::Utf8Value collection(args[1]);

  if (*collection == 0) {
    TRI_V8_THROW_TYPE_ERROR("<collection> must be an UTF8 filename");
  }


  ImportHelper ih(ClientConnection->getHttpClient(), ChunkSize);

  string fileName = TRI_ObjectToString(args[0]);
  string collectionName = TRI_ObjectToString(args[1]);

  if (ih.importJson(collectionName, fileName)) {
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    result->Set(TRI_V8_ASCII_STRING("lines"),   v8::Integer::New(isolate, (int32_t) ih.getReadLines()));
    result->Set(TRI_V8_ASCII_STRING("created"), v8::Integer::New(isolate, (int32_t) ih.getImportedLines()));
    result->Set(TRI_V8_ASCII_STRING("errors"),  v8::Integer::New(isolate, (int32_t) ih.getErrorLines()));
    TRI_V8_RETURN(result);
  }

  TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, ih.getErrorMessage().c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_normalize_string (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("NORMALIZE_STRING(<string>)");
  }

  TRI_normalize_V8_Obj(args, args[0]);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_compare_string (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("COMPARE_STRING(<left string>, <right string>)");
  }

  v8::String::Value left(args[0]);
  v8::String::Value right(args[1]);

  int result = Utf8Helper::DefaultUtf8Helper.compareUtf16(*left, left.length(), *right, right.length());

  TRI_V8_RETURN(v8::Integer::New(isolate, result));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     private enums
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief enum for wrapped V8 objects
////////////////////////////////////////////////////////////////////////////////

enum WRAP_CLASS_TYPES {WRAP_TYPE_CONNECTION = 1};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////
typedef enum __eRunMode {
  eInteractive,
  eExecuteScript,
  eExecuteString,
  eCheckScripts,
  eUnitTests,
  eJsLint
} eRunMode;

static vector<string> ParseProgramOptions (int argc, char* args[], eRunMode *runMode) {
  ProgramOptionsDescription description("STANDARD options");
  ProgramOptionsDescription javascript("JAVASCRIPT options");

  javascript
    ("javascript.execute", &ExecuteScripts, "execute Javascript code from file")
    ("javascript.execute-string", &ExecuteString, "execute Javascript code from string")
    ("javascript.check", &CheckScripts, "syntax check code Javascript code from file")
    ("javascript.gc-interval", &GcInterval, "JavaScript request-based garbage collection interval (each x commands)")
    ("javascript.startup-directory", &StartupPath, "startup paths containing the JavaScript files")
    ("javascript.unit-tests", &UnitTests, "do not start as shell, run unit tests instead")
    ("javascript.current-module-directory", &UseCurrentModulePath, "add current directory to module path")
    ("jslint", &JsLint, "do not start as shell, run jslint instead")
  ;

#ifdef _WIN32
  description
    ("code-page", &CodePage, "windows codepage")
  ;
#endif

#ifdef __APPLE__
  description
    ("voice", "enable voice based welcome")
  ;
#endif

  description
    ("chunk-size", &ChunkSize, "maximum size for individual data batches (in bytes)")
    ("prompt", &Prompt, "command prompt")
    (javascript, false)
  ;

  vector<string> arguments;
  *runMode = eInteractive;

  description.arguments(&arguments);

  // fill in used options
  BaseClient.setupGeneral(description);
  BaseClient.setupColors(description);
  BaseClient.setupAutoComplete(description);
  BaseClient.setupPrettyPrint(description);
  BaseClient.setupPager(description);
  BaseClient.setupLog(description);
  BaseClient.setupServer(description);

  // and parse the command line and config file
  ProgramOptions options;

  char* p = TRI_BinaryName(args[0]);
  string conf = p;
  TRI_FreeString(TRI_CORE_MEM_ZONE, p);
  conf += ".conf";

  BaseClient.parse(options, description, "<options>", argc, args, conf);

  // set V8 options
  v8::V8::SetFlagsFromCommandLine(&argc, args, true);

  // derive other paths from `--javascript.directory`
  StartupModules = StartupPath + TRI_DIR_SEPARATOR_STR + "client" + TRI_DIR_SEPARATOR_STR + "modules;" +
                   StartupPath + TRI_DIR_SEPARATOR_STR + "common" + TRI_DIR_SEPARATOR_STR + "modules;" +
                   StartupPath + TRI_DIR_SEPARATOR_STR + "node";

  if (UseCurrentModulePath) {
    StartupModules += ";" + FileUtils::currentDirectory();
  }

  // turn on paging automatically if "pager" option is set
  if (options.has("pager") && ! options.has("use-pager")) {
    BaseClient.setUsePager(true);
  }

  // disable excessive output in non-interactive mode
  if (! ExecuteScripts.empty() || ! ExecuteString.empty() || ! CheckScripts.empty() || ! UnitTests.empty() || ! JsLint.empty()) {
    BaseClient.shutup();
  }

  // voice mode
  if (options.has("voice")) {
    VoiceMode = true;
  }

  if (! ExecuteScripts.empty()) {
    *runMode = eExecuteScript;
  }
  else if (! ExecuteString.empty()) {
    *runMode = eExecuteString;
  }
  else if (! CheckScripts.empty()) {
    *runMode = eCheckScripts;
  }
  else if (! UnitTests.empty()) {
    *runMode = eUnitTests;
  }
  else if (! JsLint.empty()) {
    *runMode = eJsLint;
  }

  // return the positional arguments
  return arguments;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies v8::Object to std::map<string, string>
////////////////////////////////////////////////////////////////////////////////

static void objectToMap (v8::Isolate* isolate, map<string, string>& myMap, v8::Handle<v8::Value> val) {
  v8::Handle<v8::Object> v8Headers = val.As<v8::Object>();

  if (v8Headers->IsObject()) {
    v8::Handle<v8::Array> const props = v8Headers->GetPropertyNames();

    for (uint32_t i = 0; i < props->Length(); i++) {
      v8::Handle<v8::Value> key = props->Get(v8::Integer::New(isolate, i));
      myMap.emplace(TRI_ObjectToString(key), TRI_ObjectToString(v8Headers->Get(key)));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a new client connection instance
////////////////////////////////////////////////////////////////////////////////
std::map< void*, v8::Persistent<v8::External> > Connections;

static V8ClientConnection* CreateConnection () {
  return new V8ClientConnection(BaseClient.endpointServer(),
                                BaseClient.databaseName(),
                                BaseClient.username(),
                                BaseClient.password(),
                                BaseClient.requestTimeout(),
                                BaseClient.connectTimeout(),
                                ArangoClient::DEFAULT_RETRIES,
                                BaseClient.sslProtocol(),
                                false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries (call the destructor here)
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_DestructorCallback (const v8::WeakCallbackData<v8::External, v8::Persistent<v8::External>>& data) {
  auto persistent = data.GetParameter();
  auto myConnection = v8::Local<v8::External>::New(data.GetIsolate(), *persistent);
  auto connection = static_cast<V8ClientConnection*>(myConnection->Value());

  Connections[connection].Reset();
  if (ClientConnection != connection) {
    delete connection;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wrap V8ClientConnection in a v8::Object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> wrapV8ClientConnection (v8::Isolate* isolate, V8ClientConnection* connection) {
  v8::EscapableHandleScope scope(isolate);
  auto localConnectionTempl = v8::Local<v8::ObjectTemplate>::New(isolate, ConnectionTempl);
  v8::Handle<v8::Object> result = localConnectionTempl->NewInstance();

  auto myConnection = v8::External::New(isolate, connection);
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(isolate, WRAP_TYPE_CONNECTION));
  result->SetInternalField(SLOT_CLASS, myConnection);
  Connections[connection].Reset(isolate, myConnection);
  Connections[connection].SetWeak(&Connections[connection], ClientConnection_DestructorCallback);
  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection constructor
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_ConstructorCallback (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  if (args.Length() > 0 && args[0]->IsString()) {
    string definition = TRI_ObjectToString(args[0]);

    BaseClient.createEndpoint(definition);

    if (BaseClient.endpointServer() == nullptr) {
      string errorMessage = "error in '" + definition + "'";
      TRI_V8_THROW_EXCEPTION_PARAMETER(errorMessage.c_str());
    }
  }

  if (BaseClient.endpointServer() == nullptr) {
    TRI_V8_RETURN_UNDEFINED();
  }

  V8ClientConnection* connection = CreateConnection();

  if (connection->isConnected() && connection->getLastHttpReturnCode() == HttpResponse::OK) {
    ostringstream s;
    s << "Connected to ArangoDB '" << BaseClient.endpointServer()->getSpecification()
      << "', version " << connection->getVersion() << ", database '" << BaseClient.databaseName()
      << "', username: '" << BaseClient.username() << "'";
    BaseClient.printLine(s.str());
  }
  else {
    string errorMessage = "Could not connect. Error message: " + connection->getErrorMessage();
    delete connection;
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT, errorMessage.c_str());
  }

  TRI_V8_RETURN(wrapV8ClientConnection(isolate, connection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "reconnect"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_reconnect (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("reconnect(<endpoint>, <database>, [, <username>, <password>])");
  }

  string const definition = TRI_ObjectToString(args[0]);
  string databaseName = TRI_ObjectToString(args[1]);

  string username;
  if (args.Length() < 3) {
    username = BaseClient.username();
  }
  else {
    username = TRI_ObjectToString(args[2]);
  }

  string password;
  if (args.Length() < 4) {
    BaseClient.printContinuous("Please specify a password: ");

    // now prompt for it
#ifdef TRI_HAVE_TERMIOS_H
    TRI_SetStdinVisibility(false);
    getline(cin, password);

    TRI_SetStdinVisibility(true);
#else
    getline(cin, password);
#endif
    BaseClient.printLine("");
  }
  else {
    password = TRI_ObjectToString(args[3]);
  }

  string const oldDefinition   = BaseClient.endpointString();
  string const oldDatabaseName = BaseClient.databaseName();
  string const oldUsername     = BaseClient.username();
  string const oldPassword     = BaseClient.password();

  delete connection;
  ClientConnection = nullptr;

  BaseClient.setEndpointString(definition);
  BaseClient.setDatabaseName(databaseName);
  BaseClient.setUsername(username);
  BaseClient.setPassword(password);

  // re-connect using new options
  BaseClient.createEndpoint();

  if (BaseClient.endpointServer() == nullptr) {
    BaseClient.setEndpointString(oldDefinition);
    BaseClient.setDatabaseName(oldDatabaseName);
    BaseClient.setUsername(oldUsername);
    BaseClient.setPassword(oldPassword);
    BaseClient.createEndpoint();

    string errorMessage = "error in '" + definition + "'";
    TRI_V8_THROW_EXCEPTION_PARAMETER(errorMessage.c_str());
  }

  V8ClientConnection* newConnection = CreateConnection();

  if (newConnection->isConnected() && newConnection->getLastHttpReturnCode() == HttpResponse::OK) {
    ostringstream s;
    s << "Connected to ArangoDB '" << BaseClient.endpointServer()->getSpecification()
      << "' version: " << newConnection->getVersion() << ", database: '" << BaseClient.databaseName()
      << "', username: '" << BaseClient.username() << "'";

    BaseClient.printLine(s.str());

    args.Holder()->SetInternalField(SLOT_CLASS, v8::External::New(isolate, newConnection));

    v8::Handle<v8::Value> db = isolate->GetCurrentContext()->Global()->Get(TRI_V8_ASCII_STRING("db"));
    if (db->IsObject()) {
      v8::Handle<v8::Object> dbObj = v8::Handle<v8::Object>::Cast(db);

      if (dbObj->Has(TRI_V8_ASCII_STRING("_flushCache")) && dbObj->Get(TRI_V8_ASCII_STRING("_flushCache"))->IsFunction()) {
        v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(dbObj->Get(TRI_V8_ASCII_STRING("_flushCache")));

        v8::Handle<v8::Value>* args = nullptr;
        func->Call(dbObj, 0, args);
      }
    }

    ClientConnection = newConnection;

    // ok
    TRI_V8_RETURN_TRUE();
  }
  else {
    ostringstream s;
    s << "Could not connect to endpoint '" << BaseClient.endpointString() <<
         "', username: '" << BaseClient.username() << "'";
    BaseClient.printErrLine(s.str());

    string errorMsg = "could not connect";
    if (newConnection->getErrorMessage() != "") {
      errorMsg = newConnection->getErrorMessage();
    }

    delete newConnection;

    // rollback
    BaseClient.setEndpointString(oldDefinition);
    BaseClient.setDatabaseName(oldDatabaseName);
    BaseClient.setUsername(oldUsername);
    BaseClient.setPassword(oldPassword);
    BaseClient.createEndpoint();

    ClientConnection = CreateConnection();
    args.Holder()->SetInternalField(SLOT_CLASS, v8::External::New(isolate, ClientConnection));

    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT, errorMsg.c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGetAny (const v8::FunctionCallbackInfo<v8::Value>& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || ! args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  // check header fields
  map<string, string> headerFields;

  if (args.Length() > 1) {
    objectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(connection->getData(isolate, *url, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGet (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpGetAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGetRaw (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpGetAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHeadAny (const v8::FunctionCallbackInfo<v8::Value>& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || ! args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("head(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);

  // check header fields
  map<string, string> headerFields;

  if (args.Length() > 1) {
    objectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(connection->headData(isolate, *url, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHead (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpHeadAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHeadRaw (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpHeadAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDeleteAny (const v8::FunctionCallbackInfo<v8::Value>& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || ! args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("delete(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);

  // check header fields
  map<string, string> headerFields;
  if (args.Length() > 1) {
    objectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(connection->deleteData(isolate, *url, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDelete (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpDeleteAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDeleteRaw (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpDeleteAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptionsAny (const v8::FunctionCallbackInfo<v8::Value>& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || ! args[0]->IsString() || ! args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("options(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  v8::String::Utf8Value body(args[1]);

  // check header fields
  map<string, string> headerFields;
  if (args.Length() > 2) {
    objectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(connection->optionsData(isolate, *url, *body, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptions (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpOptionsAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptionsRaw (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpOptionsAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPostAny (const v8::FunctionCallbackInfo<v8::Value>& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || ! args[0]->IsString() || ! args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("post(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  v8::String::Utf8Value body(args[1]);

  // check header fields
  map<string, string> headerFields;
  if (args.Length() > 2) {
    objectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(connection->postData(isolate, *url, *body, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPost (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpPostAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPostRaw (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpPostAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPutAny (const v8::FunctionCallbackInfo<v8::Value>& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || ! args[0]->IsString() || ! args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("put(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  v8::String::Utf8Value body(args[1]);

  // check header fields
  map<string, string> headerFields;
  if (args.Length() > 2) {
    objectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(connection->putData(isolate, *url, *body, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPut (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpPutAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPutRaw (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpPutAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatchAny (const v8::FunctionCallbackInfo<v8::Value>& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || ! args[0]->IsString() || ! args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("patch(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  v8::String::Utf8Value body(args[1]);

  // check header fields
  map<string, string> headerFields;
  if (args.Length() > 2) {
    objectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(connection->patchData(isolate, *url, *body, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatch (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpPatchAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatchRaw (const v8::FunctionCallbackInfo<v8::Value>& args) {
  ClientConnection_httpPatchAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection send file helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpSendFile (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 2 || ! args[0]->IsString() || ! args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("sendFile(<url>, <file>)");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);

  const string infile = TRI_ObjectToString(args[1]);

  if (! TRI_ExistsFile(infile.c_str())) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FILE_NOT_FOUND);
  }

  size_t bodySize;
  char* body = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, infile.c_str(), &bodySize);

  if (body == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), "could not read file");
  }

  v8::TryCatch tryCatch;

  // check header fields
  map<string, string> headerFields;

  v8::Handle<v8::Value> result = connection->postData(isolate, *url, body, bodySize, headerFields);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, body);

  if (tryCatch.HasCaught()) {
      string exception = TRI_StringifyV8Exception(isolate, &tryCatch);
      isolate->ThrowException(tryCatch.Exception());
    return;
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getEndpoint"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getEndpoint (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getEndpoint()");
  }

  const string endpoint = BaseClient.endpointString();
  TRI_V8_RETURN_STD_STRING(endpoint);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "lastError"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_lastHttpReturnCode (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("lastHttpReturnCode()");
  }

  TRI_V8_RETURN(v8::Integer::New(isolate, connection->getLastHttpReturnCode()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "lastErrorMessage"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_lastErrorMessage (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("lastErrorMessage()");
  }

  TRI_V8_RETURN_STD_STRING(connection->getErrorMessage());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "isConnected"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_isConnected (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("isConnected()");
  }

  if (connection->isConnected()) {
    TRI_V8_RETURN_TRUE();
  }
  else {
    TRI_V8_RETURN_FALSE();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "isConnected"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_toString (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("toString()");
  }

  string result = "[object ArangoConnection:" + BaseClient.endpointServer()->getSpecification();

  if (connection->isConnected()) {
    result += ","
            + connection->getVersion()
            + ",connected]";
  }
  else {
    result += ",unconnected]";
  }

  TRI_V8_RETURN_STD_STRING(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getVersion"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getVersion (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getVersion()");
  }

  TRI_V8_RETURN_STD_STRING(connection->getVersion());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getDatabaseName"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getDatabaseName (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getDatabaseName()");
  }

  TRI_V8_RETURN_STD_STRING(connection->getDatabaseName());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "setDatabaseName"
////////////////////////////////////////////////////////////////////////////////
static void ClientConnection_setDatabaseName (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);


  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 1 || ! args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("setDatabaseName(<name>)");
  }

  string const dbName = TRI_ObjectToString(args[0]);
  connection->setDatabaseName(dbName);
  BaseClient.setDatabaseName(dbName);

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dynamically replace %d, %e, %u in the prompt
////////////////////////////////////////////////////////////////////////////////

static std::string BuildPrompt () {
  string result;

  char const* p = Prompt.c_str();
  bool esc = false;

  while (true) {
    const char c = *p;

    if (c == '\0') {
      break;
    }

    if (esc) {
      if (c == '%') {
        result.push_back(c);
      }
      else if (c == 'd') {
        result.append(BaseClient.databaseName());
      }
      else if (c == 'e') {
        result.append(BaseClient.endpointString());
      }
      else if (c == 'u') {
        result.append(BaseClient.username());
      }

      esc = false;
    }
    else {
      if (c == '%') {
        esc = true;
      }
      else {
        result.push_back(c);
      }
    }

    ++p;
  }


  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief signal handler for CTRL-C
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
  // TODO

#else

static void SignalHandler (int signal) {
  if (Console != nullptr) {
    Console->close();
    Console = nullptr;
  }
  printf("\n");

  TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the shell
////////////////////////////////////////////////////////////////////////////////

static void RunShell (v8::Isolate* isolate, v8::Handle<v8::Context> context, bool promptError) {
  v8::Context::Scope contextScope(context);
  v8::Local<v8::String> name(TRI_V8_ASCII_STRING("(shell)"));

  Console = new V8LineEditor(context, ".arangosh.history");
  Console->open(BaseClient.autoComplete());

  // install signal handler for CTRL-C
#ifdef _WIN32
  // TODO

#else
  struct sigaction sa;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = &SignalHandler;

  int res = sigaction(SIGINT, &sa, 0);

  if (res != 0) {
    LOG_ERROR("unable to install signal handler");
  }
#endif

  uint64_t nrCommands = 0;

#ifdef __APPLE__
  if (VoiceMode) {
    system("say -v zarvox 'welcome to Arango shell' &");
  }
#endif

  while (true) {
    // set up prompts
    string dynamicPrompt;
    if (ClientConnection != nullptr) {
      dynamicPrompt = BuildPrompt();
    }
    else {
      dynamicPrompt = "disconnected> ";
    }

    string goodPrompt;
    string badPrompt;


#ifdef __APPLE__

    // ........................................................................................
    // MacOS uses libedit, which does not support ignoring of non-printable characters in the prompt
    // using non-printable characters in the prompt will lead to wrong prompt lengths being calculated
    // we will therefore disable colorful prompts for MacOS.
    // ........................................................................................

    goodPrompt = badPrompt = dynamicPrompt;

#elif _WIN32

    // ........................................................................................
    // Windows console is not coloured by escape sequences. So the method given below will not
    // work. For now we simply ignore the colours until we move the windows version into
    // a GUI Window.
    // ........................................................................................

    goodPrompt = badPrompt = dynamicPrompt;

#else

    if (BaseClient.colors()) {

#ifdef TRI_HAVE_LINENOISE
      // linenoise doesn't need escape sequences for escape sequences
      goodPrompt = TRI_SHELL_COLOR_BOLD_GREEN + dynamicPrompt + TRI_SHELL_COLOR_RESET;
      badPrompt  = TRI_SHELL_COLOR_BOLD_RED   + dynamicPrompt + TRI_SHELL_COLOR_RESET;

#else
      // readline does...
      goodPrompt = string()
                 + ArangoClient::PROMPT_IGNORE_START + TRI_SHELL_COLOR_BOLD_GREEN + ArangoClient::PROMPT_IGNORE_END
                 + dynamicPrompt
                 + ArangoClient::PROMPT_IGNORE_START + TRI_SHELL_COLOR_RESET + ArangoClient::PROMPT_IGNORE_END;

      badPrompt = string()
                + ArangoClient::PROMPT_IGNORE_START + TRI_SHELL_COLOR_BOLD_RED + ArangoClient::PROMPT_IGNORE_END
                + dynamicPrompt
                + ArangoClient::PROMPT_IGNORE_START + TRI_SHELL_COLOR_RESET + ArangoClient::PROMPT_IGNORE_END;
#endif
    }
    else {
      goodPrompt = badPrompt = dynamicPrompt;
    }

#endif

    // gc
    if (++nrCommands >= GcInterval) {
      nrCommands = 0;

      isolate->LowMemoryNotification();
      //   todo 1000 was the old V8-default, is this really good?
      while (! isolate->IdleNotification(1000)) {
      }
    }

#ifdef __APPLE__
  if (VoiceMode && promptError) {
    system("say -v 'whisper' 'oh, no' &");
  }
#endif

    char* input = Console->prompt(promptError ? badPrompt.c_str() : goodPrompt.c_str());

    if (input == nullptr) {
      break;
    }

    if (*input == '\0') {
      // input string is empty, but we must still free it
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);
      continue;
    }

    BaseClient.log("%s%s\n", dynamicPrompt.c_str(), input);

    string i = triagens::basics::StringUtils::trim(input);

    if (i == "exit" || i == "quit" || i == "exit;" || i == "quit;") {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);
      break;
    }

    if (i == "help" || i == "help;") {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);
      input = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, "help()");
      if (input == nullptr) {
        LOG_FATAL_AND_EXIT("out of memory");
      }
    }

    Console->addHistory(input);

    v8::TryCatch tryCatch;

    BaseClient.startPager();

    // assume the command succeeds
    promptError = false;

    // execute command and register its result in __LAST__
    v8::Handle<v8::Value> v = TRI_ExecuteJavaScriptString(isolate, context, TRI_V8_STRING(input), name, true);

    if (v.IsEmpty()) {
      context->Global()->Set(TRI_V8_ASCII_STRING("_last"), v8::Undefined(isolate));
    }
    else {
      context->Global()->Set(TRI_V8_ASCII_STRING("_last"), v);
    }

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);

    if (tryCatch.HasCaught()) {
      // command failed
      string exception(TRI_StringifyV8Exception(isolate, &tryCatch));

      BaseClient.printErrLine(exception);
      BaseClient.log("%s", exception.c_str());

      // this will change the prompt for the next round
      promptError = true;
    }

    BaseClient.stopPager();
    BaseClient.printLine("");

    BaseClient.log("%s\n", "");
    // make sure the last command result makes it into the log file
    BaseClient.flushLog();
  }

#ifdef __APPLE__
  if (VoiceMode) {
    system("say -v zarvox 'Good-Bye' &");
  }
#endif

  Console->close();
  delete Console;
  Console = nullptr;

  BaseClient.printLine("");

  BaseClient.printByeBye();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the unit tests
////////////////////////////////////////////////////////////////////////////////

static bool RunUnitTests (v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);

  bool ok;

  // set-up unit tests array
  v8::Handle<v8::Array> sysTestFiles = v8::Array::New(isolate);

  for (size_t i = 0;  i < UnitTests.size();  ++i) {
    sysTestFiles->Set((uint32_t) i, TRI_V8_STD_STRING(UnitTests[i]));
  }

  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_UNIT_TESTS"), sysTestFiles);
  // do not use TRI_AddGlobalVariableVocBase because it creates read-only variables!!
  context->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT"), v8::True(isolate));

  // run tests
  auto input = TRI_V8_ASCII_STRING("require(\"org/arangodb/testrunner\").runCommandLineTests();");
  auto name  = TRI_V8_ASCII_STRING("(arangosh)");
  TRI_ExecuteJavaScriptString(isolate, context, input, name, true);

  if (tryCatch.HasCaught()) {
    string exception(TRI_StringifyV8Exception(isolate, &tryCatch));
    BaseClient.printErrLine(exception);
    ok = false;
  }
  else {
    ok = TRI_ObjectToBoolean(context->Global()->Get(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT")));
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the Javascript files
////////////////////////////////////////////////////////////////////////////////

static bool RunScripts (v8::Isolate* isolate,
                        v8::Handle<v8::Context> context,
                        const vector<string>& scripts,
                        const bool execute) {
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);

  bool ok;

  ok = true;

  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL(ExecuteFileCallback, v8::Function);

  if (ExecuteFileCallback.IsEmpty()) {
    string msg = "no execute function has been registered";

    BaseClient.printErrLine(msg.c_str());
    BaseClient.log("%s", msg.c_str());

    BaseClient.flushLog();

    return false;
  }

  for (size_t i = 0;  i < scripts.size();  ++i) {
    if (! FileUtils::exists(scripts[i])) {
      string msg = "error: Javascript file not found: '" + scripts[i] + "'";

      BaseClient.printErrLine(msg.c_str());
      BaseClient.log("%s", msg.c_str());

      ok = false;
      break;
    }

    if (execute) {
      v8::Handle<v8::String> name = TRI_V8_STD_STRING(scripts[i]);
      v8::Handle<v8::Value> args[] = { name };

      ExecuteFileCallback->Call(ExecuteFileCallback, 1, args);
    }
    else {
      TRI_ParseJavaScriptFile(isolate, scripts[i].c_str());
    }

    if (tryCatch.HasCaught()) {
      string exception(TRI_StringifyV8Exception(isolate, &tryCatch));

      BaseClient.printErrLine(exception);
      BaseClient.log("%s\n", exception.c_str());

      ok = false;
      break;
    }
  }

  BaseClient.flushLog();

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the Javascript string
////////////////////////////////////////////////////////////////////////////////

static bool RunString (v8::Isolate* isolate,
                       v8::Handle<v8::Context> context,
                       const string& script) {
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);
  bool ok = true;

  v8::Handle<v8::Value> result = TRI_ExecuteJavaScriptString(isolate, 
                                                             context,
                                                             TRI_V8_STD_STRING(script),
                                                             TRI_V8_ASCII_STRING("(command-line)"),
                                                             false);

  if (tryCatch.HasCaught()) {
    string exception(TRI_StringifyV8Exception(isolate, &tryCatch));

    BaseClient.printErrLine(exception);
    BaseClient.log("%s\n", exception.c_str());
    ok = false;
  }
  else {
    // check return value of script
    if (result->IsNumber()) {
      int64_t intResult = TRI_ObjectToInt64(result);

      if (intResult != 0) {
        ok = false;
      }
    }
  }

  BaseClient.flushLog();

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the jslint tests
////////////////////////////////////////////////////////////////////////////////

static bool RunJsLint (v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);
  bool ok;

  // set-up jslint files array
  v8::Handle<v8::Array> sysTestFiles = v8::Array::New(isolate);

  for (size_t i = 0;  i < JsLint.size();  ++i) {
    sysTestFiles->Set((uint32_t) i, TRI_V8_STD_STRING(JsLint[i]));
  }

  context->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS"), sysTestFiles);
  context->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT"), v8::True(isolate));

  // run tests
  auto input = TRI_V8_ASCII_STRING("require(\"jslint\").runCommandLineTests({ });");
  auto name  = TRI_V8_ASCII_STRING("(arangosh)");
  TRI_ExecuteJavaScriptString(isolate, context, input, name, true);

  if (tryCatch.HasCaught()) {
    BaseClient.printErrLine(TRI_StringifyV8Exception(isolate, &tryCatch));
    ok = false;
  }
  else {
    ok = TRI_ObjectToBoolean(context->Global()->Get(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT")));
  }

  return ok;
}


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief startup and exit functions
////////////////////////////////////////////////////////////////////////////////

void* arangoshResourcesAllocated = nullptr;
static void arangoshEntryFunction ();
static void arangoshExitFunction (int, void*);

#ifdef _WIN32

// .............................................................................
// Call this function to do various initialistions for windows only
//
// TODO can we move this to a general function for all binaries?
// .............................................................................

void arangoshEntryFunction() {
  int maxOpenFiles = 1024;
  int res = 0;

  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  //res = initialiseWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initialiseWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);

  if (res != 0) {
    _exit(1);
  }

  res = initialiseWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,(const char*)(&maxOpenFiles));

  if (res != 0) {
    _exit(1);
  }

  res = initialiseWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    _exit(1);
  }

  TRI_Application_Exit_SetExit(arangoshExitFunction);
}

static void arangoshExitFunction(int exitCode, void* data) {
  // ...........................................................................
  // TODO: need a terminate function for windows to be called and cleanup
  // any windows specific stuff.
  // ...........................................................................

  int res = finaliseWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(1);
  }

  exit(exitCode);
}
#else

static void arangoshEntryFunction() {
}

static void arangoshExitFunction(int exitCode, void* data) {
}

#endif

bool print_helo(bool useServer, bool promptError) {
  // .............................................................................
  // banner
  // .............................................................................

  // http://www.network-science.de/ascii/   Font: ogre

  if (! BaseClient.quiet()) {

#ifdef _WIN32

    // .............................................................................
    // Quick hack for windows
    // .............................................................................

    if (BaseClient.colors()) {
      int greenColour   = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
      int redColour     = FOREGROUND_RED | FOREGROUND_INTENSITY;
      int defaultColour = 0;
      CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

      if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbiInfo) != 0) {
        defaultColour = csbiInfo.wAttributes;
      }

      // not sure about the code page. let user set code page by command-line argument if required
      if (CodePage > 0) {
        SetConsoleOutputCP((UINT) CodePage);
      }
      else {
        UINT cp = GetConsoleOutputCP();
        SetConsoleOutputCP(cp);
      }

      // TODO we should have a special "printf" which can handle the color escape sequences!
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), greenColour);
      printf("                                  ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), redColour);
      printf("     _     ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), defaultColour);
      printf("\n");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), greenColour);
      printf("  __ _ _ __ __ _ _ __   __ _  ___ ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), redColour);
      printf(" ___| |__  ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), defaultColour);
      printf("\n");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), greenColour);
      printf(" / _` | '__/ _` | '_ \\ / _` |/ _ \\");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), redColour);
      printf("/ __| '_ \\ ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), defaultColour);
      printf("\n");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), greenColour);
      printf("| (_| | | | (_| | | | | (_| | (_) ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), redColour);
      printf("\\__ \\ | | |");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), defaultColour);
      printf("\n");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), greenColour);
      printf(" \\__,_|_|  \\__,_|_| |_|\\__, |\\___/");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), redColour);
      printf("|___/_| |_|");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), defaultColour);
      printf("\n");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), greenColour);
      printf("                       |___/      ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), redColour);
      printf("           ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), defaultColour);
      printf("\n");
    }

#else

    char const* g = TRI_SHELL_COLOR_GREEN;
    char const* r = TRI_SHELL_COLOR_RED;
    char const* z = TRI_SHELL_COLOR_RESET;

    if (! BaseClient.colors()) {
      g = "";
      r = "";
      z = "";
    }

    BaseClient.printLine("");

    printf("%s                                  %s     _     %s\n", g, r, z);
    printf("%s  __ _ _ __ __ _ _ __   __ _  ___ %s ___| |__  %s\n", g, r, z);
    printf("%s / _` | '__/ _` | '_ \\ / _` |/ _ \\%s/ __| '_ \\ %s\n", g, r, z);
    printf("%s| (_| | | | (_| | | | | (_| | (_) %s\\__ \\ | | |%s\n", g, r, z);
    printf("%s \\__,_|_|  \\__,_|_| |_|\\__, |\\___/%s|___/_| |_|%s\n", g, r, z);
    printf("%s                       |___/      %s           %s\n", g, r, z);

#endif
    BaseClient.printLine("");

    ostringstream s;
    s << "Welcome to arangosh " << TRI_VERSION_FULL << ". Copyright (c) ArangoDB GmbH";

    BaseClient.printLine(s.str(), true);

    ostringstream info;
    info << "Using ";

#ifdef TRI_V8_VERSION
    info << "Google V8 " << TRI_V8_VERSION << " JavaScript engine";
#else
    info << "Google V8 JavaScript engine";
#endif

#ifdef TRI_READLINE_VERSION
    info << ", READLINE " << TRI_READLINE_VERSION;
#endif

#ifdef TRI_ICU_VERSION
    info << ", ICU " << TRI_ICU_VERSION;
#endif

    BaseClient.printLine(info.str(), true);
    BaseClient.printLine("", true);

    BaseClient.printWelcomeInfo();

    if (useServer) {
      if (ClientConnection->isConnected() && ClientConnection->getLastHttpReturnCode() == HttpResponse::OK) {
        ostringstream is;
        is << "Connected to ArangoDB '" << BaseClient.endpointString()
           << "' version: " << ClientConnection->getVersion() << ", database: '" << BaseClient.databaseName()
           << "', username: '" << BaseClient.username() << "'";

        BaseClient.printLine(is.str(), true);
      }
      else {
        ostringstream is;
        is << "Could not connect to endpoint '" << BaseClient.endpointString()
           << "', database: '" << BaseClient.databaseName()
           << "', username: '" << BaseClient.username() << "'";
        BaseClient.printErrLine(is.str());

        if (ClientConnection->getErrorMessage() != "") {
          ostringstream is2;
          is2 << "Error message '" << ClientConnection->getErrorMessage() << "'";
          BaseClient.printErrLine(is2.str());
        }
        promptError = true;
      }

      BaseClient.printLine("", true);
    }
  }
  return promptError;
}

void InitCallbacks(v8::Isolate *isolate,
                   bool useServer,
                   eRunMode runMode) {

  auto context = isolate->GetCurrentContext();
  // set pretty print default: (used in print.js)
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("PRETTY_PRINT"), v8::Boolean::New(isolate, BaseClient.prettyPrint()));

  // add colors for print.js
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("COLOR_OUTPUT"), v8::Boolean::New(isolate, BaseClient.colors()));

  // add function SYS_OUTPUT to use pager
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_OUTPUT"), v8::FunctionTemplate::New(isolate, JS_PagerOutput)->GetFunction());

  TRI_InitV8Buffer(isolate, context);

  TRI_InitV8Utils(isolate, context, StartupPath, StartupModules);
  TRI_InitV8Shell(isolate, context);

  // .............................................................................
  // define ArangoConnection class
  // .............................................................................

  if (useServer) {
    v8::Handle<v8::FunctionTemplate> connection_templ = v8::FunctionTemplate::New(isolate);
    connection_templ->SetClassName(TRI_V8_ASCII_STRING("ArangoConnection"));

    v8::Handle<v8::ObjectTemplate> connection_proto = connection_templ->PrototypeTemplate();

    connection_proto->Set(isolate, "DELETE",             v8::FunctionTemplate::New(isolate, ClientConnection_httpDelete));
    connection_proto->Set(isolate, "DELETE_RAW",         v8::FunctionTemplate::New(isolate, ClientConnection_httpDeleteRaw));
    connection_proto->Set(isolate, "GET",                v8::FunctionTemplate::New(isolate, ClientConnection_httpGet));
    connection_proto->Set(isolate, "GET_RAW",            v8::FunctionTemplate::New(isolate, ClientConnection_httpGetRaw));
    connection_proto->Set(isolate, "HEAD",               v8::FunctionTemplate::New(isolate, ClientConnection_httpHead));
    connection_proto->Set(isolate, "HEAD_RAW",           v8::FunctionTemplate::New(isolate, ClientConnection_httpHeadRaw));
    connection_proto->Set(isolate, "OPTIONS",            v8::FunctionTemplate::New(isolate, ClientConnection_httpOptions));
    connection_proto->Set(isolate, "OPTIONS_RAW",        v8::FunctionTemplate::New(isolate, ClientConnection_httpOptionsRaw));
    connection_proto->Set(isolate, "PATCH",              v8::FunctionTemplate::New(isolate, ClientConnection_httpPatch));
    connection_proto->Set(isolate, "PATCH_RAW",          v8::FunctionTemplate::New(isolate, ClientConnection_httpPatchRaw));
    connection_proto->Set(isolate, "POST",               v8::FunctionTemplate::New(isolate, ClientConnection_httpPost));
    connection_proto->Set(isolate, "POST_RAW",           v8::FunctionTemplate::New(isolate, ClientConnection_httpPostRaw));
    connection_proto->Set(isolate, "PUT",                v8::FunctionTemplate::New(isolate, ClientConnection_httpPut));
    connection_proto->Set(isolate, "PUT_RAW",            v8::FunctionTemplate::New(isolate, ClientConnection_httpPutRaw));
    connection_proto->Set(isolate, "SEND_FILE",          v8::FunctionTemplate::New(isolate, ClientConnection_httpSendFile));
    connection_proto->Set(isolate, "getEndpoint",        v8::FunctionTemplate::New(isolate, ClientConnection_getEndpoint));
    connection_proto->Set(isolate, "lastHttpReturnCode", v8::FunctionTemplate::New(isolate, ClientConnection_lastHttpReturnCode));
    connection_proto->Set(isolate, "lastErrorMessage",   v8::FunctionTemplate::New(isolate, ClientConnection_lastErrorMessage));
    connection_proto->Set(isolate, "isConnected",        v8::FunctionTemplate::New(isolate, ClientConnection_isConnected));
    connection_proto->Set(isolate, "reconnect",          v8::FunctionTemplate::New(isolate, ClientConnection_reconnect));
    connection_proto->Set(isolate, "toString",           v8::FunctionTemplate::New(isolate, ClientConnection_toString));
    connection_proto->Set(isolate, "getVersion",         v8::FunctionTemplate::New(isolate, ClientConnection_getVersion));
    connection_proto->Set(isolate, "getDatabaseName",    v8::FunctionTemplate::New(isolate, ClientConnection_getDatabaseName));
    connection_proto->Set(isolate, "setDatabaseName",    v8::FunctionTemplate::New(isolate, ClientConnection_setDatabaseName));
    connection_proto->SetCallAsFunctionHandler(ClientConnection_ConstructorCallback);

    v8::Handle<v8::ObjectTemplate> connection_inst = connection_templ->InstanceTemplate();
    connection_inst->SetInternalFieldCount(2);

    TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("ArangoConnection"), connection_proto->NewInstance());
    ConnectionTempl.Reset(isolate, connection_inst);

    // add the client connection to the context:
    TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_ARANGO"), wrapV8ClientConnection(isolate, ClientConnection));
  }

  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_START_PAGER"),      v8::FunctionTemplate::New(isolate, JS_StartOutputPager)->GetFunction());
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_STOP_PAGER"),       v8::FunctionTemplate::New(isolate, JS_StopOutputPager)->GetFunction());
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_IMPORT_CSV_FILE"),  v8::FunctionTemplate::New(isolate, JS_ImportCsvFile)->GetFunction());
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_IMPORT_JSON_FILE"), v8::FunctionTemplate::New(isolate, JS_ImportJsonFile)->GetFunction());
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("NORMALIZE_STRING"),     v8::FunctionTemplate::New(isolate, JS_normalize_string)->GetFunction());
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("COMPARE_STRING"),       v8::FunctionTemplate::New(isolate, JS_compare_string)->GetFunction());

  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("ARANGO_QUIET"),         v8::Boolean::New(isolate, BaseClient.quiet()));
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("VALGRIND"),             v8::Boolean::New(isolate, (RUNNING_ON_VALGRIND > 0)));

  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("IS_EXECUTE_SCRIPT"),    v8::Boolean::New(isolate, runMode == eExecuteScript));
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("IS_EXECUTE_STRING"),    v8::Boolean::New(isolate, runMode == eExecuteString));
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("IS_CHECK_SCRIPT"),      v8::Boolean::New(isolate, runMode == eCheckScripts));
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("IS_UNIT_TESTS"),        v8::Boolean::New(isolate, runMode == eUnitTests));
  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("IS_JS_LINT"),           v8::Boolean::New(isolate, runMode == eJsLint));

}

int warmupEnvironment(v8::Isolate *isolate,
                      vector<string> &positionals,
                      eRunMode runMode) {
  auto context = isolate->GetCurrentContext();
  // .............................................................................
  // read files
  // .............................................................................

  // load java script from js/bootstrap/*.h files
  if (StartupPath.empty()) {
    LOG_FATAL_AND_EXIT("no 'javascript.startup-directory' has been supplied, giving up");
  }

  LOG_DEBUG("using JavaScript startup files at '%s'", StartupPath.c_str());
  StartupLoader.setDirectory(StartupPath);

  // load all init files
  vector<string> files;

  files.push_back("common/bootstrap/modules.js");
  files.push_back("common/bootstrap/module-internal.js");
  files.push_back("common/bootstrap/module-fs.js");
  files.push_back("common/bootstrap/module-console.js");  // needs internal
  files.push_back("common/bootstrap/errors.js");

  if (runMode != eJsLint) {
    files.push_back("common/bootstrap/monkeypatches.js");
  }

  files.push_back("client/bootstrap/module-internal.js");
  files.push_back("client/client.js"); // needs internal

  for (size_t i = 0;  i < files.size();  ++i) {
    bool ok = StartupLoader.loadScript(isolate, context, files[i]);

    if (ok) {
      LOG_TRACE("loaded JavaScript file '%s'", files[i].c_str());
    }
    else {
      LOG_FATAL_AND_EXIT("cannot load JavaScript file '%s'", files[i].c_str());
    }
  }

  // .............................................................................
  // create arguments
  // .............................................................................

  v8::Handle<v8::Array> p = v8::Array::New(isolate, (int) positionals.size());

  for (uint32_t i = 0;  i < positionals.size();  ++i) {
    p->Set(i, TRI_V8_STD_STRING(positionals[i]));
  }

  TRI_AddGlobalVariableVocbase(isolate, context, TRI_V8_ASCII_STRING("ARGUMENTS"), p);
  return EXIT_SUCCESS;
}

int run(v8::Isolate *isolate, eRunMode runMode, bool promptError) {
  auto context = isolate->GetCurrentContext();
  bool ok = false;
  try {
    switch (runMode) {
    case eInteractive: 
       RunShell(isolate, context, promptError);
       ok = true; /// TODO
      break;
    case eExecuteScript:
      // we have scripts to execute
      ok = RunScripts(isolate, context, ExecuteScripts, true);
      break;
    case eExecuteString:
      // we have string to execute
      ok = RunString(isolate, context, ExecuteString);
      break;
    case eCheckScripts:
      // we have scripts to syntax check
      ok = RunScripts(isolate, context, CheckScripts, false);
      break;
    case eUnitTests:
      // we have unit tests
      ok = RunUnitTests(isolate, context);
      break;
    case eJsLint:
      // we don't have unittests, but we have files to jslint
      ok = RunJsLint(isolate, context);
      break;
    }
  }
  catch (std::exception const& ex) {
    cerr << "caught exception " << ex.what() << endl;
    ok = false;
  }
  catch (...) {
    cerr << "caught unknown exception" << endl;
    ok = false;
  }
  return (ok)? EXIT_SUCCESS : EXIT_FAILURE;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* args[]) {
  int ret = EXIT_SUCCESS;
  eRunMode runMode = eInteractive;
  // reset the prompt error flag (will determine prompt colors)
  bool promptError = false;

  arangoshEntryFunction();

  TRIAGENS_C_INITIALISE(argc, args);
  TRIAGENS_REST_INITIALISE(argc, args);

  TRI_InitialiseLogging(false);

  BaseClient.setEndpointString(Endpoint::getDefaultEndpoint());

  // .............................................................................
  // parse the program options
  // .............................................................................

  vector<string> positionals = ParseProgramOptions(argc, args, &runMode);

  // .............................................................................
  // set-up client connection
  // .............................................................................

  // check if we want to connect to a server
  bool useServer = (BaseClient.endpointString() != "none");

  // if we are in jslint mode, we will not need the server at all
  if (! JsLint.empty()) {
    useServer = false;
  }

  if (useServer) {
    BaseClient.createEndpoint();

    if (BaseClient.endpointServer() == nullptr) {
      ostringstream s;
      s << "invalid value for --server.endpoint ('" << BaseClient.endpointString() << "')";

      BaseClient.printErrLine(s.str());

      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }

    ClientConnection = CreateConnection();
  }

  // .............................................................................
  // set-up V8 objects
  // .............................................................................

  v8::V8::InitializeICU();
  v8::Platform* platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platform);
  v8::Isolate* isolate = v8::Isolate::New();
  isolate->Enter();
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    {
      // create the global template
      v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

      // create the context
      v8::Persistent<v8::Context> context;
      context.Reset(isolate, v8::Context::New(isolate, 0, global));
      auto localContext = v8::Local<v8::Context>::New(isolate, context);

      if (localContext.IsEmpty()) {
        BaseClient.printErrLine("cannot initialize V8 engine");
        TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
      }

      localContext->Enter();

      InitCallbacks(isolate, useServer, runMode);

      promptError = print_helo(useServer, promptError);

      ret = warmupEnvironment(isolate, positionals, runMode);
      if (ret == EXIT_SUCCESS) {
        BaseClient.openLog();

        ret = run(isolate, runMode, promptError);
      }

      isolate->LowMemoryNotification();

      // todo 1000 was the old V8-default, is this really good?
      while (! isolate->IdleNotification(1000)) {
      }
      localContext->Exit();
      context.Reset();
    }
  }
  isolate->Exit();
  isolate->Dispose();

  BaseClient.closeLog();

  if (ClientConnection != nullptr) {
    delete ClientConnection;
  }

  TRIAGENS_REST_SHUTDOWN;

  arangoshExitFunction(ret, nullptr);

  return ret;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
