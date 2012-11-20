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

#include "BasicsC/csv.h"

#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "BasicsC/files.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "Logger/Logger.h"
#include "Rest/AddressPort.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "V8/JSLoader.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8Client/V8ClientConnection.h"
#include "Variant/VariantArray.h"
#include "Variant/VariantBoolean.h"
#include "Variant/VariantInt64.h"
#include "Variant/VariantString.h"

#include "ImportHelper.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::httpclient;
using namespace triagens::v8client;
using namespace triagens::arango;

#include "js/common/bootstrap/js-print.h"
#include "js/common/bootstrap/js-modules.h"
#include "js/common/bootstrap/js-errors.h"
#include "js/common/bootstrap/js-monkeypatches.h"
#include "js/client/js-client.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief connection default values
////////////////////////////////////////////////////////////////////////////////

static string const  DEFAULT_SERVER_NAME = "127.0.0.1";
static int const     DEFAULT_SERVER_PORT = 8529;
static int64_t const DEFAULT_REQUEST_TIMEOUT = 300;
static size_t const  DEFAULT_RETRIES = 5;
static int64_t const DEFAULT_CONNECTION_TIMEOUT = 5;

////////////////////////////////////////////////////////////////////////////////
/// @brief colors for output
////////////////////////////////////////////////////////////////////////////////

static char const DEF_RED[6]         = "\x1b[31m";
static char const DEF_BOLD_RED[8]    = "\x1b[1;31m";
static char const DEF_GREEN[6]       = "\x1b[32m";
static char const DEF_BOLD_GREEN[8]  = "\x1b[1;32m";
static char const DEF_BLUE[6]        = "\x1b[34m";
static char const DEF_BOLD_BLUE[8]   = "\x1b[1;34m";
static char const DEF_YELLOW[8]      = "\x1b[1;33m";
static char const DEF_WHITE[6]       = "\x1b[37m";
static char const DEF_BOLD_WHITE[8]  = "\x1b[1;37m";
static char const DEF_BLACK[6]       = "\x1b[30m";
static char const DEF_BOLD_BLACK[8]  = "\x1b[1;39m";
static char const DEF_BLINK[5]       = "\x1b[5m";
static char const DEF_BRIGHT[5]      = "\x1b[1m";
static char const DEF_RESET[5]       = "\x1b[0m";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection* ClientConnection = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief object template for the initial connection
////////////////////////////////////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> ConnectionTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief connect timeout (in s) 
////////////////////////////////////////////////////////////////////////////////

static int64_t ConnectTimeout = DEFAULT_CONNECTION_TIMEOUT;

////////////////////////////////////////////////////////////////////////////////
/// @brief max size body size (used for imports)
////////////////////////////////////////////////////////////////////////////////

static uint64_t MaxUploadSize = 500000;

////////////////////////////////////////////////////////////////////////////////
/// @brief disable auto completion
////////////////////////////////////////////////////////////////////////////////

static bool NoAutoComplete = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief deactivate colors
////////////////////////////////////////////////////////////////////////////////

static bool NoColors = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief the output pager
////////////////////////////////////////////////////////////////////////////////

static string OutputPager = "less -X -R -F -L";

////////////////////////////////////////////////////////////////////////////////
/// @brief the pager FILE 
////////////////////////////////////////////////////////////////////////////////

static FILE *PAGER = stdout;

////////////////////////////////////////////////////////////////////////////////
/// @brief use pretty print
////////////////////////////////////////////////////////////////////////////////

static bool PrettyPrint = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief quiet start
////////////////////////////////////////////////////////////////////////////////

static bool Quiet = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief request timeout (in s) 
////////////////////////////////////////////////////////////////////////////////

static int64_t RequestTimeout = DEFAULT_REQUEST_TIMEOUT;

////////////////////////////////////////////////////////////////////////////////
/// @brief server address and port
////////////////////////////////////////////////////////////////////////////////

static string ServerAddressPort = DEFAULT_SERVER_NAME + ":" + StringUtils::itoa(DEFAULT_SERVER_PORT);

////////////////////////////////////////////////////////////////////////////////
/// @brief server address
////////////////////////////////////////////////////////////////////////////////

static string ServerAddress = DEFAULT_SERVER_NAME;

////////////////////////////////////////////////////////////////////////////////
/// @brief server port
////////////////////////////////////////////////////////////////////////////////

static int ServerPort = DEFAULT_SERVER_PORT;

////////////////////////////////////////////////////////////////////////////////
/// @brief user name
////////////////////////////////////////////////////////////////////////////////

static string Username = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief password
////////////////////////////////////////////////////////////////////////////////

