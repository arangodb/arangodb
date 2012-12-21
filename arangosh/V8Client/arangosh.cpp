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
#include <fstream>

#include "build.h"

#include "3rdParty/valgrind/valgrind.h"

#include "ArangoShell/ArangoClient.h"
#include "Basics/FileUtils.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "BasicsC/csv.h"
#include "BasicsC/files.h"
#include "BasicsC/init.h"
#include "BasicsC/strings.h"
#include "Rest/Endpoint.h"
#include "Rest/InitialiseRest.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "V8/JSLoader.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8Client/ImportHelper.h"
#include "V8Client/V8ClientConnection.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::httpclient;
using namespace triagens::v8client;
using namespace triagens::arango;

#include "js/common/bootstrap/js-print.h"
#include "js/common/bootstrap/js-modules.h"
#include "js/common/bootstrap/js-monkeypatches.h"
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
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient;

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection* ClientConnection = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief object template for the initial connection
////////////////////////////////////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> ConnectionTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief max size body size (used for imports)
////////////////////////////////////////////////////////////////////////////////

static uint64_t MaxUploadSize = 500000;

////////////////////////////////////////////////////////////////////////////////
/// @brief startup JavaScript files
////////////////////////////////////////////////////////////////////////////////

static JSLoader StartupLoader;

////////////////////////////////////////////////////////////////////////////////
/// @brief path for JavaScript modules files
////////////////////////////////////////////////////////////////////////////////

static string StartupModules = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief path for JavaScript bootstrap files
////////////////////////////////////////////////////////////////////////////////

static string StartupPath = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief javascript files to execute
////////////////////////////////////////////////////////////////////////////////

static vector<string> ExecuteScripts;

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

    BaseClient.internalPrint(str.c_str());
  }

  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the output pager
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_StartOutputPager (v8::Arguments const& ) {
  if (BaseClient.usePager()) {
    BaseClient.internalPrint("Using pager already.\n");        
  }
  else {
    BaseClient.setUsePager(true);
    BaseClient.internalPrint("Using pager '%s' for output buffering.\n", BaseClient.outputPager().c_str());    
  }

  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the output pager
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_StopOutputPager (v8::Arguments const& ) {
  if (BaseClient.usePager()) {
    BaseClient.internalPrint("Stopping pager.\n");
  }
  else {
    BaseClient.internalPrint("Pager not running.\n");    
  }
  
  BaseClient.setUsePager(false);

  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   import function
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

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

static v8::Handle<v8::Value> JS_ImportCsvFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: importCsvFile(<filename>, <collection>[, <options>])")));
  }

  // extract the filename
  v8::String::Utf8Value filename(argv[0]);

  if (*filename == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be an UTF8 filename")));
  }

  v8::String::Utf8Value collection(argv[1]);

  if (*collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<collection> must be an UTF8 filename")));
  }

  // extract the options
  v8::Handle<v8::String> separatorKey = v8::String::New("separator");
  v8::Handle<v8::String> quoteKey = v8::String::New("quote");

  string separator = ",";
  string quote = "\"";

  if (3 <= argv.Length()) {
    v8::Handle<v8::Object> options = argv[2]->ToObject();
    // separator
    if (options->Has(separatorKey)) {
      separator = TRI_ObjectToString(options->Get(separatorKey));

      if (separator.length() < 1) {
        return scope.Close(v8::ThrowException(v8::String::New("<options>.separator must be at least one character")));
      }
    }

    // quote
    if (options->Has(quoteKey)) {
      quote = TRI_ObjectToString(options->Get(quoteKey));

      if (quote.length() > 1) {
        return scope.Close(v8::ThrowException(v8::String::New("<options>.quote must be at most one character")));
      }
    }
  }

  ImportHelper ih(ClientConnection->getHttpClient(), MaxUploadSize);
  
  ih.setQuote(quote);
  ih.setSeparator(separator.c_str());

  string fileName = TRI_ObjectToString(argv[0]);
  string collectionName = TRI_ObjectToString(argv[1]);
 
  if (ih.importDelimited(collectionName, fileName, ImportHelper::CSV)) {
    v8::Handle<v8::Object> result = v8::Object::New();
    result->Set(v8::String::New("lines"), v8::Integer::New(ih.getReadLines()));
    result->Set(v8::String::New("created"), v8::Integer::New(ih.getImportedLines()));
    result->Set(v8::String::New("errors"), v8::Integer::New(ih.getErrorLines()));
    return scope.Close(result);
  }
  
  return scope.Close(v8::ThrowException(v8::String::New(ih.getErrorMessage().c_str())));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a JSON file
///
/// @FUN{importJsonFile(@FA{filename}, @FA{collection})}
///
/// Imports data of a CSV file. The data is imported to @FA{collection}.
///
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ImportJsonFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: importJsonFile(<filename>, <collection>)")));
  }

  // extract the filename
  v8::String::Utf8Value filename(argv[0]);

  if (*filename == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be an UTF8 filename")));
  }

  v8::String::Utf8Value collection(argv[1]);

  if (*collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<collection> must be an UTF8 filename")));
  }

  
  ImportHelper ih(ClientConnection->getHttpClient(), MaxUploadSize);
  
  string fileName = TRI_ObjectToString(argv[0]);
  string collectionName = TRI_ObjectToString(argv[1]);
 
  if (ih.importJson(collectionName, fileName)) {
    v8::Handle<v8::Object> result = v8::Object::New();
    result->Set(v8::String::New("lines"), v8::Integer::New(ih.getReadLines()));
    result->Set(v8::String::New("created"), v8::Integer::New(ih.getImportedLines()));
    result->Set(v8::String::New("errors"), v8::Integer::New(ih.getErrorLines()));
    return scope.Close(result);
  }
  
  return scope.Close(v8::ThrowException(v8::String::New(ih.getErrorMessage().c_str())));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief normalize UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_normalize_string (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: NORMALIZE_STRING(<string>)")));
  }

  return scope.Close(TRI_normalize_V8_Obj(argv[0]));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_compare_string (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: COMPARE_STRING(<left string>, <right string>)")));
  }

  v8::String::Value left(argv[0]);
  v8::String::Value right(argv[1]);
  
  int result = Utf8Helper::DefaultUtf8Helper.compareUtf16(*left, left.length(), *right, right.length());
  
  return scope.Close(v8::Integer::New(result));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return a new client connection instance
