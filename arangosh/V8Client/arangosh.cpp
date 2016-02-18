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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include <iostream>

#include <v8.h>
#include <libplatform/libplatform.h>

#include "ArangoShell/ArangoClient.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/Logger.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/files.h"
#include "Basics/init.h"
#include "Basics/messages.h"
#include "Basics/shell-colors.h"
#include "Basics/terminal-utils.h"
#include "Basics/tri-strings.h"
#include "Rest/Endpoint.h"
#include "Rest/HttpResponse.h"
#include "Rest/InitializeRest.h"
#include "Rest/Version.h"
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
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::httpclient;
using namespace arangodb::v8client;

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief command prompt
////////////////////////////////////////////////////////////////////////////////

static std::string Prompt = "arangosh [%d]> ";

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for clients
////////////////////////////////////////////////////////////////////////////////

ArangoClient BaseClient("arangosh");

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection* ClientConnection = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief map of connection objects
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<void*, v8::Persistent<v8::External>> Connections;

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

static std::string StartupModules = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief path for JavaScript files
////////////////////////////////////////////////////////////////////////////////

static std::string StartupPath = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief put current directory into module path
////////////////////////////////////////////////////////////////////////////////

static bool UseCurrentModulePath = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief options to pass to V8
////////////////////////////////////////////////////////////////////////////////

static std::string V8Options;

////////////////////////////////////////////////////////////////////////////////
/// @brief javascript files to execute
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> ExecuteScripts;

////////////////////////////////////////////////////////////////////////////////
/// @brief javascript string to execute
////////////////////////////////////////////////////////////////////////////////

static std::string ExecuteString;

////////////////////////////////////////////////////////////////////////////////
/// @brief javascript files to syntax check
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> CheckScripts;

////////////////////////////////////////////////////////////////////////////////
/// @brief unit file test cases
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> UnitTests;

////////////////////////////////////////////////////////////////////////////////
/// @brief files to jslint
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> JsLint;

////////////////////////////////////////////////////////////////////////////////
/// @brief garbage collection interval
////////////////////////////////////////////////////////////////////////////////

static uint64_t GcInterval = 10;

////////////////////////////////////////////////////////////////////////////////
/// @brief voice mode
////////////////////////////////////////////////////////////////////////////////

static bool VoiceMode = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs the arguments
///
/// @FUN{internal.output(@FA{string1}, @FA{string2}, @FA{string3}, ...)}
///
/// Outputs the arguments to standard output.
///
/// @verbinclude fluent39
////////////////////////////////////////////////////////////////////////////////

static void JS_PagerOutput(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  for (int i = 0; i < args.Length(); i++) {
    // extract the next argument
    v8::Handle<v8::Value> val = args[i];

    std::string str = TRI_ObjectToString(val);

    BaseClient.internalPrint(str);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the output pager
////////////////////////////////////////////////////////////////////////////////

static void JS_StartOutputPager(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (BaseClient.usePager()) {
    BaseClient.internalPrint("Using pager already.\n");
  } else {
    BaseClient.setUsePager(true);
    BaseClient.internalPrint(std::string(std::string("Using pager ") +
                                         BaseClient.outputPager() +
                                         " for output buffering.\n"));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the output pager
////////////////////////////////////////////////////////////////////////////////

static void JS_StopOutputPager(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (BaseClient.usePager()) {
    BaseClient.internalPrint("Stopping pager.\n");
  } else {
    BaseClient.internalPrint("Pager not running.\n");
  }

  BaseClient.setUsePager(false);

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a CSV file
///
/// @FUN{importCsvFile(@FA{filename}, @FA{collection})}
///
/// Imports data of a CSV file. The data is imported to @FA{collection}.
////The separator is @CODE{\,} and the quote is @CODE{"}.
///
/// @FUN{importCsvFile(@FA{filename}, @FA{collection}, @FA{options})}
///
/// Imports data of a CSV file. The data is imported to @FA{collection}.
////The separator is @CODE{\,} and the quote is @CODE{"}.
////////////////////////////////////////////////////////////////////////////////

static void JS_ImportCsvFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "importCsvFile(<filename>, <collection>[, <options>])");
  }

  // extract the filename
  v8::String::Utf8Value filename(args[0]);

  if (*filename == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 filename");
  }

  v8::String::Utf8Value collection(args[1]);

  if (*collection == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<collection> must be a UTF-8 filename");
  }

  // extract the options
  v8::Handle<v8::String> separatorKey = TRI_V8_ASCII_STRING("separator");
  v8::Handle<v8::String> quoteKey = TRI_V8_ASCII_STRING("quote");

  std::string separator = ",";
  std::string quote = "\"";

  if (3 <= args.Length()) {
    v8::Handle<v8::Object> options = args[2]->ToObject();
    // separator
    if (options->Has(separatorKey)) {
      separator = TRI_ObjectToString(options->Get(separatorKey));

      if (separator.length() < 1) {
        TRI_V8_THROW_EXCEPTION_PARAMETER(
            "<options>.separator must be at least one character");
      }
    }

    // quote
    if (options->Has(quoteKey)) {
      quote = TRI_ObjectToString(options->Get(quoteKey));

      if (quote.length() > 1) {
        TRI_V8_THROW_EXCEPTION_PARAMETER(
            "<options>.quote must be at most one character");
      }
    }
  }

  ImportHelper ih(ClientConnection->getHttpClient(), ChunkSize);

  ih.setQuote(quote);
  ih.setSeparator(separator.c_str());

  std::string fileName = TRI_ObjectToString(args[0]);
  std::string collectionName = TRI_ObjectToString(args[1]);

  if (ih.importDelimited(collectionName, fileName, ImportHelper::CSV)) {
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    result->Set(TRI_V8_ASCII_STRING("lines"),
                v8::Integer::New(isolate, (int32_t)ih.getReadLines()));
    result->Set(TRI_V8_ASCII_STRING("created"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberCreated()));
    result->Set(TRI_V8_ASCII_STRING("errors"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberErrors()));
    result->Set(TRI_V8_ASCII_STRING("updated"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberUpdated()));
    result->Set(TRI_V8_ASCII_STRING("ignored"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberIgnored()));
    TRI_V8_RETURN(result);
  }

  TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                 ih.getErrorMessage().c_str());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a JSON file
///
/// @FUN{importJsonFile(@FA{filename}, @FA{collection})}
///
/// Imports data of a CSV file. The data is imported to @FA{collection}.
///
////////////////////////////////////////////////////////////////////////////////

static void JS_ImportJsonFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("importJsonFile(<filename>, <collection>)");
  }

  // extract the filename
  v8::String::Utf8Value filename(args[0]);

  if (*filename == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 filename");
  }

  v8::String::Utf8Value collection(args[1]);

  if (*collection == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<collection> must be a UTF-8 filename");
  }

  ImportHelper ih(ClientConnection->getHttpClient(), ChunkSize);

  std::string fileName = TRI_ObjectToString(args[0]);
  std::string collectionName = TRI_ObjectToString(args[1]);

  if (ih.importJson(collectionName, fileName)) {
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    result->Set(TRI_V8_ASCII_STRING("lines"),
                v8::Integer::New(isolate, (int32_t)ih.getReadLines()));
    result->Set(TRI_V8_ASCII_STRING("created"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberCreated()));
    result->Set(TRI_V8_ASCII_STRING("errors"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberErrors()));
    result->Set(TRI_V8_ASCII_STRING("updated"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberUpdated()));
    result->Set(TRI_V8_ASCII_STRING("ignored"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberIgnored()));
    TRI_V8_RETURN(result);
  }

  TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                 ih.getErrorMessage().c_str());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_NormalizeString(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("NORMALIZE_STRING(<string>)");
  }

  TRI_normalize_V8_Obj(args, args[0]);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_CompareString(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "COMPARE_STRING(<left string>, <right string>)");
  }

  v8::String::Value left(args[0]);
  v8::String::Value right(args[1]);

  int result = Utf8Helper::DefaultUtf8Helper.compareUtf16(
      *left, left.length(), *right, right.length());

  TRI_V8_RETURN(v8::Integer::New(isolate, result));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enum for wrapped V8 objects
