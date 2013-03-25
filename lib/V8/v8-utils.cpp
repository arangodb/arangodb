////////////////////////////////////////////////////////////////////////////////
/// @brief V8 utility functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

#include "build.h"

#include "v8-utils.h"

#include <fstream>
#include <locale>

#include "Basics/Dictionary.h"
#include "Basics/Nonce.h"
#include "Basics/RandomGenerator.h"
#include "Basics/StringUtils.h"
#include "BasicsC/conversions.h"
#include "BasicsC/csv.h"
#include "BasicsC/files.h"
#include "BasicsC/logging.h"
#include "BasicsC/process-utils.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/strings.h"
#include "BasicsC/utf8-helper.h"
#include "BasicsC/zip.h"
#include "Basics/FileUtils.h"
#include "Rest/HttpRequest.h"
#include "Rest/SslInterface.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Statistics/statistics.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"

#include "unicode/normalizer2.h"

#include "3rdParty/valgrind/valgrind.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                           GENERAL
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Random string generators
////////////////////////////////////////////////////////////////////////////////

namespace {
  static Random::UniformCharacter JSAlphaNumGenerator("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  static Random::UniformCharacter JSNumGenerator("0123456789");
  static Random::UniformCharacter JSSaltGenerator("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*(){}[]:;<>,.?/|");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public classes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Converts an object to a UTF-8-encoded and normalized character array.
////////////////////////////////////////////////////////////////////////////////

TRI_Utf8ValueNFC::TRI_Utf8ValueNFC (TRI_memory_zone_t* memoryZone, v8::Handle<v8::Value> obj) :
  _str(0), _length(0), _memoryZone(memoryZone) {

   v8::String::Value str(obj);
   size_t str_len = str.length();

   _str = TRI_normalize_utf16_to_NFC(_memoryZone, *str, str_len, &_length);
}

TRI_Utf8ValueNFC::~TRI_Utf8ValueNFC () {
  if (_str) {
    TRI_Free(_memoryZone, _str);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a Javascript error object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> CreateErrorObject (int errorNumber, string const& message) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::String> errorMessage = v8::String::New(message.c_str());

  v8::Handle<v8::Object> errorObject = v8::Exception::Error(errorMessage)->ToObject();
  v8::Handle<v8::Value> proto = v8g->ErrorTempl->NewInstance();

  errorObject->Set(v8::String::New("errorNum"), v8::Number::New(errorNumber));
  errorObject->Set(v8::String::New("errorMessage"), errorMessage);

  if (! proto.IsEmpty()) {
    errorObject->SetPrototype(proto);
  }

  return scope.Close(errorObject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads/execute a file into/in the current context
////////////////////////////////////////////////////////////////////////////////

static bool LoadJavaScriptFile (char const* filename,
                                bool execute,
                                bool useGlobalContext) {
  v8::HandleScope handleScope;

  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, filename, NULL);

  if (content == 0) {
    LOG_TRACE("cannot load java script file '%s': %s", filename, TRI_last_error());
    return false;
  }

  if (useGlobalContext) {
    char* contentWrapper = TRI_Concatenate3StringZ(TRI_UNKNOWN_MEM_ZONE,
                                                   "(function() { ",
                                                   content,
                                                   "/* end-of-file */ })()");

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

    content = contentWrapper;
  }

  v8::Handle<v8::String> name = v8::String::New(filename);
  v8::Handle<v8::String> source = v8::String::New(content);

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);

  // compilation failed, print errors that happened during compilation
  if (script.IsEmpty()) {
    return false;
  }

  if (execute) {
    // execute script
    v8::Handle<v8::Value> result = script->Run();

    if (result.IsEmpty()) {
      return false;
    }
  }

  LOG_TRACE("loaded java script file: '%s'", filename);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads all files from a directory into the current context
////////////////////////////////////////////////////////////////////////////////

static bool LoadJavaScriptDirectory (char const* path,
                                     bool execute,
                                     bool useGlobalContext) {
  v8::HandleScope scope;
  TRI_vector_string_t files;
  bool result;
  regex_t re;
  size_t i;

  LOG_TRACE("loading JavaScript directory: '%s'", path);

  files = TRI_FilesDirectory(path);

  regcomp(&re, "^(.*)\\.js$", REG_ICASE | REG_EXTENDED);

  result = true;

  for (i = 0;  i < files._length;  ++i) {
    v8::TryCatch tryCatch;
    bool ok;
    char const* filename;
    char* full;

    filename = files._buffer[i];

    if (! regexec(&re, filename, 0, 0, 0) == 0) {
      continue;
    }

    full = TRI_Concatenate2File(path, filename);

    ok = LoadJavaScriptFile(full, execute, useGlobalContext);
    TRI_FreeString(TRI_CORE_MEM_ZONE, full);

    result = result && ok;

    if (! ok) {
      TRI_LogV8Exception(&tryCatch);
    }
  }

  TRI_DestroyVectorString(&files);
  regfree(&re);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a distribution vector
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Array> DistributionList (StatisticsVector const& dist) {
  v8::HandleScope scope;

  v8::Handle<v8::Array> result = v8::Array::New();

  for (uint32_t i = 0;  i < (uint32_t) dist._value.size();  ++i) {
    result->Set(i, v8::Number::New(dist._value[i]));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the distribution
////////////////////////////////////////////////////////////////////////////////

static void FillDistribution (v8::Handle<v8::Object> list,
                              char const* name,
                              StatisticsDistribution const& dist) {
  v8::Handle<v8::Object> result = v8::Object::New();

  result->Set(TRI_V8_SYMBOL("count"), v8::Number::New(dist._count));

  v8::Handle<v8::Array> counts = v8::Array::New(dist._counts.size());
  size_t pos = 0;

  for (vector<uint64_t>::const_iterator i = dist._counts.begin();  i != dist._counts.end();  ++i, ++pos) {
    counts->Set(pos, v8::Number::New(*i));
  }

  result->Set(TRI_V8_SYMBOL("counts"), counts);

  list->Set(TRI_V8_SYMBOL(name), result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a Javascript snippet, but do not execute it
///
/// @FUN{internal.parse(@FA{script})}
///
/// Parses the @FA{script} code, but does not execute it.
/// Will return @LIT{true} if the code does not have a parse error, and throw
/// an exception otherwise.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Parse (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() < 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: parse(<script>)")));
  }

  v8::Handle<v8::Value> source = argv[0];
  v8::Handle<v8::Value> filename;

  if (argv.Length() > 1) {
    filename = argv[1];
  }
  else {
    filename = v8::String::New("(snippet)");
  }

  if (! source->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("<script> must be a string")));
  }

  v8::Handle<v8::Script> script = v8::Script::Compile(source->ToString(), filename);

  // compilation failed, we have caught an exception
  if (tryCatch.HasCaught()) {
    string err = TRI_StringifyV8Exception(&tryCatch);
    return scope.Close(v8::ThrowException(v8::String::New(err.c_str())));
  }

  // compilation failed, we don't know why
  if (script.IsEmpty()) {
    return scope.Close(v8::False());
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief downloads data from a URL
///
/// @FUN{internal.download(@FA{url}, @FA{method}, @FA{outfile}, @FA{timeout})}
///
/// Downloads the data from the URL specified by @FA{url} and saves the 
/// response body to @FA{outfile}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Download (v8::Arguments const& argv) {
  v8::HandleScope scope;
  
  if (argv.Length() < 3) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: download(<url>, <method>, <outfile>, <timeout>)")));
  }

  string url = TRI_ObjectToString(argv[0]);

  HttpRequest::HttpRequestType method = HttpRequest::HTTP_REQUEST_GET;
  const string methodString = TRI_ObjectToString(argv[1]);
  if (methodString == "head") {
    method = HttpRequest::HTTP_REQUEST_HEAD;
  }
  else if (methodString == "delete") {
    method = HttpRequest::HTTP_REQUEST_DELETE;
  }

  const string outfile = TRI_ObjectToString(argv[2]);

  double timeout = 10.0;
  if (argv.Length() > 3) {
    timeout = TRI_ObjectToDouble(argv[3]);
  }

  if (TRI_ExistsFile(outfile.c_str())) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_CANNOT_OVERWRITE_FILE)));
  }

  int numRedirects = 0;

  while (numRedirects++ < 5) {
    string endpoint;
    string relative;

    if (url.substr(0, 7) == "http://") {
      size_t found = url.find('/', 7);

      relative = "/";
      if (found != string::npos) {
        relative.append(url.substr(found + 1));
        endpoint = "tcp://" + url.substr(7, found - 7) + ":80";
      }
      else {
        endpoint = "tcp://" + url.substr(7) + ":80";
      }
    }
    else if (url.substr(0, 8) == "https://") {
      size_t found = url.find('/', 8);

      relative = "/";
      if (found != string::npos) {
        relative.append(url.substr(found + 1));
        endpoint = "ssl://" + url.substr(8, found - 8) + ":443";
      }
      else {
        endpoint = "ssl://" + url.substr(8) + ":443";
      }
    }
    else {
      return scope.Close(v8::ThrowException(v8::String::New("unsupported URL specified")));
    }
    
    LOGGER_TRACE("downloading file. endpoint: " << endpoint << ", relative URL: " << url);

    GeneralClientConnection* connection = GeneralClientConnection::factory(Endpoint::clientFactory(endpoint), timeout, timeout, 3);

    if (connection == 0) {
      return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_OUT_OF_MEMORY)));
    }

    SimpleHttpClient client(connection, timeout, false);

    v8::Handle<v8::Object> result = v8::Object::New();

    // connect to server and get version number
    map<string, string> headerFields;
    SimpleHttpResult* response = client.request(method, relative, 0, 0, headerFields);

    int returnCode;
    string returnMessage;

    if (! response || ! response->isComplete()) {
      // save error message
      returnMessage = client.getErrorMessage();
      returnCode = 500;

      if (response && response->getHttpReturnCode() > 0) {
        returnCode = response->getHttpReturnCode();
      }
    }
    else {
      returnMessage = response->getHttpReturnMessage();
      returnCode = response->getHttpReturnCode();

      if (returnCode == 301 || returnCode == 302) {
        bool found;
        url = response->getHeaderField(string("location"), found);

        delete response;
        delete connection;

        if (! found) {
          return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "caught invalid redirect URL")));
        }
        continue;
      }

      result->Set(v8::String::New("code"),    v8::Number::New(returnCode));
      result->Set(v8::String::New("message"), v8::String::New(returnMessage.c_str()));

      const map<string, string> responseHeaders = response->getHeaderFields();
      map<string, string>::const_iterator it;

      v8::Handle<v8::Object> headers = v8::Object::New();
      for (it = responseHeaders.begin(); it != responseHeaders.end(); ++it) {
        headers->Set(v8::String::New((*it).first.c_str()), v8::String::New((*it).second.c_str()));
      }
      result->Set(v8::String::New("headers"), headers);

      if (returnCode >= 200 && returnCode <= 299) {
        try {
          FileUtils::spit(outfile, response->getBody().str());
        }
        catch (...) {

        }
      }
    }
      
    result->Set(v8::String::New("code"),    v8::Number::New(returnCode));
    result->Set(v8::String::New("message"), v8::String::New(returnMessage.c_str()));

    if (response) {
      delete response;
    }

    delete connection;
    return scope.Close(result);
  }
  
  return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "too many redirects")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a script
