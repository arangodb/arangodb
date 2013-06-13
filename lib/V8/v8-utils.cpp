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

#include "v8-utils.h"

#include "build.h"

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
#include "BasicsC/tri-strings.h"
#include "BasicsC/tri-zip.h"
#include "BasicsC/utf8-helper.h"
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

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a Javascript error object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> CreateErrorObject (int errorNumber, string const& message) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::String> errorMessage = v8::String::New(message.c_str(), message.size());

  v8::Handle<v8::Object> errorObject = v8::Exception::Error(errorMessage)->ToObject();

  errorObject->Set(v8::String::New("errorNum"), v8::Number::New(errorNumber));
  errorObject->Set(v8::String::New("errorMessage"), errorMessage);

  v8::Handle<v8::Value> proto = v8g->ErrorTempl->NewInstance();
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

  result->Set(TRI_V8_SYMBOL("sum"), v8::Number::New(dist._total));
  result->Set(TRI_V8_SYMBOL("count"), v8::Number::New(dist._count));

  v8::Handle<v8::Array> counts = v8::Array::New(dist._counts.size());
  size_t pos = 0;

  for (vector<uint64_t>::const_iterator i = dist._counts.begin();  i != dist._counts.end();  ++i, ++pos) {
    counts->Set(pos, v8::Number::New(*i));
  }

  result->Set(TRI_V8_SYMBOL("counts"), counts);

  list->Set(TRI_V8_SYMBOL(name), result);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

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
    TRI_V8_EXCEPTION_USAGE(scope, "parse(<script>)");
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
    TRI_V8_TYPE_ERROR(scope, "<script> must be a string");
  }

  v8::Handle<v8::Script> script = v8::Script::Compile(source->ToString(), filename);

  // compilation failed, we have caught an exception
  if (tryCatch.HasCaught()) {
    string err = TRI_StringifyV8Exception(&tryCatch);

    TRI_V8_SYNTAX_ERROR(scope, err.c_str());
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
/// @FUN{internal.download(@FA{url}, @FA{body}, @FA{options}, @FA{outfile})}
///
/// Downloads the data from the URL specified by @FA{url} and saves the
/// response body to @FA{outfile}. The following @FA{options} are supported:
///
/// - @LIT{method}: the HTTP method to be used. The supported HTTP methods are 
///   @LIT{DELETE}, @LIT{GET}, @LIT{HEAD}, @LIT{POST}, @LIT{PUT}, @LIT{PATCH}
///
/// - @LIT{timeout}: a timeout value for the connection
///
/// - @LIT{followRedirects}: whether or not to follow redirects
///
/// - @LIT{returnBodyOnError}: whether or not to return / save body on HTTP
///   error
///
/// - @LIT{headers}: an optional array of headers to be sent for the first
///   (non-redirect) request.
///
/// Up to 5 redirects will be followed. Any user-defined headers will only be
/// sent for the first request. If no timeout is given, a default timeout will
/// be used.
///
/// If @FA{outfile} is specified, the result body will be saved in a file
/// specified by @FA{outfile}. If @FA{outfile} already exists, an error will
/// be thrown.
///
/// If @FA{outfile} is not specified, the result body will be returned in the
/// @LIT{body} attribute of the result object.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Download (v8::Arguments const& argv) {
  v8::HandleScope scope;

  const string signature = "download(<url>, <body>, <options>, <outfile>)";

  if (argv.Length() < 3) {
    TRI_V8_EXCEPTION_USAGE(scope, signature);
  }

  string url = TRI_ObjectToString(argv[0]);

  string body;
  if (argv[1]->IsString() || argv[1]->IsStringObject()) {
    body = TRI_ObjectToString(argv[1]);
  }

  // options
  // ------------------------------------------------------------------------

  if (! argv[2]->IsObject()) {
    TRI_V8_EXCEPTION_USAGE(scope, signature);
  }

  v8::Handle<v8::Array> options = v8::Handle<v8::Array>::Cast(argv[2]);
  if (options.IsEmpty()) {
    TRI_V8_EXCEPTION_USAGE(scope, signature);
  }

  // method
  HttpRequest::HttpRequestType method = HttpRequest::HTTP_REQUEST_GET;
  if (options->Has(TRI_V8_SYMBOL("method"))) {
    string methodString = TRI_ObjectToString(options->Get(TRI_V8_SYMBOL("method")));
  
    method = HttpRequest::translateMethod(methodString);
  }

  // headers
  map<string, string> headerFields;
  if (options->Has(TRI_V8_SYMBOL("headers"))) {
    v8::Handle<v8::Object> v8Headers = options->Get(TRI_V8_SYMBOL("headers")).As<v8::Object> ();
    if (v8Headers->IsObject()) {
      v8::Handle<v8::Array> props = v8Headers->GetPropertyNames();

      for (uint32_t i = 0; i < props->Length(); i++) {
        v8::Handle<v8::Value> key = props->Get(v8::Integer::New(i));
        headerFields[TRI_ObjectToString(key)] = TRI_ObjectToString(v8Headers->Get(key));
      }
    }
  }

  // timeout
  double timeout = 10.0;
  if (options->Has(TRI_V8_SYMBOL("timeout"))) {
    if (! options->Get(TRI_V8_SYMBOL("timeout"))->IsNumber()) {
      TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_BAD_PARAMETER, "invalid option value for timeout");
    }

    timeout = TRI_ObjectToDouble(options->Get(TRI_V8_SYMBOL("timeout")));
  }

  // follow redirects
  bool followRedirects = true;
  if (options->Has(TRI_V8_SYMBOL("followRedirects"))) {
    followRedirects = TRI_ObjectToBoolean(options->Get(TRI_V8_SYMBOL("followRedirects")));
  }

  if (body.size() > 0 &&
      (method == HttpRequest::HTTP_REQUEST_GET ||
       method == HttpRequest::HTTP_REQUEST_HEAD)) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_BAD_PARAMETER, "should not provide a body value for this request method");
  }

  bool returnBodyOnError = false;
  if (options->Has(TRI_V8_SYMBOL("returnBodyOnError"))) {
    returnBodyOnError = TRI_ObjectToBoolean(options->Get(TRI_V8_SYMBOL("returnBodyOnError")));
  }


  // outfile
  string outfile;
  if (argv.Length() == 4) {
    if (argv[3]->IsString() || argv[3]->IsStringObject()) {
      outfile = TRI_ObjectToString(argv[3]);
    }

    if (outfile == "") {
      TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_BAD_PARAMETER, "invalid value provided for outfile");
    }

    if (TRI_ExistsFile(outfile.c_str())) {
      TRI_V8_EXCEPTION(scope, TRI_ERROR_CANNOT_OVERWRITE_FILE);
    }
  }


  int numRedirects = 0;

  while (numRedirects < 5) {
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
      TRI_V8_SYNTAX_ERROR(scope, "unsupported URL specified");
    }

    LOGGER_TRACE("downloading file. endpoint: " << endpoint << ", relative URL: " << url);

    GeneralClientConnection* connection = GeneralClientConnection::factory(Endpoint::clientFactory(endpoint), timeout, timeout, 3);

    if (connection == 0) {
      TRI_V8_EXCEPTION_MEMORY(scope);
    }

    SimpleHttpClient client(connection, timeout, false);

    v8::Handle<v8::Object> result = v8::Object::New();
    
    if (numRedirects > 0) {
      // do not send extra headers now
      headerFields.clear();
    }

    // send the actual request
    SimpleHttpResult* response = client.request(method, 
                                                relative, 
                                                (body.size() > 0 ? body.c_str() : 0), 
                                                body.size(), 
                                                headerFields);

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

      // follow redirects?
      if (followRedirects && 
          (returnCode == 301 || returnCode == 302)) {
        bool found;
        url = response->getHeaderField(string("location"), found);

        delete response;
        delete connection;

        if (! found) {
          TRI_V8_EXCEPTION_INTERNAL(scope, "caught invalid redirect URL");
        }

        numRedirects++;
        continue;
      }

      result->Set(v8::String::New("code"),    v8::Number::New(returnCode));
      result->Set(v8::String::New("message"), v8::String::New(returnMessage.c_str()));

      // process response headers
      const map<string, string> responseHeaders = response->getHeaderFields();
      map<string, string>::const_iterator it;

      v8::Handle<v8::Object> headers = v8::Object::New();
      for (it = responseHeaders.begin(); it != responseHeaders.end(); ++it) {
        headers->Set(v8::String::New((*it).first.c_str()), v8::String::New((*it).second.c_str()));
      }
      result->Set(v8::String::New("headers"), headers);


      if (returnBodyOnError || (returnCode >= 200 && returnCode <= 299)) {
        try {
          if (outfile.size() > 0) {
            // save outfile
            FileUtils::spit(outfile, response->getBody().str());
          }
          else {
            // set "body" attribute in result
            result->Set(v8::String::New("body"), v8::String::New(response->getBody().str().c_str(), response->getBody().str().length()));
          }
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

  TRI_V8_EXCEPTION_INTERNAL(scope, "too many redirects");
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
    TRI_V8_EXCEPTION_USAGE(scope, "execute(<script>, <sandbox>, <filename>)");
  }

  v8::Handle<v8::Value> source = argv[0];
  v8::Handle<v8::Value> sandboxValue = argv[1];
  v8::Handle<v8::Value> filename = argv[2];

  if (! source->IsString()) {
    TRI_V8_TYPE_ERROR(scope, "<script> must be a string");
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
    TRI_V8_EXCEPTION_USAGE(scope, "exists(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<path> must be a string");
  }

  return scope.Close(TRI_ExistsFile(*name) ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the size of a file
///
/// @FUN{fs.size(@FA{path})}
///
/// Returns the size of the file specified by @FA{path}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SizeFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "size(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<path> must be a string");
  }

  if (! TRI_ExistsFile(*name) || TRI_IsDirectory(*name)) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_FILE_NOT_FOUND);
  }

  int64_t size = TRI_SizeFile(*name);

  if (size < 0) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_FILE_NOT_FOUND);
  }

  return scope.Close(v8::Number::New((double) size));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads in a line from stdin
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
    TRI_V8_EXCEPTION_USAGE(scope, "getTempPath()");
  }

  char* path = TRI_GetUserTempPath();

  if (path == 0) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }
  
  v8::Handle<v8::Value> result = v8::String::New(path);
  TRI_Free(TRI_CORE_MEM_ZONE, path);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name for a (new) temporary file