////////////////////////////////////////////////////////////////////////////////

enum WRAP_CLASS_TYPES { WRAP_TYPE_CONNECTION = 1 };

typedef enum __eRunMode {
  eInteractive,
  eExecuteScript,
  eExecuteString,
  eCheckScripts,
  eUnitTests,
  eJsLint
} eRunMode;

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> ParseProgramOptions(int argc, char* args[],
                                                    eRunMode* runMode) {
  ProgramOptionsDescription description("STANDARD options");
  ProgramOptionsDescription javascript("JAVASCRIPT options");

  javascript("javascript.execute", &ExecuteScripts,
             "execute Javascript code from file")(
      "javascript.execute-string", &ExecuteString,
      "execute Javascript code from string")(
      "javascript.check", &CheckScripts,
      "syntax check code Javascript code from file")(
      "javascript.gc-interval", &GcInterval,
      "JavaScript request-based garbage collection interval (each x commands)")(
      "javascript.startup-directory", &StartupPath,
      "startup paths containing the JavaScript files")(
      "javascript.unit-tests", &UnitTests,
      "do not start as shell, run unit tests instead")(
      "javascript.current-module-directory", &UseCurrentModulePath,
      "add current directory to module path")(
      "javascript.v8-options", &V8Options, "options to pass to v8")(
      "jslint", &JsLint, "do not start as shell, run jslint instead");

#ifdef _WIN32
  description("code-page", &CodePage, "windows codepage");
#endif

#ifdef __APPLE__
  description("voice", "enable voice based welcome");
#endif

  description("chunk-size", &ChunkSize,
              "maximum size for individual data batches (in bytes)")(
      "prompt", &Prompt, "command prompt")(javascript, false);

  std::vector<std::string> arguments;
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

  std::string conf = TRI_BinaryName(args[0]) + ".conf";

  BaseClient.parse(options, description, "<options>", argc, args, conf);

  // derive other paths from `--javascript.directory`
  StartupModules = StartupPath + TRI_DIR_SEPARATOR_STR + "client" +
                   TRI_DIR_SEPARATOR_STR + "modules;" + StartupPath +
                   TRI_DIR_SEPARATOR_STR + "common" + TRI_DIR_SEPARATOR_STR +
                   "modules;" + StartupPath + TRI_DIR_SEPARATOR_STR + "node";

  if (UseCurrentModulePath) {
    StartupModules += ";" + FileUtils::currentDirectory();
  }

  // turn on paging automatically if "pager" option is set
  if (options.has("pager") && !options.has("use-pager")) {
    BaseClient.setUsePager(true);
  }

  // disable excessive output in non-interactive mode
  if (!ExecuteScripts.empty() || !ExecuteString.empty() ||
      !CheckScripts.empty() || !UnitTests.empty() || !JsLint.empty()) {
    BaseClient.shutup();
  }

  // voice mode
  if (options.has("voice")) {
    VoiceMode = true;
  }

  if (!ExecuteScripts.empty()) {
    *runMode = eExecuteScript;
  } else if (!ExecuteString.empty()) {
    *runMode = eExecuteString;
  } else if (!CheckScripts.empty()) {
    *runMode = eCheckScripts;
  } else if (!UnitTests.empty()) {
    *runMode = eUnitTests;
  } else if (!JsLint.empty()) {
    *runMode = eJsLint;
  }

  // return the positional arguments
  return arguments;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies v8::Object to std::map<std::string, std::string>
////////////////////////////////////////////////////////////////////////////////

static void ObjectToMap(v8::Isolate* isolate,
                        std::map<std::string, std::string>& myMap,
                        v8::Handle<v8::Value> val) {
  v8::Handle<v8::Object> v8Headers = val.As<v8::Object>();

  if (v8Headers->IsObject()) {
    v8::Handle<v8::Array> const props = v8Headers->GetPropertyNames();

    for (uint32_t i = 0; i < props->Length(); i++) {
      v8::Handle<v8::Value> key = props->Get(v8::Integer::New(isolate, i));
      myMap.emplace(TRI_ObjectToString(key),
                    TRI_ObjectToString(v8Headers->Get(key)));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries (call the destructor here)
////////////////////////////////////////////////////////////////////////////////

static void DestroyConnection(V8ClientConnection* connection) {
  TRI_ASSERT(connection != nullptr);

  auto it = Connections.find(connection);

  if (it != Connections.end()) {
    (*it).second.Reset();
    Connections.erase(it);
  }

  delete connection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a new client connection instance
////////////////////////////////////////////////////////////////////////////////

static V8ClientConnection* CreateConnection() {
  return new V8ClientConnection(
      BaseClient.endpointServer(), BaseClient.databaseName(),
      BaseClient.username(), BaseClient.password(), BaseClient.requestTimeout(),
      BaseClient.connectTimeout(), ArangoClient::DEFAULT_RETRIES,
      BaseClient.sslProtocol(), false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries (call the destructor here)
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_DestructorCallback(
    const v8::WeakCallbackData<v8::External, v8::Persistent<v8::External>>&
        data) {
  auto persistent = data.GetParameter();
  auto myConnection =
      v8::Local<v8::External>::New(data.GetIsolate(), *persistent);
  auto connection = static_cast<V8ClientConnection*>(myConnection->Value());

  DestroyConnection(connection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wrap V8ClientConnection in a v8::Object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> WrapV8ClientConnection(
    v8::Isolate* isolate, V8ClientConnection* connection) {
  v8::EscapableHandleScope scope(isolate);
  auto localConnectionTempl =
      v8::Local<v8::ObjectTemplate>::New(isolate, ConnectionTempl);
  v8::Handle<v8::Object> result = localConnectionTempl->NewInstance();

  auto myConnection = v8::External::New(isolate, connection);
  result->SetInternalField(SLOT_CLASS_TYPE,
                           v8::Integer::New(isolate, WRAP_TYPE_CONNECTION));
  result->SetInternalField(SLOT_CLASS, myConnection);
  Connections[connection].Reset(isolate, myConnection);
  Connections[connection].SetWeak(&Connections[connection],
                                  ClientConnection_DestructorCallback);
  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection constructor
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_ConstructorCallback(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() > 0 && args[0]->IsString()) {
    std::string definition = TRI_ObjectToString(args[0]);

    BaseClient.createEndpoint(definition);

    if (BaseClient.endpointServer() == nullptr) {
      std::string errorMessage = "error in '" + definition + "'";
      TRI_V8_THROW_EXCEPTION_PARAMETER(errorMessage.c_str());
    }
  }

  if (BaseClient.endpointServer() == nullptr) {
    TRI_V8_RETURN_UNDEFINED();
  }

  V8ClientConnection* connection = CreateConnection();

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (connection->isConnected() &&
      connection->getLastHttpReturnCode() == HttpResponse::OK) {
    ostringstream s;
    s << "Connected to ArangoDB '"
      << BaseClient.endpointServer()->getSpecification() << "', version "
      << connection->getVersion() << " [" << connection->getMode()
      << "], database '" << BaseClient.databaseName() << "', username: '"
      << BaseClient.username() << "'";
    BaseClient.printLine(s.str());
  } else {
    std::string errorMessage =
        "Could not connect. Error message: " + connection->getErrorMessage();
    delete connection;
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                   errorMessage.c_str());
  }

  TRI_V8_RETURN(WrapV8ClientConnection(isolate, connection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "reconnect"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_reconnect(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "reconnect(<endpoint>, <database>, [, <username>, <password>])");
  }

  std::string const definition = TRI_ObjectToString(args[0]);
  std::string databaseName = TRI_ObjectToString(args[1]);

  std::string username;
  if (args.Length() < 3) {
    username = BaseClient.username();
  } else {
    username = TRI_ObjectToString(args[2]);
  }

  std::string password;
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
  } else {
    password = TRI_ObjectToString(args[3]);
  }

  std::string const oldDefinition = BaseClient.endpointString();
  std::string const oldDatabaseName = BaseClient.databaseName();
  std::string const oldUsername = BaseClient.username();
  std::string const oldPassword = BaseClient.password();

  DestroyConnection(connection);
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

    std::string errorMessage = "error in '" + definition + "'";
    TRI_V8_THROW_EXCEPTION_PARAMETER(errorMessage.c_str());
  }

  V8ClientConnection* newConnection = CreateConnection();

  if (newConnection->isConnected() &&
      newConnection->getLastHttpReturnCode() == HttpResponse::OK) {
    ostringstream s;
    s << "Connected to ArangoDB '"
      << BaseClient.endpointServer()->getSpecification()
      << "' version: " << newConnection->getVersion() << " ["
      << newConnection->getMode() << "], database: '"
      << BaseClient.databaseName() << "', username: '" << BaseClient.username()
      << "'";

    BaseClient.printLine(s.str());

    args.Holder()->SetInternalField(SLOT_CLASS,
                                    v8::External::New(isolate, newConnection));

    v8::Handle<v8::Value> db =
        isolate->GetCurrentContext()->Global()->Get(TRI_V8_ASCII_STRING("db"));
    if (db->IsObject()) {
      v8::Handle<v8::Object> dbObj = v8::Handle<v8::Object>::Cast(db);

      if (dbObj->Has(TRI_V8_ASCII_STRING("_flushCache")) &&
          dbObj->Get(TRI_V8_ASCII_STRING("_flushCache"))->IsFunction()) {
        v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(
            dbObj->Get(TRI_V8_ASCII_STRING("_flushCache")));

        v8::Handle<v8::Value>* args = nullptr;
        func->Call(dbObj, 0, args);
      }
    }

    ClientConnection = newConnection;

    // ok
    TRI_V8_RETURN_TRUE();
  } else {
    ostringstream s;
    s << "Could not connect to endpoint '" << BaseClient.endpointString()
      << "', username: '" << BaseClient.username() << "'";
    BaseClient.printErrLine(s.str());

    std::string errorMsg = "could not connect";
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
    args.Holder()->SetInternalField(
        SLOT_CLASS, v8::External::New(isolate, ClientConnection));

    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                   errorMsg.c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGetAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 1) {
    ObjectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(connection->getData(isolate, *url, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGet(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpGetAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGetRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpGetAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHeadAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("head(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);

  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 1) {
    ObjectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(connection->headData(isolate, *url, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHead(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpHeadAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHeadRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpHeadAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDeleteAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("delete(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);

  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 1) {
    ObjectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(connection->deleteData(isolate, *url, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDelete(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpDeleteAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDeleteRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpDeleteAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptionsAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("options(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  v8::String::Utf8Value body(args[1]);

  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(
      connection->optionsData(isolate, *url, *body, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptions(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpOptionsAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptionsRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpOptionsAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPostAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("post(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  v8::String::Utf8Value body(args[1]);

  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(connection->postData(isolate, *url, *body, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPost(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPostAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPostRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPostAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPutAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("put(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  v8::String::Utf8Value body(args[1]);

  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(connection->putData(isolate, *url, *body, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPut(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPutAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPutRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPutAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatchAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("patch(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  v8::String::Utf8Value body(args[1]);

  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(connection->patchData(isolate, *url, *body, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatch(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPatchAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatchRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPatchAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection send file helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpSendFile(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("sendFile(<url>, <file>)");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);

  std::string const infile = TRI_ObjectToString(args[1]);

  if (!TRI_ExistsFile(infile.c_str())) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FILE_NOT_FOUND);
  }

  size_t bodySize;
  char* body = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, infile.c_str(), &bodySize);

  if (body == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), "could not read file");
  }

  v8::TryCatch tryCatch;

  // check header fields
  std::map<std::string, std::string> headerFields;

  v8::Handle<v8::Value> result =
      connection->postData(isolate, *url, body, bodySize, headerFields);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, body);

  if (tryCatch.HasCaught()) {
    // string exception = TRI_StringifyV8Exception(isolate, &tryCatch);
    isolate->ThrowException(tryCatch.Exception());
    return;
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getEndpoint"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getEndpoint(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getEndpoint()");
  }

  std::string const endpoint = BaseClient.endpointString();
  TRI_V8_RETURN_STD_STRING(endpoint);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "lastError"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_lastHttpReturnCode(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

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

static void ClientConnection_lastErrorMessage(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

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

static void ClientConnection_isConnected(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("isConnected()");
  }

  if (connection->isConnected()) {
    TRI_V8_RETURN_TRUE();
  } else {
    TRI_V8_RETURN_FALSE();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "isConnected"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_toString(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("toString()");
  }

  std::string result = "[object ArangoConnection:" +
                       BaseClient.endpointServer()->getSpecification();

  if (connection->isConnected()) {
    result += "," + connection->getVersion() + ",connected]";
  } else {
    result += ",unconnected]";
  }

  TRI_V8_RETURN_STD_STRING(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getVersion"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getVersion(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getVersion()");
  }

  TRI_V8_RETURN_STD_STRING(connection->getVersion());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getMode"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getMode(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getMode()");
  }

  TRI_V8_RETURN_STD_STRING(connection->getMode());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getDatabaseName"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getDatabaseName(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

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

static void ClientConnection_setDatabaseName(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("setDatabaseName(<name>)");
  }

  std::string const dbName = TRI_ObjectToString(args[0]);
  connection->setDatabaseName(dbName);
  BaseClient.setDatabaseName(dbName);

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dynamically replace %d, %e, %u in the prompt
////////////////////////////////////////////////////////////////////////////////

static std::string BuildPrompt() {
  std::string result;

  char const* p = Prompt.c_str();
  bool esc = false;

  while (true) {
    char const c = *p;

    if (c == '\0') {
      break;
    }

    if (esc) {
      if (c == '%') {
        result.push_back(c);
      } else if (c == 'd') {
        result.append(BaseClient.databaseName());
      } else if (c == 'e' || c == 'E') {
        std::string ep;

        if (ClientConnection == nullptr) {
          ep = "none";
        } else {
          ep = BaseClient.endpointString();
        }

        if (c == 'E') {
          // replace protocol
          if (ep.find("tcp://") == 0) {
            ep = ep.substr(6);
          } else if (ep.find("ssl://") == 0) {
            ep = ep.substr(6);
          } else if (ep.find("unix://") == 0) {
            ep = ep.substr(7);
          }
        }

        result.append(ep);
      } else if (c == 'u') {
        result.append(BaseClient.username());
      }

      esc = false;
    } else {
      if (c == '%') {
        esc = true;
      } else {
        result.push_back(c);
      }
    }

    ++p;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the shell
////////////////////////////////////////////////////////////////////////////////

static int RunShell(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                    bool promptError) {
  v8::Context::Scope contextScope(context);
  v8::Local<v8::String> name(TRI_V8_ASCII_STRING(TRI_V8_SHELL_COMMAND_NAME));

  auto cc = ClientConnection;

  V8LineEditor console(isolate, context, ".arangosh.history");
  console.setSignalFunction([&cc]() {
    if (cc != nullptr) {
      cc->setInterrupted(true);
    }
  });

  console.open(BaseClient.autoComplete());

  uint64_t nrCommands = 0;

#ifdef __APPLE__
  if (VoiceMode) {
    system("say -v zarvox 'welcome to Arango shell' &");
  }
#endif

  while (true) {
    // set up prompts
    std::string dynamicPrompt = BuildPrompt();

    std::string goodPrompt;
    std::string badPrompt;

#if _WIN32

    // ........................................................................................
    // Windows console is not coloured by escape sequences. So the method given
    // below will not
    // work. For now we simply ignore the colours until we move the windows
    // version into
    // a GUI Window.
    // ........................................................................................

    goodPrompt = badPrompt = dynamicPrompt;

#else

    if (BaseClient.colors() && console.supportsColors()) {
      // TODO: this should be a function defined in "console"
      goodPrompt =
          TRI_SHELL_COLOR_BOLD_GREEN + dynamicPrompt + TRI_SHELL_COLOR_RESET;
      badPrompt =
          TRI_SHELL_COLOR_BOLD_RED + dynamicPrompt + TRI_SHELL_COLOR_RESET;
    } else {
      goodPrompt = badPrompt = dynamicPrompt;
    }

#endif

    // gc
    if (++nrCommands >= GcInterval) {
      nrCommands = 0;

      TRI_RunGarbageCollectionV8(isolate, 500.0);
    }

#ifdef __APPLE__
    if (VoiceMode && promptError) {
      system("say -v 'whisper' 'oh, no' &");
    }
#endif

    bool eof;
    std::string input = console.prompt(
        promptError ? badPrompt.c_str() : goodPrompt.c_str(), "arangosh", eof);

    if (eof) {
      break;
    }

    if (input.empty()) {
      // input string is empty, but we must still free it
      continue;
    }

    BaseClient.log("%s%s\n", dynamicPrompt, input);

    std::string i = arangodb::basics::StringUtils::trim(input);

    if (i == "exit" || i == "quit" || i == "exit;" || i == "quit;") {
      break;
    }

    if (i == "help" || i == "help;") {
      input = "help()";
    }

    console.addHistory(input);

    v8::TryCatch tryCatch;

    BaseClient.startPager();

    // assume the command succeeds
    promptError = false;

    console.setExecutingCommand(true);

    // execute command and register its result in __LAST__
    v8::Handle<v8::Value> v = TRI_ExecuteJavaScriptString(
        isolate, context, TRI_V8_STRING(input.c_str()), name, true);

    console.setExecutingCommand(false);

    if (v.IsEmpty()) {
      context->Global()->Set(TRI_V8_ASCII_STRING("_last"),
                             v8::Undefined(isolate));
    } else {
      context->Global()->Set(TRI_V8_ASCII_STRING("_last"), v);
    }

    if (tryCatch.HasCaught()) {
      // command failed
      std::string exception;

      if (!tryCatch.CanContinue() || tryCatch.HasTerminated()) {
        exception = "command locally aborted\n";
      } else {
        exception = TRI_StringifyV8Exception(isolate, &tryCatch);
      }

      BaseClient.printErrLine(exception);
      BaseClient.log("%s", exception.c_str());

      // this will change the prompt for the next round
      promptError = true;
    }

    if (ClientConnection) {
      ClientConnection->setInterrupted(false);
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

  BaseClient.printLine("");

  BaseClient.printByeBye();

  return promptError ? TRI_ERROR_INTERNAL : TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the unit tests
////////////////////////////////////////////////////////////////////////////////

static bool RunUnitTests(v8::Isolate* isolate,
                         v8::Handle<v8::Context> context) {
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);

  bool ok;

  // set-up unit tests array
  v8::Handle<v8::Array> sysTestFiles = v8::Array::New(isolate);

  for (size_t i = 0; i < UnitTests.size(); ++i) {
    sysTestFiles->Set((uint32_t)i, TRI_V8_STD_STRING(UnitTests[i]));
  }

  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("SYS_UNIT_TESTS"), sysTestFiles);
  // do not use TRI_AddGlobalVariableVocBase because it creates read-only
  // variables!!
  context->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT"),
                         v8::True(isolate));

  // run tests
  auto input = TRI_V8_ASCII_STRING(
      "require(\"@arangodb/testrunner\").runCommandLineTests();");
  auto name = TRI_V8_ASCII_STRING(TRI_V8_SHELL_COMMAND_NAME);
  TRI_ExecuteJavaScriptString(isolate, context, input, name, true);

  if (tryCatch.HasCaught()) {
    std::string exception(TRI_StringifyV8Exception(isolate, &tryCatch));
    BaseClient.printErrLine(exception);
    ok = false;
  } else {
    ok = TRI_ObjectToBoolean(
        context->Global()->Get(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT")));
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the Javascript files
////////////////////////////////////////////////////////////////////////////////

static bool RunScripts(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                       std::vector<std::string> const& scripts, bool execute) {
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);

  bool ok = true;

  for (size_t i = 0; i < scripts.size(); ++i) {
    if (!FileUtils::exists(scripts[i])) {
      std::string msg =
          "error: Javascript file not found: '" + scripts[i] + "'";

      BaseClient.printErrLine(msg.c_str());
      BaseClient.log("%s", msg.c_str());

      ok = false;
      break;
    }

    if (execute) {
      v8::Handle<v8::String> name = TRI_V8_STD_STRING(scripts[i]);
      v8::Handle<v8::Value> args[] = {name};
      v8::Handle<v8::Value> filename = args[0];

      v8::Handle<v8::Object> current = isolate->GetCurrentContext()->Global();
      auto oldFilename = current->Get(TRI_V8_ASCII_STRING("__filename"));
      current->ForceSet(TRI_V8_ASCII_STRING("__filename"), filename);

      auto oldDirname = current->Get(TRI_V8_ASCII_STRING("__dirname"));
      auto dirname = TRI_Dirname(TRI_ObjectToString(filename).c_str());
      current->ForceSet(TRI_V8_ASCII_STRING("__dirname"),
                        TRI_V8_STRING(dirname));
      TRI_FreeString(TRI_CORE_MEM_ZONE, dirname);

      ok = TRI_ExecuteGlobalJavaScriptFile(isolate, scripts[i].c_str());

      // restore old values for __dirname and __filename
      if (oldFilename.IsEmpty() || oldFilename->IsUndefined()) {
        current->Delete(TRI_V8_ASCII_STRING("__filename"));
      } else {
        current->ForceSet(TRI_V8_ASCII_STRING("__filename"), oldFilename);
      }
      if (oldDirname.IsEmpty() || oldDirname->IsUndefined()) {
        current->Delete(TRI_V8_ASCII_STRING("__dirname"));
      } else {
        current->ForceSet(TRI_V8_ASCII_STRING("__dirname"), oldDirname);
      }
    } else {
      TRI_ParseJavaScriptFile(isolate, scripts[i].c_str());
    }

    if (tryCatch.HasCaught()) {
      std::string exception(TRI_StringifyV8Exception(isolate, &tryCatch));

      BaseClient.printErrLine(exception);
      BaseClient.log("%s\n", exception.c_str());

      ok = false;
      break;
    }
    if (!ok) {
      break;
    }
  }

  BaseClient.flushLog();

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the Javascript string
////////////////////////////////////////////////////////////////////////////////

static bool RunString(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                      std::string const& script) {
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);
  bool ok = true;

  v8::Handle<v8::Value> result =
      TRI_ExecuteJavaScriptString(isolate, context, TRI_V8_STD_STRING(script),
                                  TRI_V8_ASCII_STRING("(command-line)"), false);

  if (tryCatch.HasCaught()) {
    std::string exception(TRI_StringifyV8Exception(isolate, &tryCatch));

    BaseClient.printErrLine(exception);
    BaseClient.log("%s\n", exception.c_str());
    ok = false;
  } else {
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

static bool RunJsLint(v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);
  bool ok;

  // set-up jslint files array
  v8::Handle<v8::Array> sysTestFiles = v8::Array::New(isolate);

  for (size_t i = 0; i < JsLint.size(); ++i) {
    sysTestFiles->Set((uint32_t)i, TRI_V8_STD_STRING(JsLint[i]));
  }

  context->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS"), sysTestFiles);
  context->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT"),
                         v8::True(isolate));

  // run tests
  auto input =
      TRI_V8_ASCII_STRING("require(\"jslint\").runCommandLineTests({ });");
  auto name = TRI_V8_ASCII_STRING(TRI_V8_SHELL_COMMAND_NAME);
  TRI_ExecuteJavaScriptString(isolate, context, input, name, true);

  if (tryCatch.HasCaught()) {
    BaseClient.printErrLine(TRI_StringifyV8Exception(isolate, &tryCatch));
    ok = false;
  } else {
    ok = TRI_ObjectToBoolean(
        context->Global()->Get(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT")));
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief startup and exit functions
////////////////////////////////////////////////////////////////////////////////

void* arangoshResourcesAllocated = nullptr;
static void LocalEntryFunction();
static void LocalExitFunction(int, void*);

#ifdef _WIN32

// .............................................................................
// Call this function to do various initializations for windows only
//
// TODO can we move this to a general function for all binaries?
// .............................................................................

void LocalEntryFunction() {
  int maxOpenFiles = 1024;
  int res = 0;

  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  // res = initializeWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initializeWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);

  if (res != 0) {
    _exit(1);
  }

  res = initializeWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,
                          (char const*)(&maxOpenFiles));

  if (res != 0) {
    _exit(1);
  }

  res = initializeWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    _exit(1);
  }

  TRI_Application_Exit_SetExit(LocalExitFunction);
}

static void LocalExitFunction(int exitCode, void* data) {
  int res = finalizeWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(1);
  }

  exit(exitCode);
}
#else

static void LocalEntryFunction() {}

static void LocalExitFunction(int exitCode, void* data) {}

#endif

static bool PrintHelo(bool useServer) {
  bool promptError = false;

  // .............................................................................
  // banner
  // .............................................................................

  // http://www.network-science.de/ascii/   Font: ogre

  if (!BaseClient.quiet()) {
#ifdef _WIN32

    // .............................................................................
    // Quick hack for windows
    // .............................................................................

    if (BaseClient.colors()) {
      int greenColour = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
      int redColour = FOREGROUND_RED | FOREGROUND_INTENSITY;
      int defaultColour = 0;
      CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

      if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),
                                     &csbiInfo) != 0) {
        defaultColour = csbiInfo.wAttributes;
      }

      // not sure about the code page. let user set code page by command-line
      // argument if required
      if (CodePage > 0) {
        SetConsoleOutputCP((UINT)CodePage);
      } else {
        UINT cp = GetConsoleOutputCP();
        SetConsoleOutputCP(cp);
      }

      // TODO we should have a special "printf" which can handle the color
      // escape sequences!
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

    if (!BaseClient.colors()) {
      g = "";
      r = "";
      z = "";
    }

    BaseClient.printLine("");

    printf("%s                                  %s     _     %s\n", g, r, z);
    printf("%s  __ _ _ __ __ _ _ __   __ _  ___ %s ___| |__  %s\n", g, r, z);
    printf("%s / _` | '__/ _` | '_ \\ / _` |/ _ \\%s/ __| '_ \\ %s\n", g, r, z);
    printf("%s| (_| | | | (_| | | | | (_| | (_) %s\\__ \\ | | |%s\n", g, r, z);
    printf("%s \\__,_|_|  \\__,_|_| |_|\\__, |\\___/%s|___/_| |_|%s\n", g, r,
           z);
    printf("%s                       |___/      %s           %s\n", g, r, z);

#endif
    BaseClient.printLine("");

    ostringstream s;
    s << "arangosh (" << arangodb::rest::Version::getVerboseVersionString()
      << ")" << std::endl;
    s << "Copyright (c) ArangoDB GmbH";

    BaseClient.printLine(s.str(), true);
    BaseClient.printLine("", true);

    BaseClient.printWelcomeInfo();

    if (useServer) {
      if (ClientConnection && ClientConnection->isConnected() &&
          ClientConnection->getLastHttpReturnCode() == HttpResponse::OK) {
        ostringstream is;
        is << "Connected to ArangoDB '" << BaseClient.endpointString()
           << "' version: " << ClientConnection->getVersion() << " ["
           << ClientConnection->getMode() << "], database: '"
           << BaseClient.databaseName() << "', username: '"
           << BaseClient.username() << "'";

        BaseClient.printLine(is.str(), true);
      } else {
        ostringstream is;
        is << "Could not connect to endpoint '" << BaseClient.endpointString()
           << "', database: '" << BaseClient.databaseName() << "', username: '"
           << BaseClient.username() << "'";
        BaseClient.printErrLine(is.str());

        if (ClientConnection && ClientConnection->getErrorMessage() != "") {
          ostringstream is2;
          is2 << "Error message '" << ClientConnection->getErrorMessage()
              << "'";
          BaseClient.printErrLine(is2.str());
        }
        promptError = true;
      }

      BaseClient.printLine("", true);
    }
  }

  return promptError;
}

static void InitCallbacks(v8::Isolate* isolate, bool useServer,
                          eRunMode runMode) {
  auto context = isolate->GetCurrentContext();
  // set pretty print default: (used in print.js)
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("PRETTY_PRINT"),
      v8::Boolean::New(isolate, BaseClient.prettyPrint()));

  // add colors for print.js
  TRI_AddGlobalVariableVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("COLOR_OUTPUT"),
                               v8::Boolean::New(isolate, BaseClient.colors()));

  // add function SYS_OUTPUT to use pager
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("SYS_OUTPUT"),
      v8::FunctionTemplate::New(isolate, JS_PagerOutput)->GetFunction());

  TRI_InitV8Buffer(isolate, context);

  TRI_InitV8Utils(isolate, context, StartupPath, StartupModules);
  TRI_InitV8Shell(isolate, context);

  // .............................................................................
  // define ArangoConnection class
  // .............................................................................

  if (useServer) {
    v8::Handle<v8::FunctionTemplate> connection_templ =
        v8::FunctionTemplate::New(isolate);
    connection_templ->SetClassName(TRI_V8_ASCII_STRING("ArangoConnection"));

    v8::Handle<v8::ObjectTemplate> connection_proto =
        connection_templ->PrototypeTemplate();

    connection_proto->Set(
        isolate, "DELETE",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpDelete));
    connection_proto->Set(
        isolate, "DELETE_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpDeleteRaw));
    connection_proto->Set(
        isolate, "GET",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpGet));
    connection_proto->Set(
        isolate, "GET_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpGetRaw));
    connection_proto->Set(
        isolate, "HEAD",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpHead));
    connection_proto->Set(
        isolate, "HEAD_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpHeadRaw));
    connection_proto->Set(
        isolate, "OPTIONS",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpOptions));
    connection_proto->Set(
        isolate, "OPTIONS_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpOptionsRaw));
    connection_proto->Set(
        isolate, "PATCH",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpPatch));
    connection_proto->Set(
        isolate, "PATCH_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpPatchRaw));
    connection_proto->Set(
        isolate, "POST",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpPost));
    connection_proto->Set(
        isolate, "POST_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpPostRaw));
    connection_proto->Set(
        isolate, "PUT",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpPut));
    connection_proto->Set(
        isolate, "PUT_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpPutRaw));
    connection_proto->Set(
        isolate, "SEND_FILE",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpSendFile));
    connection_proto->Set(
        isolate, "getEndpoint",
        v8::FunctionTemplate::New(isolate, ClientConnection_getEndpoint));
    connection_proto->Set(isolate, "lastHttpReturnCode",
                          v8::FunctionTemplate::New(
                              isolate, ClientConnection_lastHttpReturnCode));
    connection_proto->Set(
        isolate, "lastErrorMessage",
        v8::FunctionTemplate::New(isolate, ClientConnection_lastErrorMessage));
    connection_proto->Set(
        isolate, "isConnected",
        v8::FunctionTemplate::New(isolate, ClientConnection_isConnected));
    connection_proto->Set(
        isolate, "reconnect",
        v8::FunctionTemplate::New(isolate, ClientConnection_reconnect));
    connection_proto->Set(
        isolate, "toString",
        v8::FunctionTemplate::New(isolate, ClientConnection_toString));
    connection_proto->Set(
        isolate, "getVersion",
        v8::FunctionTemplate::New(isolate, ClientConnection_getVersion));
    connection_proto->Set(
        isolate, "getMode",
        v8::FunctionTemplate::New(isolate, ClientConnection_getMode));
    connection_proto->Set(
        isolate, "getDatabaseName",
        v8::FunctionTemplate::New(isolate, ClientConnection_getDatabaseName));
    connection_proto->Set(
        isolate, "setDatabaseName",
        v8::FunctionTemplate::New(isolate, ClientConnection_setDatabaseName));
    connection_proto->SetCallAsFunctionHandler(
        ClientConnection_ConstructorCallback);

    v8::Handle<v8::ObjectTemplate> connection_inst =
        connection_templ->InstanceTemplate();
    connection_inst->SetInternalFieldCount(2);

    TRI_AddGlobalVariableVocbase(isolate, context,
                                 TRI_V8_ASCII_STRING("ArangoConnection"),
                                 connection_proto->NewInstance());
    ConnectionTempl.Reset(isolate, connection_inst);

    // add the client connection to the context:
    TRI_AddGlobalVariableVocbase(
        isolate, context, TRI_V8_ASCII_STRING("SYS_ARANGO"),
        WrapV8ClientConnection(isolate, ClientConnection));
  }

  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("SYS_START_PAGER"),
      v8::FunctionTemplate::New(isolate, JS_StartOutputPager)->GetFunction());
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("SYS_STOP_PAGER"),
      v8::FunctionTemplate::New(isolate, JS_StopOutputPager)->GetFunction());
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("SYS_IMPORT_CSV_FILE"),
      v8::FunctionTemplate::New(isolate, JS_ImportCsvFile)->GetFunction());
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("SYS_IMPORT_JSON_FILE"),
      v8::FunctionTemplate::New(isolate, JS_ImportJsonFile)->GetFunction());
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("NORMALIZE_STRING"),
      v8::FunctionTemplate::New(isolate, JS_NormalizeString)->GetFunction());
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("COMPARE_STRING"),
      v8::FunctionTemplate::New(isolate, JS_CompareString)->GetFunction());

  TRI_AddGlobalVariableVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ARANGO_QUIET"),
                               v8::Boolean::New(isolate, BaseClient.quiet()));
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("VALGRIND"),
      v8::Boolean::New(isolate, (RUNNING_ON_VALGRIND > 0)));

  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("IS_EXECUTE_SCRIPT"),
      v8::Boolean::New(isolate, runMode == eExecuteScript));
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("IS_EXECUTE_STRING"),
      v8::Boolean::New(isolate, runMode == eExecuteString));
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("IS_CHECK_SCRIPT"),
      v8::Boolean::New(isolate, runMode == eCheckScripts));
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("IS_UNIT_TESTS"),
      v8::Boolean::New(isolate, runMode == eUnitTests));
  TRI_AddGlobalVariableVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("IS_JS_LINT"),
                               v8::Boolean::New(isolate, runMode == eJsLint));
}

static int WarmupEnvironment(v8::Isolate* isolate,
                             std::vector<std::string>& positionals,
                             eRunMode runMode) {
  auto context = isolate->GetCurrentContext();
  // .............................................................................
  // read files
  // .............................................................................

  // load java script from js/bootstrap/*.h files
  if (StartupPath.empty()) {
    LOG(FATAL) << "no 'javascript.startup-directory' has been supplied, giving up"; FATAL_ERROR_EXIT();
  }

  LOG(DEBUG) << "using JavaScript startup files at '" << StartupPath << "'";

  StartupLoader.setDirectory(StartupPath);

  // load all init files
  std::vector<std::string> files;

  files.push_back("common/bootstrap/scaffolding.js");
  files.push_back("common/bootstrap/modules/internal.js");  // deps: -
  files.push_back("common/bootstrap/errors.js");            // deps: internal
  files.push_back("client/bootstrap/modules/internal.js");  // deps: internal
  files.push_back("common/bootstrap/modules/vm.js");        // deps: internal
  files.push_back("common/bootstrap/modules/console.js");   // deps: internal
  files.push_back("common/bootstrap/modules/assert.js");    // deps: -
  files.push_back("common/bootstrap/modules/buffer.js");    // deps: internal
  files.push_back(
      "common/bootstrap/modules/fs.js");  // deps: internal, buffer (hidden)
  files.push_back("common/bootstrap/modules/path.js");     // deps: internal, fs
  files.push_back("common/bootstrap/modules/events.js");   // deps: -
  files.push_back("common/bootstrap/modules/process.js");  // deps: internal,
                                                           // fs, events,
                                                           // console
  files.push_back(
      "common/bootstrap/modules.js");  // must come last before patches

  if (runMode != eJsLint) {
    files.push_back("common/bootstrap/monkeypatches.js");
  }

  files.push_back("client/client.js");  // needs internal

  for (size_t i = 0; i < files.size(); ++i) {
    switch (StartupLoader.loadScript(isolate, context, files[i])) {
      case JSLoader::eSuccess:
        LOG(TRACE) << "loaded JavaScript file '" << files[i] << "'";
        break;
      case JSLoader::eFailLoad:
        LOG(FATAL) << "cannot load JavaScript file '" << files[i].c_str() << "'"; FATAL_ERROR_EXIT();
        break;
      case JSLoader::eFailExecute:
        LOG(FATAL) << "error during execution of JavaScript file '" << files[i].c_str() << "'"; FATAL_ERROR_EXIT();
        break;
    }
  }

  // .............................................................................
  // create arguments
  // .............................................................................

  v8::Handle<v8::Array> p = v8::Array::New(isolate, (int)positionals.size());

  for (uint32_t i = 0; i < positionals.size(); ++i) {
    p->Set(i, TRI_V8_STD_STRING(positionals[i]));
  }

  TRI_AddGlobalVariableVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ARGUMENTS"), p);
  return EXIT_SUCCESS;
}

static int Run(v8::Isolate* isolate, eRunMode runMode, bool promptError) {
  auto context = isolate->GetCurrentContext();
  bool ok = false;
  try {
    switch (runMode) {
      case eInteractive:
        ok = RunShell(isolate, context, promptError) == TRI_ERROR_NO_ERROR;
        if (isatty(STDIN_FILENO)) {
          ok = true;
        }
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
  } catch (std::exception const& ex) {
    cerr << "caught exception " << ex.what() << endl;
    ok = false;
  } catch (...) {
    cerr << "caught unknown exception" << endl;
    ok = false;
  }
  return (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
}

class BufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* data = AllocateUninitialized(length);
    if (data != nullptr) {
      memset(data, 0, length);
    }
    return data;
  }
  virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
  virtual void Free(void* data, size_t) {
    if (data != nullptr) {
      free(data);
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* args[]) {
  int ret = EXIT_SUCCESS;
  eRunMode runMode = eInteractive;
#if _WIN32
  extern bool cygwinShell;
  if (getenv("SHELL") != nullptr) {
    cygwinShell = true;
  }
  if (!TRI_InitWindowsEventLog()) {
    std::cerr << "failed to init event log" << std::endl;
    return EXIT_FAILURE;
  }
#endif
  LocalEntryFunction();

  TRIAGENS_C_INITIALIZE(argc, args);
  TRIAGENS_REST_INITIALIZE(argc, args);

  Logger::initialize(false);

  {
    std::ostringstream foxxManagerHelp;
    foxxManagerHelp
        << "Use  " << args[0]
        << " help  to get an overview of the actions specific to foxx-manager."
        << endl
        << endl;
    foxxManagerHelp << "There is also an online manual available at:" << endl
                    << "https://docs.arangodb.com/Foxx/Install/" << endl
                    << endl;

    BaseClient.setupSpecificHelp("foxx-manager", foxxManagerHelp.str());
  }

  BaseClient.setEndpointString(Endpoint::getDefaultEndpoint());

  // .............................................................................
  // parse the program options
  // .............................................................................

  std::vector<std::string> positionals =
      ParseProgramOptions(argc, args, &runMode);

  // .............................................................................
  // set-up client connection
  // .............................................................................

  // check if we want to connect to a server
  bool useServer = (BaseClient.endpointString() != "none");

  // if we are in jslint mode, we will not need the server at all
  if (!JsLint.empty()) {
    useServer = false;
  }

  if (useServer) {
    BaseClient.createEndpoint();

    if (BaseClient.endpointServer() == nullptr) {
      ostringstream s;
      s << "invalid value for --server.endpoint ('"
        << BaseClient.endpointString() << "')";

      BaseClient.printErrLine(s.str());

      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }

    ClientConnection = CreateConnection();
  }

  // .............................................................................
  // set-up V8 objects
  // .............................................................................

  if (!Utf8Helper::DefaultUtf8Helper.setCollatorLanguage("en")) {
    std::string msg =
        "cannot initialize ICU; please make sure ICU*dat is available ; "
        "ICU_DATA='";
    if (getenv("ICU_DATA") != nullptr) {
      msg += getenv("ICU_DATA");
    }
    msg += "'";
    BaseClient.printErrLine(msg);
    return EXIT_FAILURE;
  }
  v8::V8::InitializeICU();

  // set V8 options
  if (!V8Options.empty()) {
    // explicit option --javascript.v8-options used
    v8::V8::SetFlagsFromString(V8Options.c_str(), (int)V8Options.size());
  } else {
    // no explicit option used, now pass all command-line arguments to v8
    v8::V8::SetFlagsFromCommandLine(&argc, args, true);
  }

#ifdef TRI_FORCE_ARMV6
  std::string const forceARMv6 = "--noenable-armv7";
  v8::V8::SetFlagsFromString(forceARMv6.c_str(), (int)forceARMv6.size());
#endif

  v8::Platform* platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platform);
  v8::V8::Initialize();

  BufferAllocator bufferAllocator;
  v8::V8::SetArrayBufferAllocator(&bufferAllocator);

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

      v8::Handle<v8::Object> globalObj = localContext->Global();
      globalObj->Set(TRI_V8_ASCII_STRING("GLOBAL"), globalObj);
      globalObj->Set(TRI_V8_ASCII_STRING("global"), globalObj);
      globalObj->Set(TRI_V8_ASCII_STRING("root"), globalObj);

      InitCallbacks(isolate, useServer, runMode);

      // reset the prompt error flag (will determine prompt colors)
      bool promptError = PrintHelo(useServer);

      ret = WarmupEnvironment(isolate, positionals, runMode);

      if (ret == EXIT_SUCCESS) {
        BaseClient.openLog();

        try {
          ret = Run(isolate, runMode, promptError);
        } catch (std::bad_alloc const&) {
          LOG(ERR) << "caught exception " << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
          ret = EXIT_FAILURE;
        } catch (...) {
          LOG(ERR) << "caught unknown exception";
          ret = EXIT_FAILURE;
        }
      }

      isolate->LowMemoryNotification();

      // spend at least 3 seconds in GC
      LOG(DEBUG) << "entering final garbage collection";
      TRI_RunGarbageCollectionV8(isolate, 3000);
      LOG(DEBUG) << "final garbage collection completed";

      localContext->Exit();
      context.Reset();
    }
  }

  if (ClientConnection != nullptr) {
    DestroyConnection(ClientConnection);
    ClientConnection = nullptr;
  }

  TRI_v8_global_t* v8g = TRI_GetV8Globals(isolate);
  delete v8g;

  isolate->Exit();
  isolate->Dispose();

  BaseClient.closeLog();

  TRIAGENS_REST_SHUTDOWN;

  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  delete platform;
  LocalExitFunction(ret, nullptr);

  return ret;
}