///
/// @FUN{internal.execute(@FA{script}, @FA{sandbox}, @FA{filename})}
///
/// Executes the @FA{script} with the @FA{sandbox} as context. Global variables
/// assigned inside the @FA{script}, will be visible in the @FA{sandbox} object
/// after execution. The @FA{filename} is used for displaying error
/// messages.
///
/// If @FA{sandbox} is undefined, then @FN{execute} uses the current context.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Execute (v8::Arguments const& argv) {
  v8::HandleScope scope;
  size_t i;

  // extract arguments
  if (argv.Length() != 3) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: execute(<script>, <sandbox>, <filename>)")));
  }

  v8::Handle<v8::Value> source = argv[0];
  v8::Handle<v8::Value> sandboxValue = argv[1];
  v8::Handle<v8::Value> filename = argv[2];

  if (! source->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("<script> must be a string")));
  }

  bool useSandbox = sandboxValue->IsObject();
  v8::Handle<v8::Object> sandbox;
  v8::Handle<v8::Context> context;

  if (useSandbox) {
    sandbox = sandboxValue->ToObject();

    // create new context
    context = v8::Context::New();
    context->Enter();

    // copy sandbox into context
    v8::Handle<v8::Array> keys = sandbox->GetPropertyNames();

    for (i = 0; i < keys->Length(); i++) {
      v8::Handle<v8::String> key = keys->Get(v8::Integer::New(i))->ToString();
      v8::Handle<v8::Value> value = sandbox->Get(key);

      if (TRI_IsTraceLogging(__FILE__)) {
        TRI_Utf8ValueNFC keyName(TRI_UNKNOWN_MEM_ZONE, key);

        if (*keyName != 0) {
          LOG_TRACE("copying key '%s' from sandbox to context", *keyName);
        }
      }

      if (value == sandbox) {
        value = context->Global();
      }

      context->Global()->Set(key, value);
    }
  }

  // execute script inside the context
  v8::Handle<v8::Script> script = v8::Script::Compile(source->ToString(), filename);

  // compilation failed, print errors that happened during compilation
  if (script.IsEmpty()) {
    if (useSandbox) {
      context->DetachGlobal();
      context->Exit();
    }

    return scope.Close(v8::Undefined());
  }

  // compilation succeeded, run the script
  v8::Handle<v8::Value> result = script->Run();

  if (result.IsEmpty()) {
    if (useSandbox) {
      context->DetachGlobal();
      context->Exit();
    }

    return scope.Close(v8::Undefined());
  }

  // copy result back into the sandbox
  if (useSandbox) {
    v8::Handle<v8::Array> keys = context->Global()->GetPropertyNames();

    for (i = 0; i < keys->Length(); i++) {
      v8::Handle<v8::String> key = keys->Get(v8::Integer::New(i))->ToString();
      v8::Handle<v8::Value> value = context->Global()->Get(key);

      if (TRI_IsTraceLogging(__FILE__)) {
        TRI_Utf8ValueNFC keyName(TRI_UNKNOWN_MEM_ZONE, key);

        if (*keyName != 0) {
          LOG_TRACE("copying key '%s' from context to sandbox", *keyName);
        }
      }

      if (value == context->Global()) {
        value = sandbox;
      }

      sandbox->Set(key, value);
    }

    context->DetachGlobal();
    context->Exit();
  }

  if (useSandbox) {
    return scope.Close(v8::True());
  }
  else {
    return scope.Close(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a directory
///
/// @FUN{fs.createDirectory(@FA{path}, @FA{createParents})}
///
/// Creates the directory specified @FA{path}. If @FA{createParents} is set to
/// @LIT{true}, then the path is created recursively.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateDirectory (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() == 0 || argv.Length() > 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: createDirectory(<path>, <createParents>)")));
  }

  const string path = TRI_ObjectToString(argv[0]);
  bool createParents = false;
  if (argv.Length() > 1) {
    createParents = TRI_ObjectToBoolean(argv[1]);
  }

  bool result; 
  if (createParents) { 
    result = TRI_CreateRecursiveDirectory(path.c_str());
  }
  else {
    result = TRI_CreateDirectory(path.c_str());
  }

  return scope.Close(result ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a file of any type or directory exists
///
/// @FUN{fs.exists(@FA{path})}
///
/// Returns true if a file (of any type) or a directory exists at a given
/// path. If the file is a broken symbolic link, returns false.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Exists (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: exists(<filename>)")));
  }

  string filename = TRI_ObjectToString(argv[0]);

  return scope.Close(TRI_ExistsFile(filename.c_str()) ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads in a line from stdin
///
/// @FUN{console.getline()}
///
/// Reads in a line from the console.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Getline (v8::Arguments const& argv) {
  v8::HandleScope scope;

  string line;
  getline(cin, line);

  return scope.Close(v8::String::New(line.c_str(), line.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the temporary directory
///
/// @FUN{fs.getTempPath()}
///
/// Returns the absolute path of the temporary directory
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetTempPath (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: getTempPath()")));
  }

  // return result
  return scope.Close(v8::String::New(TempPath.c_str(), TempPath.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name for a (new) temporary file
///
/// @FUN{fs.getTempFile(@FA{directory})}
///
/// Returns the name for a new temporary file in directory @FA{directory}. 
/// If @FA{createFile} is @LIT{true}, an empty file will be created so no other 
/// process can create a file of the same name.
///
/// Note that the directory @FA{directory} must exist.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetTempFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() > 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: getTempFile(<directory>, <createFile>)")));
  }

  const char* p = NULL;
  string path;
  if (argv.Length() > 0) {
    path = TRI_ObjectToString(argv[0]);
    p = path.c_str();
  }

  bool create = false;
  if (argv.Length() > 1) {
    create = TRI_ObjectToBoolean(argv[1]);
  }

  char* result = 0;
  if (TRI_GetTempName(p, &result, create) != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_INTERNAL, "could not create temp file")));
  }

  const string tempfile(result);
  TRI_Free(TRI_CORE_MEM_ZONE, result);

  // return result
  return scope.Close(v8::String::New(tempfile.c_str(), tempfile.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if path is a directory
///
/// @FUN{fs.isDirectory(@FA{path})}
///
/// Returns true if the @FA{path} points to a directory.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_IsDirectory (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: isDirectory(<path>)")));
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<path> must be a string")));
  }

  // return result
  return scope.Close(TRI_IsDirectory(*name) ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if path is a file
///
/// @FUN{fs.isFile(@FA{path})}
///
/// Returns true if the @FA{path} points to a file.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_IsFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: isFile(<path>)")));
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<path> must be a string")));
  }

  // return result
  return scope.Close(TRI_ExistsFile(*name) ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the directory tree
///
/// @FUN{fs.listTree(@FA{path})}
///
/// The function returns an array that starts with the given path, and all of
/// the paths relative to the given path, discovered by a depth first traversal
/// of every directory in any visited directory, reporting but not traversing
/// symbolic links to directories. The first path is always @LIT{""}, the path
/// relative to itself.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ListTree (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: listTree(<path>)")));
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<path> must be a string")));
  }

  // constructed listing
  v8::Handle<v8::Array> result = v8::Array::New();
  TRI_vector_string_t list = TRI_FullTreeDirectory(*name);

  uint32_t j = 0;
  for (size_t i = 0;  i < list._length;  ++i) {
    const char* f = list._buffer[i]; 
    result->Set(j++, v8::String::New(f));
  }

  TRI_DestroyVectorString(&list);

  // return result
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unzips a file
///
/// @FUN{fs.unzip(@FA{filename}, @FA{outpath}, @FA{skipPaths}, @FA{overwrite}, @FA{password})}
///
/// Unzips the zip file specified by @FA{filename} into the path specified by
/// @FA{outpath}. Overwrites any existing target files if @FA{overwrite} is set
/// to @LIT{true}.
///
/// Returns @LIT{true} if the file was unzipped successfully.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UnzipFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() < 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: unzip(<filename>, <outPath>, <skipPaths>, <overwrite>, <password>)")));
  }

  const string filename = TRI_ObjectToString(argv[0]);
  const string outPath = TRI_ObjectToString(argv[1]);

  bool skipPaths = false;
  if (argv.Length() > 2) {
    skipPaths = TRI_ObjectToBoolean(argv[2]);
  }

  bool overwrite = false;
  if (argv.Length() > 3) {
    overwrite = TRI_ObjectToBoolean(argv[3]);
  }

  const char* p;
  string password;
  if (argv.Length() > 4) {
    password = TRI_ObjectToString(argv[4]);
    p = password.c_str();
  }
  else {
    p = NULL;
  }

  int res = TRI_UnzipFile(filename.c_str(), outPath.c_str(), skipPaths, overwrite, p);

  if (res == TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::True());
  }

  return scope.Close(v8::ThrowException(v8::String::New(TRI_errno_string(res))));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief zips a file
///
/// @FUN{fs.zip(@FA{filename}, @FA{files})}
///
/// Stores the files specified by @FA{files} in the zip file @FA{filename}.
///
/// Returns @LIT{true} if the file was zipped successfully.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ZipFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() < 2 || argv.Length() > 3) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: zip(<filename>, <files>, <password>)")));
  }

  const string filename = TRI_ObjectToString(argv[0]);

  if (! argv[1]->IsArray()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: zip(<filename>, <files>, <password>)")));
  }

  v8::Handle<v8::Array> files = v8::Handle<v8::Array>::Cast(argv[1]);
 
  int res = TRI_ERROR_NO_ERROR; 
  TRI_vector_string_t filenames;
  TRI_InitVectorString(&filenames, TRI_UNKNOWN_MEM_ZONE);

  for (uint32_t i = 0 ; i < files->Length(); ++i) {
    v8::Handle<v8::Value> file = files->Get(i);
    if (file->IsString()) {
      string fname = TRI_ObjectToString(file);
      TRI_PushBackVectorString(&filenames, TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, fname.c_str()));
    }
    else {
      res = TRI_ERROR_BAD_PARAMETER;
      break;
    }
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_ClearVectorString(&filenames);
    TRI_DestroyVectorString(&filenames);
    return scope.Close(v8::ThrowException(v8::String::New("usage: zip(<filename>, <files>, <password>)")));
  }
 
  const char* p; 
  string password;
  if (argv.Length() == 3) {
    password = TRI_ObjectToString(argv[2]);
    p = password.c_str();
  }
  else {
    p = NULL;
  }

  res = TRI_ZipFile(filename.c_str(), &filenames, p);
  TRI_ClearVectorString(&filenames);
  TRI_DestroyVectorString(&filenames);

  if (res == TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::True());
  }

  return scope.Close(v8::ThrowException(v8::String::New(TRI_errno_string(res))));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file and executes it