////////////////////////////////////////////////////////////////////////////////
  
static V8ClientConnection* CreateConnection () {
  return new V8ClientConnection(BaseClient.endpointServer(),
                                BaseClient.username(),
                                BaseClient.password(), 
                                BaseClient.requestTimeout(), 
                                BaseClient.connectTimeout(), 
                                ArangoClient::DEFAULT_RETRIES,
                                false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static void ParseProgramOptions (int argc, char* argv[]) {
  ProgramOptionsDescription description("STANDARD options");
  ProgramOptionsDescription javascript("JAVASCRIPT options");

  javascript
    ("javascript.execute", &ExecuteScripts, "execute Javascript code from file")
    ("javascript.check", &CheckScripts, "syntax check code Javascript code from file")
    ("javascript.modules-path", &StartupModules, "one or more directories separated by cola")
    ("javascript.startup-directory", &StartupPath, "startup paths containing the JavaScript files; multiple directories can be separated by cola")
    ("javascript.unit-tests", &UnitTests, "do not start as shell, run unit tests instead")
    ("jslint", &JsLint, "do not start as shell, run jslint instead")
  ;

  description
    ("max-upload-size", &MaxUploadSize, "maximum size of import chunks (in bytes)")
    (javascript, false)
  ;

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
  BaseClient.parse(options, description, argc, argv, "arangosh.conf");

  // set V8 options
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);

  // check module path
  if (StartupModules.empty()) {
    LOGGER_FATAL << "module path not known, please use '--javascript.modules-path'";
    exit(EXIT_FAILURE);
  }
  
  // turn on paging automatically if "pager" option is set
  if (options.has("pager") && ! options.has("use-pager")) {
    BaseClient.setUsePager(true);
  }

  // disable excessive output in non-interactive mode
  if (! ExecuteScripts.empty() || ! CheckScripts.empty() || ! UnitTests.empty() || ! JsLint.empty()) {
    BaseClient.shutup();
  }
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

enum WRAP_CLASS_TYPES {WRAP_TYPE_CONNECTION = 1};

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries (call the destructor here)
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_DestructorCallback (v8::Persistent<v8::Value> , void* parameter) {
  V8ClientConnection* client = (V8ClientConnection*) parameter;
  delete client;
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
/// @brief ClientConnection constructor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_ConstructorCallback (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() > 0 && argv[0]->IsString()) {
    string definition = TRI_ObjectToString(argv[0]);
  
    BaseClient.createEndpoint(definition);

    if (BaseClient.endpointServer() == 0) { 
      string errorMessage = "error in '" + definition + "'";
      return scope.Close(v8::ThrowException(v8::String::New(errorMessage.c_str())));      
    }
  }

  if (BaseClient.endpointServer() == 0) { 
    return v8::Undefined();
  }   
  
  V8ClientConnection* connection = CreateConnection();
  
  if (connection->isConnected()) {
    cout << "Connected to ArangoDB '" << BaseClient.endpointServer()->getSpecification() 
         << "' Version " << connection->getVersion() << endl; 
  }
  else {
    string errorMessage = "Could not connect. Error message: " + connection->getErrorMessage();
    delete connection;
    return scope.Close(v8::ThrowException(v8::String::New(errorMessage.c_str())));
  }

  return scope.Close(wrapV8ClientConnection(connection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "httpGet"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_httpGet (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  // check params
  if (argv.Length() < 1 || argv.Length() > 2 || !argv[0]->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: get(<url>[, <headers>])")));
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, argv[0]);

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

static v8::Handle<v8::Value> ClientConnection_httpDelete (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  // check params
  if (argv.Length() < 1 || argv.Length() > 2 || !argv[0]->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: delete(<url>[, <headers>])")));
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, argv[0]);

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

static v8::Handle<v8::Value> ClientConnection_httpPost (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  // check params
  if (argv.Length() < 2 || argv.Length() > 3 || !argv[0]->IsString() || !argv[1]->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: post(<url>, <body>[, <headers>])")));
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, argv[0]);
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

static v8::Handle<v8::Value> ClientConnection_httpPut (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  // check params
  if (argv.Length() < 2 || argv.Length() > 3 || !argv[0]->IsString() || !argv[1]->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: put(<url>, <body>[, <headers>])")));
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, argv[0]);
  v8::String::Utf8Value body(argv[1]);

  // check header fields
  map<string, string> headerFields;
  if (argv.Length() > 2) {
    objectToMap(headerFields, argv[2]);
  }

  return scope.Close(connection->putData(*url, *body, headerFields));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "httpPatch"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_httpPatch (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  // check params
  if (argv.Length() < 2 || argv.Length() > 3 || !argv[0]->IsString() || !argv[1]->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: patch(<url>, <body>[, <headers>])")));
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, argv[0]);
  v8::String::Utf8Value body(argv[1]);

  // check header fields
  map<string, string> headerFields;
  if (argv.Length() > 2) {
    objectToMap(headerFields, argv[2]);
  }

  return scope.Close(connection->patchData(*url, *body, headerFields));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "lastError"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_lastHttpReturnCode (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

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

static v8::Handle<v8::Value> ClientConnection_lastErrorMessage (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

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

static v8::Handle<v8::Value> ClientConnection_isConnected (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

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

static v8::Handle<v8::Value> ClientConnection_toString (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: toString()")));
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
  
  return scope.Close(v8::String::New(result.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getVersion"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_getVersion (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // get the connection
  V8ClientConnection* connection = TRI_UnwrapClass<V8ClientConnection>(argv.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("connection class corrupted")));
  }
  
  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: getVersion()")));
  }

  return scope.Close(v8::String::New(connection->getVersion().c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the shell
////////////////////////////////////////////////////////////////////////////////

static void RunShell (v8::Handle<v8::Context> context, bool promptError) {
  v8::Context::Scope contextScope(context);
  v8::Local<v8::String> name(v8::String::New("(shell)"));

  V8LineEditor console(context, ".arangosh");

  console.open(BaseClient.autoComplete());

  // set up prompts
  string goodPrompt;
  string badPrompt;
  
  if (BaseClient.colors()) {
    goodPrompt = ArangoClient::COLOR_BOLD_GREEN + string("arangosh>") + ArangoClient::COLOR_RESET + ' ';
    badPrompt  = ArangoClient::COLOR_BOLD_RED   + string("arangosh>") + ArangoClient::COLOR_RESET + ' ';
  }
  else {
    goodPrompt = badPrompt = string("arangosh> ");
  }
  
  cout << endl;

  while (true) {
    // gc
    while (! v8::V8::IdleNotification()) {
    }
  
    char* input = console.prompt(promptError ? badPrompt.c_str() : goodPrompt.c_str());

    if (input == 0) {
      break;
    }

    if (*input == '\0') {
      continue;
    }
    
    BaseClient.log("arangosh> %s\n", input); 

    string i = triagens::basics::StringUtils::trim(input);

    if (i == "exit" || i == "quit" || i == "exit;" || i == "quit;") {
      TRI_FreeString(TRI_CORE_MEM_ZONE, input);
      break;
    }

    if (i == "help" || i == "help;") {
      TRI_FreeString(TRI_CORE_MEM_ZONE, input);
      input = TRI_DuplicateString("help()");
    }
   
    console.addHistory(input);
    
    v8::HandleScope scope;
    v8::TryCatch tryCatch;
    
    BaseClient.startPager();

    // assume the command succeeds
    promptError = false;
    TRI_ExecuteJavaScriptString(context, v8::String::New(input), name, true);
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);

    if (tryCatch.HasCaught()) {
      // command failed
      string exception(TRI_StringifyV8Exception(&tryCatch));

      cerr << exception;
      BaseClient.log("%s", exception.c_str()); 

      // this will change the prompt for the next round
      promptError = true;
    }

    BaseClient.stopPager();
    cout << endl;

    BaseClient.log("%s\n", "");
    // make sure the last command result makes it into the log file
    BaseClient.flushLog();
  }

  console.close();

  cout << endl;

  BaseClient.printByeBye();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the unit tests
////////////////////////////////////////////////////////////////////////////////

static bool RunUnitTests (v8::Handle<v8::Context> context) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;
  bool ok;

  // set-up unit tests array
  v8::Handle<v8::Array> sysTestFiles = v8::Array::New();

  for (size_t i = 0;  i < UnitTests.size();  ++i) {
    sysTestFiles->Set((uint32_t) i, v8::String::New(UnitTests[i].c_str()));
  }

  context->Global()->Set(v8::String::New("SYS_UNIT_TESTS"), sysTestFiles);
  context->Global()->Set(v8::String::New("SYS_UNIT_TESTS_RESULT"), v8::True());

  // run tests
  char const* input = "require(\"jsunity\").runCommandLineTests();";
  v8::Local<v8::String> name(v8::String::New("(arangosh)"));
  TRI_ExecuteJavaScriptString(context, v8::String::New(input), name, true);
      
  if (tryCatch.HasCaught()) {
    cerr << TRI_StringifyV8Exception(&tryCatch);
    ok = false;
  }
  else {
    ok = TRI_ObjectToBoolean(context->Global()->Get(v8::String::New("SYS_UNIT_TESTS_RESULT")));
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the Javascript files
////////////////////////////////////////////////////////////////////////////////

static bool RunScripts (v8::Handle<v8::Context> context, 
                        const vector<string>& scripts, 
                        const bool execute) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;
  bool ok;

  ok = true;

  for (size_t i = 0;  i < scripts.size();  ++i) {
    if (! FileUtils::exists(scripts[i])) {
      cerr << "error: Javascript file not found: '" << scripts[i].c_str() << "'" << endl;
      BaseClient.log("error: Javascript file not found: '%s'\n", scripts[i].c_str());
      ok = false;
      break;
    }

    if (execute) {
      TRI_ExecuteJavaScriptFile(context, scripts[i].c_str());
    }
    else {
      TRI_LoadJavaScriptFile(context, scripts[i].c_str());
    }
  
    if (tryCatch.HasCaught()) {
      string exception(TRI_StringifyV8Exception(&tryCatch));

      cerr << exception << endl;
      BaseClient.log("%s\n", exception.c_str()); 
      ok = false;
      break;
    }
  }

  BaseClient.flushLog();

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the jslint tests
////////////////////////////////////////////////////////////////////////////////

static bool RunJsLint (v8::Handle<v8::Context> context) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;
  bool ok;

  // set-up jslint files array
  v8::Handle<v8::Array> sysTestFiles = v8::Array::New();

  for (size_t i = 0;  i < JsLint.size();  ++i) {
    sysTestFiles->Set((uint32_t) i, v8::String::New(JsLint[i].c_str()));
  }

  context->Global()->Set(v8::String::New("SYS_UNIT_TESTS"), sysTestFiles);
  context->Global()->Set(v8::String::New("SYS_UNIT_TESTS_RESULT"), v8::True());

  // run tests
  char const* input = "require(\"jslint\").runCommandLineTests({ });";
  v8::Local<v8::String> name(v8::String::New("(arangosh)"));
  TRI_ExecuteJavaScriptString(context, v8::String::New(input), name, true);
      
  if (tryCatch.HasCaught()) {
    cerr << TRI_StringifyV8Exception(&tryCatch);
    ok = false;
  }
  else {
    ok = TRI_ObjectToBoolean(context->Global()->Get(v8::String::New("SYS_UNIT_TESTS_RESULT")));
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adding colors for output
////////////////////////////////////////////////////////////////////////////////

static void AddColors (v8::Handle<v8::Context> context) {  
  context->Global()->Set(v8::String::New("COLOR_RED"),
                         v8::String::New(ArangoClient::COLOR_RED),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_BOLD_RED"),
                         v8::String::New(ArangoClient::COLOR_BOLD_RED),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_GREEN"),
                         v8::String::New(ArangoClient::COLOR_GREEN),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_BOLD_GREEN"),
                         v8::String::New(ArangoClient::COLOR_BOLD_GREEN),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_BLUE"),
                         v8::String::New(ArangoClient::COLOR_BLUE),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_BOLD_BLUE"),
                         v8::String::New(ArangoClient::COLOR_BOLD_BLUE),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_YELLOW"),
                         v8::String::New(ArangoClient::COLOR_YELLOW),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_BOLD_YELLOW"),
                         v8::String::New(ArangoClient::COLOR_BOLD_YELLOW),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_WHITE"),
                         v8::String::New(ArangoClient::COLOR_WHITE),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_BOLD_WHITE"),
                         v8::String::New(ArangoClient::COLOR_BOLD_WHITE),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_BLACK"),
                         v8::String::New(ArangoClient::COLOR_BLACK),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_BOLD_BLACK"),
                         v8::String::New(ArangoClient::COLOR_BOLD_BLACK),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_BLINK"),
                         v8::String::New(ArangoClient::COLOR_BLINK),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_BRIGHT"),
                         v8::String::New(ArangoClient::COLOR_BRIGHT),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COLOR_OUTPUT_RESET"),
                         v8::String::New(ArangoClient::COLOR_RESET),
                         v8::ReadOnly);    

  context->Global()->Set(v8::String::New("COLOR_OUTPUT"), v8::Boolean::New(BaseClient.colors()));
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
  TRIAGENS_C_INITIALISE(argc, argv);
  TRIAGENS_REST_INITIALISE(argc, argv);
  
  TRI_InitialiseLogging(false);
  int ret = EXIT_SUCCESS;

  BaseClient.setEndpointString(Endpoint::getDefaultEndpoint());

  // .............................................................................
  // parse the program options
  // .............................................................................

  ParseProgramOptions(argc, argv);

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

    if (BaseClient.endpointServer() == 0) {
      cerr << "invalid value for --server.endpoint ('" << BaseClient.endpointString() << "')" << endl;
      exit(EXIT_FAILURE);
    }

    ClientConnection = CreateConnection();
  }
  
  // .............................................................................
  // set-up V8 objects
  // .............................................................................

  v8::HandleScope handle_scope;

  // create the global template
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

  // create the context
  v8::Persistent<v8::Context> context = v8::Context::New(0, global);

  if (context.IsEmpty()) {
    cerr << "cannot initialize V8 engine" << endl;
    exit(EXIT_FAILURE);
  }

  context->Enter();

  // add function SYS_OUTPUT to use pager
  context->Global()->Set(v8::String::New("SYS_OUTPUT"),
                         v8::FunctionTemplate::New(JS_PagerOutput)->GetFunction(),
                         v8::ReadOnly);
  
  TRI_InitV8Utils(context, StartupModules);
  
  TRI_InitV8Shell(context);

  // set pretty print default: (used in print.js)
  context->Global()->Set(v8::String::New("PRETTY_PRINT"), v8::Boolean::New(BaseClient.prettyPrint()));
  
  // add colors for print.js
  AddColors(context);

  // reset the prompt error flag (will determine prompt colors)
  bool promptError = false;  

  // .............................................................................
  // define ArangoConnection class
  // .............................................................................  

  if (useServer) {
    v8::Handle<v8::FunctionTemplate> connection_templ = v8::FunctionTemplate::New();
    connection_templ->SetClassName(v8::String::New("ArangoConnection"));

    v8::Handle<v8::ObjectTemplate> connection_proto = connection_templ->PrototypeTemplate();
    
    connection_proto->Set("GET", v8::FunctionTemplate::New(ClientConnection_httpGet));
    connection_proto->Set("POST", v8::FunctionTemplate::New(ClientConnection_httpPost));
    connection_proto->Set("DELETE", v8::FunctionTemplate::New(ClientConnection_httpDelete));
    connection_proto->Set("PUT", v8::FunctionTemplate::New(ClientConnection_httpPut));
    connection_proto->Set("PATCH", v8::FunctionTemplate::New(ClientConnection_httpPatch));
    connection_proto->Set("lastHttpReturnCode", v8::FunctionTemplate::New(ClientConnection_lastHttpReturnCode));
    connection_proto->Set("lastErrorMessage", v8::FunctionTemplate::New(ClientConnection_lastErrorMessage));
    connection_proto->Set("isConnected", v8::FunctionTemplate::New(ClientConnection_isConnected));
    connection_proto->Set("toString", v8::FunctionTemplate::New(ClientConnection_toString));
    connection_proto->Set("getVersion", v8::FunctionTemplate::New(ClientConnection_getVersion));
    connection_proto->SetCallAsFunctionHandler(ClientConnection_ConstructorCallback);    
    
    v8::Handle<v8::ObjectTemplate> connection_inst = connection_templ->InstanceTemplate();
    connection_inst->SetInternalFieldCount(2);    
    
    context->Global()->Set(v8::String::New("ArangoConnection"), connection_proto->NewInstance());    
    ConnectionTempl = v8::Persistent<v8::ObjectTemplate>::New(connection_inst);

    // add the client connection to the context:
    context->Global()->Set(v8::String::New("arango"), 
                           wrapV8ClientConnection(ClientConnection),
                           v8::ReadOnly);
  }
    
  context->Global()->Set(v8::String::New("SYS_START_PAGER"),
                         v8::FunctionTemplate::New(JS_StartOutputPager)->GetFunction(),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("SYS_STOP_PAGER"),
                         v8::FunctionTemplate::New(JS_StopOutputPager)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("importCsvFile"),
                         v8::FunctionTemplate::New(JS_ImportCsvFile)->GetFunction(),
                         v8::ReadOnly);
  context->Global()->Set(v8::String::New("importJsonFile"),
                         v8::FunctionTemplate::New(JS_ImportJsonFile)->GetFunction(),
                         v8::ReadOnly);
 
  context->Global()->Set(v8::String::New("NORMALIZE_STRING"),
                         v8::FunctionTemplate::New(JS_normalize_string)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("COMPARE_STRING"),
                         v8::FunctionTemplate::New(JS_compare_string)->GetFunction(),
                         v8::ReadOnly);
  
  context->Global()->Set(v8::String::New("HAS_ICU"),
#ifdef TRI_ICU_VERSION
                         v8::Boolean::New(true),
#else
                         v8::Boolean::New(false),
#endif
                         v8::ReadOnly);

  // .............................................................................
  // banner
  // .............................................................................  

  // http://www.network-science.de/ascii/   Font: ogre
  if (! BaseClient.quiet()) {
    char const* g = ArangoClient::COLOR_GREEN;
    char const* r = ArangoClient::COLOR_RED;
    char const* z = ArangoClient::COLOR_RESET;

    if (! BaseClient.colors()) {
      g = "";
      r = "";
      z = "";
    }

    cout << endl;

    printf("%s                                  %s     _     %s\n", g, r, z);
    printf("%s  __ _ _ __ __ _ _ __   __ _  ___ %s ___| |__  %s\n", g, r, z);
    printf("%s / _` | '__/ _` | '_ \\ / _` |/ _ \\%s/ __| '_ \\ %s\n", g, r, z);
    printf("%s| (_| | | | (_| | | | | (_| | (_) %s\\__ \\ | | |%s\n", g, r, z);
    printf("%s \\__,_|_|  \\__,_|_| |_|\\__, |\\___/%s|___/_| |_|%s\n", g, r, z);
    printf("%s                       |___/      %s           %s\n", g, r, z);

    cout << endl << "Welcome to arangosh " << TRIAGENS_VERSION << ". Copyright (c) 2012 triAGENS GmbH" << endl;

    ostringstream info;

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
    
    cout << "Using " << info.str() << endl << endl;

    BaseClient.printWelcomeInfo();

    if (useServer) {
      if (ClientConnection->isConnected()) {
        cout << "Connected to ArangoDB '" << BaseClient.endpointServer()->getSpecification()
             << "' version " << ClientConnection->getVersion() << endl; 
      }
      else {
        cerr << "Could not connect to endpoint '" << BaseClient.endpointString() << "'" << endl;
        if (ClientConnection->getErrorMessage() != "") {
          cerr << "Error message '" << ClientConnection->getErrorMessage() << "'" << endl;
        }
        promptError = true;
      }
    
      cout << endl;
    }
  }

  // .............................................................................
  // read files
  // .............................................................................

  // load java script from js/bootstrap/*.h files
  if (StartupPath.empty()) {
    StartupLoader.defineScript("common/bootstrap/modules.js", JS_common_bootstrap_modules);
    StartupLoader.defineScript("common/bootstrap/monkeypatches.js", JS_common_bootstrap_monkeypatches);
    StartupLoader.defineScript("common/bootstrap/print.js", JS_common_bootstrap_print);
    StartupLoader.defineScript("common/bootstrap/errors.js", JS_common_bootstrap_errors);
    StartupLoader.defineScript("client/client.js", JS_client_client);
  }
  else {
    LOGGER_DEBUG << "using JavaScript startup files at '" << StartupPath << "'";
    StartupLoader.setDirectory(StartupPath);
  }

  context->Global()->Set(v8::String::New("ARANGO_QUIET"), v8::Boolean::New(BaseClient.quiet()), v8::ReadOnly);
  context->Global()->Set(v8::String::New("VALGRIND"), v8::Boolean::New((RUNNING_ON_VALGRIND) > 0));

  // load all init files
  char const* files[] = {
    "common/bootstrap/modules.js",
    "common/bootstrap/monkeypatches.js",
    "common/bootstrap/print.js",
    "common/bootstrap/errors.js",
    "client/client.js"
  };
  
  for (size_t i = 0;  i < sizeof(files) / sizeof(files[0]);  ++i) {
    bool ok = StartupLoader.loadScript(context, files[i]);
    
    if (ok) {
      LOGGER_TRACE << "loaded JavaScript file '" << files[i] << "'";
    }
    else {
      LOGGER_ERROR << "cannot load JavaScript file '" << files[i] << "'";
      exit(EXIT_FAILURE);
    }
  }

  BaseClient.openLog();

  // .............................................................................
  // run normal shell
  // .............................................................................

  if (ExecuteScripts.empty() && CheckScripts.empty() && UnitTests.empty() && JsLint.empty()) {
    RunShell(context, promptError);
  }

  // .............................................................................
  // run unit tests or jslint
  // .............................................................................

  else {
    bool ok = false;

    if (! ExecuteScripts.empty()) {
      // we have scripts to execute or syntax check
      ok = RunScripts(context, ExecuteScripts, true);
    }
    else if (! CheckScripts.empty()) {
      // we have scripts to syntax check
      ok = RunScripts(context, CheckScripts, false);
    }
    else if (! UnitTests.empty()) {
      // we have unit tests
      ok = RunUnitTests(context);
    }
    else if (! JsLint.empty()) {
      // we don't have unittests, but we have files to jslint
      ok = RunJsLint(context);
    }

    if (! ok) {
      ret = EXIT_FAILURE;
    }
  }
  
  // .............................................................................
  // cleanup
  // .............................................................................

  context->Exit();
  context.Dispose();
  
  BaseClient.closeLog();

  // calling dispose in V8 3.10.x causes a segfault. the v8 docs says its not necessary to call it upon program termination
  // v8::V8::Dispose();

  TRIAGENS_REST_SHUTDOWN;

  return ret;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
