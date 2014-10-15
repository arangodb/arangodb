////////////////////////////////////////////////////////////////////////////////
/// @brief V8 utility functions
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
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "v8-utils.h"
#include "v8-buffer.h"

#include <regex.h>

#include "Basics/Dictionary.h"
#include "Basics/Nonce.h"
#include "Basics/RandomGenerator.h"
#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "Basics/csv.h"
#include "Basics/files.h"
#include "Basics/logging.h"
#include "Basics/process-utils.h"
#include "Basics/string-buffer.h"
#include "Basics/tri-strings.h"
#include "Basics/tri-zip.h"
#include "Basics/utf8-helper.h"
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
using namespace triagens::arango;
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

TRI_Utf8ValueNFC::TRI_Utf8ValueNFC (TRI_memory_zone_t* memoryZone,
                                    v8::Handle<v8::Value> const obj)
  : _str(nullptr),
    _length(0),
    _memoryZone(memoryZone) {

   v8::String::Value str(obj);
   int str_len = str.length();

   _str = TRI_normalize_utf16_to_NFC(_memoryZone, *str, (size_t) str_len, &_length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Destroys a normalized string object
////////////////////////////////////////////////////////////////////////////////

TRI_Utf8ValueNFC::~TRI_Utf8ValueNFC () {
  if (_str != nullptr) {
    TRI_Free(_memoryZone, _str);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a Javascript error object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> CreateErrorObject (int errorNumber,
                                                 string const& message,
                                                 const char* file,
                                                 int line) {
  v8::HandleScope scope;

  if (errorNumber == TRI_ERROR_OUT_OF_MEMORY) {
    LOG_ERROR("encountered out-of-memory error in %s at line %d", file, line);
  }

  v8::Handle<v8::String> errorMessage = v8::String::New(message.c_str(), (int) message.size());

  if (errorMessage.IsEmpty()) {
    return scope.Close(v8::Object::New());
  }

  v8::Handle<v8::Value> err = v8::Exception::Error(errorMessage);

  if (err.IsEmpty()) {
    return scope.Close(v8::Object::New());
  }

  v8::Handle<v8::Object> errorObject = err->ToObject();

  if (errorObject.IsEmpty()) {
    return scope.Close(v8::Object::New());
  }

  errorObject->Set(v8::String::New("errorNum"), v8::Number::New(errorNumber));
  errorObject->Set(v8::String::New("errorMessage"), errorMessage);

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  v8::Handle<v8::Value> proto = v8g->ArangoErrorTempl->NewInstance();

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

  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, filename, &length);

  if (content == nullptr) {
    LOG_TRACE("cannot load java script file '%s': %s", filename, TRI_last_error());
    return false;
  }

  if (useGlobalContext) {
    char const* prologue = "(function() { ";
    char const* epilogue = "/* end-of-file */ })()";

    char* contentWrapper = TRI_Concatenate3StringZ(TRI_UNKNOWN_MEM_ZONE,
                                                   prologue,
                                                   content,
                                                   epilogue);

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

    length += strlen(prologue) + strlen(epilogue);
    content = contentWrapper;
  }

  if (content == nullptr) {
    LOG_TRACE("cannot load java script file '%s': %s", filename, TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));
    return false;
  }

  v8::Handle<v8::String> name = v8::String::New(filename);
  v8::Handle<v8::String> source = v8::String::New(content, (int) length);

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

    if (regexec(&re, filename, 0, 0, 0) != 0) {
      continue;
    }

    full = TRI_Concatenate2File(path, filename);

    ok = LoadJavaScriptFile(full, execute, useGlobalContext);
    TRI_FreeString(TRI_CORE_MEM_ZONE, full);

    result = result && ok;

    if (! ok) {
      if (tryCatch.CanContinue()) {
        TRI_LogV8Exception(&tryCatch);
      }
      else {
        TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

        v8g->_canceled = true;
      }
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
  result->Set(TRI_V8_SYMBOL("count"), v8::Number::New((double) dist._count));

  v8::Handle<v8::Array> counts = v8::Array::New((int) dist._counts.size());
  uint32_t pos = 0;

  for (vector<uint64_t>::const_iterator i = dist._counts.begin();  i != dist._counts.end();  ++i, ++pos) {
    counts->Set(pos, v8::Number::New((double) *i));
  }

  result->Set(TRI_V8_SYMBOL("counts"), counts);

  list->Set(TRI_V8_SYMBOL(name), result);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief decodes a base64-encoded string
///
/// @FUN{internal.base64Decode(@FA{value})}
///
/// Base64-decodes the string @FA{value}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Base64Decode (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "base64Decode(<value>)");
  }

  try {
    string const value = TRI_ObjectToString(argv[0]);
    string const base64 = StringUtils::decodeBase64(value);

    return scope.Close(v8::String::New(base64.c_str(), (int) base64.size()));
  }
  catch (...) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), TRI_last_error());
  }

  TRI_ASSERT(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief encodes a string as base64
///
/// @FUN{internal.base64Encode(@FA{value})}
///
/// Base64-encodes the string @FA{value}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Base64Encode (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "base64Encode(<value>)");
  }

  try {
    string const&& value = TRI_ObjectToString(argv[0]);
    string const&& base64 = StringUtils::encodeBase64(value);

    return scope.Close(v8::String::New(base64.c_str(), (int) base64.size()));
  }
  catch (...) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), TRI_last_error());
  }

  TRI_ASSERT(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a Javascript snippet, but do not execute it
///
/// @FUN{internal.parse(@FA{script})}
///
/// Parses the @FA{script} code, but does not execute it.
/// Will return *true* if the code does not have a parse error, and throw
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
    if (tryCatch.CanContinue()) {
      string err = TRI_StringifyV8Exception(&tryCatch);

      TRI_V8_SYNTAX_ERROR(scope, err.c_str());
    }
    else {
      TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

      v8g->_canceled = true;
      return scope.Close(v8::Undefined());
    }
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
/// - @LIT{maxRedirects}: maximum number of redirects to follow, only useful
///   if @LIT{followRedirects} is *true*.
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

  if (argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope, signature);
  }

  string url = TRI_ObjectToString(argv[0]);

  string body;
  if (argv.Length() > 1) {
    if (argv[1]->IsString() || argv[1]->IsStringObject()) {
      body = TRI_ObjectToString(argv[1]);
    }
  }

  // options
  // ------------------------------------------------------------------------

  map<string, string> headerFields;
  double timeout = 10.0;
  bool followRedirects = true;
  HttpRequest::HttpRequestType method = HttpRequest::HTTP_REQUEST_GET;
  bool returnBodyOnError = false;
  int maxRedirects = 5;

  if (argv.Length() > 2) {
    if (! argv[2]->IsObject()) {
      TRI_V8_EXCEPTION_USAGE(scope, signature);
    }

    v8::Handle<v8::Array> options = v8::Handle<v8::Array>::Cast(argv[2]);

    if (options.IsEmpty()) {
      TRI_V8_EXCEPTION_USAGE(scope, signature);
    }

    // method
    if (options->Has(TRI_V8_SYMBOL("method"))) {
      string methodString = TRI_ObjectToString(options->Get(TRI_V8_SYMBOL("method")));

      method = HttpRequest::translateMethod(methodString);
    }

    // headers
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
    if (options->Has(TRI_V8_SYMBOL("timeout"))) {
      if (! options->Get(TRI_V8_SYMBOL("timeout"))->IsNumber()) {
        TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_BAD_PARAMETER, "invalid option value for timeout");
      }

      timeout = TRI_ObjectToDouble(options->Get(TRI_V8_SYMBOL("timeout")));
    }

    // follow redirects
    if (options->Has(TRI_V8_SYMBOL("followRedirects"))) {
      followRedirects = TRI_ObjectToBoolean(options->Get(TRI_V8_SYMBOL("followRedirects")));
    }

    // max redirects
    if (options->Has(TRI_V8_SYMBOL("maxRedirects"))) {
      if (! options->Get(TRI_V8_SYMBOL("maxRedirects"))->IsNumber()) {
        TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_BAD_PARAMETER, "invalid option value for maxRedirects");
      }

      maxRedirects = (int) TRI_ObjectToInt64(options->Get(TRI_V8_SYMBOL("maxRedirects")));
    }

    if (body.size() > 0 &&
        (method == HttpRequest::HTTP_REQUEST_GET ||
         method == HttpRequest::HTTP_REQUEST_HEAD)) {
      TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_BAD_PARAMETER, "should not provide a body value for this request method");
    }

    if (options->Has(TRI_V8_SYMBOL("returnBodyOnError"))) {
      returnBodyOnError = TRI_ObjectToBoolean(options->Get(TRI_V8_SYMBOL("returnBodyOnError")));
    }
  }


  // outfile
  string outfile;
  if (argv.Length() >= 4) {
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

  while (numRedirects < maxRedirects) {
    string endpoint;
    string relative;

    if (url.substr(0, 7) == "http://") {
      size_t found = url.find('/', 7);

      relative = "/";
      if (found != string::npos) {
        relative.append(url.substr(found + 1));
        endpoint = url.substr(7, found - 7);
      }
      else {
        endpoint = url.substr(7);
      }
      found = endpoint.find(":");
      if (found == string::npos) {
        endpoint = endpoint + ":80";
      }
      endpoint = "tcp://" + endpoint;
    }
    else if (url.substr(0, 8) == "https://") {
      size_t found = url.find('/', 8);

      relative = "/";
      if (found != string::npos) {
        relative.append(url.substr(found + 1));
        endpoint = url.substr(8, found - 8);
      }
      else {
        endpoint = url.substr(8);
      }
      found = endpoint.find(":");
      if (found == string::npos) {
        endpoint = endpoint + ":443";
      }
      endpoint = "ssl://" + endpoint;
    }
    else {
      TRI_V8_SYNTAX_ERROR(scope, "unsupported URL specified");
    }

    LOG_TRACE("downloading file. endpoint: %s, relative URL: %s",
              endpoint.c_str(),
              url.c_str());

    Endpoint* ep = Endpoint::clientFactory(endpoint);
    if (ep == 0) {
      TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_BAD_PARAMETER, "invalid URL");
    }

    GeneralClientConnection* connection = GeneralClientConnection::factory(ep, timeout, timeout, 3, 0);

    if (connection == 0) {
      delete ep;
      TRI_V8_EXCEPTION_MEMORY(scope);
    }

    SimpleHttpClient* client = new SimpleHttpClient(connection, timeout, false);

    v8::Handle<v8::Object> result = v8::Object::New();

    if (numRedirects > 0) {
      // do not send extra headers now
      headerFields.clear();
    }

    // send the actual request
    SimpleHttpResult* response = client->request(method,
                                                relative,
                                                (body.size() > 0 ? body.c_str() : 0),
                                                body.size(),
                                                headerFields);

    int returnCode;
    string returnMessage;

    if (! response || ! response->isComplete()) {
      // save error message
      returnMessage = client->getErrorMessage();
      returnCode = 500;

      delete client;

      if (response && response->getHttpReturnCode() > 0) {
        returnCode = response->getHttpReturnCode();
      }
    }
    else {
      delete client;

      returnMessage = response->getHttpReturnMessage();
      returnCode = response->getHttpReturnCode();

      // follow redirects?
      if (followRedirects &&
          (returnCode == 301 || returnCode == 302 || returnCode == 307)) {
        bool found;
        url = response->getHeaderField(string("location"), found);

        delete response;
        delete connection;
        connection = 0;
        delete ep;

        if (! found) {
          TRI_V8_EXCEPTION_INTERNAL(scope, "caught invalid redirect URL");
        }

        numRedirects++;
        continue;
      }

      result->Set(v8::String::New("code"),    v8::Number::New(returnCode));
      result->Set(v8::String::New("message"), v8::String::New(returnMessage.c_str()));

      // process response headers
      auto responseHeaders = response->getHeaderFields();

      v8::Handle<v8::Object> headers = v8::Object::New();

      for (auto it = responseHeaders.begin(); it != responseHeaders.end(); ++it) {
        headers->Set(v8::String::New((*it).first.c_str()), v8::String::New((*it).second.c_str()));
      }

      result->Set(v8::String::New("headers"), headers);

      if (returnBodyOnError || (returnCode >= 200 && returnCode <= 299)) {
        try {
          if (outfile.size() > 0) {
            // save outfile
            FileUtils::spit(outfile, response->getBody());
          }
          else {
            // set "body" attribute in result
            const StringBuffer& sb = response->getBody();
            result->Set(v8::String::New("body"),
                        v8::String::New(sb.c_str(), (int) sb.length()));
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

    if (connection) {
      delete connection;
    }
    delete ep;

    return scope.Close(result);
  }

  TRI_V8_EXCEPTION_INTERNAL(scope, "too many redirects");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a script
///
/// @FUN{internal.execute(@FA{script}, @FA{sandbox}, *filename*)}
///
/// Executes the @FA{script} with the @FA{sandbox} as context. Global variables
/// assigned inside the @FA{script}, will be visible in the @FA{sandbox} object
/// after execution. The *filename* is used for displaying error
/// messages.
///
/// If @FA{sandbox} is undefined, then @FN{execute} uses the current context.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Execute (v8::Arguments const& argv) {
  v8::HandleScope scope;

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

    for (uint32_t i = 0; i < keys->Length(); i++) {
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
  v8::Handle<v8::Script> script;
  v8::Handle<v8::Value> result;

  {
    v8::TryCatch tryCatch;

    script = v8::Script::Compile(source->ToString(), filename);

    // compilation failed, print errors that happened during compilation
    if (script.IsEmpty()) {
      if (useSandbox) {
        context->DetachGlobal();
        context->Exit();
      }

      if (tryCatch.CanContinue()) {
        TRI_LogV8Exception(&tryCatch);
        return scope.Close(v8::ThrowException(tryCatch.Exception()));
      }
      else {
        TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

        v8g->_canceled = true;
        return scope.Close(v8::Undefined());
      }
    }

    // compilation succeeded, run the script
    result = script->Run();

    if (result.IsEmpty()) {
      if (useSandbox) {
        context->DetachGlobal();
        context->Exit();
      }

      if (tryCatch.CanContinue()) {
        TRI_LogV8Exception(&tryCatch);
        return scope.Close(v8::ThrowException(tryCatch.Exception()));
      }
      else {
        TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

        v8g->_canceled = true;
        return scope.Close(v8::Undefined());
      }
    }
  }

  // copy result back into the sandbox
  if (useSandbox) {
    v8::Handle<v8::Array> keys = context->Global()->GetPropertyNames();

    for (uint32_t i = 0; i < keys->Length(); i++) {
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
/// @brief registers the executes file function
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RegisterExecuteFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(isolate->GetData());

  // extract arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "registerExecuteCallback(<func>)");
  }

  v8g->ExecuteFileCallback
  = v8::Persistent<v8::Function>::New(isolate, v8::Handle<v8::Function>::Cast(argv[0]));

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a file of any type or directory exists
/// @startDocuBlock JS_Exists
/// `fs.exists(path)`
///
/// Returns true if a file (of any type) or a directory exists at a given
/// path. If the file is a broken symbolic link, returns false.
/// @endDocuBlock
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
/// @brief gets the size of a file
/// @startDocuBlock JS_Size
/// `fs.size(path)`
///
/// Returns the size of the file specified by *path*.
/// @endDocuBlock
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

  return scope.Close(v8::String::New(line.c_str(), (int) line.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the temporary directory
/// @startDocuBlock JS_GetTempPath
/// `fs.getTempPath()`
///
/// Returns the absolute path of the temporary directory
///
/// @endDocuBlock
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
/// @brief returns the name for a (new) temporary file
/// @startDocuBlock JS_GetTempFile
/// `fs.getTempFile(directory, createFile)`
///
/// Returns the name for a new temporary file in directory *directory*.
/// If *createFile* is *true*, an empty file will be created so no other
/// process can create a file of the same name.
///
/// **Note**: The directory *directory* must exist.
/// @endDocuBlock
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
  return scope.Close(v8::String::New(tempfile.c_str(), (int) tempfile.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if path is a directory
/// @startDocuBlock JS_IsDirectory
/// `fs.isDirectory(path)`
///
/// Returns true if the *path* points to a directory.
/// @endDocuBlock
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
/// @startDocuBlock JS_IsFile
/// `fs.isFile(path)`
///
/// Returns true if the *path* points to a file.
/// @endDocuBlock
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
/// @brief makes a given path absolute
/// @startDocuBlock JS_MakeAbsolute
/// `fs.makeAbsolute(path)`
///
/// Returns the given string if it is an absolute path, otherwise an
/// absolute path to the same location is returned.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_MakeAbsolute(v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "makeAbsolute(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<path> must be a string");
  }

  int err = 0;
  string cwd = triagens::basics::FileUtils::currentDirectory(&err);
  if (0 != err) {
    TRI_V8_EXCEPTION_MESSAGE(scope, err,"cannot get current working directory");
  }

  char *abs = TRI_GetAbsolutePath (*name, cwd.c_str());
  v8::Handle<v8::String> res;

  if (0 != abs) {
    res = v8::String::New(abs);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, abs);
  }
  else {
    res = v8::String::New(cwd.c_str(), (int) cwd.size());
  }

  // return result
  return scope.Close(res);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the directory listing
/// @startDocuBlock JS_List
/// `fs.list(path)`
///
/// The functions returns the names of all the files in a directory, in
/// lexically sorted order. Throws an exception if the directory cannot be
/// traversed (or path is not a directory).
///
/// **Note**: this means that list("x") of a directory containing "a" and "b" would
/// return ["a", "b"], not ["x/a", "x/b"].
/// @endDocuBlock
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
/// @startDocuBlock JS_ListTree
/// `fs.listTree(path)`
///
/// The function returns an array that starts with the given path, and all of
/// the paths relative to the given path, discovered by a depth first traversal
/// of every directory in any visited directory, reporting but not traversing
/// symbolic links to directories. The first path is always *""*, the path
/// relative to itself.
/// @endDocuBlock
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
/// @brief creates a directory
/// @startDocuBlock JS_MakeDirectory
/// `fs.makeDirectory(path)`
///
/// Creates the directory specified by *path*.
/// @endDocuBlock
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

  int res = TRI_CreateDirectory(*name);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unzips a file
/// @startDocuBlock JS_Unzip
/// `fs.unzip(filename, outpath, skipPaths, overwrite, password)`
///
/// Unzips the zip file specified by *filename* into the path specified by
/// * outpath*. Overwrites any existing target files if *overwrite* is set
/// to *true*.
///
/// Returns *true* if the file was unzipped successfully.
/// @endDocuBlock
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
/// @startDocuBlock JS_Zip
/// `fs.zip(filename, files)`
///
/// Stores the files specified by *files* in the zip file *filename*.
///
/// Returns *true* if the file was zipped successfully.
/// @endDocuBlock
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
/// @FUN{internal.load(*filename*)}
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
    TRI_V8_TYPE_ERROR(scope, "<filename> must be a UTF-8 string");
  }

  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *name, &length);

  if (content == nullptr) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "cannot read file");
  }

  v8::Handle<v8::Value> result =
    TRI_ExecuteJavaScriptString(v8::Context::GetCurrent(), v8::String::New(content, (int) length), argv[0], false);

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

  return scope.Close(result);
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
/// @brief computes the md5 sum of a text
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

  v8::String::Utf8Value str(argv[0]);

  if (*str == 0) {
    return scope.Close(v8::Undefined());
  }

  // create md5
  char hash[17];
  char* p = &hash[0];
  size_t length;

  SslInterface::sslMD5(*str, str.length(), p, length);

  // as hex
  char hex[33];
  p = &hex[0];

  SslInterface::sslHEX(hash, 16, p, length);

  // and return
  v8::Handle<v8::String> hashStr = v8::String::New(hex, 32);

  return scope.Close(hashStr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates random numbers
///
/// @FUN{internal.genRandomNumbers(@FA{length})}
///
/// Generates a string of a given @FA{length} containing numbers.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RandomNumbers (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1 || ! argv[0]->IsNumber()) {
    TRI_V8_EXCEPTION_USAGE(scope, "genRandomNumbers(<length>)");
  }

  int length = (int) TRI_ObjectToInt64(argv[0]);

  string str = JSNumGenerator.random(length);
  return scope.Close(v8::String::New(str.c_str(), (int) str.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates random alpha-numbers
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
  return scope.Close(v8::String::New(str.c_str(), (int) str.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a salt
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
  return scope.Close(v8::String::New(str.c_str(), (int) str.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a nonce
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

  string str = Nonce::createNonce();

  return scope.Close(v8::String::New(str.c_str(), (int) str.length()));
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
/// @startDocuBlock JS_Move
/// `fs.move(source, destination)`
///
/// Moves *source* to destination. Failure to move the file, or
/// specifying a directory for target when source is a file will throw an
/// exception.
/// @endDocuBlock
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
      ssize_t n = TRI_WRITE(1, ptr, (TRI_write_t) len);

      if (n < 0) {
        break;
      }

      len -= n;
      ptr += n;
    }
  }

  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current process information
/// `internal.processStatistics()`
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
/// - residentSizePercent: resident size as percent of the total physical
///   memory size.
///
/// - virtualSize: Virtual memory size in bytes.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{internalStatistics}
///   require("internal").processStat();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ProcessStatistics (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> result = v8::Object::New();

  TRI_process_info_t info = TRI_ProcessInfoSelf();
  double rss = (double) info._residentSize;
  double rssp = 0;

  if (TRI_PhysicalMemory != 0) {
    rssp = rss / TRI_PhysicalMemory;
  }

  result->Set(v8::String::New("minorPageFaults"), v8::Number::New((double) info._minorPageFaults));
  result->Set(v8::String::New("majorPageFaults"), v8::Number::New((double) info._majorPageFaults));
  result->Set(v8::String::New("userTime"), v8::Number::New((double) info._userTime / (double) info._scClkTck));
  result->Set(v8::String::New("systemTime"), v8::Number::New((double) info._systemTime / (double) info._scClkTck));
  result->Set(v8::String::New("numberOfThreads"), v8::Number::New((double) info._numberThreads));
  result->Set(v8::String::New("residentSize"), v8::Number::New(rss));
  result->Set(v8::String::New("residentSizePercent"), v8::Number::New(rssp));
  result->Set(v8::String::New("virtualSize"), v8::Number::New((double) info._virtualSize));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a random number using OpenSSL
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
/// @startDocuBlock JS_Read
/// `fs.read(filename)`
///
/// Reads in a file and returns the content as string. Please note that the
/// file content must be encoded in UTF-8.
/// @endDocuBlock
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

  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *name, &length);

  if (content == nullptr) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), TRI_last_error());
  }

  v8::Handle<v8::String> result = v8::String::New(content, (int) length);

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads in a file
/// @startDocuBlock JS_ReadBuffer
/// `fs.readBuffer(filename)`
///
/// Reads in a file and returns its content in a Buffer object.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ReadBuffer (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "readBuffer(<filename>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<filename> must be a UTF-8 string");
  }

  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *name, &length);

  if (content == nullptr) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), TRI_last_error());
  }

  V8Buffer* buffer = V8Buffer::New(content, length); 
  
  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

  return scope.Close(buffer->_handle);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads in a file as base64