///
/// @FUN{internal.load(@FA{filename})}
///
/// Reads in a files and executes the contents in the current context.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Load (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: load(<filename>)")));
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be a string")));
  }

  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *name, NULL);

  if (content == 0) {
    return scope.Close(v8::ThrowException(v8::String::New(TRI_last_error())));
  }

  TRI_ExecuteJavaScriptString(v8::Context::GetCurrent(), v8::String::New(content), argv[0], false);
  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a message
///
/// @FUN{internal.log(@FA{level}, @FA{message})}
///
/// Logs the @FA{message} at the given log @FA{level}.
///
/// Valid log-level are:
///
/// - fatal
/// - error
/// - warning
/// - info
/// - debug
/// - trace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Log (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: log(<level>, <message>)")));
  }

  TRI_Utf8ValueNFC level(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*level == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<level> must be a string")));
  }

  TRI_Utf8ValueNFC message(TRI_UNKNOWN_MEM_ZONE, argv[1]);

  if (*message == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<message> must be a string")));
  }

  if (TRI_CaseEqualString(*level, "fatal")) {
    LOG_ERROR("(FATAL) %s", *message);
  }
  else if (TRI_CaseEqualString(*level, "error")) {
    LOG_ERROR("%s", *message);
  }
  else if (TRI_CaseEqualString(*level, "warning")) {
    LOG_WARNING("%s", *message);
  }
  else if (TRI_CaseEqualString(*level, "info")) {
    LOG_INFO("%s", *message);
  }
  else if (TRI_CaseEqualString(*level, "debug")) {
    LOG_DEBUG("%s", *message);
  }
  else if (TRI_CaseEqualString(*level, "trace")) {
    LOG_TRACE("%s", *message);
  }
  else {
    LOG_ERROR("(unknown log level '%s') %s", *level, *message);
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the log-level
///
/// @FUN{internal.logLevel()}
///
/// Returns the current log-level as string.
///
/// @verbinclude fluent37
///
/// @FUN{internal.logLevel(@FA{level})}
///
/// Changes the current log-level. Valid log-level are:
///
/// - fatal
/// - error
/// - warning
/// - info
/// - debug
/// - trace
///
/// @verbinclude fluent38
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LogLevel (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (1 <= argv.Length()) {
    TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, argv[0]);

    TRI_SetLogLevelLogging(*str);
  }

  return scope.Close(v8::String::New(TRI_LogLevelLogging()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief md5 sum
///
/// @FUN{internal.md5(@FA{text})}
///
/// Computes an md5 for the @FA{text}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Md5 (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1 || ! argv[0]->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: md5(<text>)")));
  }

  string key = TRI_ObjectToString(argv[0]);

  // create md5
  char* hash = 0;
  size_t hashLen;

  SslInterface::sslMD5(key.c_str(), key.size(), hash, hashLen);

  // as hex
  char* hex = 0;
  size_t hexLen;

  SslInterface::sslHEX(hash, hashLen, hex, hexLen);

  delete[] hash;

  // and return
  v8::Handle<v8::String> hashStr = v8::String::New(hex, hexLen);

  delete[] hex;

  return scope.Close(hashStr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate random numbers
///
/// @FUN{internal.genRandomNumbers(@FA{length})}
///
/// Generates a string of a given @FA{length} containing numbers.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RandomNumbers (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 1 || ! argv[0]->IsNumber()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: genRandomNumbers(<length>)")));
  }

  int length = (int) TRI_ObjectToInt64(argv[0]);

  string str = JSNumGenerator.random(length);
  return scope.Close(v8::String::New(str.c_str(), str.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate random alpha-numbers
///
/// @FUN{internal.genRandomAlphaNumbers(@FA{length})}
///
/// Generates a string of a given @FA{length} containing numbers and characters.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RandomAlphaNum (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1 || ! argv[0]->IsNumber()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: genRandomAlphaNumbers(<length>)")));
  }

  int length = (int) TRI_ObjectToInt64(argv[0]);

  string str = JSAlphaNumGenerator.random(length);
  return scope.Close(v8::String::New(str.c_str(), str.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gernate a salt
///
/// @FUN{internal.genRandomSalt()}
///
/// Generates a string containing numbers and characters (length 8).
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RandomSalt (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: genRandomSalt()")));
  }

  string str = JSSaltGenerator.random(8);
  return scope.Close(v8::String::New(str.c_str(), str.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief nonce generator
///
/// @FUN{internal.createNonce()}
///
/// Generates a base64 encoded nonce string. (length of the string is 16)
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateNonce (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: createNonce()")));
  }

  string str =  Nonce::createNonce();

  return scope.Close(v8::String::New(str.c_str(), str.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a base64 encoded nonce
///
/// @FUN{internal.checkAndMarkNonce(@FA{nonce})}
///
/// Checks and marks a @FA{nonce}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_MarkNonce (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1 || ! argv[0]->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: checkAndMarkNonce(<nonce>)")));
  }

  TRI_Utf8ValueNFC base64u(TRI_CORE_MEM_ZONE, argv[0]);

  if (base64u.length() != 16) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting 16-Byte base64url-encoded nonce")));
  }

  string raw = StringUtils::decodeBase64U(*base64u);

  if (raw.size() != 12) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting 12-Byte nonce")));
  }

  bool valid = Nonce::checkAndMark(raw);

  return scope.Close(v8::Boolean::New(valid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a file
///
/// @FUN{fs.move(@FA{source}, @FA{destination})}
///
/// Moves @FA{source} to @FA{destination}. Failure to move the file, or
/// specifying a directory for target when source is a file will throw an
/// exception.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Move (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract two arguments
  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: move(<source>, <destination>)")));
  }

  string source = TRI_ObjectToString(argv[0]);
  string destination = TRI_ObjectToString(argv[1]);

  int res = TRI_RenameFile(source.c_str(), destination.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::ThrowException(CreateErrorObject(res, "cannot move file")));
  }

  return scope.Close(v8::Undefined());
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

static v8::Handle<v8::Value> JS_Output (v8::Arguments const& argv) {
  for (int i = 0; i < argv.Length(); i++) {
    v8::HandleScope scope;

    // extract the next argument
    v8::Handle<v8::Value> val = argv[i];

    // convert it into a string
    v8::String::Utf8Value utf8(val);
    // TRI_Utf8ValueNFC utf8(TRI_UNKNOWN_MEM_ZONE, val);

    if (*utf8 == 0) {
      continue;
    }

    // print the argument
    char const* ptr = *utf8;
    size_t len = utf8.length();

    while (0 < len) {
      ssize_t n = TRI_WRITE(1, ptr, len);

      if (n < 0) {
        return v8::Undefined();
      }

      len -= n;
      ptr += n;
    }
  }

  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current process information
///
/// @FUN{internal.processStat()}
///
/// Returns information about the current process:
///
/// - minorPageFaults: The number of minor faults the process has made
///   which have not required loading a memory page from disk.
///
/// - majorPageFaults: The number of major faults the process has made
///   which have required loading a memory page from disk.
///
/// - userTime: Amount of time that this process has been scheduled in
///   user mode, measured in clock ticks.
///
/// - systemTime: Amount of time that this process has been scheduled
///   in kernel mode, measured in clock ticks.
///
/// - numberThreads: Number of threads in this process.
///
/// - residentSize: Resident Set Size: number of pages the process has
///   in real memory.  This is just the pages which count toward text,
///   data, or stack space.  This does not include pages which have
///   not been demand-loaded in, or which are swapped out.
///
/// - virtualSize: Virtual memory size in bytes.
///
/// @verbinclude system1
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ProcessStat (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> result = v8::Object::New();

  TRI_process_info_t info = TRI_ProcessInfoSelf();

  result->Set(v8::String::New("minorPageFaults"), v8::Number::New((double) info._minorPageFaults));
  result->Set(v8::String::New("majorPageFaults"), v8::Number::New((double) info._majorPageFaults));
  result->Set(v8::String::New("userTime"), v8::Number::New((double) info._userTime / (double) info._scClkTck));
  result->Set(v8::String::New("systemTime"), v8::Number::New((double) info._systemTime / (double) info._scClkTck));
  result->Set(v8::String::New("numberThreads"), v8::Number::New((double) info._numberThreads));
  result->Set(v8::String::New("residentSize"), v8::Number::New((double) info._residentSize));
  result->Set(v8::String::New("virtualSize"), v8::Number::New((double) info._virtualSize));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a random number using OpenSSL
///
/// @FUN{internal.rand()}
///
/// Generates a random number
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Rand (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // check arguments
  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: rand()")));
  }

  int iterations = 0;
  while (iterations++ < 5) {
    int32_t value;
    int result = SslInterface::sslRand(&value);

    if (result != 0) {
      // error
      break;
    }

    // no error, now check what random number was produced

    if (value != 0) {
      // a number != 0 was produced. that is sufficient
      return scope.Close(v8::Number::New(value));
    }

    // we don't want to return 0 as the result, so we try again
  }

  // we failed to produce a valid random number
  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads in a file
///
/// @FUN{internal.read(@FA{filename})}
///
/// Reads in a file and returns the content as string.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Read (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: read(<filename>)")));
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be a string")));
  }

  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *name, NULL);

  if (content == 0) {
    return scope.Close(v8::ThrowException(v8::String::New(TRI_last_error())));
  }

  v8::Handle<v8::String> result = v8::String::New(content);

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes to a file
///
/// @FUN{internal.save(@FA{filename})}
///
/// Writes the content into a file.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Save (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: save(<filename>, <content>)")));
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be a string")));
  }

  TRI_Utf8ValueNFC content(TRI_UNKNOWN_MEM_ZONE, argv[1]);

  if (*content == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<content> must be a string")));
  }

  ofstream file;

  file.open(*name, ios::out | ios::binary);
  if (file.is_open()) {
    file << *content;
    file.close();
    return scope.Close(v8::True());
  }

  return scope.Close(v8::ThrowException(v8::String::New("cannot write to file")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a file
///
/// @FUN{fs.remove(@FA{filename})}
///
/// Removes the file @FA{filename} at the given path. Throws an exception if the
/// path corresponds to anything that is not a file or a symbolic link. If
/// "path" refers to a symbolic link, removes the symbolic link.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Remove (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract two arguments
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: remove(<filename>)")));
  }

  string filename = TRI_ObjectToString(argv[0]);

  int res = TRI_UnlinkFile(filename.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res, "cannot remove file")));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief formats the arguments
///
/// @FUN{internal.sprintf(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to the format string @FA{format}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SPrintF (v8::Arguments const& argv) {
  v8::HandleScope scope;

  size_t len = argv.Length();

  if (len == 0) {
    return scope.Close(v8::String::New(""));
  }

  TRI_Utf8ValueNFC format(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*format == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<format> must be a string")));
  }

  string result;

  size_t p = 1;

  for (char const* ptr = *format;  *ptr;  ++ptr) {
    if (*ptr == '%') {
      ++ptr;

      switch (*ptr) {
        case '%':
          result += '%';
          break;

        case 'd':
        case 'f':
        case 'i': {
          if (len <= p) {
            return scope.Close(v8::ThrowException(v8::String::New("not enough arguments")));
          }

          bool e;
          double f = TRI_ObjectToDouble(argv[p], e);

          if (e) {
            string msg = StringUtils::itoa(p) + ".th argument must be a number";
            return scope.Close(v8::ThrowException(v8::String::New(msg.c_str())));
          }

          char b[1024];

          if (*ptr == 'f') {
            snprintf(b, sizeof(b), "%f", f);
          }
          else {
            snprintf(b, sizeof(b), "%ld", (long) f);
          }

          ++p;

          result += b;

          break;
        }

        case 'o':
        case 's': {
          if (len <= p) {
            return scope.Close(v8::ThrowException(v8::String::New("not enough arguments")));
          }

          TRI_Utf8ValueNFC text(TRI_UNKNOWN_MEM_ZONE, argv[p]);

          if (*text == 0) {
            string msg = StringUtils::itoa(p) + ".th argument must be a string";
            return scope.Close(v8::ThrowException(v8::String::New(msg.c_str())));
          }

          ++p;

          result += *text;

          break;
        }

        default: {
          string msg = "found illegal format directive '" + string(1, *ptr) + "'";
          return scope.Close(v8::ThrowException(v8::String::New(msg.c_str())));
        }
      }
    }
    else {
      result += *ptr;
    }
  }

  for (size_t i = p;  i < len;  ++i) {
    TRI_Utf8ValueNFC text(TRI_UNKNOWN_MEM_ZONE, argv[i]);

    if (*text == 0) {
      string msg = StringUtils::itoa(i) + ".th argument must be a string";
      return scope.Close(v8::ThrowException(v8::String::New(msg.c_str())));
    }

    result += " ";
    result += *text;
  }

  return scope.Close(v8::String::New(result.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha256 sum
///
/// @FUN{internal.sha256(@FA{text})}
///
/// Computes an sha256 for the @FA{text}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Sha256 (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1 || ! argv[0]->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: sha256(<text>)")));
  }

  string key = TRI_ObjectToString(argv[0]);

  // create sha256
  char* hash = 0;
  size_t hashLen;

  SslInterface::sslSHA256(key.c_str(), key.size(), hash, hashLen);

  // as hex
  char* hex = 0;
  size_t hexLen;

  SslInterface::sslHEX(hash, hashLen, hex, hexLen);

  delete[] hash;

  // and return
  v8::Handle<v8::String> hashStr = v8::String::New(hex, hexLen);

  delete[] hex;

  return scope.Close(hashStr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current time
///
/// @FUN{internal.time()}
///
/// Returns the current time in seconds.
///
/// @verbinclude fluent36
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Time (v8::Arguments const& argv) {
  v8::HandleScope scope;

  return scope.Close(v8::Number::New(TRI_microtime()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current time
///
/// @FUN{internal.wait(@FA{seconds})}
///
/// Wait for @FA{seconds}, call the garbage collection.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Wait (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: wait(<seconds>)")));
  }

  double n = TRI_ObjectToDouble(argv[0]);
  double until = TRI_microtime() + n;

  v8::V8::LowMemoryNotification();
  while(! v8::V8::IdleNotification()) {
  }

  size_t i = 0;
  while (TRI_microtime() < until) {
    if (++i % 1000 == 0) {
      // garbage collection only every x iterations, otherwise we'll use too much CPU
      v8::V8::LowMemoryNotification();
      while(! v8::V8::IdleNotification()) {
      }
    }

    usleep(100);
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current request and connection statistics
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RequestStatistics (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> result = v8::Object::New();

  StatisticsCounter httpConnections;
  StatisticsDistribution connectionTime;

  TRI_FillConnectionStatistics(httpConnections, connectionTime);

  result->Set(v8::String::New("httpConnections"), v8::Number::New(httpConnections._count));
  FillDistribution(result, "connectionTime", connectionTime);

  StatisticsDistribution totalTime;
  StatisticsDistribution requestTime;
  StatisticsDistribution queueTime;
  StatisticsDistribution bytesSent;
  StatisticsDistribution bytesReceived;

  TRI_FillRequestStatistics(totalTime, requestTime, queueTime, bytesSent, bytesReceived);

  FillDistribution(result, "totalTime", totalTime);
  FillDistribution(result, "requestTime", requestTime);
  FillDistribution(result, "queueTime", queueTime);
  FillDistribution(result, "bytesSent", bytesSent);
  FillDistribution(result, "bytesReceived", bytesReceived);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Utils
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds attributes to array
////////////////////////////////////////////////////////////////////////////////

void TRI_AugmentObject (v8::Handle<v8::Value> value, TRI_json_t const* json) {
  v8::HandleScope scope;

  if (! value->IsObject()) {
    return;
  }

  if (json->_type != TRI_JSON_ARRAY) {
    return;
  }

  v8::Handle<v8::Object> object = value->ToObject();

  size_t n = json->_value._objects._length;

  for (size_t i = 0;  i < n;  i += 2) {
    TRI_json_t* key = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i);

    if (key->_type != TRI_JSON_STRING) {
      continue;
    }

    TRI_json_t* j = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i + 1);
    v8::Handle<v8::Value> val = TRI_ObjectJson(j);

    object->Set(v8::String::New(key->_value._string.data), val);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reports an exception
////////////////////////////////////////////////////////////////////////////////

string TRI_StringifyV8Exception (v8::TryCatch* tryCatch) {
  v8::HandleScope handle_scope;

  TRI_Utf8ValueNFC exception(TRI_UNKNOWN_MEM_ZONE, tryCatch->Exception());
  const char* exceptionString = *exception;
  v8::Handle<v8::Message> message = tryCatch->Message();
  string result;

  // V8 didn't provide any extra information about this error; just print the exception.
  if (message.IsEmpty()) {
    if (exceptionString == 0) {
      result = "JavaScript exception\n";
    }
    else {
      result = "JavaScript exception: " + string(exceptionString) + "\n";
    }
  }
  else {
    TRI_Utf8ValueNFC filename(TRI_UNKNOWN_MEM_ZONE, message->GetScriptResourceName());
    const char* filenameString = *filename;
    int linenum = message->GetLineNumber();
    int start = message->GetStartColumn() + 1;
    int end = message->GetEndColumn();

    if (filenameString == 0) {
      if (exceptionString == 0) {
        result = "JavaScript exception\n";
      }
      else {
        result = "JavaScript exception: " + string(exceptionString) + "\n";
      }
    }
    else {
      if (exceptionString == 0) {
        result = "JavaScript exception in file '" + string(filenameString) + "' at "
               + StringUtils::itoa(linenum) + "," + StringUtils::itoa(start) + "\n";
      }
      else {
        result = "JavaScript exception in file '" + string(filenameString) + "' at "
               + StringUtils::itoa(linenum) + "," + StringUtils::itoa(start)
               + ": " + exceptionString + "\n";
      }
    }

    TRI_Utf8ValueNFC sourceline(TRI_UNKNOWN_MEM_ZONE, message->GetSourceLine());

    if (*sourceline) {
      string l = *sourceline;

      result += "!" + l + "\n";

      if (1 < start) {
        l = string(start - 1, ' ');
      }
      else {
        l = "";
      }

      l += string((size_t)(end - start + 1), '^');

      result += "!" + l + "\n";
    }

    TRI_Utf8ValueNFC stacktrace(TRI_UNKNOWN_MEM_ZONE, tryCatch->StackTrace());

    if (*stacktrace && stacktrace.length() > 0) {
      result += "stacktrace: " + string(*stacktrace) + "\n";
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an exception and stacktrace
////////////////////////////////////////////////////////////////////////////////

void TRI_LogV8Exception (v8::TryCatch* tryCatch) {
  v8::HandleScope handle_scope;

  TRI_Utf8ValueNFC exception(TRI_UNKNOWN_MEM_ZONE, tryCatch->Exception());
  const char* exceptionString = *exception;
  v8::Handle<v8::Message> message = tryCatch->Message();

  // V8 didn't provide any extra information about this error; just print the exception.
  if (message.IsEmpty()) {
    if (exceptionString == 0) {
      LOG_ERROR("JavaScript exception");
    }
    else {
      LOG_ERROR("JavaScript exception: %s", exceptionString);
    }
  }
  else {
    TRI_Utf8ValueNFC filename(TRI_UNKNOWN_MEM_ZONE, message->GetScriptResourceName());
    const char* filenameString = *filename;
#ifdef TRI_ENABLE_LOGGER
    // if ifdef is not used, the compiler will complain about linenum being unused
    int linenum = message->GetLineNumber();
#endif
    int start = message->GetStartColumn() + 1;
    int end = message->GetEndColumn();

    if (filenameString == 0) {
      if (exceptionString == 0) {
        LOG_ERROR("JavaScript exception");
      }
      else {
        LOG_ERROR("JavaScript exception: %s", exceptionString);
      }
    }
    else {
      if (exceptionString == 0) {
        LOG_ERROR("JavaScript exception in file '%s' at %d,%d", filenameString, linenum, start);
      }
      else {
        LOG_ERROR("JavaScript exception in file '%s' at %d,%d: %s", filenameString, linenum, start, exceptionString);
      }
    }

    TRI_Utf8ValueNFC sourceline(TRI_UNKNOWN_MEM_ZONE, message->GetSourceLine());

    if (*sourceline) {
      string l = *sourceline;

      LOG_ERROR("!%s", l.c_str());

      if (1 < start) {
        l = string(start - 1, ' ');
      }
      else {
        l = "";
      }

      l += string((size_t)(end - start + 1), '^');

      LOG_ERROR("!%s", l.c_str());
    }

    TRI_Utf8ValueNFC stacktrace(TRI_UNKNOWN_MEM_ZONE, tryCatch->StackTrace());

    if (*stacktrace && stacktrace.length() > 0) {
      LOG_ERROR("stacktrace: %s", *stacktrace);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file into the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteGlobalJavaScriptFile (char const* filename) {
  return LoadJavaScriptFile(filename, true, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads all files from a directory into the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteGlobalJavaScriptDirectory (char const* path) {
  return LoadJavaScriptDirectory(path, true, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a file in a local context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteLocalJavaScriptFile (char const* filename) {
  return LoadJavaScriptFile(filename, true, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes all files from a directory in a local context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteLocalJavaScriptDirectory (char const* path) {
  return LoadJavaScriptDirectory(path, true, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseJavaScriptFile (char const* path) {
  return LoadJavaScriptDirectory(path, false, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a string within a V8 context, optionally print the result
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ExecuteJavaScriptString (v8::Handle<v8::Context> context,
                                                   v8::Handle<v8::String> source,
                                                   v8::Handle<v8::Value> name,
                                                   bool printResult) {
  v8::HandleScope scope;

  v8::Handle<v8::Value> result;
  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);

  // compilation failed, print errors that happened during compilation
  if (script.IsEmpty()) {
    return scope.Close(result);
  }

  // compilation succeeded, run the script
  result = script->Run();

  if (result.IsEmpty()) {
    return scope.Close(result);
  }
  else {

    // if all went well and the result wasn't undefined then print the returned value
    if (printResult && ! result->IsUndefined()) {
      v8::TryCatch tryCatch;

      v8::Handle<v8::String> printFuncName = v8::String::New("print");
      v8::Handle<v8::Function> print = v8::Handle<v8::Function>::Cast(context->Global()->Get(printFuncName));

      v8::Handle<v8::Value> args[] = { result };
      print->Call(print, 1, args);

      if (tryCatch.HasCaught()) {
        TRI_LogV8Exception(&tryCatch);
      }
    }

    return scope.Close(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error in a javascript object, based on error number only
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_CreateErrorObject (int errorNumber) {
  return CreateErrorObject(errorNumber, TRI_errno_string(errorNumber));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error in a javascript object, using supplied text
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_CreateErrorObject (int errorNumber, string const& message) {
  return CreateErrorObject(errorNumber, message);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error in a javascript object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_CreateErrorObject (int errorNumber, string const& message, bool autoPrepend) {
  if (autoPrepend) {
    return CreateErrorObject(errorNumber, message + ": " + string(TRI_errno_string(errorNumber)));
  }
  else {
    return CreateErrorObject(errorNumber, message);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize a v8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_normalize_V8_Obj (v8::Handle<v8::Value> obj) {
  v8::HandleScope scope;

  v8::String::Value str(obj);
  size_t str_len = str.length();
  if (str_len > 0) {
    UErrorCode erroCode = U_ZERO_ERROR;
    const Normalizer2* normalizer = Normalizer2::getInstance(NULL, "nfc", UNORM2_COMPOSE ,erroCode);

    if (U_FAILURE(erroCode)) {
      //LOGGER_ERROR << "error in Normalizer2::getNFCInstance(erroCode): " << u_errorName(erroCode);
      return scope.Close(v8::String::New(*str, str_len));
    }

    UnicodeString result = normalizer->normalize(UnicodeString((UChar*)(*str), str_len), erroCode);

    if (U_FAILURE(erroCode)) {
      //LOGGER_ERROR << "error in normalizer->normalize(UnicodeString(*str, str_len), erroCode): " << u_errorName(erroCode);
      return scope.Close(v8::String::New(*str, str_len));
    }

    // ..........................................................................
    // Take note here: we are assuming that the ICU type UChar is two bytes.
    // There is no guarantee that this will be the case on all platforms and
    // compilers. v8 expects uint16_t (2 bytes)
    // ..........................................................................

    return scope.Close(v8::String::New( (const uint16_t*)(result.getBuffer()), result.length()));
  }
  else {
    return scope.Close(v8::String::New(""));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the path list
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Array> TRI_V8PathList (string const& modules) {
  v8::HandleScope scope;

#ifdef _WIN32
  vector<string> paths = StringUtils::split(modules, ";", '\0');
#else
  vector<string> paths = StringUtils::split(modules, ";:");
#endif

  const uint32_t n = (uint32_t) paths.size();
  v8::Handle<v8::Array> result = v8::Array::New(n);

  for (uint32_t i = 0;  i < n;  ++i) {
    result->Set(i, v8::String::New(paths[i].c_str(), paths[i].size()));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 utils functions inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Utils (v8::Handle<v8::Context> context,
                      string const& modules,
                      string const& nodes,
                      string const& tempPath) {
  v8::HandleScope scope;

  v8::Handle<v8::FunctionTemplate> ft;
  v8::Handle<v8::ObjectTemplate> rt;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  if (v8g == 0) {
    // this check is necessary because when building arangosh, we do not include v8-vocbase and
    // this init function is the first one we call
    v8g = new TRI_v8_global_t;
    isolate->SetData(v8g);
  }

  TempPath = tempPath;

  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(context, "FS_CREATE_DIRECTORY", JS_CreateDirectory);
  TRI_AddGlobalFunctionVocbase(context, "FS_EXISTS", JS_Exists);
  TRI_AddGlobalFunctionVocbase(context, "FS_GET_TEMP_FILE", JS_GetTempFile);
  TRI_AddGlobalFunctionVocbase(context, "FS_GET_TEMP_PATH", JS_GetTempPath);
  TRI_AddGlobalFunctionVocbase(context, "FS_IS_DIRECTORY", JS_IsDirectory);
  TRI_AddGlobalFunctionVocbase(context, "FS_IS_FILE", JS_IsFile);
  TRI_AddGlobalFunctionVocbase(context, "FS_LIST_TREE", JS_ListTree);
  TRI_AddGlobalFunctionVocbase(context, "FS_MOVE", JS_Move);
  TRI_AddGlobalFunctionVocbase(context, "FS_REMOVE", JS_Remove);
  TRI_AddGlobalFunctionVocbase(context, "FS_UNZIP_FILE", JS_UnzipFile);
  TRI_AddGlobalFunctionVocbase(context, "FS_ZIP_FILE", JS_ZipFile);

  TRI_AddGlobalFunctionVocbase(context, "SYS_DOWNLOAD", JS_Download);
  TRI_AddGlobalFunctionVocbase(context, "SYS_EXECUTE", JS_Execute);
  TRI_AddGlobalFunctionVocbase(context, "SYS_GETLINE", JS_Getline);
  TRI_AddGlobalFunctionVocbase(context, "SYS_LOAD", JS_Load);
  TRI_AddGlobalFunctionVocbase(context, "SYS_LOG", JS_Log);
  TRI_AddGlobalFunctionVocbase(context, "SYS_LOG_LEVEL", JS_LogLevel);
  TRI_AddGlobalFunctionVocbase(context, "SYS_MD5", JS_Md5);
  TRI_AddGlobalFunctionVocbase(context, "SYS_GEN_RANDOM_NUMBERS", JS_RandomNumbers);
  TRI_AddGlobalFunctionVocbase(context, "SYS_GEN_RANDOM_ALPHA_NUMBERS", JS_RandomAlphaNum);
  TRI_AddGlobalFunctionVocbase(context, "SYS_GEN_RANDOM_SALT", JS_RandomSalt);
  TRI_AddGlobalFunctionVocbase(context, "SYS_CREATE_NONCE", JS_CreateNonce);
  TRI_AddGlobalFunctionVocbase(context, "SYS_CHECK_AND_MARK_NONCE", JS_MarkNonce);
  TRI_AddGlobalFunctionVocbase(context, "SYS_OUTPUT", JS_Output);
  TRI_AddGlobalFunctionVocbase(context, "SYS_PARSE", JS_Parse);
  TRI_AddGlobalFunctionVocbase(context, "SYS_PROCESS_STAT", JS_ProcessStat);
  TRI_AddGlobalFunctionVocbase(context, "SYS_RAND", JS_Rand);
  TRI_AddGlobalFunctionVocbase(context, "SYS_READ", JS_Read);
  TRI_AddGlobalFunctionVocbase(context, "SYS_REQUEST_STATISTICS", JS_RequestStatistics);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SAVE", JS_Save);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SHA256", JS_Sha256);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SPRINTF", JS_SPrintF);
  TRI_AddGlobalFunctionVocbase(context, "SYS_TIME", JS_Time);
  TRI_AddGlobalFunctionVocbase(context, "SYS_WAIT", JS_Wait);

  // .............................................................................
  // create the global variables
  // .............................................................................

  TRI_AddGlobalVariableVocbase(context, "HOME", v8::String::New(FileUtils::homeDirectory().c_str()));

  TRI_AddGlobalVariableVocbase(context, "MODULES_PATH", TRI_V8PathList(modules));
  TRI_AddGlobalVariableVocbase(context, "PACKAGE_PATH", TRI_V8PathList(nodes));
  TRI_AddGlobalVariableVocbase(context, "PATH_SEPARATOR", v8::String::New(TRI_DIR_SEPARATOR_STR));
  TRI_AddGlobalVariableVocbase(context, "VALGRIND", RUNNING_ON_VALGRIND > 0 ? v8::True() : v8::False());
  TRI_AddGlobalVariableVocbase(context, "VERSION", v8::String::New(TRIAGENS_VERSION));

  TRI_AddGlobalVariableVocbase(context, "CONNECTION_TIME_DISTRIBUTION", DistributionList(ConnectionTimeDistributionVector));
  TRI_AddGlobalVariableVocbase(context, "REQUEST_TIME_DISTRIBUTION", DistributionList(RequestTimeDistributionVector));
  TRI_AddGlobalVariableVocbase(context, "BYTES_SENT_DISTRIBUTION", DistributionList(BytesSentDistributionVector));
  TRI_AddGlobalVariableVocbase(context, "BYTES_RECEIVED_DISTRIBUTION", DistributionList(BytesReceivedDistributionVector));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