static string Password = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief use password flag
////////////////////////////////////////////////////////////////////////////////

static bool HasPassword = false;

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
/// @brief unit file test cases
////////////////////////////////////////////////////////////////////////////////

static vector<string> UnitTests;

////////////////////////////////////////////////////////////////////////////////
/// @brief files to jslint
////////////////////////////////////////////////////////////////////////////////

static vector<string> JsLint;

////////////////////////////////////////////////////////////////////////////////
/// @brief use pager
////////////////////////////////////////////////////////////////////////////////

static bool UsePager = false;

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

static v8::Handle<v8::Value> JS_StartOutputPager (v8::Arguments const& ) {
  if (UsePager) {
    internalPrint("Using pager already.\n");        
  }
  else {
    UsePager = true;
    internalPrint("Using pager '%s' for output buffering.\n", OutputPager.c_str());    
  }
  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the output pager
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_StopOutputPager (v8::Arguments const& ) {
  if (UsePager) {
    internalPrint("Stopping pager.\n");
  }
  else {
    internalPrint("Pager not running.\n");    
  }
  UsePager = false;
  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           function for CSV import
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

  char separator = ',';
  char quote = '"';

  if (3 <= argv.Length()) {
    v8::Handle<v8::Object> options = argv[2]->ToObject();
    bool error;

    // separator
    if (options->Has(separatorKey)) {
      separator = TRI_ObjectToCharacter(options->Get(separatorKey), error);

      if (error) {
        return scope.Close(v8::ThrowException(v8::String::New("<options>.separator must be a character")));
      }
    }

    // quote
    if (options->Has(quoteKey)) {
      quote = TRI_ObjectToCharacter(options->Get(quoteKey), error);

      if (error) {
        return scope.Close(v8::ThrowException(v8::String::New("<options>.quote must be a character")));
      }
    }
  }

  ImportHelper ih(ClientConnection->getHttpClient(), MaxUploadSize);
  
  ih.setQuote(quote);
  ih.setSeparator(separator);

  string fileName = TRI_ObjectToString(argv[0]);
  string collectionName = TRI_ObjectToString(argv[1]);
 
  if (ih.importCsv(collectionName, fileName)) {
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

// -----------------------------------------------------------------------------
// --SECTION--                                              JSON import function
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

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
  if (!UsePager || OutputPager == "" || OutputPager == "stdout") {
    PAGER= stdout;
    return;
  }
  
  if (!(PAGER = popen(OutputPager.c_str(), "w"))) {
    printf("popen() failed! defaulting PAGER to stdout!\n");
    PAGER= stdout;
    UsePager = false;
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
    ("connect-timeout", &ConnectTimeout, "connect timeout in seconds")
    ("help,h", "help message")
    ("javascript.modules-path", &StartupModules, "one or more directories separated by cola")
    ("javascript.startup-directory", &StartupPath, "startup paths containing the JavaScript files; multiple directories can be separated by cola")
    ("jslint", &JsLint, "do not start as shell, run jslint instead")
    ("log.level,l", &level,  "log level")
    ("max-upload-size", &MaxUploadSize, "maximum size of import chunks")
    ("no-auto-complete", "disable auto completion")
    ("no-colors", "deactivate color support")
    ("pager", &OutputPager, "output pager")
    ("pretty-print", "pretty print values")          
    ("quiet,s", "no banner")
    ("request-timeout", &RequestTimeout, "request timeout in seconds")
    ("server", &ServerAddressPort, "server address and port, use 'none' to start without a server")
    ("username", &Username, "username to use when connecting")
    ("password", &Password, "password to use when connecting (leave empty for prompt)")
    ("javascript.unit-tests", &UnitTests, "do not start as shell, run unit tests instead")
    ("use-pager", "use pager")
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
  
  // check if have a password
  HasPassword = options.has("password");

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

  if (options.has("quiet")) {
    Quiet = true;
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

enum WRAP_CLASS_TYPES {WRAP_TYPE_CONNECTION = 1};

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries (call the destructor here)
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_DestructorCallback (v8::Persistent<v8::Value> , void* parameter) {
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
/// @brief ClientConnection constructor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ClientConnection_ConstructorCallback (v8::Arguments const& argv) {
  v8::HandleScope scope;

  string server = ServerAddress;
  int port = ServerPort;
  size_t retries = DEFAULT_RETRIES;
  
  if (argv.Length() > 0 && argv[0]->IsString()) {
    string definition = TRI_ObjectToString(argv[0]);
    
    AddressPort ap;

    if (! ap.split(definition)) {
      string errorMessage = "error in '" + definition + "'";
      return scope.Close(v8::ThrowException(v8::String::New(errorMessage.c_str())));      
    }    

    if (! ap._address.empty()) {
      server = ap._address;
    }

    port = ap._port;
  }
  
  V8ClientConnection* connection = new V8ClientConnection(server, Username, Password, port, (double) RequestTimeout, retries, (double) ConnectTimeout, false);
  
  if (connection->isConnected()) {
    printf("Connected to Arango DB %s:%d Version %s\n", 
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
  
  string result = "[object ArangoConnection:" 
                + connection->getHostname()
                + ":"
                + triagens::basics::StringUtils::itoa(connection->getPort());
          
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
/// @brief ClientConnection method "isConnected"
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
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the shell
////////////////////////////////////////////////////////////////////////////////

static void RunShell (v8::Handle<v8::Context> context) {
  v8::Context::Scope contextScope(context);
  v8::Local<v8::String> name(v8::String::New("(shell)"));

  V8LineEditor* console = new V8LineEditor(context, ".arangosh");

  console->open(!NoAutoComplete);

  while (true) {
    while (! v8::V8::IdleNotification()) {
    }

    char* input = console->prompt("arangosh> ");

    if (input == 0) {
      break;
    }

    if (*input == '\0') {
      continue;
    }

    string i = triagens::basics::StringUtils::trim(input);

    if (i == "exit" || i == "quit") {
      TRI_FreeString(TRI_CORE_MEM_ZONE, input);
      break;
    }

    if (i == "help") {
      TRI_FreeString(TRI_CORE_MEM_ZONE, input);
      input = TRI_DuplicateString("help()");
    }
    
    console->addHistory(input);
    
    v8::HandleScope scope;
    v8::TryCatch tryCatch;
    
    StartPager();

    TRI_ExecuteJavaScriptString(context, v8::String::New(input), name, true);
    TRI_FreeString(TRI_CORE_MEM_ZONE, input);

    if (tryCatch.HasCaught()) {
      cout << TRI_StringifyV8Exception(&tryCatch);
    }

    StopPager();
  }

  console->close();

  if (Quiet) {
    printf("\n");
  }
  else {
    printf("\nBye Bye! Auf Wiedersehen!\n");
  }
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
    cout << TRI_StringifyV8Exception(&tryCatch);
    ok = false;
  }
  else {
    ok = TRI_ObjectToBoolean(context->Global()->Get(v8::String::New("SYS_UNIT_TESTS_RESULT")));
  }

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
    cout << TRI_StringifyV8Exception(&tryCatch);
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
  if (!NoColors) {
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
  TRIAGENS_C_INITIALISE(argc, argv);
  TRI_InitialiseLogging(false);
  int ret = EXIT_SUCCESS;

  // .............................................................................
  // use relative system paths
  // .............................................................................

  {
    char* binaryPath = TRI_LocateBinaryPath(argv[0]);

#ifdef TRI_ENABLE_RELATIVE_SYSTEM
  
    StartupModules = string(binaryPath) + "/../share/arango/js/client/modules"
             + ";" + string(binaryPath) + "/../share/arango/js/common/modules";

#else

  // .............................................................................
  // use relative development paths
  // .............................................................................

#ifdef TRI_ENABLE_RELATIVE_DEVEL

#ifdef TRI_STARTUP_MODULES_PATH
    StartupModules = TRI_STARTUP_MODULES_PATH;
#else
    StartupModules = string(binaryPath) + "/../js/client/modules"
             + ";" + string(binaryPath) + "/../js/common/modules";
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

    TRI_FreeString(TRI_CORE_MEM_ZONE, binaryPath);
  }

  // .............................................................................
  // parse the program options
  // .............................................................................

  ParseProgramOptions(argc, argv);
  
  // check connection args
  if (ConnectTimeout <= 0) {
    cout << "invalid value for connect-timeout." << endl;
    exit(EXIT_FAILURE);
  }

  if (RequestTimeout <= 0) {
    cout << "invalid value for request-timeout." << endl;
    exit(EXIT_FAILURE);
  }

  // must specify a user name
  if (! HasPassword && Username != "") {
    cout << "Please specify a password: " << flush;

    // now prompt for it
    getline(cin, Password);
  }

  // .............................................................................
  // set-up client connection
  // .............................................................................

  // check if we want to connect to a server
  bool useServer = (ServerAddressPort != "none");

  if (!JsLint.empty()) {
    // if we are in jslint mode, we will not need the server at all
    useServer = false;
  }

  if (useServer) {
    AddressPort ap;

    if (! ap.split(ServerAddressPort)) {
      if (! ServerAddress.empty()) {
        printf("Could not split %s.\n", ServerAddress.c_str());
        exit(EXIT_FAILURE);
      }
    }

    if (! ap._address.empty()) {
      ServerAddress = ap._address;
    }

    ServerPort = ap._port;
    
    ClientConnection = new V8ClientConnection(
      ServerAddress, 
      Username,
      Password,
      ServerPort, 
      (double) RequestTimeout,
      DEFAULT_RETRIES, 
      (double) ConnectTimeout,
      false);
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
    printf("cannot initialize V8 engine\n");
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
  context->Global()->Set(v8::String::New("PRETTY_PRINT"), v8::Boolean::New(PrettyPrint));
  
  // add colors for print.js
  addColors(context);

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
  
  
  // .............................................................................
  // banner
  // .............................................................................  

  // http://www.network-science.de/ascii/   Font: ogre
  if (! Quiet) {
    char const* g = DEF_GREEN;
    char const* r = DEF_RED;
    char const* z = DEF_RESET;

    if (NoColors) {
      g = "";
      r = "";
      z = "";
    }

    printf("%s                                  %s     _     %s\n", g, r, z);
    printf("%s  __ _ _ __ __ _ _ __   __ _  ___ %s ___| |__  %s\n", g, r, z);
    printf("%s / _` | '__/ _` | '_ \\ / _` |/ _ \\%s/ __| '_ \\ %s\n", g, r, z);
    printf("%s| (_| | | | (_| | | | | (_| | (_) %s\\__ \\ | | |%s\n", g, r, z);
    printf("%s \\__,_|_|  \\__,_|_| |_|\\__, |\\___/%s|___/_| |_|%s\n", g, r, z);
    printf("%s                       |___/      %s           %s\n", g, r, z);

    printf("\n");
    printf("Welcome to arangosh %s. Copyright (c) 2012 triAGENS GmbH.\n", TRIAGENS_VERSION);

#ifdef TRI_V8_VERSION
    printf("Using Google V8 %s JavaScript engine.\n", TRI_V8_VERSION);
#else
    printf("Using Google V8 JavaScript engine.\n\n");
#endif
  
#ifdef TRI_READLINE_VERSION
    printf("Using READLINE %s.\n", TRI_READLINE_VERSION);
#endif

    printf("\n");

    // set up output
    if (UsePager) {
      printf("Using pager '%s' for output buffering.\n", OutputPager.c_str());    
    }

    if (PrettyPrint) {
      printf("Pretty print values.\n");    
    }

    if (useServer) {
      if (ClientConnection->isConnected()) {
        if (! Quiet) {
          printf("Connected to Arango DB %s:%d Version %s\n", 
                 ClientConnection->getHostname().c_str(), 
                 ClientConnection->getPort(), 
                 ClientConnection->getVersion().c_str());
        }
      }
      else {
        printf("Could not connect to server %s:%d\n", ServerAddress.c_str(), ServerPort);
        string errorMessage = ClientConnection->getErrorMessage();
        if (errorMessage != "") {
          printf("Error message '%s'\n", errorMessage.c_str());
        }
      }
    }
  }

  // .............................................................................
  // read files
  // .............................................................................

  // load java script from js/bootstrap/*.h files
  if (StartupPath.empty()) {
    StartupLoader.defineScript("common/bootstrap/modules.js", JS_common_bootstrap_modules);
    StartupLoader.defineScript("common/bootstrap/print.js", JS_common_bootstrap_print);
    StartupLoader.defineScript("common/bootstrap/errors.js", JS_common_bootstrap_errors);
    StartupLoader.defineScript("common/bootstrap/monkeypatches.js", JS_common_bootstrap_monkeypatches);
    StartupLoader.defineScript("client/client.js", JS_client_client);
  }
  else {
    LOGGER_DEBUG << "using JavaScript startup files at '" << StartupPath << "'";
    StartupLoader.setDirectory(StartupPath);
  }

  context->Global()->Set(v8::String::New("ARANGO_QUIET"), Quiet ? v8::True() : v8::False(), v8::ReadOnly);

  // load all init files
  char const* files[] = {
    "common/bootstrap/modules.js",
    "common/bootstrap/print.js",
    "common/bootstrap/errors.js",
    "common/bootstrap/monkeypatches.js",
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
  
  // .............................................................................
  // run normal shell
  // .............................................................................

  if (UnitTests.empty() && JsLint.empty()) {
    RunShell(context);
  }

  // .............................................................................
  // run unit tests or jslint
  // .............................................................................

  else {
    bool ok;
   
    if (!UnitTests.empty()) {
      // we have unit tests
      ok = RunUnitTests(context);
    }
    else {
      assert(!JsLint.empty());

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