/// @startDocuBlock JS_Read64
/// `fs.read64(filename)`
///
/// Reads in a file and returns the content as string. The file content is
/// Base64 encoded.
/// @endDocuBlock
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
    string&& content = FileUtils::slurp(*name);
    base64 = StringUtils::encodeBase64(content);
  }
  catch (...) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), TRI_last_error());
  }

  return scope.Close(v8::String::New(base64.c_str(), (int) base64.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes to a file
///
/// @startDocuBlock JS_Save
/// `fs.write(filename, content)`
///
/// Writes the content into a file. Content can be a string or a Buffer object.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Save (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "write(<filename>, <content>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<filename> must be a string");
  }

  if (argv[1]->IsObject() && V8Buffer::hasInstance(argv[1])) {
    // content is a buffer
    const char* data = V8Buffer::data(argv[1].As<v8::Object>());
    size_t size = V8Buffer::length(argv[1].As<v8::Object>());
    
    ofstream file;

    file.open(*name, ios::out | ios::binary);
  
    if (file.is_open()) {
      file.write(data, size);
      file.close();
      return scope.Close(v8::True());
    }
  }
  else {
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
  }

  TRI_V8_EXCEPTION_SYS(scope, "cannot write file");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a file
/// @startDocuBlock JS_Remove
/// `fs.remove(filename)`
///
/// Removes the file *filename* at the given path. Throws an exception if the
/// path corresponds to anything that is not a file or a symbolic link. If
/// "path" refers to a symbolic link, removes the symbolic link.
/// @endDocuBlock
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
/// @startDocuBlock JS_RemoveDirectory
/// `fs.removeDirectory(path)`
///
/// Removes a directory if it is empty. Throws an exception if the path is not
/// an empty directory.
///
/// @endDocuBlock
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
/// @startDocuBlock JS_RemoveDirectoryRecursive
/// `fs.removeDirectoryRecursive(path)`
///
/// Removes a directory with all subelements. Throws an exception if the path
/// is not a directory.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RemoveRecursiveDirectory (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the arguments
  if (argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "removeDirectoryRecursive(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<path> must be a string");
  }

  if (! TRI_IsDirectory(*name)) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "<path> must be a valid directory name");
  }

  bool force = false;

  if (argv.Length() > 1) {
    force = TRI_ObjectToBoolean(argv[1]);
  }

  if (! force) {
    // check if we're inside the temp directory. force will override this check
    char* tempPath = TRI_GetUserTempPath();

    if (tempPath == 0 || strlen(tempPath) < 6) {
      // some security measure so we don't accidently delete all our files
      if (tempPath != 0) {
        TRI_FreeString(TRI_CORE_MEM_ZONE, tempPath);
      }

      TRI_V8_EXCEPTION_PARAMETER(scope, "temporary directory name is too short. will not remove directory");
    }

    const string path(*name);
#ifdef _WIN32
    // windows paths are case-insensitive
    if (! TRI_CaseEqualString2(path.c_str(), tempPath, strlen(tempPath))) {
#else
    if (! TRI_EqualString2(path.c_str(), tempPath, strlen(tempPath))) {
#endif
      TRI_FreeString(TRI_CORE_MEM_ZONE, tempPath);

      TRI_V8_EXCEPTION_PARAMETER(scope, "directory to be removed is outside of temporary path");
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, tempPath);
  }

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
  result->Set(v8::String::New("physicalMemory"), v8::Number::New((double) TRI_PhysicalMemory));

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
            result += "%";
            result += *ptr;
          }
          else {
            bool e;
            double f = TRI_ObjectToDouble(argv[(int) p], e);

            if (e) {
              result += "NaN";
            }
            else {
              char b[1024];

              if (*ptr == 'f') {
                snprintf(b, sizeof(b), "%f", f);
              }
              else {
                snprintf(b, sizeof(b), "%ld", (long) f);
              }

              result += b;
            }

            ++p;
          }

          break;
        }

        case 'o':
        case 's': {
          if (len <= p) {
            result += "%";
            result += *ptr;
          }
          else {
            TRI_Utf8ValueNFC text(TRI_UNKNOWN_MEM_ZONE, argv[(int) p]);

            if (*text == 0) {
              string msg = StringUtils::itoa(p) + ".th argument must be a string in format '" + *format + "'";
              TRI_V8_EXCEPTION_PARAMETER(scope, msg.c_str());
            }

            ++p;

            result += *text;
          }

          break;
        }

        default:
          result += "%";
          result += *ptr;
          break;
      }
    }
    else {
      result += *ptr;
    }
  }

  for (size_t i = p;  i < len;  ++i) {
    TRI_Utf8ValueNFC text(TRI_UNKNOWN_MEM_ZONE, argv[(int) i]);

    if (*text == 0) {
      string msg = StringUtils::itoa(i) + ".th argument must be a string in format '" + *format + "'";
      TRI_V8_TYPE_ERROR(scope, msg.c_str());
    }

    result += " ";
    result += *text;
  }

  return scope.Close(v8::String::New(result.c_str(), (int) result.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the sha512 sum
///
/// @FUN{internal.sha512(@FA{text})}
///
/// Computes an sha512 for the @FA{text}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Sha512 (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1 || ! argv[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "sha512(<text>)");
  }

  string key = TRI_ObjectToString(argv[0]);

  // create sha512
  char* hash = 0;
  size_t hashLen;

  SslInterface::sslSHA512(key.c_str(), key.size(), hash, hashLen);

  // as hex
  char* hex = 0;
  size_t hexLen;

  SslInterface::sslHEX(hash, hashLen, hex, hexLen);

  delete[] hash;

  // and return
  v8::Handle<v8::String> hashStr = v8::String::New(hex, (int) hexLen);

  delete[] hex;

  return scope.Close(hashStr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the sha384 sum
///
/// @FUN{internal.sha384(@FA{text})}
///
/// Computes an sha384 for the @FA{text}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Sha384 (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1 || ! argv[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "sha384(<text>)");
  }

  string key = TRI_ObjectToString(argv[0]);

  // create sha384
  char* hash = 0;
  size_t hashLen;

  SslInterface::sslSHA384(key.c_str(), key.size(), hash, hashLen);

  // as hex
  char* hex = 0;
  size_t hexLen;

  SslInterface::sslHEX(hash, hashLen, hex, hexLen);

  delete[] hash;

  // and return
  v8::Handle<v8::String> hashStr = v8::String::New(hex, (int) hexLen);

  delete[] hex;

  return scope.Close(hashStr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the sha256 sum
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
  v8::Handle<v8::String> hashStr = v8::String::New(hex, (int) hexLen);

  delete[] hex;

  return scope.Close(hashStr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the sha224 sum
///
/// @FUN{internal.sha224(@FA{text})}
///
/// Computes an sha224 for the @FA{text}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Sha224 (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1 || ! argv[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "sha224(<text>)");
  }

  string key = TRI_ObjectToString(argv[0]);

  // create sha224
  char* hash = 0;
  size_t hashLen;

  SslInterface::sslSHA224(key.c_str(), key.size(), hash, hashLen);

  // as hex
  char* hex = 0;
  size_t hexLen;

  SslInterface::sslHEX(hash, hashLen, hex, hexLen);

  delete[] hash;

  // and return
  v8::Handle<v8::String> hashStr = v8::String::New(hex, (int) hexLen);

  delete[] hex;

  return scope.Close(hashStr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the sha1 sum
///
/// @FUN{internal.sha1(@FA{text})}
///
/// Computes an sha1 for the @FA{text}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Sha1 (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1 || ! argv[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "sha1(<text>)");
  }

  string key = TRI_ObjectToString(argv[0]);

  // create sha1
  char* hash = 0;
  size_t hashLen;

  SslInterface::sslSHA1(key.c_str(), key.size(), hash, hashLen);

  // as hex
  char* hex = 0;
  size_t hexLen;

  SslInterface::sslHEX(hash, hashLen, hex, hexLen);

  delete[] hash;

  // and return
  v8::Handle<v8::String> hashStr = v8::String::New(hex, (int) hexLen);

  delete[] hex;

  return scope.Close(hashStr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sleeps
///
/// @FUN{internal.sleep(@FA{seconds})}
///
/// Wait for @FA{seconds}, without calling the garbage collection.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Sleep (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "sleep(<seconds>)");
  }

  double n = TRI_ObjectToDouble(argv[0]);
  double until = TRI_microtime() + n;

  // TODO: use select etc. to wait until point in time
  while (TRI_microtime() < until) {
    usleep(10000);
  }

  return scope.Close(v8::Undefined());
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
/// @brief waits for the specified amount of time and calls the garbage
/// collection.
///
/// @FUN{internal.wait(@FA{seconds})}
///
/// Wait for @FA{seconds}, call the garbage collection.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Wait (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "wait(<seconds>, <gc>)");
  }

  double n = TRI_ObjectToDouble(argv[0]);
  double until = TRI_microtime() + n;

  bool gc = true; // default is to trigger the gc
  if (argv.Length() > 1) {
    gc = TRI_ObjectToBoolean(argv[1]);
  }

  if (gc) {
    // wait with gc
    v8::V8::LowMemoryNotification();
    while (! v8::V8::IdleNotification()) {
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
  }
  else {
    // wait without gc
    while (TRI_microtime() < until) {
      usleep(100);
    }
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief intentionally causes a segfault
///
/// @FUN{internal.debugSegfault(@FA{message})}
///
/// intentionally cause a segmentation violation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FAILURE_TESTS

static v8::Handle<v8::Value> JS_DebugSegfault (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "debugSegfault(<message>)");
  }

  string const message = TRI_ObjectToString(argv[0]);

  TRI_SegfaultDebugging(message.c_str());

  // we may get here if we are in non-maintainer mode

  return scope.Close(v8::Undefined());
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a failure point
///
/// @FUN{internal.debugSetFailAt(@FA{point})}
///
/// Set a point for an intentional system failure
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FAILURE_TESTS

static v8::Handle<v8::Value> JS_DebugSetFailAt (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "debugSetFailAt(<point>)");
  }

  string const point = TRI_ObjectToString(argv[0]);

  TRI_AddFailurePointDebugging(point.c_str());

  return scope.Close(v8::Undefined());
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a failure point
///
/// @FUN{internal.debugRemoveFailAt(@FA{point})}
///
/// Remove a point for an intentional system failure
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FAILURE_TESTS

static v8::Handle<v8::Value> JS_DebugRemoveFailAt (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "debugRemoveFailAt(<point>)");
  }

  string const point = TRI_ObjectToString(argv[0]);

  TRI_RemoveFailurePointDebugging(point.c_str());

  return scope.Close(v8::Undefined());
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief clears all failure points
///
/// @FUN{internal.debugClearFailAt()}
///
/// Remove all points for intentional system failures
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FAILURE_TESTS