///
/// @FUN{fs.getTempFile(@FA{directory}, @FA{createFile})}
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
    TRI_V8_EXCEPTION_USAGE(scope, "getTempFile(<directory>, <createFile>)");
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
    TRI_V8_EXCEPTION_INTERNAL(scope, "could not create temp file");
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
    TRI_V8_EXCEPTION_USAGE(scope, "isDirectory(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<path> must be a string");
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
    TRI_V8_EXCEPTION_USAGE(scope, "isFile(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<path> must be a string");
  }

  // return result
  return scope.Close((TRI_ExistsFile(*name) && ! TRI_IsDirectory(*name)) ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the directory tree
///
/// @FUN{fs.list(@FA{path})}
///
/// The functions returns the names of all the files in a directory, in
/// lexically sorted order. Throws an exception if the directory cannot be
/// traversed (or path is not a directory).
///
/// Note: this means that list("x") of a directory containing "a" and "b" would
/// return ["a", "b"], not ["x/a", "x/b"].
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_List (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "listTree(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<path> must be a string");
  }

  // constructed listing
  v8::Handle<v8::Array> result = v8::Array::New();
  TRI_vector_string_t list = TRI_FilesDirectory(*name);

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
    TRI_V8_EXCEPTION_USAGE(scope, "listTree(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<path> must be a string");
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
/// @brief create a directory
///
/// @FUN{fs.makeDirectory(@FA{path})}
///
/// Creates the directory specified by @FA{path}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_MakeDirectory (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // 2nd argument (permissions) are ignored for now

  // extract arguments
  if (argv.Length() != 1 && argv.Length() != 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "makeDirectory(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<path> must be a string");
  }

  bool result = TRI_CreateDirectory(*name);

  if (! result) {
    TRI_V8_EXCEPTION_SYS(scope, "cannot create directory");
  }

  return scope.Close(v8::Undefined());
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
    TRI_V8_EXCEPTION_USAGE(scope, "unzip(<filename>, <outPath>, <skipPaths>, <overwrite>, <password>)");
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

  TRI_V8_EXCEPTION(scope, res);
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
  if (argv.Length() < 3 || argv.Length() > 4) {
    TRI_V8_EXCEPTION_USAGE(scope, "zip(<filename>, <chdir>, <files>, <password>)");
  }

  const string filename = TRI_ObjectToString(argv[0]);
  const string dir = TRI_ObjectToString(argv[1]);

  if (! argv[2]->IsArray()) {
    TRI_V8_EXCEPTION_USAGE(scope, "zip(<filename>, <chdir>, <files>, <password>)");
  }

  v8::Handle<v8::Array> files = v8::Handle<v8::Array>::Cast(argv[2]);

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
    TRI_DestroyVectorString(&filenames);

    TRI_V8_EXCEPTION_USAGE(scope, "zip(<filename>, <files>, <password>)");
  }

  const char* p;
  string password;
  if (argv.Length() == 4) {
    password = TRI_ObjectToString(argv[3]);
    p = password.c_str();
  }
  else {
    p = NULL;
  }

  res = TRI_ZipFile(filename.c_str(), dir.c_str(), &filenames, p);
  TRI_DestroyVectorString(&filenames);

  if (res == TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::True());
  }

  TRI_V8_EXCEPTION(scope, res);
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
    TRI_V8_EXCEPTION_USAGE(scope, "load(<filename>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<filename> must be a string");
  }

  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *name, NULL);

  if (content == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "cannot read file");
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
    TRI_V8_EXCEPTION_USAGE(scope, "log(<level>, <message>)");
  }

  TRI_Utf8ValueNFC level(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*level == 0) {
    TRI_V8_TYPE_ERROR(scope, "<level> must be a string");
  }

  TRI_Utf8ValueNFC message(TRI_UNKNOWN_MEM_ZONE, argv[1]);

  if (*message == 0) {
    TRI_V8_TYPE_ERROR(scope, "<message> must be a string");
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
    TRI_V8_EXCEPTION_USAGE(scope, "md5(<text>)");
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
    TRI_V8_EXCEPTION_USAGE(scope, "genRandomNumbers(<length>)");
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
    TRI_V8_EXCEPTION_USAGE(scope, "genRandomAlphaNumbers(<length>)");
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
    TRI_V8_EXCEPTION_USAGE(scope, "genRandomSalt()");
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
    TRI_V8_EXCEPTION_USAGE(scope, "createNonce()");
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
    TRI_V8_EXCEPTION_USAGE(scope, "checkAndMarkNonce(<nonce>)");
  }

  TRI_Utf8ValueNFC base64u(TRI_CORE_MEM_ZONE, argv[0]);

  if (base64u.length() != 16) {
    TRI_V8_TYPE_ERROR(scope, "expecting 16-Byte base64url-encoded nonce");
  }

  string raw = StringUtils::decodeBase64U(*base64u);

  if (raw.size() != 12) {
    TRI_V8_TYPE_ERROR(scope, "expecting 12-Byte nonce");
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
    TRI_V8_EXCEPTION_USAGE(scope, "move(<source>, <destination>)");
  }

  string source = TRI_ObjectToString(argv[0]);
  string destination = TRI_ObjectToString(argv[1]);

  int res = TRI_RenameFile(source.c_str(), destination.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot move file");
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
/// @FUN{internal.processStatistics()}
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
/// - numberOfThreads: Number of threads in this process.
///
/// - residentSize: Resident Set Size: total size of the number of pages 
///   the process has in real memory.  This is just the pages which count 
///   toward text, data, or stack space.  This does not include pages which 
///   have not been demand-loaded in, or which are swapped out.
///
///   The resident set size is reported in bytes.
///
/// - virtualSize: Virtual memory size in bytes.
///
/// @verbinclude system1
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ProcessStatistics (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> result = v8::Object::New();

  TRI_process_info_t info = TRI_ProcessInfoSelf();

  result->Set(v8::String::New("minorPageFaults"), v8::Number::New((double) info._minorPageFaults));
  result->Set(v8::String::New("majorPageFaults"), v8::Number::New((double) info._majorPageFaults));
  result->Set(v8::String::New("userTime"), v8::Number::New((double) info._userTime / (double) info._scClkTck));
  result->Set(v8::String::New("systemTime"), v8::Number::New((double) info._systemTime / (double) info._scClkTck));
  result->Set(v8::String::New("numberOfThreads"), v8::Number::New((double) info._numberThreads));
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
    TRI_V8_EXCEPTION_USAGE(scope, "rand()");
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
/// @FUN{fs.read(@FA{filename})}
///
/// Reads in a file and returns the content as string. Please note that the
/// file content must be encoded in UTF-8.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Read (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "read(<filename>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<filename> must be a UTF-8 string");
  }

  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *name, NULL);

  if (content == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), TRI_last_error());
  }

  v8::Handle<v8::String> result = v8::String::New(content);

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads in a file as base64
///
/// @FUN{fs.read64(@FA{filename})}
///
/// Reads in a file and returns the content as string. The file content is
/// Base64 encoded.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Read64 (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "read(<filename>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<filename> must be a UTF-8 string");
  }

  string base64;

  try {
    string content = FileUtils::slurp(*name);
    base64 = StringUtils::encodeBase64(content);
  }
  catch (...) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), TRI_last_error());
  }

  return scope.Close(v8::String::New(base64.c_str()));
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
    TRI_V8_EXCEPTION_USAGE(scope, "save(<filename>, <content>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<filename> must be a string");
  }

  TRI_Utf8ValueNFC content(TRI_UNKNOWN_MEM_ZONE, argv[1]);

  if (*content == 0) {
    TRI_V8_TYPE_ERROR(scope, "<content> must be a string");
  }

  ofstream file;

  file.open(*name, ios::out | ios::binary);

  if (file.is_open()) {
    file << *content;
    file.close();
    return scope.Close(v8::True());
  }

  TRI_V8_EXCEPTION_SYS(scope, "cannot write file");
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

  // extract the arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "remove(<filename>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<path> must be a string");
  }

  int res = TRI_UnlinkFile(*name);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot remove file");
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an empty directory
///
/// @FUN{fs.removeDirectory(@FA{path})}
///
/// Removes a directory if it is empty. Throws an exception if the path is not
/// an empty directory.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RemoveDirectory (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "removeDirectory(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<path> must be a string");
  }

  if (! TRI_IsDirectory(*name)) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "<path> must be a valid directory name");
  }

  int res = TRI_RemoveEmptyDirectory(*name);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot remove directory");
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a directory
///
/// @FUN{fs.removeDirectoryRecursive(@FA{path})}
///
/// Removes a directory with all subelements. Throws an exception if the path
/// is not a directory.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RemoveRecursiveDirectory (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "removeDirectoryRecursive(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<path> must be a string");
  }

  if (! TRI_IsDirectory(*name)) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "<path> must be a valid directory name");
  }

  char* tempPath = TRI_GetUserTempPath();
  
  if (tempPath == NULL || strlen(tempPath) < 6) {
    // some security measure so we don't accidently delete all our files
    TRI_FreeString(TRI_CORE_MEM_ZONE, tempPath);

    TRI_V8_EXCEPTION_PARAMETER(scope, "temporary directory name is too short. will not remove directory");
  }

  const string path(*name);
  if (! TRI_EqualString2(path.c_str(), tempPath, strlen(tempPath))) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, tempPath);

    TRI_V8_EXCEPTION_PARAMETER(scope, "directory to be removed is outside of temporary path");
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, tempPath);

  int res = TRI_RemoveDirectory(*name);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot remove directory");
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns server statistics
///
/// @FUN{internal.serverStatistics()}
///
/// Returns information about the server:
///
/// - `uptime`: time since server start in seconds.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ServerStatistics (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_server_statistics_t info = TRI_GetServerStatistics();
  
  v8::Handle<v8::Object> result = v8::Object::New();

  result->Set(v8::String::New("uptime"), v8::Number::New((double) info._uptime));

  return scope.Close(result);
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
    TRI_V8_TYPE_ERROR(scope, "<format> must be a UTF-8 string");
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
            string msg = string("not enough arguments for ") + *ptr;
            TRI_V8_EXCEPTION_PARAMETER(scope, msg.c_str());
          }

          bool e;
          double f = TRI_ObjectToDouble(argv[p], e);

          if (e) {
            string msg = StringUtils::itoa(p) + ".th argument must be a number";
            TRI_V8_EXCEPTION_PARAMETER(scope, msg.c_str());
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
            string msg = string("not enough arguments for ") + *ptr;
            TRI_V8_EXCEPTION_PARAMETER(scope, msg.c_str());
          }

          TRI_Utf8ValueNFC text(TRI_UNKNOWN_MEM_ZONE, argv[p]);

          if (*text == 0) {
            string msg = StringUtils::itoa(p) + ".th argument must be a string";
            TRI_V8_EXCEPTION_PARAMETER(scope, msg.c_str());
          }

          ++p;

          result += *text;

          break;
        }

        default: {
          string msg = "found illegal format directive '" + string(1, *ptr) + "'";
          TRI_V8_EXCEPTION_PARAMETER(scope, msg.c_str());
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
      TRI_V8_TYPE_ERROR(scope, msg.c_str());
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
    TRI_V8_EXCEPTION_USAGE(scope, "sha256(<text>)");
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
    TRI_V8_EXCEPTION_USAGE(scope, "wait(<seconds>)");
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
/// @brief set a failure point
///
/// @FUN{internal.debugSetFailAt(@FA{point})}
///
/// Set a point for an intentional system failure
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DebugSetFailAt (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "debugSetFailAt(<point>)");
  }

  string point = TRI_ObjectToString(argv[0]);

  TRI_AddFailurePointDebugging(point.c_str());

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a failure point
///
/// @FUN{internal.debugRemoveFailAt(@FA{point})}
///
/// Remove a point for an intentional system failure
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DebugRemoveFailAt (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "debugRemoveFailAt(<point>)");
  }

  string point = TRI_ObjectToString(argv[0]);

  TRI_RemoveFailurePointDebugging(point.c_str());

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clear all failure points
///
/// @FUN{internal.debugClearFailAt()}
///
/// Remove all points for intentional system failures
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DebugClearFailAt (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "debugClearFailAt()");
  }

  TRI_ClearFailurePointsDebugging();

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether failure points can be used
///
/// @FUN{internal.debugCanUseFailAt()}
///
/// Returns whether failure points can be be used
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DebugCanUseFailAt (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "debugCanUseFailAt()");
  }

  return scope.Close(TRI_CanUseFailurePointsDebugging() ? v8::True() : v8::False());
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
/// @brief ArangoError
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ArangoError (const v8::Arguments& args) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> self = args.Holder()->ToObject();

  self->Set(v8g->ErrorKey, v8::True());
  self->Set(v8g->ErrorNumKey, v8::Integer::New(TRI_ERROR_FAILED));

  if (0 < args.Length() && args[0]->IsObject()) {
    v8::Handle<v8::Object> data = args[0]->ToObject();

    if (data->Has(v8g->ErrorKey)) {
      self->Set(v8g->ErrorKey, data->Get(v8g->ErrorKey));
    }

    if (data->Has(v8g->CodeKey)) {
      self->Set(v8g->CodeKey, data->Get(v8g->CodeKey));
    }

    if (data->Has(v8g->ErrorNumKey)) {
      self->Set(v8g->ErrorNumKey, data->Get(v8g->ErrorNumKey));
    }

    if (data->Has(v8g->ErrorMessageKey)) {
      self->Set(v8g->ErrorMessageKey, data->Get(v8g->ErrorMessageKey));
    }
  }

  return scope.Close(self);
} 

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

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
  v8::HandleScope scope;

  return scope.Close(CreateErrorObject(errorNumber, TRI_errno_string(errorNumber)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error in a javascript object, using supplied text
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_CreateErrorObject (int errorNumber, string const& message) {
  v8::HandleScope scope;

  return scope.Close(CreateErrorObject(errorNumber, message));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error in a javascript object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_CreateErrorObject (int errorNumber, string const& message, bool autoPrepend) {
  v8::HandleScope scope;

  if (autoPrepend) {
    return scope.Close(CreateErrorObject(errorNumber, message + ": " + string(TRI_errno_string(errorNumber))));
  }
  else {
    return scope.Close(CreateErrorObject(errorNumber, message));
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
                      string const& packages) {
  v8::HandleScope scope;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = TRI_CreateV8Globals(isolate);

  v8::Handle<v8::FunctionTemplate> ft;
  v8::Handle<v8::ObjectTemplate> rt;

  // .............................................................................
  // generate the general error template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoError"));
  ft->SetCallHandler(JS_ArangoError);

  // ArangoError is a "sub-class" of Error
  v8::Handle<v8::Function> ArangoErrorFunc = ft->GetFunction();
  v8::Handle<v8::Value> ErrorObject = context->Global()->Get(TRI_V8_STRING("Error"));
  v8::Handle<v8::Value> ErrorPrototype = ErrorObject->ToObject()->Get(TRI_V8_STRING("prototype"));

  ArangoErrorFunc->Get(TRI_V8_SYMBOL("prototype"))->ToObject()->SetPrototype(ErrorPrototype);

  TRI_AddGlobalFunctionVocbase(context, "ArangoError", ArangoErrorFunc);

  rt = ft->InstanceTemplate();
  v8g->ErrorTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);

  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(context, "FS_EXISTS", JS_Exists);
  TRI_AddGlobalFunctionVocbase(context, "FS_GET_TEMP_FILE", JS_GetTempFile);
  TRI_AddGlobalFunctionVocbase(context, "FS_GET_TEMP_PATH", JS_GetTempPath);
  TRI_AddGlobalFunctionVocbase(context, "FS_IS_DIRECTORY", JS_IsDirectory);
  TRI_AddGlobalFunctionVocbase(context, "FS_IS_FILE", JS_IsFile);
  TRI_AddGlobalFunctionVocbase(context, "FS_LIST", JS_List);
  TRI_AddGlobalFunctionVocbase(context, "FS_LIST_TREE", JS_ListTree);
  TRI_AddGlobalFunctionVocbase(context, "FS_MAKE_DIRECTORY", JS_MakeDirectory);
  TRI_AddGlobalFunctionVocbase(context, "FS_MOVE", JS_Move);
  TRI_AddGlobalFunctionVocbase(context, "FS_REMOVE", JS_Remove);
  TRI_AddGlobalFunctionVocbase(context, "FS_REMOVE_DIRECTORY", JS_RemoveDirectory);
  TRI_AddGlobalFunctionVocbase(context, "FS_REMOVE_RECURSIVE_DIRECTORY", JS_RemoveRecursiveDirectory);
  TRI_AddGlobalFunctionVocbase(context, "FS_FILESIZE", JS_SizeFile);
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
  TRI_AddGlobalFunctionVocbase(context, "SYS_PROCESS_STATISTICS", JS_ProcessStatistics);
  TRI_AddGlobalFunctionVocbase(context, "SYS_RAND", JS_Rand);
  TRI_AddGlobalFunctionVocbase(context, "SYS_READ", JS_Read);
  TRI_AddGlobalFunctionVocbase(context, "SYS_READ64", JS_Read64);
  TRI_AddGlobalFunctionVocbase(context, "SYS_REQUEST_STATISTICS", JS_RequestStatistics);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SAVE", JS_Save);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SERVER_STATISTICS", JS_ServerStatistics);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SHA256", JS_Sha256);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SPRINTF", JS_SPrintF);
  TRI_AddGlobalFunctionVocbase(context, "SYS_TIME", JS_Time);
  TRI_AddGlobalFunctionVocbase(context, "SYS_WAIT", JS_Wait);

  // debugging functions
  TRI_AddGlobalFunctionVocbase(context, "SYS_DEBUG_SET_FAILAT", JS_DebugSetFailAt);
  TRI_AddGlobalFunctionVocbase(context, "SYS_DEBUG_REMOVE_FAILAT", JS_DebugRemoveFailAt);
  TRI_AddGlobalFunctionVocbase(context, "SYS_DEBUG_CLEAR_FAILAT", JS_DebugClearFailAt);
  TRI_AddGlobalFunctionVocbase(context, "SYS_DEBUG_CAN_USE_FAILAT", JS_DebugCanUseFailAt);

  // .............................................................................
  // create the global variables
  // .............................................................................

  TRI_AddGlobalVariableVocbase(context, "HOME", v8::String::New(FileUtils::homeDirectory().c_str()));

  TRI_AddGlobalVariableVocbase(context, "MODULES_PATH", TRI_V8PathList(modules));
  TRI_AddGlobalVariableVocbase(context, "PACKAGE_PATH", TRI_V8PathList(packages));
  TRI_AddGlobalVariableVocbase(context, "PATH_SEPARATOR", v8::String::New(TRI_DIR_SEPARATOR_STR));
  TRI_AddGlobalVariableVocbase(context, "VALGRIND", RUNNING_ON_VALGRIND > 0 ? v8::True() : v8::False());
  TRI_AddGlobalVariableVocbase(context, "VERSION", v8::String::New(TRIAGENS_VERSION));

  TRI_AddGlobalVariableVocbase(context, "CONNECTION_TIME_DISTRIBUTION", DistributionList(ConnectionTimeDistributionVector));
  TRI_AddGlobalVariableVocbase(context, "REQUEST_TIME_DISTRIBUTION", DistributionList(RequestTimeDistributionVector));
  TRI_AddGlobalVariableVocbase(context, "BYTES_SENT_DISTRIBUTION", DistributionList(BytesSentDistributionVector));
  TRI_AddGlobalVariableVocbase(context, "BYTES_RECEIVED_DISTRIBUTION", DistributionList(BytesReceivedDistributionVector));

  TRI_AddGlobalVariableVocbase(context, "SYS_PLATFORM", v8::String::New(TRI_PLATFORM));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