static v8::Handle<v8::Value> JS_DebugClearFailAt (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "debugClearFailAt()");
  }

  TRI_ClearFailurePointsDebugging();

  return scope.Close(v8::Undefined());
}

#endif

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

static v8::Handle<v8::Value> JS_ClientStatistics (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> result = v8::Object::New();

  StatisticsCounter httpConnections;
  StatisticsCounter totalRequests;
  vector<StatisticsCounter> methodRequests;
  StatisticsCounter asyncRequests;
  StatisticsDistribution connectionTime;

  TRI_FillConnectionStatistics(httpConnections, totalRequests, methodRequests, asyncRequests, connectionTime);

  result->Set(v8::String::New("httpConnections"), v8::Number::New((double) httpConnections._count));
  FillDistribution(result, "connectionTime", connectionTime);

  StatisticsDistribution totalTime;
  StatisticsDistribution requestTime;
  StatisticsDistribution queueTime;
  StatisticsDistribution ioTime;
  StatisticsDistribution bytesSent;
  StatisticsDistribution bytesReceived;

  TRI_FillRequestStatistics(totalTime, requestTime, queueTime, ioTime, bytesSent, bytesReceived);

  FillDistribution(result, "totalTime", totalTime);
  FillDistribution(result, "requestTime", requestTime);
  FillDistribution(result, "queueTime", queueTime);
  FillDistribution(result, "ioTime", ioTime);
  FillDistribution(result, "bytesSent", bytesSent);
  FillDistribution(result, "bytesReceived", bytesReceived);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the PBKDF2 HMAC SHA1 derived key
///
/// @FUN{internal.PBKDF2(@FA{salt}, @FA{password}, @FA{iterations}, @FA{keyLength})}
///
/// Computes the PBKDF2 HMAC SHA1 derived key for the @FA{password}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_PBKDF2 (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() < 4 || ! argv[0]->IsString() || ! argv[1]->IsString() || ! argv[2]->IsNumber() || ! argv[3]->IsNumber()) {
    TRI_V8_EXCEPTION_USAGE(scope, "PBKDF2(<salt>, <password>, <iterations>, <keyLength>, <algorithm>)");
  }

  string salt = TRI_ObjectToString(argv[0]);
  string password = TRI_ObjectToString(argv[1]);
  int iterations = (int) TRI_ObjectToInt64(argv[2]);
  int keyLength = (int) TRI_ObjectToInt64(argv[3]);

  string result = SslInterface::sslPBKDF2(salt.c_str(), salt.size(), password.c_str(), password.size(), iterations, keyLength);
  return scope.Close(v8::String::New(result.c_str(), (int) result.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the HMAC signature
///
/// @FUN{internal.HMAC(@FA{key}, @FA{message}, @FA{algorithm})}
///
/// Computes the HMAC for the @FA{message}.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HMAC (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract arguments
  if (argv.Length() < 2 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "HMAC(<key>, <text>, <algorithm>)");
  }

  string key = TRI_ObjectToString(argv[0]);
  string message = TRI_ObjectToString(argv[1]);

  SslInterface::Algorithm al = SslInterface::Algorithm::ALGORITHM_SHA256;
  if (argv.Length() > 2 && ! argv[2]->IsUndefined()) {
    string algorithm = TRI_ObjectToString(argv[2]);
    StringUtils::tolowerInPlace(&algorithm);

    if (algorithm == "sha1") {
      al = SslInterface::Algorithm::ALGORITHM_SHA1;
    }
    else if (algorithm == "sha512") {
      al = SslInterface::Algorithm::ALGORITHM_SHA512;
    }
    else if (algorithm == "sha384") {
      al = SslInterface::Algorithm::ALGORITHM_SHA384;
    }
    else if (algorithm == "sha256") {
      al = SslInterface::Algorithm::ALGORITHM_SHA256;
    }
    else if (algorithm == "sha224") {
      al = SslInterface::Algorithm::ALGORITHM_SHA224;
    }
    else if (algorithm == "md5") {
      al = SslInterface::Algorithm::ALGORITHM_MD5;
    }
    else {
      TRI_V8_EXCEPTION_PARAMETER(scope, "invalid value for <algorithm>");
    }
  }

  string result = SslInterface::sslHMAC(key.c_str(), key.size(), message.c_str(), message.size(), al);
  return scope.Close(v8::String::New(result.c_str(), (int) result.size()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current http statistics
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HttpStatistics (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> result = v8::Object::New();

  StatisticsCounter httpConnections;
  StatisticsCounter totalRequests;
  vector<StatisticsCounter> methodRequests;
  StatisticsCounter asyncRequests;
  StatisticsDistribution connectionTime;

  TRI_FillConnectionStatistics(httpConnections, totalRequests, methodRequests, asyncRequests, connectionTime);

  // request counters
  result->Set(v8::String::New("requestsTotal"), v8::Number::New((double) totalRequests._count));
  result->Set(v8::String::New("requestsAsync"), v8::Number::New((double) asyncRequests._count));
  result->Set(v8::String::New("requestsGet"), v8::Number::New((double) methodRequests[(int) HttpRequest::HTTP_REQUEST_GET]._count));
  result->Set(v8::String::New("requestsHead"), v8::Number::New((double) methodRequests[(int) HttpRequest::HTTP_REQUEST_HEAD]._count));
  result->Set(v8::String::New("requestsPost"), v8::Number::New((double) methodRequests[(int) HttpRequest::HTTP_REQUEST_POST]._count));
  result->Set(v8::String::New("requestsPut"), v8::Number::New((double) methodRequests[(int) HttpRequest::HTTP_REQUEST_PUT]._count));
  result->Set(v8::String::New("requestsPatch"), v8::Number::New((double) methodRequests[(int) HttpRequest::HTTP_REQUEST_PATCH]._count));
  result->Set(v8::String::New("requestsDelete"), v8::Number::New((double) methodRequests[(int) HttpRequest::HTTP_REQUEST_DELETE]._count));
  result->Set(v8::String::New("requestsOptions"), v8::Number::New((double) methodRequests[(int) HttpRequest::HTTP_REQUEST_OPTIONS]._count));
  result->Set(v8::String::New("requestsOther"), v8::Number::New((double) methodRequests[(int) HttpRequest::HTTP_REQUEST_ILLEGAL]._count));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a external program
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExecuteExternal (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the arguments
  if (3 < argv.Length() || argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope,
      "executeExternal(<filename>[, <arguments> [,<usePipes>] ])");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<filename> must be a string");
  }

  char** arguments = 0;
  uint32_t n = 0;

  if (2 <= argv.Length()) {
    v8::Handle<v8::Value> a = argv[1];

    if (a->IsArray()) {
      v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(a);

      n = arr->Length();
      arguments = (char**) TRI_Allocate(TRI_CORE_MEM_ZONE, n * sizeof(char*), false);

      for (uint32_t i = 0;  i < n;  ++i) {
        TRI_Utf8ValueNFC arg(TRI_UNKNOWN_MEM_ZONE, arr->Get(i));

        if (*arg == 0) {
          arguments[i] = TRI_DuplicateString("");
        }
        else {
          arguments[i] = TRI_DuplicateString(*arg);
        }
      }
    }
    else {
      n = 1;
      arguments = (char**) TRI_Allocate(TRI_CORE_MEM_ZONE, n * sizeof(char*), false);

        TRI_Utf8ValueNFC arg(TRI_UNKNOWN_MEM_ZONE, a);

        if (*arg == 0) {
          arguments[0] = TRI_DuplicateString("");
        }
        else {
          arguments[0] = TRI_DuplicateString(*arg);
        }
    }
  }
  bool usePipes = false;
  if (3 <= argv.Length()) {
    usePipes = TRI_ObjectToBoolean(argv[2]);
  }

  TRI_external_id_t external;
  TRI_CreateExternalProcess(*name, (const char**) arguments, (size_t) n,
                            usePipes, &external);
  if (arguments != 0) {
    for (uint32_t i = 0;  i < n;  ++i) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, arguments[i]);
    }

    TRI_Free(TRI_CORE_MEM_ZONE, arguments);
  }
  if (external._pid == TRI_INVALID_PROCESS_ID) {
    TRI_V8_ERROR(scope, "Process could not be started");
  }
  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8::String::New("pid"), v8::Number::New(external._pid));
  // Now report about possible stdin and stdout pipes:
#ifndef _WIN32
  if (external._readPipe >= 0) {
    result->Set(v8::String::New("readPipe"),
                v8::Number::New(external._readPipe));
  }
  if (external._writePipe >= 0) {
    result->Set(v8::String::New("writePipe"),
                v8::Number::New(external._writePipe));
  }
#else
  size_t readPipe_len, writePipe_len;
  if (0 != external._readPipe) {
    char* readPipe = TRI_EncodeHexString((const char *)external._readPipe,
                                         sizeof(HANDLE), &readPipe_len);
    result->Set(v8::String::New("readPipe"),
                v8::String::New(readPipe, (int) readPipe_len));
    TRI_FreeString(TRI_CORE_MEM_ZONE, readPipe);
  }
  if (0 != external._writePipe) {
    char* writePipe = TRI_EncodeHexString((const char *)external._writePipe,
                                          sizeof(HANDLE), &writePipe_len);
    result->Set(v8::String::New("writePipe"),
                v8::String::New(writePipe, (int) writePipe_len));
    TRI_FreeString(TRI_CORE_MEM_ZONE, writePipe);
  }
#endif
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of an external process
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_StatusExternal (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::Handle<v8::String> pidname = v8::String::New("pid");

  // extract the arguments
  if (argv.Length() < 1 || argv.Length() > 2 ||
      !argv[0]->IsObject()) {
    TRI_V8_EXCEPTION_USAGE(scope,
                           "statusExternal(<external-identifier>[, <wait>])");
  }

  v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(argv[0]);

  if (! obj->Has(pidname)) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_BAD_PARAMETER,
                             "statusExternal: pid must be given");
  }

  TRI_external_id_t pid;
  memset(&pid, 0, sizeof(TRI_external_id_t));

#ifndef _WIN32
  pid._pid = static_cast<TRI_pid_t>(TRI_ObjectToUInt64(obj->Get(pidname), true));
#else
  pid._pid = static_cast<DWORD>(TRI_ObjectToUInt64(obj->Get(pidname), true));
#endif
  bool wait = false;
  if (argv.Length() == 2) {
    wait = TRI_ObjectToBoolean(argv[1]);
  }

  TRI_external_status_t external = TRI_CheckExternalProcess(pid, wait);

  v8::Handle<v8::Object> result = v8::Object::New();
  const char* status = "UNKNOWN";

  switch (external._status) {
    case TRI_EXT_NOT_STARTED: status = "NOT-STARTED"; break;
    case TRI_EXT_PIPE_FAILED: status = "FAILED"; break;
    case TRI_EXT_FORK_FAILED: status = "FAILED"; break;
    case TRI_EXT_RUNNING: status = "RUNNING"; break;
    case TRI_EXT_NOT_FOUND: status = "NOT-FOUND"; break;
    case TRI_EXT_TERMINATED: status = "TERMINATED"; break;
    case TRI_EXT_ABORTED: status = "ABORTED"; break;
    case TRI_EXT_STOPPED: status = "STOPPED"; break;
  }

  result->Set(v8::String::New("status"), v8::String::New(status));

  if (external._status == TRI_EXT_TERMINATED) {
    result->Set(v8::String::New("exit"), v8::Number::New(external._exitStatus));
  }
  else if (external._status == TRI_EXT_ABORTED) {
    result->Set(v8::String::New("signal"), v8::Number::New(external._exitStatus));
  }
  if (external._errorMessage.length() > 0) {
    result->Set(v8::String::New("errorMessage"), v8::String::New(external._errorMessage.c_str(),
                                                                 (int) external._errorMessage.size()));
  }
  // return the result
  return scope.Close(result);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes a external program
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExecuteAndWaitExternal (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the arguments
  if (3 < argv.Length() || argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope,
      "executeAndWaitExternal(<filename>[, <arguments> [,<usePipes>] ])");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<filename> must be a string");
  }

  char** arguments = 0;
  uint32_t n = 0;

  if (2 <= argv.Length()) {
    v8::Handle<v8::Value> a = argv[1];

    if (a->IsArray()) {
      v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(a);

      n = arr->Length();
      arguments = (char**) TRI_Allocate(TRI_CORE_MEM_ZONE, n * sizeof(char*), false);

      for (uint32_t i = 0;  i < n;  ++i) {
        TRI_Utf8ValueNFC arg(TRI_UNKNOWN_MEM_ZONE, arr->Get(i));

        if (*arg == 0) {
          arguments[i] = TRI_DuplicateString("");
        }
        else {
          arguments[i] = TRI_DuplicateString(*arg);
        }
      }
    }
    else {
      n = 1;
      arguments = (char**) TRI_Allocate(TRI_CORE_MEM_ZONE, n * sizeof(char*), false);

      TRI_Utf8ValueNFC arg(TRI_UNKNOWN_MEM_ZONE, a);

      if (*arg == 0) {
        arguments[0] = TRI_DuplicateString("");
      }
      else {
        arguments[0] = TRI_DuplicateString(*arg);
      }
    }
  }
  bool usePipes = false;
  if (3 <= argv.Length()) {
    usePipes = TRI_ObjectToBoolean(argv[2]);
  }

  TRI_external_id_t external;
  TRI_CreateExternalProcess(*name, (const char**) arguments, (size_t) n,
                            usePipes, &external);
  if (arguments != 0) {
    for (uint32_t i = 0;  i < n;  ++i) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, arguments[i]);
    }

    TRI_Free(TRI_CORE_MEM_ZONE, arguments);
  }
  if (external._pid == TRI_INVALID_PROCESS_ID) {
    TRI_V8_ERROR(scope, "Process could not be started");
  }
  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8::String::New("pid"), v8::Number::New(external._pid));
  // Now report about possible stdin and stdout pipes:
#ifndef _WIN32
  if (external._readPipe >= 0) {
    result->Set(v8::String::New("readPipe"),
                v8::Number::New(external._readPipe));
  }
  if (external._writePipe >= 0) {
    result->Set(v8::String::New("writePipe"),
                v8::Number::New(external._writePipe));
  }
#else
  size_t readPipe_len, writePipe_len;
  if (0 != external._readPipe) {
    char* readPipe = TRI_EncodeHexString((const char *)external._readPipe,
                                         sizeof(HANDLE), &readPipe_len);
    result->Set(v8::String::New("readPipe"),
                v8::String::New(readPipe, (int) readPipe_len));
    TRI_FreeString(TRI_CORE_MEM_ZONE, readPipe);
  }
  if (0 != external._writePipe) {
    char* writePipe = TRI_EncodeHexString((const char *)external._writePipe,
                                          sizeof(HANDLE), &writePipe_len);
    result->Set(v8::String::New("writePipe"),
                v8::String::New(writePipe, (int) writePipe_len));
    TRI_FreeString(TRI_CORE_MEM_ZONE, writePipe);
  }
#endif

  TRI_external_id_t pid;
  memset(&pid, 0, sizeof(TRI_external_id_t));

  pid._pid = external._pid;

  TRI_external_status_t external_status = TRI_CheckExternalProcess(pid, true);

  const char* status = "UNKNOWN";

  switch (external_status._status) {
    case TRI_EXT_NOT_STARTED: status = "NOT-STARTED"; break;
    case TRI_EXT_PIPE_FAILED: status = "FAILED"; break;
    case TRI_EXT_FORK_FAILED: status = "FAILED"; break;
    case TRI_EXT_RUNNING: status = "RUNNING"; break;
    case TRI_EXT_NOT_FOUND: status = "NOT-FOUND"; break;
    case TRI_EXT_TERMINATED: status = "TERMINATED"; break;
    case TRI_EXT_ABORTED: status = "ABORTED"; break;
    case TRI_EXT_STOPPED: status = "STOPPED"; break;
  }

  result->Set(v8::String::New("status"), v8::String::New(status));

  if (external_status._status == TRI_EXT_TERMINATED) {
    result->Set(v8::String::New("exit"), v8::Number::New(external_status._exitStatus));
  }
  else if (external_status._status == TRI_EXT_ABORTED) {
    result->Set(v8::String::New("signal"), v8::Number::New(external_status._exitStatus));
  }
  if (external_status._errorMessage.length() > 0) {
    result->Set(v8::String::New("errorMessage"),
                v8::String::New(external_status._errorMessage.c_str(),
                                (int) external_status._errorMessage.size()));
  }
  // return the result
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief kills an external process
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KillExternal (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::Handle<v8::String> pidname = v8::String::New("pid");
  
  // extract the arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "killExternal(<external-identifier>)");
  }

  v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(argv[0]);
  if (!obj->Has(pidname)) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_BAD_PARAMETER,
                             "statusExternal: pid must be given");
  }
  TRI_external_id_t pid;
  memset(&pid, 0, sizeof(TRI_external_id_t));

#ifndef _WIN32
  pid._pid = static_cast<TRI_pid_t>(TRI_ObjectToUInt64(obj->Get(pidname),true));
#else
  pid._pid = static_cast<DWORD>(TRI_ObjectToUInt64(obj->Get(pidname), true));
#endif

  // return the result
  return scope.Close(v8::Boolean::New(TRI_KillExternalProcess(pid)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a port is available
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_TestPort (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the arguments
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "testPort(<address>)");
  }

  string address = TRI_ObjectToString(argv[0]);
  Endpoint* endpoint = Endpoint::serverFactory(address, 10, false);
  if (0 == endpoint) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_BAD_PARAMETER,
                      "address description invalid, cannot create endpoint");
  }
  TRI_socket_t s = endpoint->connect(1, 1);
  bool available = TRI_isvalidsocket(s);

  if (available) {
    endpoint->disconnect();
  }

  delete endpoint;

  // return the result
  return scope.Close(v8::Boolean::New(available));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoError
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ArangoError (const v8::Arguments& args) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

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

////////////////////////////////////////////////////////////////////////////////
/// @brief SleepAndRequeue
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SleepAndRequeue (const v8::Arguments& args) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  v8::Handle<v8::Object> self = args.Holder()->ToObject();

  if (0 < args.Length() && args[0]->IsObject()) {
    v8::Handle<v8::Object> data = args[0]->ToObject();

    if (data->Has(v8g->SleepKey)) {
      self->Set(v8g->SleepKey, data->Get(v8g->SleepKey));
    }
  }

  return scope.Close(self);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief isIP
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_IsIP (const v8::Arguments& args) {
  v8::HandleScope scope;

  if (args.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "base64Decode(<value>)");
  }

  TRI_Utf8ValueNFC address(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (TRI_InetPton4(*address, NULL) == TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::Number::New(4));
  }
  else if (TRI_InetPton6(*address, NULL) == 0) {
    return scope.Close(v8::Number::New(6));
  }
  else {
    return scope.Close(v8::Number::New(0));
  }
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

    if (! TRI_IsStringJson(key)) {
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

bool TRI_ParseJavaScriptFile (char const* filename) {
  return LoadJavaScriptFile(filename, false, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a string within a V8 context, optionally print the result
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ExecuteJavaScriptString (v8::Handle<v8::Context> context,
                                                   v8::Handle<v8::String> const source,
                                                   v8::Handle<v8::Value> const name,
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
        if (tryCatch.CanContinue()) {
          TRI_LogV8Exception(&tryCatch);
        }
        else {
          TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

          v8g->_canceled = true;
          return scope.Close(v8::Undefined());
        }
      }
    }

    return scope.Close(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error in a javascript object, based on error number only
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_CreateErrorObject (const char* file,
                                              int line,
                                              int errorNumber) {
  v8::HandleScope scope;

  return scope.Close(CreateErrorObject(errorNumber, TRI_errno_string(errorNumber), file, line));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error in a javascript object, using supplied text
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_CreateErrorObject (const char* file,
                                              int line,
                                              int errorNumber,
                                              string const& message) {
  v8::HandleScope scope;

  return scope.Close(CreateErrorObject(errorNumber, message, file, line));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error in a javascript object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_CreateErrorObject (const char* file,
                                              int line,
                                              int errorNumber,
                                              string const& message,
                                              bool autoPrepend) {
  v8::HandleScope scope;

  if (autoPrepend) {
    return scope.Close(CreateErrorObject(errorNumber, message + ": " + string(TRI_errno_string(errorNumber)), file, line));
  }
  else {
    return scope.Close(CreateErrorObject(errorNumber, message, file, line));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes a v8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_normalize_V8_Obj (v8::Handle<v8::Value> obj) {
  v8::HandleScope scope;

  v8::String::Value str(obj);
  size_t str_len = str.length();
  if (str_len > 0) {
    UErrorCode errorCode = U_ZERO_ERROR;
    const Normalizer2* normalizer = Normalizer2::getInstance(NULL, "nfc", UNORM2_COMPOSE ,errorCode);

    if (U_FAILURE(errorCode)) {
      return scope.Close(v8::String::New(*str, (int) str_len));
    }

    UnicodeString result = normalizer->normalize(UnicodeString((UChar*) *str, (int32_t) str_len), errorCode);

    if (U_FAILURE(errorCode)) {
      return scope.Close(v8::String::New(*str, (int) str_len));
    }

    // ..........................................................................
    // Take note here: we are assuming that the ICU type UChar is two bytes.
    // There is no guarantee that this will be the case on all platforms and
    // compilers. v8 expects uint16_t (2 bytes)
    // ..........................................................................

    return scope.Close(v8::String::New((const uint16_t*) result.getBuffer(), (int) result.length()));
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
    result->Set(i, v8::String::New(paths[i].c_str(), (int) paths[i].size()));
  }

  return scope.Close(result);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             module initialisation
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 utils functions inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Utils (v8::Handle<v8::Context> context,
                      string const& startupPath,
                      string const& modules) {
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
  v8g->ArangoErrorTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);

  // .............................................................................
  // sleep and requeue a job
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("SleepAndRequeue"));
  ft->SetCallHandler(JS_SleepAndRequeue);

  // SleepAndRequeue is a "sub-class" of Error
  v8::Handle<v8::Function> SleepAndRequeueFunc = ft->GetFunction();

  SleepAndRequeueFunc->Get(TRI_V8_SYMBOL("prototype"))->ToObject()->SetPrototype(ErrorPrototype);

  TRI_AddGlobalFunctionVocbase(context, "SleepAndRequeue", SleepAndRequeueFunc);

  rt = ft->InstanceTemplate();
  v8g->SleepAndRequeueTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  v8g->SleepAndRequeueFuncTempl = v8::Persistent<v8::FunctionTemplate>::New(isolate, ft);

  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(context, "FS_EXISTS", JS_Exists);
  TRI_AddGlobalFunctionVocbase(context, "FS_GET_TEMP_FILE", JS_GetTempFile);
  TRI_AddGlobalFunctionVocbase(context, "FS_GET_TEMP_PATH", JS_GetTempPath);
  TRI_AddGlobalFunctionVocbase(context, "FS_IS_DIRECTORY", JS_IsDirectory);
  TRI_AddGlobalFunctionVocbase(context, "FS_IS_FILE", JS_IsFile);
  TRI_AddGlobalFunctionVocbase(context, "FS_MAKE_ABSOLUTE", JS_MakeAbsolute);
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

  TRI_AddGlobalFunctionVocbase(context, "SYS_BASE64DECODE", JS_Base64Decode);
  TRI_AddGlobalFunctionVocbase(context, "SYS_BASE64ENCODE", JS_Base64Encode);
  TRI_AddGlobalFunctionVocbase(context, "SYS_CHECK_AND_MARK_NONCE", JS_MarkNonce);
  TRI_AddGlobalFunctionVocbase(context, "SYS_CLIENT_STATISTICS", JS_ClientStatistics);
  TRI_AddGlobalFunctionVocbase(context, "SYS_CREATE_NONCE", JS_CreateNonce);
  TRI_AddGlobalFunctionVocbase(context, "SYS_DOWNLOAD", JS_Download);
  TRI_AddGlobalFunctionVocbase(context, "SYS_EXECUTE", JS_Execute);
  TRI_AddGlobalFunctionVocbase(context, "SYS_EXECUTE_EXTERNAL", JS_ExecuteExternal);
  TRI_AddGlobalFunctionVocbase(context, "SYS_EXECUTE_EXTERNAL_AND_WAIT", JS_ExecuteAndWaitExternal);
  TRI_AddGlobalFunctionVocbase(context, "SYS_GEN_RANDOM_ALPHA_NUMBERS", JS_RandomAlphaNum);
  TRI_AddGlobalFunctionVocbase(context, "SYS_GEN_RANDOM_NUMBERS", JS_RandomNumbers);
  TRI_AddGlobalFunctionVocbase(context, "SYS_GEN_RANDOM_SALT", JS_RandomSalt);
  TRI_AddGlobalFunctionVocbase(context, "SYS_GETLINE", JS_Getline);
  TRI_AddGlobalFunctionVocbase(context, "SYS_HMAC", JS_HMAC);
  TRI_AddGlobalFunctionVocbase(context, "SYS_PBKDF2", JS_PBKDF2);
  TRI_AddGlobalFunctionVocbase(context, "SYS_HTTP_STATISTICS", JS_HttpStatistics);
  TRI_AddGlobalFunctionVocbase(context, "SYS_IS_IP", JS_IsIP);
  TRI_AddGlobalFunctionVocbase(context, "SYS_KILL_EXTERNAL", JS_KillExternal);
  TRI_AddGlobalFunctionVocbase(context, "SYS_LOAD", JS_Load);
  TRI_AddGlobalFunctionVocbase(context, "SYS_LOG", JS_Log);
  TRI_AddGlobalFunctionVocbase(context, "SYS_LOG_LEVEL", JS_LogLevel);
  TRI_AddGlobalFunctionVocbase(context, "SYS_MD5", JS_Md5);
  TRI_AddGlobalFunctionVocbase(context, "SYS_OUTPUT", JS_Output);
  TRI_AddGlobalFunctionVocbase(context, "SYS_PARSE", JS_Parse);
  TRI_AddGlobalFunctionVocbase(context, "SYS_PROCESS_STATISTICS", JS_ProcessStatistics);
  TRI_AddGlobalFunctionVocbase(context, "SYS_RAND", JS_Rand);
  TRI_AddGlobalFunctionVocbase(context, "SYS_READ", JS_Read);
  TRI_AddGlobalFunctionVocbase(context, "SYS_READ_BUFFER", JS_ReadBuffer);
  TRI_AddGlobalFunctionVocbase(context, "SYS_READ64", JS_Read64);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SAVE", JS_Save);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SERVER_STATISTICS", JS_ServerStatistics);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SHA1", JS_Sha1);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SHA224", JS_Sha224);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SHA256", JS_Sha256);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SHA384", JS_Sha384);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SHA512", JS_Sha512);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SLEEP", JS_Sleep);
  TRI_AddGlobalFunctionVocbase(context, "SYS_SPRINTF", JS_SPrintF);
  TRI_AddGlobalFunctionVocbase(context, "SYS_STATUS_EXTERNAL", JS_StatusExternal);
  TRI_AddGlobalFunctionVocbase(context, "SYS_TEST_PORT", JS_TestPort);
  TRI_AddGlobalFunctionVocbase(context, "SYS_TIME", JS_Time);
  TRI_AddGlobalFunctionVocbase(context, "SYS_WAIT", JS_Wait);

  // register callback functions
  TRI_AddGlobalFunctionVocbase(context, "REGISTER_EXECUTE_FILE", JS_RegisterExecuteFile);

  // debugging functions
#ifdef TRI_ENABLE_FAILURE_TESTS
  TRI_AddGlobalFunctionVocbase(context, "SYS_DEBUG_SEGFAULT", JS_DebugSegfault);
  TRI_AddGlobalFunctionVocbase(context, "SYS_DEBUG_SET_FAILAT", JS_DebugSetFailAt);
  TRI_AddGlobalFunctionVocbase(context, "SYS_DEBUG_REMOVE_FAILAT", JS_DebugRemoveFailAt);
  TRI_AddGlobalFunctionVocbase(context, "SYS_DEBUG_CLEAR_FAILAT", JS_DebugClearFailAt);
#endif
  TRI_AddGlobalFunctionVocbase(context, "SYS_DEBUG_CAN_USE_FAILAT", JS_DebugCanUseFailAt);

  // .............................................................................
  // create the global variables
  // .............................................................................

  TRI_AddGlobalVariableVocbase(context, "HOME", v8::String::New(FileUtils::homeDirectory().c_str()));

  TRI_AddGlobalVariableVocbase(context, "MODULES_PATH", TRI_V8PathList(modules));
  TRI_AddGlobalVariableVocbase(context, "STARTUP_PATH", v8::String::New(startupPath.c_str()));
  TRI_AddGlobalVariableVocbase(context, "PATH_SEPARATOR", v8::String::New(TRI_DIR_SEPARATOR_STR));
  TRI_AddGlobalVariableVocbase(context, "VALGRIND", RUNNING_ON_VALGRIND > 0 ? v8::True() : v8::False());
  TRI_AddGlobalVariableVocbase(context, "VERSION", v8::String::New(TRI_VERSION));

  TRI_AddGlobalVariableVocbase(context, "CONNECTION_TIME_DISTRIBUTION", DistributionList(TRI_ConnectionTimeDistributionVectorStatistics));
  TRI_AddGlobalVariableVocbase(context, "REQUEST_TIME_DISTRIBUTION", DistributionList(TRI_RequestTimeDistributionVectorStatistics));
  TRI_AddGlobalVariableVocbase(context, "BYTES_SENT_DISTRIBUTION", DistributionList(TRI_BytesSentDistributionVectorStatistics));
  TRI_AddGlobalVariableVocbase(context, "BYTES_RECEIVED_DISTRIBUTION", DistributionList(TRI_BytesReceivedDistributionVectorStatistics));

  TRI_AddGlobalVariableVocbase(context, "SYS_PLATFORM", v8::String::New(TRI_PLATFORM));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
