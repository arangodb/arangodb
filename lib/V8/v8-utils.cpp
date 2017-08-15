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

#include "v8-utils.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include <signal.h>
#include <fstream>
#include <iostream>

#include "3rdParty/valgrind/valgrind.h"
#include "unicode/normalizer2.h"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/Nonce.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/files.h"
#include "Basics/process-utils.h"
#include "Basics/tri-strings.h"
#include "Basics/tri-zip.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Random/UniformCharacter.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/SslInterface.h"
#include "Ssl/ssl-helper.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-vpack.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

/// @brief Random string generators
namespace {
static UniformCharacter JSAlphaNumGenerator(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
static UniformCharacter JSNumGenerator("0123456789");
static UniformCharacter JSSaltGenerator(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*(){}"
    "[]:;<>,.?/|");
}

/// @brief Converts an object to a UTF-8-encoded and normalized character array.
TRI_Utf8ValueNFC::TRI_Utf8ValueNFC(TRI_memory_zone_t* memoryZone,
                                   v8::Handle<v8::Value> const obj)
    : _str(nullptr), _length(0), _memoryZone(memoryZone) {
  v8::String::Value str(obj);

  _str = TRI_normalize_utf16_to_NFC(_memoryZone, *str, (size_t)str.length(),
                                    &_length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Destroys a normalized string object
////////////////////////////////////////////////////////////////////////////////

TRI_Utf8ValueNFC::~TRI_Utf8ValueNFC() {
  if (_str != nullptr) {
    TRI_Free(_memoryZone, _str);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a Javascript error object
////////////////////////////////////////////////////////////////////////////////

static void CreateErrorObject(v8::Isolate* isolate, int errorNumber,
                              std::string const& message) noexcept {
  try {
    if (errorNumber == TRI_ERROR_OUT_OF_MEMORY) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "encountered out-of-memory error";
    }

    v8::Handle<v8::String> errorMessage = TRI_V8_STD_STRING(message);

    if (errorMessage.IsEmpty()) {
      isolate->ThrowException(v8::Object::New(isolate));
      return;
    }

    v8::Handle<v8::Value> err = v8::Exception::Error(errorMessage);

    if (err.IsEmpty()) {
      isolate->ThrowException(v8::Object::New(isolate));
      return;
    }

    v8::Handle<v8::Object> errorObject = err->ToObject();

    if (errorObject.IsEmpty()) {
      isolate->ThrowException(v8::Object::New(isolate));
      return;
    }

    errorObject->Set(TRI_V8_ASCII_STRING("errorNum"),
                     v8::Number::New(isolate, errorNumber));
    errorObject->Set(TRI_V8_ASCII_STRING("errorMessage"), errorMessage);

    TRI_GET_GLOBALS();
    TRI_GET_GLOBAL(ArangoErrorTempl, v8::ObjectTemplate);

    v8::Handle<v8::Object> ArangoError = ArangoErrorTempl->NewInstance();

    if (!ArangoError.IsEmpty()) {
      errorObject->SetPrototype(ArangoError);
    }

    isolate->ThrowException(errorObject);
  } catch (...) {
    // must not throw from here, as a C++ exception must not escape the
    // C++ bindings that are called by V8. if it does, the program will crash
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads/execute a file into/in the current context
////////////////////////////////////////////////////////////////////////////////

static bool LoadJavaScriptFile(v8::Isolate* isolate, char const* filename,
                               bool stripShebang, bool execute,
                               bool useGlobalContext) {
  v8::HandleScope handleScope(isolate);

  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, filename, &length);

  if (content == nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot load java script file '"
                                            << filename
                                            << "': " << TRI_last_error();
    return false;
  }

  // detect shebang
  size_t bangOffset = 0;
  if (stripShebang) {
    if (strncmp(content, "#!", 2) == 0) {
      // shebang
      char const* endOfBang = strchr(content, '\n');
      if (endOfBang != nullptr) {
        bangOffset = size_t(endOfBang - content + 1);
        TRI_ASSERT(bangOffset <= length);
        length -= bangOffset;
      }
    }
  }

  if (useGlobalContext) {
    char const* prologue = "(function() { ";
    char const* epilogue = "/* end-of-file */ })()";

    char* contentWrapper = TRI_Concatenate3String(
        TRI_UNKNOWN_MEM_ZONE, prologue, content + bangOffset, epilogue);

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

    length += strlen(prologue) + strlen(epilogue);
    content = contentWrapper;

    // shebang already handled here
    bangOffset = 0;
  }

  if (content == nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot load java script file '" << filename
        << "': " << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
    return false;
  }

  v8::Handle<v8::String> name = TRI_V8_STRING(filename);
  v8::Handle<v8::String> source =
      TRI_V8_PAIR_STRING(content + bangOffset, (int)length);

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

  v8::TryCatch tryCatch;

  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);

  if (tryCatch.HasCaught()) {
    TRI_LogV8Exception(isolate, &tryCatch);
    return false;
  }

  // compilation failed, print errors that happened during compilation
  if (script.IsEmpty()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot load java script file '"
                                            << filename
                                            << "': compilation failed.";
    return false;
  }

  if (execute) {
    // execute script
    v8::Handle<v8::Value> result = script->Run();

    if (tryCatch.HasCaught()) {
      TRI_LogV8Exception(isolate, &tryCatch);
      return false;
    }

    if (result.IsEmpty()) {
      return false;
    }
  }

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "loaded java script file: '"
                                            << filename << "'";
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads all files from a directory into the current context
////////////////////////////////////////////////////////////////////////////////

static bool LoadJavaScriptDirectory(v8::Isolate* isolate, char const* path,
                                    bool stripShebang, bool execute,
                                    bool useGlobalContext) {
  v8::HandleScope scope(isolate);
  bool result;

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "loading JavaScript directory: '"
                                            << path << "'";

  std::vector<std::string> files = TRI_FilesDirectory(path);

  result = true;

  for (auto const& filename : files) {
    v8::TryCatch tryCatch;
    bool ok;
    char* full;

    if (!StringUtils::isSuffix(filename, ".js")) {
      continue;
    }

    full = TRI_Concatenate2File(path, filename.c_str());

    ok = LoadJavaScriptFile(isolate, full, stripShebang, execute,
                            useGlobalContext);
    TRI_FreeString(TRI_CORE_MEM_ZONE, full);

    result = result && ok;

    if (!ok) {
      if (tryCatch.CanContinue()) {
        TRI_LogV8Exception(isolate, &tryCatch);
      } else {
        TRI_GET_GLOBALS();
        v8g->_canceled = true;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the program options
////////////////////////////////////////////////////////////////////////////////

static void JS_Options(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("options()");
  }

  VPackBuilder builder =
      ApplicationServer::server->options({"server.password"});
  auto result = TRI_VPackToV8(isolate, builder.slice());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decodes a base64-encoded string
///
/// @FUN{internal.base64Decode(@FA{value})}
///
/// Base64-decodes the string @FA{value}.
////////////////////////////////////////////////////////////////////////////////

static void JS_Base64Decode(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("base64Decode(<value>)");
  }

  try {
    std::string const value = TRI_ObjectToString(isolate, args[0]);
    std::string const base64 = StringUtils::decodeBase64(value);

    TRI_V8_RETURN_STD_STRING(base64);
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), TRI_last_error());
  }

  TRI_ASSERT(false);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief encodes a string as base64
///
/// @FUN{internal.base64Encode(@FA{value})}
///
/// Base64-encodes the string @FA{value}.
////////////////////////////////////////////////////////////////////////////////

static void JS_Base64Encode(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("base64Encode(<value>)");
  }

  try {
    std::string const&& value = TRI_ObjectToString(isolate, args[0]);
    std::string const&& base64 = StringUtils::encodeBase64(value);

    TRI_V8_RETURN_STD_STRING(base64);
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), TRI_last_error());
  }

  TRI_ASSERT(false);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a Javascript snippet, but does not execute it
///
/// @FUN{internal.parse(@FA{script})}
///
/// Parses the @FA{script} code, but does not execute it.
/// Will return *true* if the code does not have a parse error, and throw
/// an exception otherwise.
////////////////////////////////////////////////////////////////////////////////

static void JS_Parse(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("parse(<script>)");
  }

  v8::Handle<v8::Value> source = args[0];
  v8::Handle<v8::Value> filename;

  if (args.Length() > 1) {
    filename = args[1];
  } else {
    filename = TRI_V8_ASCII_STRING("<snippet>");
  }

  if (!source->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("<script> must be a string");
  }

  v8::TryCatch tryCatch;
  v8::Handle<v8::Script> script =
      v8::Script::Compile(source->ToString(), filename->ToString());

  // compilation failed, we have caught an exception
  if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      v8::Local<v8::Object> exceptionObj =
          tryCatch.Exception().As<v8::Object>();
      v8::Handle<v8::Message> message = tryCatch.Message();
      exceptionObj->Set(TRI_V8_ASCII_STRING("lineNumber"),
                        v8::Number::New(isolate, message->GetLineNumber()));
      exceptionObj->Set(TRI_V8_ASCII_STRING("columnNumber"),
                        v8::Number::New(isolate, message->GetStartColumn()));
      exceptionObj->Set(TRI_V8_ASCII_STRING("fileName"), filename->ToString());
      tryCatch.ReThrow();
      return;
    } else {
      TRI_GET_GLOBALS();
      v8g->_canceled = true;
      tryCatch.ReThrow();
      return;
    }
  }

  // compilation failed, we don't know why
  if (script.IsEmpty()) {
    TRI_V8_RETURN_FALSE();
  }
  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a Javascript file, but does not execute it
///
/// @FUN{internal.parseFile(@FA{filename})}
///
/// Parses the file @FA{filename}, but does not execute it.
/// Will return *true* if the code does not have a parse error, and throw
/// an exception otherwise.
////////////////////////////////////////////////////////////////////////////////

static void JS_ParseFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("parseFile(<filename>)");
  }

  if (!args[0]->IsString() && !args[0]->IsStringObject()) {
    TRI_V8_THROW_EXCEPTION_USAGE("parseFile(<filename>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 string");
  }

  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *name, &length);

  if (content == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), "cannot read file");
  }

  v8::TryCatch tryCatch;
  v8::Handle<v8::Script> script = v8::Script::Compile(
      TRI_V8_PAIR_STRING(content, (int)length), args[0]->ToString());

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

  // compilation failed, we have caught an exception
  if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      v8::Local<v8::Object> exceptionObj =
          tryCatch.Exception().As<v8::Object>();
      v8::Handle<v8::Message> message = tryCatch.Message();
      exceptionObj->Set(TRI_V8_ASCII_STRING("lineNumber"),
                        v8::Number::New(isolate, message->GetLineNumber()));
      exceptionObj->Set(TRI_V8_ASCII_STRING("columnNumber"),
                        v8::Number::New(isolate, message->GetStartColumn()));
      exceptionObj->Set(TRI_V8_ASCII_STRING("fileName"), args[0]);
      tryCatch.ReThrow();
      return;
    } else {
      TRI_GET_GLOBALS();
      v8g->_canceled = true;
      tryCatch.ReThrow();
      return;
    }
  }

  // compilation failed, we don't know why
  if (script.IsEmpty()) {
    TRI_V8_RETURN_FALSE();
  }
  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for internal.download()
////////////////////////////////////////////////////////////////////////////////

static std::string GetEndpointFromUrl(std::string const& url) {
  char const* p = url.c_str();
  char const* e = p + url.size();
  size_t slashes = 0;

  while (p < e) {
    if (*p == '?') {
      // http(s)://example.com?foo=bar
      return url.substr(0, p - url.c_str());
    } else if (*p == '/') {
      if (++slashes == 3) {
        return url.substr(0, p - url.c_str());
      }
    }
    ++p;
  }

  return url;
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
/// - @LIT{returnBodyAsBuffer}: whether or not to return the result body in
///   a Buffer object (default is false, body will be returned in a String)
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

void JS_Download(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  std::string const signature = "download(<url>, <body>, <options>, <outfile>)";

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(signature);
  }

  std::string url = TRI_ObjectToString(isolate, args[0]);

  if (!url.empty() && url[0] == '/') {
    std::vector<std::string> endpoints;

    // check if we are a server
    try {
      HttpEndpointProvider* server =
          ApplicationServer::getFeature<HttpEndpointProvider>("Endpoint");
      endpoints = server->httpEndpoints();
    } catch (...) {
      HttpEndpointProvider* client =
          ApplicationServer::getFeature<HttpEndpointProvider>("Client");
      endpoints = client->httpEndpoints();
    }

    // a relative url. now make this an absolute URL if possible
    for (auto const& endpoint : endpoints) {
      std::string fullurl = endpoint;

      // ipv4: replace 0.0.0.0 with 127.0.0.1
      auto pos = fullurl.find("//0.0.0.0");

      if (pos != std::string::npos) {
        fullurl.replace(pos, strlen("//0.0.0.0"), "//127.0.0.1");
      }

      // ipv6: replace [::] with [::1]
      else {
        pos = fullurl.find("//[::]");

        if (pos != std::string::npos) {
          fullurl.replace(pos, strlen("//[::]"), "//[::1]");
        }
      }

      url = fullurl + url;

      break;
    }
  }

  std::string lastEndpoint = GetEndpointFromUrl(url);

  std::string body;
  if (args.Length() > 1) {
    if (args[1]->IsString() || args[1]->IsStringObject()) {
      // supplied body is a string
      body = TRI_ObjectToString(isolate, args[1]);
    } else if (args[1]->IsObject() && V8Buffer::hasInstance(isolate, args[1])) {
      // supplied body is a Buffer object
      char const* data = V8Buffer::data(args[1].As<v8::Object>());
      size_t size = V8Buffer::length(args[1].As<v8::Object>());

      if (data == nullptr) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "invalid <body> buffer value");
      }

      body.assign(data, size);
    }
  }

  // options
  // ------------------------------------------------------------------------

  std::unordered_map<std::string, std::string> headerFields;
  double timeout = 10.0;
  bool returnBodyAsBuffer = false;
  bool followRedirects = true;
  rest::RequestType method = rest::RequestType::GET;
  bool returnBodyOnError = false;
  int maxRedirects = 5;
  uint64_t sslProtocol = TLS_V12;

  if (args.Length() > 2) {
    if (!args[2]->IsObject()) {
      TRI_V8_THROW_EXCEPTION_USAGE(signature);
    }

    v8::Handle<v8::Array> options = v8::Handle<v8::Array>::Cast(args[2]);

    if (options.IsEmpty()) {
      TRI_V8_THROW_EXCEPTION_USAGE(signature);
    }

    // ssl protocol
    if (options->Has(TRI_V8_ASCII_STRING("sslProtocol"))) {
      if (sslProtocol >= SSL_LAST) {
        // invalid protocol
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "invalid option value for sslProtocol");
      }

      sslProtocol = TRI_ObjectToUInt64(
          options->Get(TRI_V8_ASCII_STRING("sslProtocol")), false);
    }

    // method
    if (options->Has(TRI_V8_ASCII_STRING("method"))) {
      std::string methodString = TRI_ObjectToString(
          isolate, options->Get(TRI_V8_ASCII_STRING("method")));

      method = HttpRequest::translateMethod(methodString);
    }

    // headers
    if (options->Has(TRI_V8_ASCII_STRING("headers"))) {
      v8::Handle<v8::Object> v8Headers =
          options->Get(TRI_V8_ASCII_STRING("headers")).As<v8::Object>();
      if (v8Headers->IsObject()) {
        v8::Handle<v8::Array> props = v8Headers->GetPropertyNames();

        for (uint32_t i = 0; i < props->Length(); i++) {
          v8::Handle<v8::Value> key = props->Get(i);
          headerFields[TRI_ObjectToString(isolate, key)] =
              TRI_ObjectToString(isolate, v8Headers->Get(key));
        }
      }
    }

    // timeout
    if (options->Has(TRI_V8_ASCII_STRING("timeout"))) {
      if (!options->Get(TRI_V8_ASCII_STRING("timeout"))->IsNumber()) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "invalid option value for timeout");
      }

      timeout =
          TRI_ObjectToDouble(options->Get(TRI_V8_ASCII_STRING("timeout")));
    }

    // return body as a buffer?
    if (options->Has(TRI_V8_ASCII_STRING("returnBodyAsBuffer"))) {
      returnBodyAsBuffer = TRI_ObjectToBoolean(
          options->Get(TRI_V8_ASCII_STRING("returnBodyAsBuffer")));
    }

    // follow redirects
    if (options->Has(TRI_V8_ASCII_STRING("followRedirects"))) {
      followRedirects = TRI_ObjectToBoolean(
          options->Get(TRI_V8_ASCII_STRING("followRedirects")));
    }

    // max redirects
    if (options->Has(TRI_V8_ASCII_STRING("maxRedirects"))) {
      if (!options->Get(TRI_V8_ASCII_STRING("maxRedirects"))->IsNumber()) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "invalid option value for maxRedirects");
      }

      maxRedirects = (int)TRI_ObjectToInt64(
          options->Get(TRI_V8_ASCII_STRING("maxRedirects")));
    }

    if (!body.empty() && (method == rest::RequestType::GET ||
                          method == rest::RequestType::HEAD)) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "should not provide a body value for this request method");
    }

    if (options->Has(TRI_V8_ASCII_STRING("returnBodyOnError"))) {
      returnBodyOnError = TRI_ObjectToBoolean(
          options->Get(TRI_V8_ASCII_STRING("returnBodyOnError")));
    }
  }

  // outfile
  std::string outfile;
  if (args.Length() >= 4) {
    if (args[3]->IsString() || args[3]->IsStringObject()) {
      outfile = TRI_ObjectToString(isolate, args[3]);
    }

    if (outfile.empty()) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid value provided for outfile");
    }

    if (TRI_ExistsFile(outfile.c_str())) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_CANNOT_OVERWRITE_FILE);
    }
  }

  int numRedirects = 0;

  while (numRedirects < maxRedirects) {
    std::string endpoint;
    std::string relative;

    if (url.substr(0, 7) == "http://") {
      endpoint = GetEndpointFromUrl(url).substr(7);
      relative = url.substr(7 + endpoint.length());

      if (relative.empty() || relative[0] != '/') {
        relative = "/" + relative;
      }
      if (endpoint.find(':') == std::string::npos) {
        endpoint.append(":80");
      }
      endpoint = "tcp://" + endpoint;
    } else if (url.substr(0, 8) == "https://") {
      endpoint = GetEndpointFromUrl(url).substr(8);
      relative = url.substr(8 + endpoint.length());

      if (relative.empty() || relative[0] != '/') {
        relative = "/" + relative;
      }
      if (endpoint.find(':') == std::string::npos) {
        endpoint.append(":443");
      }
      endpoint = "ssl://" + endpoint;
    } else if (url.substr(0, 6) == "srv://") {
      size_t found = url.find('/', 6);

      relative = "/";
      if (found != std::string::npos) {
        relative.append(url.substr(found + 1));
        endpoint = url.substr(6, found - 6);
      } else {
        endpoint = url.substr(6);
      }
      endpoint = "srv://" + endpoint;
    } else if (!url.empty() && url[0] == '/') {
      size_t found;
      // relative URL. prefix it with last endpoint
      relative = url;
      url = lastEndpoint + url;
      endpoint = lastEndpoint;
      if (endpoint.substr(0, 5) == "http:") {
        endpoint = endpoint.substr(5);
        found = endpoint.find(":");
        if (found == std::string::npos) {
          endpoint = endpoint + ":80";
        }
        endpoint = "tcp:" + endpoint;
      } else if (endpoint.substr(0, 6) == "https:") {
        endpoint = endpoint.substr(6);
        found = endpoint.find(":");
        if (found == std::string::npos) {
          endpoint = endpoint + ":443";
        }
        endpoint = "ssl:" + endpoint;
      }
    } else {
      TRI_V8_THROW_SYNTAX_ERROR("unsupported URL specified");
    }

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
        << "downloading file. endpoint: " << endpoint
        << ", relative URL: " << url;

    std::unique_ptr<Endpoint> ep(Endpoint::clientFactory(endpoint));

    if (ep == nullptr) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid URL");
    }

    std::unique_ptr<GeneralClientConnection> connection(
        GeneralClientConnection::factory(ep.get(), timeout, timeout, 3,
                                         sslProtocol));

    if (connection == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }
    
    SimpleHttpClientParams params(timeout, false);
    params.setSupportDeflate(false);
    // security by obscurity won't work. Github requires a useragent nowadays.
    params.setExposeArangoDB(true);
    SimpleHttpClient client(connection.get(), params);
    
    v8::Handle<v8::Object> result = v8::Object::New(isolate);

    if (numRedirects > 0) {
      // do not send extra headers now
      headerFields.clear();
    }

    // send the actual request
    std::unique_ptr<SimpleHttpResult> response(
        client.request(method, relative, (body.size() > 0 ? body.c_str() : ""),
                       body.size(), headerFields));

    int returnCode = 500;  // set a default
    std::string returnMessage;

    if (response == nullptr || !response->isComplete()) {
      // save error message
      returnMessage = client.getErrorMessage();
      returnCode = 500;

      if (response != nullptr && response->getHttpReturnCode() > 0) {
        returnCode = response->getHttpReturnCode();
      }
    } else {
      TRI_ASSERT(response != nullptr);

      returnMessage = response->getHttpReturnMessage();
      returnCode = response->getHttpReturnCode();

      // follow redirects?
      if (followRedirects &&
          (returnCode == 301 || returnCode == 302 || returnCode == 307)) {
        bool found;
        url = response->getHeaderField(StaticStrings::Location, found);

        if (!found) {
          TRI_V8_THROW_EXCEPTION_INTERNAL("caught invalid redirect URL");
        }

        numRedirects++;

        if (url.substr(0, 5) == "http:" || url.substr(0, 6) == "https:") {
          lastEndpoint = GetEndpointFromUrl(url);
        }
        continue;
      }

      result->Set(TRI_V8_ASCII_STRING("code"),
                  v8::Number::New(isolate, returnCode));
      result->Set(TRI_V8_ASCII_STRING("message"),
                  TRI_V8_STD_STRING(returnMessage));

      // process response headers
      auto const& responseHeaders = response->getHeaderFields();

      v8::Handle<v8::Object> headers = v8::Object::New(isolate);

      for (auto const& it : responseHeaders) {
        headers->ForceSet(TRI_V8_STD_STRING(it.first),
                          TRI_V8_STD_STRING(it.second));
      }

      result->ForceSet(TRI_V8_ASCII_STRING("headers"), headers);

      if (returnBodyOnError || (returnCode >= 200 && returnCode <= 299)) {
        try {
          if (outfile.size() > 0) {
            // save outfile
            FileUtils::spit(outfile, response->getBody());
          } else {
            // set "body" attribute in result
            const StringBuffer& sb = response->getBody();

            if (returnBodyAsBuffer) {
              V8Buffer* buffer =
                  V8Buffer::New(isolate, sb.c_str(), sb.length());
              v8::Local<v8::Object> bufferObject =
                  v8::Local<v8::Object>::New(isolate, buffer->_handle);
              result->Set(TRI_V8_ASCII_STRING("body"), bufferObject);
            } else {
              result->Set(TRI_V8_ASCII_STRING("body"), TRI_V8_STD_STRING(sb));
            }
          }
        } catch (...) {
        }
      }
    }

    result->Set(TRI_V8_ASCII_STRING("code"),
                v8::Number::New(isolate, returnCode));
    result->Set(TRI_V8_ASCII_STRING("message"),
                TRI_V8_STD_STRING(returnMessage));

    TRI_V8_RETURN(result);
  }

  TRI_V8_THROW_EXCEPTION_INTERNAL("too many redirects");
  TRI_V8_TRY_CATCH_END
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

static void JS_Execute(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("execute(<script>, <sandbox>, <filename>)");
  }

  v8::Handle<v8::Value> source = args[0];
  v8::Handle<v8::Value> sandboxValue = args[1];
  v8::Handle<v8::Value> filename = args[2];

  if (!source->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("<script> must be a string");
  }

  bool useSandbox = sandboxValue->IsObject();
  v8::Handle<v8::Object> sandbox;
  v8::Handle<v8::Context> context;

  if (useSandbox) {
    sandbox = sandboxValue->ToObject();

    // create new context
    context = v8::Context::New(isolate);
    context->Enter();

    // copy sandbox into context
    v8::Handle<v8::Array> keys = sandbox->GetPropertyNames();

    for (uint32_t i = 0; i < keys->Length(); i++) {
      v8::Handle<v8::String> key =
          keys->Get(v8::Integer::New(isolate, i))->ToString();
      v8::Handle<v8::Value> value = sandbox->Get(key);

      if (Logger::logLevel() == arangodb::LogLevel::TRACE) {
        TRI_Utf8ValueNFC keyName(TRI_UNKNOWN_MEM_ZONE, key);

        if (*keyName != nullptr) {
          LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
              << "copying key '" << *keyName << "' from sandbox to context";
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

    script = v8::Script::Compile(source->ToString(), filename->ToString());

    // compilation failed, print errors that happened during compilation
    if (script.IsEmpty()) {
      if (useSandbox) {
        context->DetachGlobal();
        context->Exit();
      }

      if (tryCatch.CanContinue()) {
        v8::Local<v8::Object> exceptionObj =
            tryCatch.Exception().As<v8::Object>();
        v8::Handle<v8::Message> message = tryCatch.Message();
        exceptionObj->Set(TRI_V8_ASCII_STRING("lineNumber"),
                          v8::Number::New(isolate, message->GetLineNumber()));
        exceptionObj->Set(TRI_V8_ASCII_STRING("columnNumber"),
                          v8::Number::New(isolate, message->GetStartColumn()));
        exceptionObj->Set(TRI_V8_ASCII_STRING("fileName"),
                          filename->ToString());
        tryCatch.ReThrow();
        return;
      } else {
        TRI_GET_GLOBALS();
        v8g->_canceled = true;
        tryCatch.ReThrow();
        return;
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
        TRI_V8_LOG_THROW_EXCEPTION(tryCatch);
      } else {
        tryCatch.ReThrow();
        TRI_GET_GLOBALS();
        v8g->_canceled = true;
        TRI_V8_RETURN_UNDEFINED();
      }
    }
  }

  // copy result back into the sandbox
  if (useSandbox) {
    v8::Handle<v8::Array> keys = context->Global()->GetPropertyNames();

    for (uint32_t i = 0; i < keys->Length(); i++) {
      v8::Handle<v8::String> key =
          keys->Get(v8::Integer::New(isolate, i))->ToString();
      v8::Handle<v8::Value> value = context->Global()->Get(key);

      if (Logger::logLevel() == arangodb::LogLevel::TRACE) {
        TRI_Utf8ValueNFC keyName(TRI_UNKNOWN_MEM_ZONE, key);

        if (*keyName != nullptr) {
          LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
              << "copying key '" << *keyName << "' from context to sandbox";
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
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_Exists
////////////////////////////////////////////////////////////////////////////////

static void JS_Exists(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("exists(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }

  if (TRI_ExistsFile(*name)) {
    TRI_V8_RETURN_TRUE();
  } else {
    TRI_V8_RETURN_FALSE();
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_Chmod
////////////////////////////////////////////////////////////////////////////////

static void JS_ChMod(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("chmod(<path>, <mode>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }

  std::string const modeStr = TRI_ObjectToString(isolate, args[1]);
  size_t const length = modeStr.length();

  if (length == 0 || length > 5) {
    TRI_V8_THROW_TYPE_ERROR(
        "<mode> must be a string with up to 4 octal digits in it plus a "
        "leading zero.");
  }

  long mode = 0;
  for (uint32_t i = 0; i < length; ++i) {
    if (!isdigit(modeStr[i])) {
      TRI_V8_THROW_TYPE_ERROR(
          "<mode> must be a string with up to 4 octal digits in it plus a "
          "leading zero.");
    }
    char buf[2];
    buf[0] = modeStr[i];
    buf[1] = '\0';
    uint8_t digit = (uint8_t)atoi(buf);
    if (digit >= 8) {
      TRI_V8_THROW_TYPE_ERROR(
          "<mode> must be a string with up to 4 octal digits in it plus a "
          "leading zero.");
    }
    mode = mode | digit << ((length - i - 1) * 3);
  }
  std::string err;
  int rc = TRI_ChMod(*name, mode, err);

  if (rc != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(rc, err);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_Size
////////////////////////////////////////////////////////////////////////////////

static void JS_SizeFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("size(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }

  if (!TRI_ExistsFile(*name) || TRI_IsDirectory(*name)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FILE_NOT_FOUND);
  }

  int64_t size = TRI_SizeFile(*name);

  if (size < 0) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FILE_NOT_FOUND);
  }

  TRI_V8_RETURN(v8::Number::New(isolate, (double)size));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads in a line from stdin
////////////////////////////////////////////////////////////////////////////////

static void JS_Getline(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  std::string line;
  getline(std::cin, line);

  TRI_V8_RETURN_STD_STRING(line);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_GetTempPath
////////////////////////////////////////////////////////////////////////////////

static void JS_GetTempPath(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getTempPath()");
  }

  std::string path = TRI_GetUserTempPath();
  v8::Handle<v8::Value> result = TRI_V8_STRING(path.c_str());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_GetTempFile
////////////////////////////////////////////////////////////////////////////////

static void JS_GetTempFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("getTempFile(<directory>, <createFile>)");
  }

  char const* p = nullptr;
  std::string path;
  if (args.Length() > 0) {
    path = TRI_ObjectToString(isolate, args[0]);
    p = path.c_str();
  }

  bool create = false;
  if (args.Length() > 1) {
    create = TRI_ObjectToBoolean(args[1]);
  }

  char* result = nullptr;
  long systemError;
  std::string errorMessage;
  if (TRI_GetTempName(p, &result, create, systemError, errorMessage) !=
      TRI_ERROR_NO_ERROR) {
    errorMessage = std::string("could not create temp file: ") + errorMessage;
    TRI_V8_THROW_EXCEPTION_INTERNAL(errorMessage);
  }

  std::string const tempfile(result);
  TRI_Free(TRI_CORE_MEM_ZONE, result);

  // return result
  TRI_V8_RETURN_STD_STRING(tempfile);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_IsDirectory
////////////////////////////////////////////////////////////////////////////////

static void JS_IsDirectory(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("isDirectory(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }

  // return result
  if (TRI_IsDirectory(*name)) {
    TRI_V8_RETURN_TRUE();
  } else {
    TRI_V8_RETURN_FALSE();
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_IsFile
////////////////////////////////////////////////////////////////////////////////

static void JS_IsFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("isFile(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }

  // return result
  if (TRI_ExistsFile(*name) && !TRI_IsDirectory(*name)) {
    TRI_V8_RETURN_TRUE();
  } else {
    TRI_V8_RETURN_FALSE();
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_MakeAbsolute
////////////////////////////////////////////////////////////////////////////////

static void JS_MakeAbsolute(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("makeAbsolute(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }

  FileResultString cwd = FileUtils::currentDirectory();

  if (!cwd.ok()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        cwd.sysErrorNumber(),
        "cannot get current working directory: " + cwd.errorMessage());
  }

  char* abs = TRI_GetAbsolutePath(*name, cwd.result().c_str());
  v8::Handle<v8::String> res;

  if (nullptr != abs) {
    res = TRI_V8_STRING(abs);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, abs);
  } else {
    res = TRI_V8_STD_STRING(cwd.result());
  }

  // return result
  TRI_V8_RETURN(res);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_List
////////////////////////////////////////////////////////////////////////////////

static void JS_List(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("listTree(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }

  // constructed listing
  v8::Handle<v8::Array> result = v8::Array::New(isolate);
  std::vector<std::string> list = TRI_FilesDirectory(*name);

  uint32_t j = 0;

  for (auto const& f : list) {
    result->Set(j++, TRI_V8_STD_STRING(f));
  }

  // return result
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_ListTree
////////////////////////////////////////////////////////////////////////////////

static void JS_ListTree(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("listTree(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }

  // constructed listing
  v8::Handle<v8::Array> result = v8::Array::New(isolate);
  std::vector<std::string> files(TRI_FullTreeDirectory(*name));

  uint32_t i = 0;
  for (auto const& it : files) {
    result->Set(i++, TRI_V8_STD_STRING(it));
  }

  // return result
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_MakeDirectory
////////////////////////////////////////////////////////////////////////////////

static void JS_MakeDirectory(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // 2nd argument (permissions) are ignored for now

  // extract arguments
  if (args.Length() != 1 && args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("makeDirectory(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }
  long systemError = 0;
  std::string systemErrorStr;
  int res = TRI_CreateDirectory(*name, systemError, systemErrorStr);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, systemErrorStr);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_MakeDirectoryRecursive
////////////////////////////////////////////////////////////////////////////////

static void JS_MakeDirectoryRecursive(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // 2nd argument (permissions) are ignored for now

  // extract arguments
  if (args.Length() != 1 && args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("makeDirectoryRecursive(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }
  long systemError = 0;
  std::string systemErrorStr;
  int res = TRI_CreateRecursiveDirectory(*name, systemError, systemErrorStr);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, systemErrorStr);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_Unzip
////////////////////////////////////////////////////////////////////////////////

static void JS_UnzipFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "unzipFile(<filename>, <outPath>, <skipPaths>, <overwrite>, "
        "<password>)");
  }

  std::string const filename = TRI_ObjectToString(isolate, args[0]);
  std::string const outPath = TRI_ObjectToString(isolate, args[1]);

  bool skipPaths = false;
  if (args.Length() > 2) {
    skipPaths = TRI_ObjectToBoolean(args[2]);
  }

  bool overwrite = false;
  if (args.Length() > 3) {
    overwrite = TRI_ObjectToBoolean(args[3]);
  }

  char const* p = nullptr;
  std::string password;
  if (args.Length() > 4) {
    password = TRI_ObjectToString(isolate, args[4]);
    p = password.c_str();
  }
  std::string errMsg;
  int res = TRI_UnzipFile(filename.c_str(), outPath.c_str(), skipPaths,
                          overwrite, p, errMsg);

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_THROW_EXCEPTION_MESSAGE(res, errMsg);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_Zip
////////////////////////////////////////////////////////////////////////////////

static void JS_ZipFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() < 3 || args.Length() > 4) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "zipFile(<filename>, <chdir>, <files>, <password>)");
  }

  std::string const filename = TRI_ObjectToString(isolate, args[0]);
  std::string const dir = TRI_ObjectToString(isolate, args[1]);

  if (!args[2]->IsArray()) {
    TRI_V8_THROW_EXCEPTION_USAGE("<files> must be a list");
  }

  v8::Handle<v8::Array> files = v8::Handle<v8::Array>::Cast(args[2]);

  int res = TRI_ERROR_NO_ERROR;
  std::vector<std::string> filenames;

  for (uint32_t i = 0; i < files->Length(); ++i) {
    v8::Handle<v8::Value> file = files->Get(i);
    if (file->IsString()) {
      filenames.emplace_back(TRI_ObjectToString(isolate, file));
    } else {
      res = TRI_ERROR_BAD_PARAMETER;
      break;
    }
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "zipFile(<filename>, <chdir>, <files>, <password>)");
  }

  char const* p = nullptr;
  std::string password;
  if (args.Length() == 4) {
    password = TRI_ObjectToString(isolate, args[3]);
    p = password.c_str();
  }

  res = TRI_ZipFile(filename.c_str(), dir.c_str(), filenames, p);

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_THROW_EXCEPTION(res);
  TRI_V8_TRY_CATCH_END
}


static void JS_Adler32(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("adler32(<filename>, <useSigned>)");
  }

  bool useSigned = false;  // default is unsigned
  if (args.Length() > 1) {
    useSigned = TRI_ObjectToBoolean(args[1]);
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 string");
  }

  uint32_t chksum = 0;
  int res = TRI_Adler32(*name, chksum);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Value> result;

  if (useSigned) {
    result = v8::Number::New(isolate, static_cast<int32_t>(chksum));
  } else {
    result = v8::Number::New(isolate, chksum);
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file and executes it
///
/// @FUN{internal.load(*filename*)}
///
/// Reads in a files and executes the contents in the current context.
////////////////////////////////////////////////////////////////////////////////

static void JS_Load(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("load(<filename>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 string");
  }

  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *name, &length);

  if (content == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), "cannot read file");
  }

  v8::Handle<v8::Value> filename = args[0];

  // save state of __dirname and __filename
  v8::Handle<v8::Object> current = isolate->GetCurrentContext()->Global();
  auto oldFilename = current->Get(TRI_V8_ASCII_STRING("__filename"));
  current->ForceSet(TRI_V8_ASCII_STRING("__filename"), filename);

  auto oldDirname = current->Get(TRI_V8_ASCII_STRING("__dirname"));
  auto dirname = TRI_Dirname(TRI_ObjectToString(isolate, filename).c_str());
  current->ForceSet(TRI_V8_ASCII_STRING("__dirname"), TRI_V8_STRING(dirname));
  TRI_FreeString(TRI_CORE_MEM_ZONE, dirname);

  v8::Handle<v8::Value> result;
  {
    v8::TryCatch tryCatch;

    result = TRI_ExecuteJavaScriptString(isolate, isolate->GetCurrentContext(),
                                         TRI_V8_PAIR_STRING(content, length),
                                         filename->ToString(), false);

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

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
    if (result.IsEmpty()) {
      if (tryCatch.CanContinue()) {
        TRI_V8_LOG_THROW_EXCEPTION(tryCatch);
      } else {
        tryCatch.ReThrow();
        TRI_GET_GLOBALS();
        v8g->_canceled = true;
        TRI_V8_RETURN_UNDEFINED();
      }
    }
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
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

static void JS_Log(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("log(<level>, <message>)");
  }

  TRI_Utf8ValueNFC level(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*level == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<level> must be a string");
  }

  std::vector<std::string> splitted = StringUtils::split(*level, '=');

  std::string ls = "info";
  std::string ts = "";

  if (splitted.size() == 1) {
    ts = splitted[0];
  } else if (splitted.size() >= 2) {
    ts = splitted[0];
    ls = splitted[1];
  }

  std::string prefix;
  LogLevel ll = LogLevel::WARN;

  StringUtils::tolowerInPlace(&ls);
  StringUtils::tolowerInPlace(&ts);

  if (ls == "fatal") {
    prefix = "FATAL! ";
    ll = LogLevel::ERR;
  } else if (ls == "error") {
    ll = LogLevel::ERR;
  } else if (ls == "warning" || ls == "warn") {
    ll = LogLevel::WARN;
  } else if (ls == "info") {
    ll = LogLevel::INFO;
  } else if (ls == "debug") {
    ll = LogLevel::DEBUG;
  } else if (ls == "trace") {
    ll = LogLevel::TRACE;
  } else {
    prefix = ls + "!";
  }

  LogTopic* topic = ts.empty() ? nullptr : LogTopic::lookup(ts);

  if (args[1]->IsArray()) {
    auto loglines = v8::Handle<v8::Array>::Cast(args[1]);
    std::vector<std::string> logLineVec;

    logLineVec.reserve(loglines->Length());
    
    for (uint32_t i = 0; i < loglines->Length(); ++i) {
      v8::Handle<v8::Value> line = loglines->Get(i);
      TRI_Utf8ValueNFC message(TRI_UNKNOWN_MEM_ZONE, line);
      if (line->IsString()) {
        logLineVec.push_back(*message);
      }
    }
    for (auto& message: logLineVec) {
      if (topic == nullptr) {
        LOG_TOPIC_RAW(ll, Logger::FIXME) << prefix << message;
      } else {
        LOG_TOPIC_RAW(ll, *topic) << prefix << message;
      }
    }
  }
  else {
    TRI_Utf8ValueNFC message(TRI_UNKNOWN_MEM_ZONE, args[1]);

    if (*message == nullptr) {
      TRI_V8_THROW_TYPE_ERROR("<message> must be a string or an array");
    }

    std::string msg = *message;
    if (topic == nullptr) {
      LOG_TOPIC_RAW(ll, Logger::FIXME) << prefix << msg;
    } else {
      LOG_TOPIC_RAW(ll, *topic) << prefix << msg;
    }
  }
  
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the log-level
///
/// @FUN{internal.logLevel()}
///
/// Returns the current global log-level as string.
///
/// @FUN{internal.logLevel(true)}
///
/// Returns all topic log-level as vector of strings.
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
////////////////////////////////////////////////////////////////////////////////

static void JS_LogLevel(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (1 <= args.Length()) {
    if (args[0]->IsBoolean()) {
      auto levels = Logger::logLevelTopics();
      size_t n = levels.size();
      v8::Handle<v8::Array> object =
          v8::Array::New(isolate, static_cast<int>(n));

      uint32_t pos = 0;

      for (auto level : levels) {
        std::string output =
            level.first + "=" + Logger::translateLogLevel(level.second);
        v8::Handle<v8::String> val = TRI_V8_STD_STRING(output);

        object->Set(pos++, val);
      }

      TRI_V8_RETURN(object);
    } else {
      TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, args[0]);
      Logger::setLogLevel(*str);
      TRI_V8_RETURN_UNDEFINED();
    }
  } else {
    auto level = Logger::translateLogLevel(Logger::logLevel());
    TRI_V8_RETURN_STRING(level.c_str());
  }

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the md5 sum of a text
///
/// @FUN{internal.md5(@FA{text})}
///
/// Computes an md5 for the @FA{text}.
////////////////////////////////////////////////////////////////////////////////

static void JS_Md5(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("md5(<text>)");
  }

  v8::String::Utf8Value str(args[0]);

  if (*str == nullptr) {
    TRI_V8_RETURN_UNDEFINED();
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
  v8::Handle<v8::String> hashStr = TRI_V8_PAIR_STRING(hex, 32);

  TRI_V8_RETURN(hashStr);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates random numbers
///
/// @FUN{internal.genRandomNumbers(@FA{length})}
///
/// Generates a string of a given @FA{length} containing numbers.
////////////////////////////////////////////////////////////////////////////////

static void JS_RandomNumbers(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsNumber()) {
    TRI_V8_THROW_EXCEPTION_USAGE("genRandomNumbers(<length>)");
  }

  int length = (int)TRI_ObjectToInt64(args[0]);

  if (length <= 0 || length > 65536) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "<length> must be between 0 and 65536");
  }

  std::string str = JSNumGenerator.random(length);
  TRI_V8_RETURN_STD_STRING(str);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates random alpha-numbers
///
/// @FUN{internal.genRandomAlphaNumbers(@FA{length})}
///
/// Generates a string of a given @FA{length} containing numbers and characters.
////////////////////////////////////////////////////////////////////////////////

static void JS_RandomAlphaNum(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsNumber()) {
    TRI_V8_THROW_EXCEPTION_USAGE("genRandomAlphaNumbers(<length>)");
  }

  int length = (int)TRI_ObjectToInt64(args[0]);
  if (length <= 0 || length > 65536) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "<length> must be between 0 and 65536");
  }

  std::string&& str = JSAlphaNumGenerator.random(length);
  TRI_V8_RETURN_STD_STRING(str);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a salt
///
/// @FUN{internal.genRandomSalt(@FA{length})}
///
/// Generates a string of a given @FA{length} containing ASCII characters.
////////////////////////////////////////////////////////////////////////////////

static void JS_RandomSalt(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsNumber()) {
    TRI_V8_THROW_EXCEPTION_USAGE("genRandomSalt(<length>)");
  }

  int length = (int)TRI_ObjectToInt64(args[0]);
  if (length <= 0 || length > 65536) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "<length> must be between 0 and 65536");
  }

  std::string str = JSSaltGenerator.random(length);
  TRI_V8_RETURN_STD_STRING(str);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a nonce
///
/// @FUN{internal.createNonce()}
///
/// Generates a base64 encoded nonce string. (length of the string is 16)
////////////////////////////////////////////////////////////////////////////////

static void JS_CreateNonce(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("createNonce()");
  }

  std::string str = Nonce::createNonce();

  TRI_V8_RETURN_STD_STRING(str);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a base64 encoded nonce
///
/// @FUN{internal.checkAndMarkNonce(@FA{nonce})}
///
/// Checks and marks a @FA{nonce}
////////////////////////////////////////////////////////////////////////////////

static void JS_MarkNonce(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("checkAndMarkNonce(<nonce>)");
  }

  TRI_Utf8ValueNFC base64u(TRI_CORE_MEM_ZONE, args[0]);

  if (base64u.length() != 16) {
    TRI_V8_THROW_TYPE_ERROR("expecting 16-Byte base64url-encoded nonce");
  }

  std::string raw = StringUtils::decodeBase64U(*base64u);

  if (raw.size() != 12) {
    TRI_V8_THROW_TYPE_ERROR("expecting 12-Byte nonce");
  }

  if (Nonce::checkAndMark(raw)) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_MTime
////////////////////////////////////////////////////////////////////////////////

static void JS_MTime(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract two arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("mtime(<filename>)");
  }

  std::string filename = TRI_ObjectToString(isolate, args[0]);

  int64_t mtime;
  int res = TRI_MTimeFile(filename.c_str(), &mtime);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN(v8::Number::New(isolate, static_cast<double>(mtime)));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_MoveFile
////////////////////////////////////////////////////////////////////////////////

static void JS_MoveFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract two arguments
  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("move(<source>, <destination>)");
  }

  std::string source = TRI_ObjectToString(isolate, args[0]);
  std::string destination = TRI_ObjectToString(isolate, args[1]);

  bool const sourceIsDirectory = TRI_IsDirectory(source.c_str());
  bool const destinationIsDirectory = TRI_IsDirectory(destination.c_str());

  if (sourceIsDirectory && destinationIsDirectory) {
    // source is a directory, destination is a directory. this is unsupported
    TRI_V8_THROW_EXCEPTION_PARAMETER(
        "cannot move source directory into destination directory");
  }

  if (TRI_IsRegularFile(source.c_str()) && destinationIsDirectory) {
    // source is a file, destination is a directory. this is unsupported
    TRI_V8_THROW_EXCEPTION_PARAMETER(
        "cannot move source file into destination directory");
  }
  std::string systemErrorStr;
  long errorNo;

  int res = TRI_RenameFile(source.c_str(), destination.c_str(), &errorNo,
                           &systemErrorStr);

  if (res != TRI_ERROR_NO_ERROR) {
    std::string errMsg = "cannot move file [" + source + "] to [" +
                         destination + " ] : " + std::to_string(errorNo) +
                         ": " + systemErrorStr;
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, errMsg);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_CopyDirectoryRecursive
////////////////////////////////////////////////////////////////////////////////

static void JS_CopyRecursive(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract two arguments
  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("copyRecursive(<source>, <destination>)");
  }

  std::string source = TRI_ObjectToString(isolate, args[0]);
  std::string destination = TRI_ObjectToString(isolate, args[1]);

  bool const sourceIsDirectory = TRI_IsDirectory(source.c_str());
  bool const destinationIsDirectory = TRI_IsDirectory(destination.c_str());

  if (sourceIsDirectory && destinationIsDirectory) {
    // source is a directory, destination is a directory. this is unsupported
    TRI_V8_THROW_EXCEPTION_PARAMETER(
        "cannot copy source directory into destination directory");
  }

  if (TRI_IsRegularFile(source.c_str()) && destinationIsDirectory) {
    // source is a file, destination is a directory. this is unsupported
    TRI_V8_THROW_EXCEPTION_PARAMETER(
        "cannot copy source file into destination directory");
  }
  std::string systemErrorStr;
  long errorNo;
  int res = TRI_CreateRecursiveDirectory(destination.c_str(), errorNo,
                                         systemErrorStr);

  if (res != TRI_ERROR_NO_ERROR) {
    std::string errMsg = "cannot copy file [" + source + "] to [" +
                         destination + " ] : " + std::to_string(errorNo) +
                         " - Unable to create target directory: " +
                         systemErrorStr;
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_SYS_ERROR, errMsg);
  }

  if (!FileUtils::copyRecursive(source, destination, systemErrorStr)) {
    std::string errMsg = "cannot copy directory [" + source + "] to [" +
                         destination + " ] : " + std::to_string(errorNo) +
                         ": " + systemErrorStr;
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, errMsg);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_CopyFile
////////////////////////////////////////////////////////////////////////////////

static void JS_CopyFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract two arguments
  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("copyFile(<source>, <destination>)");
  }

  std::string source = TRI_ObjectToString(isolate, args[0]);
  std::string destination = TRI_ObjectToString(isolate, args[1]);

  bool const destinationIsDirectory = TRI_IsDirectory(destination.c_str());

  if (!TRI_IsRegularFile(source.c_str())) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("can only copy regular files.");
  }

  std::string systemErrorStr;

  if (destinationIsDirectory) {
    char const* file = strrchr(source.c_str(), TRI_DIR_SEPARATOR_CHAR);
    if (file == nullptr) {
      if (destination[destination.length()] == TRI_DIR_SEPARATOR_CHAR) {
        destination += TRI_DIR_SEPARATOR_CHAR;
      }
      destination += source;
    } else {
      destination += file;
    }
  }
  if (!TRI_CopyFile(source, destination, systemErrorStr)) {
    std::string errMsg = "cannot copy file [" + source + "] to [" +
                         destination + " ] : " + systemErrorStr;
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, errMsg);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
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

static void JS_Output(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  for (int i = 0; i < args.Length(); i++) {
    // extract the next argument
    v8::Handle<v8::Value> val = args[i];

    // convert it into a string
    v8::String::Utf8Value utf8(val);
    // TRI_Utf8ValueNFC utf8(TRI_UNKNOWN_MEM_ZONE, val);

    if (*utf8 == nullptr) {
      continue;
    }

    // print the argument
    char const* ptr = *utf8;
    size_t len = utf8.length();

    while (0 < len) {
      ssize_t n = TRI_WRITE(STDOUT_FILENO, ptr, (TRI_write_t)len);

      if (n < 0) {
        break;
      }

      len -= n;
      ptr += n;
    }
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
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

static void JS_ProcessStatistics(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  TRI_process_info_t info = TRI_ProcessInfoSelf();
  double rss = (double)info._residentSize;
  double rssp = 0;

  if (TRI_PhysicalMemory != 0) {
    rssp = rss / TRI_PhysicalMemory;
  }

  result->Set(TRI_V8_ASCII_STRING("minorPageFaults"),
              v8::Number::New(isolate, (double)info._minorPageFaults));
  result->Set(TRI_V8_ASCII_STRING("majorPageFaults"),
              v8::Number::New(isolate, (double)info._majorPageFaults));
  result->Set(TRI_V8_ASCII_STRING("userTime"),
              v8::Number::New(isolate,
                              (double)info._userTime / (double)info._scClkTck));
  result->Set(TRI_V8_ASCII_STRING("systemTime"),
              v8::Number::New(
                  isolate, (double)info._systemTime / (double)info._scClkTck));
  result->Set(TRI_V8_ASCII_STRING("numberOfThreads"),
              v8::Number::New(isolate, (double)info._numberThreads));
  result->Set(TRI_V8_ASCII_STRING("residentSize"),
              v8::Number::New(isolate, rss));
  result->Set(TRI_V8_ASCII_STRING("residentSizePercent"),
              v8::Number::New(isolate, rssp));
  result->Set(TRI_V8_ASCII_STRING("virtualSize"),
              v8::Number::New(isolate, (double)info._virtualSize));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a random number using OpenSSL
///
/// @FUN{internal.rand()}
///
/// Generates a random number
////////////////////////////////////////////////////////////////////////////////

static void JS_Rand(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  // check arguments
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("rand()");
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
      TRI_V8_RETURN(v8::Number::New(isolate, value));
    }

    // we don't want to return 0 as the result, so we try again
  }

  // we failed to produce a valid random number
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_Read
////////////////////////////////////////////////////////////////////////////////

static void JS_Read(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("read(<filename>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 string");
  }

  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *name, &length);

  if (content == nullptr) {
    std::string msg = TRI_last_error();
    msg += ": while reading ";
    msg += *name;
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), msg);
  }

  auto result = TRI_V8_PAIR_STRING(content, length);

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_ReadBuffer
////////////////////////////////////////////////////////////////////////////////

static void JS_ReadBuffer(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("readBuffer(<filename>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 string");
  }

  size_t length;
  char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *name, &length);

  if (content == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), TRI_last_error());
  }

  V8Buffer* buffer = V8Buffer::New(isolate, content, length);

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

  TRI_V8_RETURN(buffer->_handle);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_Read64
////////////////////////////////////////////////////////////////////////////////

static void JS_Read64(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("read(<filename>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 string");
  }

  std::string base64;

  try {
    std::string&& content = FileUtils::slurp(*name);
    base64 = StringUtils::encodeBase64(content);
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), TRI_last_error());
  }

  TRI_V8_RETURN_STD_STRING(base64);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append to a file
////////////////////////////////////////////////////////////////////////////////

static void JS_Append(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("append(<filename>, <content>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a string");
  }

  if (args[1]->IsObject() && V8Buffer::hasInstance(isolate, args[1])) {
    // content is a buffer
    char const* data = V8Buffer::data(args[1].As<v8::Object>());
    size_t size = V8Buffer::length(args[1].As<v8::Object>());

    if (data == nullptr) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid <content> buffer value");
    }

    std::ofstream file;

    file.open(*name, std::ios::out | std::ios::binary | std::ios::app);

    if (file.is_open()) {
      file.write(data, size);
      file.close();
      TRI_V8_RETURN_TRUE();
    }
  } else {
    TRI_Utf8ValueNFC content(TRI_UNKNOWN_MEM_ZONE, args[1]);

    if (*content == nullptr) {
      TRI_V8_THROW_TYPE_ERROR("<content> must be a string");
    }

    std::ofstream file;

    file.open(*name, std::ios::out | std::ios::binary | std::ios::app);

    if (file.is_open()) {
      file.write(*content, content.length());
      file.close();
      TRI_V8_RETURN_TRUE();
    }
  }

  TRI_V8_THROW_EXCEPTION_SYS("cannot write file");
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write to a file
////////////////////////////////////////////////////////////////////////////////

static void JS_Write(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("write(<filename>, <content>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a string");
  }

  bool flush = false;
  if (args.Length() > 2) {
    flush = TRI_ObjectToBoolean(args[2]);
  }

  if (args[1]->IsObject() && V8Buffer::hasInstance(isolate, args[1])) {
    // content is a buffer
    char const* data = V8Buffer::data(args[1].As<v8::Object>());
    size_t size = V8Buffer::length(args[1].As<v8::Object>());

    if (data == nullptr) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid <content> buffer value");
    }

    std::ofstream file;

    file.open(*name, std::ios::out | std::ios::binary);

    if (file.is_open()) {
      file.write(data, size);
      if (flush) {
        file.flush();
      }
      file.close();
      TRI_V8_RETURN_TRUE();
    }
  } else {
    TRI_Utf8ValueNFC content(TRI_UNKNOWN_MEM_ZONE, args[1]);

    if (*content == nullptr) {
      TRI_V8_THROW_TYPE_ERROR("<content> must be a string");
    }

    std::ofstream file;

    file.open(*name, std::ios::out | std::ios::binary);

    if (file.is_open()) {
      file << *content;
      if (flush) {
        file.flush();
      }
      file.close();
      TRI_V8_RETURN_TRUE();
    }
  }

  TRI_V8_THROW_EXCEPTION_SYS("cannot write file");
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_Remove
////////////////////////////////////////////////////////////////////////////////

static void JS_Remove(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("remove(<filename>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }

  int res = TRI_UnlinkFile(*name);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot remove file");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_RemoveDirectory
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveDirectory(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("removeDirectory(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }

  if (!TRI_IsDirectory(*name)) {
    std::string err =
        std::string("<path> must be a valid directory name (have '") + *name +
        "')";
    TRI_V8_THROW_EXCEPTION_PARAMETER(err);
  }

  int res = TRI_RemoveEmptyDirectory(*name);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot remove directory");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JS_RemoveDirectoryRecursive
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveRecursiveDirectory(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the arguments
  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("removeDirectoryRecursive(<path>)");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<path> must be a string");
  }

  if (!TRI_IsDirectory(*name)) {
    std::string err =
        std::string("<path> must be a valid directory name (have '") + *name +
        "')";
    TRI_V8_THROW_EXCEPTION_PARAMETER(err);
  }

  bool force = false;

  if (args.Length() > 1) {
    force = TRI_ObjectToBoolean(args[1]);
  }

  if (!force) {
    // check if we're inside the temp directory. force will override this check
    std::string tempPath = TRI_GetUserTempPath();

    if (tempPath.size() < 6) {
      TRI_V8_THROW_EXCEPTION_PARAMETER(
          "temporary directory name is too short. will not remove directory");
    }

    std::string const path(*name);
#ifdef _WIN32
    // windows paths are case-insensitive
    if (!TRI_CaseEqualString(path.c_str(), tempPath.c_str(), tempPath.size())) {
#else
    if (!TRI_EqualString(path.c_str(), tempPath.c_str(), tempPath.size())) {
#endif
      std::string errorMessage = std::string("directory to be removed [") +
                                 path + "] is outside of temporary path [" +
                                 tempPath + "]";
      TRI_V8_THROW_EXCEPTION_PARAMETER(errorMessage);
    }
  }

  int res = TRI_RemoveDirectory(*name);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot remove directory");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief formats the arguments
///
/// @FUN{internal.sprintf(@FA{format}, @FA{argument1}, ...)}
///
/// Formats the arguments according to the format string @FA{format}.
////////////////////////////////////////////////////////////////////////////////

static void JS_SPrintF(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  size_t len = args.Length();

  if (len == 0) {
    TRI_V8_RETURN_STRING("");
  }

  TRI_Utf8ValueNFC format(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*format == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<format> must be a UTF-8 string");
  }

  std::string result;

  size_t p = 1;

  for (char const* ptr = *format; *ptr; ++ptr) {
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
          } else {
            bool e;
            double f = TRI_ObjectToDouble(args[(int)p], e);

            if (e) {
              result += "NaN";
            } else {
              char b[1024];

              if (*ptr == 'f') {
                snprintf(b, sizeof(b), "%f", f);
              } else {
                snprintf(b, sizeof(b), "%ld", (long)f);
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
          } else {
            TRI_Utf8ValueNFC text(TRI_UNKNOWN_MEM_ZONE, args[(int)p]);

            if (*text == nullptr) {
              std::string msg = StringUtils::itoa(p) +
                                ".th argument must be a string in format '" +
                                *format + "'";
              TRI_V8_THROW_EXCEPTION_PARAMETER(msg.c_str());
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
    } else {
      result += *ptr;
    }
  }

  for (size_t i = p; i < len; ++i) {
    TRI_Utf8ValueNFC text(TRI_UNKNOWN_MEM_ZONE, args[(int)i]);

    if (*text == nullptr) {
      std::string msg = StringUtils::itoa(i) +
                        ".th argument must be a string in format '" + *format +
                        "'";
      TRI_V8_THROW_TYPE_ERROR(msg.c_str());
    }

    result += " ";
    result += *text;
  }

  TRI_V8_RETURN_STD_STRING(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the sha512 sum
///
/// @FUN{internal.sha512(@FA{text})}
///
/// Computes an sha512 for the @FA{text}.
////////////////////////////////////////////////////////////////////////////////

static void JS_Sha512(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("sha512(<text>)");
  }

  std::string key = TRI_ObjectToString(isolate, args[0]);

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
  v8::Handle<v8::String> hashStr = TRI_V8_PAIR_STRING(hex, (int)hexLen);

  delete[] hex;

  TRI_V8_RETURN(hashStr);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the sha384 sum
///
/// @FUN{internal.sha384(@FA{text})}
///
/// Computes an sha384 for the @FA{text}.
////////////////////////////////////////////////////////////////////////////////

static void JS_Sha384(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("sha384(<text>)");
  }

  std::string key = TRI_ObjectToString(isolate, args[0]);

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
  v8::Handle<v8::String> hashStr = TRI_V8_PAIR_STRING(hex, (int)hexLen);

  delete[] hex;

  TRI_V8_RETURN(hashStr);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the sha256 sum
///
/// @FUN{internal.sha256(@FA{text})}
///
/// Computes an sha256 for the @FA{text}.
////////////////////////////////////////////////////////////////////////////////

static void JS_Sha256(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("sha256(<text>)");
  }

  std::string key = TRI_ObjectToString(isolate, args[0]);

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
  v8::Handle<v8::String> hashStr = TRI_V8_PAIR_STRING(hex, (int)hexLen);

  delete[] hex;

  TRI_V8_RETURN(hashStr);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the sha224 sum
///
/// @FUN{internal.sha224(@FA{text})}
///
/// Computes an sha224 for the @FA{text}.
////////////////////////////////////////////////////////////////////////////////

static void JS_Sha224(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("sha224(<text>)");
  }

  std::string key = TRI_ObjectToString(isolate, args[0]);

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
  v8::Handle<v8::String> hashStr = TRI_V8_PAIR_STRING(hex, (int)hexLen);

  delete[] hex;

  TRI_V8_RETURN(hashStr);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the sha1 sum
///
/// @FUN{internal.sha1(@FA{text})}
///
/// Computes an sha1 for the @FA{text}.
////////////////////////////////////////////////////////////////////////////////

static void JS_Sha1(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("sha1(<text>)");
  }

  std::string key = TRI_ObjectToString(isolate, args[0]);

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
  v8::Handle<v8::String> hashStr = TRI_V8_PAIR_STRING(hex, (int)hexLen);

  delete[] hex;

  TRI_V8_RETURN(hashStr);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sleeps
///
/// @FUN{internal.sleep(@FA{seconds})}
///
/// Wait for @FA{seconds}, without calling the garbage collection.
////////////////////////////////////////////////////////////////////////////////

static void JS_Sleep(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("sleep(<seconds>)");
  }

  double n = TRI_ObjectToDouble(args[0]);
  double until = TRI_microtime() + n;

  while (true) {
    double now = TRI_microtime();
    if (now >= until) {
      break;
    }
    uint64_t duration = (until - now >= 0.5)
                            ? 500000
                            : static_cast<uint64_t>((until - now) * 1000000);

    usleep(static_cast<TRI_usleep_t>(duration));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
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

static void JS_Time(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_V8_RETURN(v8::Number::New(isolate, TRI_microtime()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for the specified amount of time and calls the garbage
/// collection.
///
/// @FUN{internal.wait(@FA{seconds})}
///
/// Wait for @FA{seconds}, call the garbage collection.
////////////////////////////////////////////////////////////////////////////////

static void JS_Wait(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("wait(<seconds>, <gc>)");
  }

  double n = TRI_ObjectToDouble(args[0]);

  bool gc = true;  // default is to trigger the gc
  if (args.Length() > 1) {
    gc = TRI_ObjectToBoolean(args[1]);
  }

  if (gc) {
    TRI_RunGarbageCollectionV8(isolate, n);
  }

  // wait without gc
  double until = TRI_microtime() + n;
  while (TRI_microtime() < until) {
    usleep(1000);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether failure points can be used
///
/// @FUN{internal.debugCanUseFailAt()}
///
/// Returns whether failure points can be be used
////////////////////////////////////////////////////////////////////////////////

static void JS_DebugCanUseFailAt(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("debugCanUseFailAt()");
  }

  if (TRI_CanUseFailurePointsDebugging()) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the PBKDF2 HMAC SHA1 derived key
///
/// @FUN{internal.PBKDF2HS1(@FA{salt}, @FA{password}, @FA{iterations},
/// @FA{keyLength})}
///
/// Computes the PBKDF2 HMAC SHA1 derived key for the @FA{password}.
////////////////////////////////////////////////////////////////////////////////

static void JS_PBKDF2HS1(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() < 4 || !args[0]->IsString() || !args[1]->IsString() ||
      !args[2]->IsNumber() || !args[3]->IsNumber()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "PBKDF2(<salt>, <password>, <iterations>, <keyLength>)");
  }

  std::string salt = TRI_ObjectToString(isolate, args[0]);
  std::string password = TRI_ObjectToString(isolate, args[1]);
  int iterations = (int)TRI_ObjectToInt64(args[2]);
  int keyLength = (int)TRI_ObjectToInt64(args[3]);

  std::string result =
      SslInterface::sslPBKDF2HS1(salt.c_str(), salt.size(), password.c_str(),
                                 password.size(), iterations, keyLength);
  TRI_V8_RETURN_STD_STRING(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the PBKDF2 HMAC derived key
///
/// @FUN{internal.PBKDF2(@FA{salt}, @FA{password}, @FA{iterations},
/// @FA{keyLength}, @FA{algorithm})}
///
/// Computes the PBKDF2 HMAC derived key for the @FA{password}.
////////////////////////////////////////////////////////////////////////////////

static void JS_PBKDF2(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() < 4 || !args[0]->IsString() || !args[1]->IsString() ||
      !args[2]->IsNumber() || !args[3]->IsNumber()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "PBKDF2_SHA(<salt>, <password>, <iterations>, <keyLength>, "
        "<algorithm>)");
  }

  SslInterface::Algorithm al = SslInterface::Algorithm::ALGORITHM_SHA1;
  if (args.Length() > 4 && !args[4]->IsUndefined()) {
    std::string algorithm = TRI_ObjectToString(isolate, args[4]);
    StringUtils::tolowerInPlace(&algorithm);

    if (algorithm == "sha1") {
      al = SslInterface::Algorithm::ALGORITHM_SHA1;
    } else if (algorithm == "sha512") {
      al = SslInterface::Algorithm::ALGORITHM_SHA512;
    } else if (algorithm == "sha384") {
      al = SslInterface::Algorithm::ALGORITHM_SHA384;
    } else if (algorithm == "sha256") {
      al = SslInterface::Algorithm::ALGORITHM_SHA256;
    } else if (algorithm == "sha224") {
      al = SslInterface::Algorithm::ALGORITHM_SHA224;
    } else if (algorithm == "md5") {
      al = SslInterface::Algorithm::ALGORITHM_MD5;
    } else {
      TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <algorithm>");
    }
  }

  std::string salt = TRI_ObjectToString(isolate, args[0]);
  std::string password = TRI_ObjectToString(isolate, args[1]);
  int iterations = (int)TRI_ObjectToInt64(args[2]);
  int keyLength = (int)TRI_ObjectToInt64(args[3]);

  std::string result =
      SslInterface::sslPBKDF2(salt.c_str(), salt.size(), password.c_str(),
                              password.size(), iterations, keyLength, al);
  TRI_V8_RETURN_STD_STRING(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the HMAC signature
///
/// @FUN{internal.HMAC(@FA{key}, @FA{message}, @FA{algorithm})}
///
/// Computes the HMAC for the @FA{message}.
////////////////////////////////////////////////////////////////////////////////

static void JS_HMAC(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("HMAC(<key>, <text>, <algorithm>)");
  }

  std::string key = TRI_ObjectToString(isolate, args[0]);
  std::string message = TRI_ObjectToString(isolate, args[1]);

  SslInterface::Algorithm al = SslInterface::Algorithm::ALGORITHM_SHA256;
  if (args.Length() > 2 && !args[2]->IsUndefined()) {
    std::string algorithm = TRI_ObjectToString(isolate, args[2]);
    StringUtils::tolowerInPlace(&algorithm);

    if (algorithm == "sha1") {
      al = SslInterface::Algorithm::ALGORITHM_SHA1;
    } else if (algorithm == "sha512") {
      al = SslInterface::Algorithm::ALGORITHM_SHA512;
    } else if (algorithm == "sha384") {
      al = SslInterface::Algorithm::ALGORITHM_SHA384;
    } else if (algorithm == "sha256") {
      al = SslInterface::Algorithm::ALGORITHM_SHA256;
    } else if (algorithm == "sha224") {
      al = SslInterface::Algorithm::ALGORITHM_SHA224;
    } else if (algorithm == "md5") {
      al = SslInterface::Algorithm::ALGORITHM_MD5;
    } else {
      TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <algorithm>");
    }
  }

  std::string result = StringUtils::encodeHex(SslInterface::sslHMAC(
      key.c_str(), key.size(), message.c_str(), message.size(), al));
  TRI_V8_RETURN_STD_STRING(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a external program
////////////////////////////////////////////////////////////////////////////////

static void JS_ExecuteExternal(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the arguments
  if (3 < args.Length() || args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "executeExternal(<filename>[, <arguments> [,<usePipes>] ])");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a string");
  }

  char** arguments = nullptr;
  uint32_t n = 0;

  if (2 <= args.Length()) {
    v8::Handle<v8::Value> a = args[1];

    if (a->IsArray()) {
      v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(a);

      n = arr->Length();
      arguments = static_cast<char**>(
          TRI_Allocate(TRI_CORE_MEM_ZONE, n * sizeof(char*)));

      for (uint32_t i = 0; i < n; ++i) {
        TRI_Utf8ValueNFC arg(TRI_UNKNOWN_MEM_ZONE, arr->Get(i));

        if (*arg == nullptr) {
          arguments[i] = TRI_DuplicateString("");
        } else {
          arguments[i] = TRI_DuplicateString(*arg);
        }
      }
    } else {
      n = 1;
      arguments = static_cast<char**>(
          TRI_Allocate(TRI_CORE_MEM_ZONE, n * sizeof(char*)));

      TRI_Utf8ValueNFC arg(TRI_UNKNOWN_MEM_ZONE, a);

      if (*arg == nullptr) {
        arguments[0] = TRI_DuplicateString("");
      } else {
        arguments[0] = TRI_DuplicateString(*arg);
      }
    }
  }
  bool usePipes = false;
  if (3 <= args.Length()) {
    usePipes = TRI_ObjectToBoolean(args[2]);
  }

  TRI_external_id_t external;
  TRI_CreateExternalProcess(*name, const_cast<char const**>(arguments),
                            (size_t)n, usePipes, &external);
  if (arguments != nullptr) {
    for (uint32_t i = 0; i < n; ++i) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, arguments[i]);
    }

    TRI_Free(TRI_CORE_MEM_ZONE, arguments);
  }
  if (external._pid == TRI_INVALID_PROCESS_ID) {
    TRI_V8_THROW_ERROR("Process could not be started");
  }
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("pid"),
              v8::Number::New(isolate, external._pid));
// Now report about possible stdin and stdout pipes:
#ifndef _WIN32
  if (external._readPipe >= 0) {
    result->Set(TRI_V8_ASCII_STRING("readPipe"),
                v8::Number::New(isolate, external._readPipe));
  }
  if (external._writePipe >= 0) {
    result->Set(TRI_V8_ASCII_STRING("writePipe"),
                v8::Number::New(isolate, external._writePipe));
  }
#else
  size_t readPipe_len, writePipe_len;
  if (0 != external._readPipe) {
    char* readPipe = TRI_EncodeHexString((char const*)external._readPipe,
                                         sizeof(HANDLE), &readPipe_len);
    result->Set(TRI_V8_ASCII_STRING("readPipe"),
                TRI_V8_PAIR_STRING(readPipe, (int)readPipe_len));
    TRI_FreeString(TRI_CORE_MEM_ZONE, readPipe);
  }
  if (0 != external._writePipe) {
    char* writePipe = TRI_EncodeHexString((char const*)external._writePipe,
                                          sizeof(HANDLE), &writePipe_len);
    result->Set(TRI_V8_ASCII_STRING("writePipe"),
                TRI_V8_PAIR_STRING(writePipe, (int)writePipe_len));
    TRI_FreeString(TRI_CORE_MEM_ZONE, writePipe);
  }
#endif
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of an external process
////////////////////////////////////////////////////////////////////////////////

static void JS_StatusExternal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the arguments
  if (args.Length() < 1 || args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "statusExternal(<external-identifier>[, <wait>])");
  }

  TRI_external_id_t pid;
  memset(&pid, 0, sizeof(TRI_external_id_t));

#ifndef _WIN32
  pid._pid = static_cast<TRI_pid_t>(TRI_ObjectToUInt64(args[0], true));
#else
  pid._pid = static_cast<DWORD>(TRI_ObjectToUInt64(args[0], true));
#endif
  bool wait = false;
  if (args.Length() == 2) {
    wait = TRI_ObjectToBoolean(args[1]);
  }

  TRI_external_status_t external = TRI_CheckExternalProcess(pid, wait);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  char const* status = "UNKNOWN";

  switch (external._status) {
    case TRI_EXT_NOT_STARTED:
      status = "NOT-STARTED";
      break;
    case TRI_EXT_PIPE_FAILED:
      status = "FAILED";
      break;
    case TRI_EXT_FORK_FAILED:
      status = "FAILED";
      break;
    case TRI_EXT_RUNNING:
      status = "RUNNING";
      break;
    case TRI_EXT_NOT_FOUND:
      status = "NOT-FOUND";
      break;
    case TRI_EXT_TERMINATED:
      status = "TERMINATED";
      break;
    case TRI_EXT_ABORTED:
      status = "ABORTED";
      break;
    case TRI_EXT_STOPPED:
      status = "STOPPED";
      break;
  }

  result->Set(TRI_V8_ASCII_STRING("status"), TRI_V8_STRING(status));

  if (external._status == TRI_EXT_TERMINATED) {
    result->Set(
        TRI_V8_ASCII_STRING("exit"),
        v8::Integer::New(isolate, static_cast<int32_t>(external._exitStatus)));
  } else if (external._status == TRI_EXT_ABORTED) {
    result->Set(
        TRI_V8_ASCII_STRING("signal"),
        v8::Integer::New(isolate, static_cast<int32_t>(external._exitStatus)));
  }
  if (external._errorMessage.length() > 0) {
    result->Set(TRI_V8_ASCII_STRING("errorMessage"),
                TRI_V8_STD_STRING(external._errorMessage));
  }
  // return the result
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a external program
////////////////////////////////////////////////////////////////////////////////

static void JS_ExecuteAndWaitExternal(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the arguments
  if (3 < args.Length() || args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "executeAndWaitExternal(<filename>[, <arguments> [,<usePipes>] ])");
  }

  TRI_Utf8ValueNFC name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a string");
  }

  char** arguments = nullptr;
  uint32_t n = 0;

  if (2 <= args.Length()) {
    v8::Handle<v8::Value> a = args[1];

    if (a->IsArray()) {
      v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(a);

      n = arr->Length();
      arguments = static_cast<char**>(
          TRI_Allocate(TRI_CORE_MEM_ZONE, n * sizeof(char*)));

      for (uint32_t i = 0; i < n; ++i) {
        TRI_Utf8ValueNFC arg(TRI_UNKNOWN_MEM_ZONE, arr->Get(i));

        if (*arg == nullptr) {
          arguments[i] = TRI_DuplicateString("");
        } else {
          arguments[i] = TRI_DuplicateString(*arg);
        }
      }
    } else {
      n = 1;
      arguments = static_cast<char**>(
          TRI_Allocate(TRI_CORE_MEM_ZONE, n * sizeof(char*)));

      TRI_Utf8ValueNFC arg(TRI_UNKNOWN_MEM_ZONE, a);

      if (*arg == nullptr) {
        arguments[0] = TRI_DuplicateString("");
      } else {
        arguments[0] = TRI_DuplicateString(*arg);
      }
    }
  }
  bool usePipes = false;
  if (3 <= args.Length()) {
    usePipes = TRI_ObjectToBoolean(args[2]);
  }

  TRI_external_id_t external;
  TRI_CreateExternalProcess(*name, const_cast<char const**>(arguments),
                            static_cast<size_t>(n), usePipes, &external);
  if (arguments != nullptr) {
    for (uint32_t i = 0; i < n; ++i) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, arguments[i]);
    }

    TRI_Free(TRI_CORE_MEM_ZONE, arguments);
  }
  if (external._pid == TRI_INVALID_PROCESS_ID) {
    TRI_V8_THROW_ERROR("Process could not be started");
  }
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("pid"),
              v8::Number::New(isolate, external._pid));
// Now report about possible stdin and stdout pipes:
#ifndef _WIN32
  if (external._readPipe >= 0) {
    result->Set(TRI_V8_ASCII_STRING("readPipe"),
                v8::Number::New(isolate, external._readPipe));
  }
  if (external._writePipe >= 0) {
    result->Set(TRI_V8_ASCII_STRING("writePipe"),
                v8::Number::New(isolate, external._writePipe));
  }
#else
  size_t readPipe_len, writePipe_len;
  if (0 != external._readPipe) {
    char* readPipe = TRI_EncodeHexString((char const*)external._readPipe,
                                         sizeof(HANDLE), &readPipe_len);
    result->Set(TRI_V8_ASCII_STRING("readPipe"),
                TRI_V8_PAIR_STRING(readPipe, (int)readPipe_len));
    TRI_FreeString(TRI_CORE_MEM_ZONE, readPipe);
  }
  if (0 != external._writePipe) {
    char* writePipe = TRI_EncodeHexString((char const*)external._writePipe,
                                          sizeof(HANDLE), &writePipe_len);
    result->Set(TRI_V8_ASCII_STRING("writePipe"),
                TRI_V8_PAIR_STRING(writePipe, (int)writePipe_len));
    TRI_FreeString(TRI_CORE_MEM_ZONE, writePipe);
  }
#endif

  TRI_external_id_t pid;
  memset(&pid, 0, sizeof(TRI_external_id_t));

  pid._pid = external._pid;

  TRI_external_status_t external_status = TRI_CheckExternalProcess(pid, true);

  char const* status = "UNKNOWN";

  switch (external_status._status) {
    case TRI_EXT_NOT_STARTED:
      status = "NOT-STARTED";
      break;
    case TRI_EXT_PIPE_FAILED:
      status = "FAILED";
      break;
    case TRI_EXT_FORK_FAILED:
      status = "FAILED";
      break;
    case TRI_EXT_RUNNING:
      status = "RUNNING";
      break;
    case TRI_EXT_NOT_FOUND:
      status = "NOT-FOUND";
      break;
    case TRI_EXT_TERMINATED:
      status = "TERMINATED";
      break;
    case TRI_EXT_ABORTED:
      status = "ABORTED";
      break;
    case TRI_EXT_STOPPED:
      status = "STOPPED";
      break;
  }

  result->Set(TRI_V8_ASCII_STRING("status"), TRI_V8_ASCII_STRING(status));

  if (external_status._status == TRI_EXT_TERMINATED) {
    result->Set(TRI_V8_ASCII_STRING("exit"),
                v8::Integer::New(isolate, static_cast<int32_t>(
                                              external_status._exitStatus)));
  } else if (external_status._status == TRI_EXT_ABORTED) {
    result->Set(TRI_V8_ASCII_STRING("signal"),
                v8::Integer::New(isolate, static_cast<int32_t>(
                                              external_status._exitStatus)));
  }
  if (external_status._errorMessage.length() > 0) {
    result->Set(TRI_V8_ASCII_STRING("errorMessage"),
                TRI_V8_STD_STRING(external_status._errorMessage));
  }
  // return the result
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief kills an external process
////////////////////////////////////////////////////////////////////////////////

static void JS_KillExternal(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the arguments
  if (args.Length() < 1 || args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "killExternal(<external-identifier>, <signal>)");
  }
  int signal = SIGTERM;
  if (args.Length() == 2) {
    signal = static_cast<int>(TRI_ObjectToInt64(args[1]));
  }
  TRI_external_id_t pid;
  memset(&pid, 0, sizeof(TRI_external_id_t));

#ifndef _WIN32
  pid._pid = static_cast<TRI_pid_t>(TRI_ObjectToUInt64(args[0], true));
#else
  pid._pid = static_cast<DWORD>(TRI_ObjectToUInt64(args[0], true));
#endif

  // return the result
  if (TRI_KillExternalProcess(pid, signal)) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief suspends an external process, only Unix
////////////////////////////////////////////////////////////////////////////////

static void JS_SuspendExternal(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("suspendExternal(<external-identifier>)");
  }

  TRI_external_id_t pid;
  memset(&pid, 0, sizeof(TRI_external_id_t));

#ifndef _WIN32
  pid._pid = static_cast<TRI_pid_t>(TRI_ObjectToUInt64(args[0], true));
#else
  pid._pid = static_cast<DWORD>(TRI_ObjectToUInt64(args[0], true));
#endif

  // return the result
  if (TRI_SuspendExternalProcess(pid)) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief continues an external process
////////////////////////////////////////////////////////////////////////////////

static void JS_ContinueExternal(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("continueExternal(<external-identifier>)");
  }

  TRI_external_id_t pid;
  memset(&pid, 0, sizeof(TRI_external_id_t));

#ifndef _WIN32
  pid._pid = static_cast<TRI_pid_t>(TRI_ObjectToUInt64(args[0], true));
#else
  pid._pid = static_cast<DWORD>(TRI_ObjectToUInt64(args[0], true));
#endif

  // return the result
  if (TRI_ContinueExternalProcess(pid)) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a port is available
////////////////////////////////////////////////////////////////////////////////

static void JS_TestPort(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("testPort(<address>)");
  }

  std::string address = TRI_ObjectToString(isolate, args[0]);
  Endpoint* endpoint = Endpoint::serverFactory(address, 10, false);
  if (nullptr == endpoint) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "address description invalid, cannot create endpoint");
  }
  TRI_socket_t s = endpoint->connect(1, 1);
  bool available = TRI_isvalidsocket(s);

  if (available) {
    endpoint->disconnect();
  }

  delete endpoint;

  // return the result
  if (available) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the application server is stopping
////////////////////////////////////////////////////////////////////////////////

static void JS_IsStopping(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the arguments
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("isStopping()");
  }

  if (ApplicationServer::isStopping()) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

/// @brief convert a V8 value to VPack
static void JS_V8ToVPack(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the argument
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("V8_TO_VPACK(value)");
  }

  VPackBuilder builder;
  int res = TRI_V8ToVPack(isolate, builder, args[0], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  VPackSlice slice = builder.slice();

  V8Buffer* buffer =
      V8Buffer::New(isolate, slice.startAs<char const>(), slice.byteSize());
  v8::Local<v8::Object> bufferObject =
      v8::Local<v8::Object>::New(isolate, buffer->_handle);
  TRI_V8_RETURN(bufferObject);

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

/// @brief convert a VPack value to V8
static void JS_VPackToV8(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract the argument
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("VPACK_TO_V8(value)");
  }

  if (args[0]->IsString() || args[0]->IsStringObject()) {
    // supplied argument is a string
    std::string const value = TRI_ObjectToString(isolate, args[0]);

    VPackValidator validator;
    validator.validate(value.c_str(), value.size(), false);

    VPackSlice slice(value.c_str());
    v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, slice);
    TRI_V8_RETURN(result);
  } else if (args[0]->IsObject() && V8Buffer::hasInstance(isolate, args[0])) {
    // argument is a buffer
    char const* data = V8Buffer::data(args[0].As<v8::Object>());
    size_t size = V8Buffer::length(args[0].As<v8::Object>());

    VPackValidator validator;
    validator.validate(data, size, false);

    VPackSlice slice(data);
    v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, slice);

    TRI_V8_RETURN(result);
  } else {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid argument type for VPACK_TO_V8()");
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoError
////////////////////////////////////////////////////////////////////////////////

static void JS_ArangoError(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL_STRING(ErrorKey);
  TRI_GET_GLOBAL_STRING(ErrorNumKey);

  v8::Handle<v8::Object> self = args.Holder()->ToObject();

  self->Set(ErrorKey, v8::True(isolate));
  self->Set(ErrorNumKey, v8::Integer::New(isolate, TRI_ERROR_FAILED));

  if (0 < args.Length() && args[0]->IsObject()) {
    TRI_GET_GLOBAL_STRING(CodeKey);
    TRI_GET_GLOBAL_STRING(ErrorMessageKey);

    v8::Handle<v8::Object> data = args[0]->ToObject();

    if (data->Has(ErrorKey)) {
      self->Set(ErrorKey, data->Get(ErrorKey));
    }

    if (data->Has(CodeKey)) {
      self->Set(CodeKey, data->Get(CodeKey));
    }

    if (data->Has(ErrorNumKey)) {
      self->Set(ErrorNumKey, data->Get(ErrorNumKey));
    }

    if (data->Has(ErrorMessageKey)) {
      self->Set(ErrorMessageKey, data->Get(ErrorMessageKey));
    }
  }

  {
    // call Error.captureStackTrace(this) so the ArangoError object gets a nifty
    // stack trace
    v8::Handle<v8::Object> current = isolate->GetCurrentContext()->Global();
    v8::Handle<v8::Value> errorObject =
        current->Get(TRI_V8_ASCII_STRING("Error"));
    v8::Handle<v8::Object> err = v8::Handle<v8::Object>::Cast(errorObject);
    v8::Handle<v8::Function> captureStackTrace = v8::Handle<v8::Function>::Cast(
        err->Get(TRI_V8_ASCII_STRING("captureStackTrace")));

    v8::Handle<v8::Value> arguments[] = {self};
    captureStackTrace->Call(current, 1, arguments);
  }

  TRI_V8_RETURN(self);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief isIP
////////////////////////////////////////////////////////////////////////////////

static void JS_IsIP(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("isIP(<value>)");
  }

  TRI_Utf8ValueNFC address(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (TRI_InetPton4(*address, nullptr) == TRI_ERROR_NO_ERROR) {
    TRI_V8_RETURN(v8::Number::New(isolate, 4));
  } else if (TRI_InetPton6(*address, nullptr) == 0) {
    TRI_V8_RETURN(v8::Number::New(isolate, 6));
  } else {
    TRI_V8_RETURN(v8::Number::New(isolate, 0));
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief SplitWordlist - splits words via the tokenizer
////////////////////////////////////////////////////////////////////////////////

static void JS_SplitWordlist(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if ((args.Length() < 2) || (args.Length() > 4)) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "SplitWordlist(<value>, minLength, [<maxLength>, [<lowerCase>]])");
  }

  std::string stringToTokenize = TRI_ObjectToString(isolate, args[0]);

  size_t minLength = static_cast<size_t>(TRI_ObjectToUInt64(args[1], true));

  size_t maxLength = 40;  // -> TRI_FULLTEXT_MAX_WORD_LENGTH;
  if (args.Length() > 2) {
    maxLength = static_cast<size_t>(TRI_ObjectToUInt64(args[2], true));
  }

  bool lowerCase = false;
  if (args.Length() > 3) {
    lowerCase = TRI_ObjectToBoolean(args[3]);
  }

  std::set<std::string> wordList;

  if (!Utf8Helper::DefaultUtf8Helper.tokenize(
          wordList, stringToTokenize, minLength, maxLength, lowerCase)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "SplitWordlist failed!");
  }

  v8::Handle<v8::Array> v8WordList =
      v8::Array::New(isolate, static_cast<int>(wordList.size()));

  uint32_t i = 0;
  for (std::string const& word : wordList) {
    v8::Handle<v8::String> oneWord = TRI_V8_STD_STRING(word);
    v8WordList->Set(i, oneWord);
    i++;
  }

  TRI_V8_RETURN(v8WordList);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reports an exception
////////////////////////////////////////////////////////////////////////////////

std::string TRI_StringifyV8Exception(v8::Isolate* isolate,
                                     v8::TryCatch* tryCatch) {
  v8::HandleScope handle_scope(isolate);

  TRI_Utf8ValueNFC exception(TRI_UNKNOWN_MEM_ZONE, tryCatch->Exception());
  char const* exceptionString = *exception;
  v8::Handle<v8::Message> message = tryCatch->Message();
  std::string result;

  // V8 didn't provide any extra information about this error; just print the
  // exception.
  if (message.IsEmpty()) {
    if (exceptionString == nullptr) {
      result = "JavaScript exception\n";
    } else {
      result = "JavaScript exception: " + std::string(exceptionString) + "\n";
    }
  } else {
    TRI_Utf8ValueNFC filename(TRI_UNKNOWN_MEM_ZONE,
                              message->GetScriptResourceName());
    char const* filenameString = *filename;
    int linenum = message->GetLineNumber();
    int start = message->GetStartColumn() + 1;
    int end = message->GetEndColumn();

    if ((filenameString == nullptr) ||
        (!strcmp(filenameString, TRI_V8_SHELL_COMMAND_NAME))) {
      if (exceptionString == nullptr) {
        result = "JavaScript exception\n";
      } else {
        result = "JavaScript exception: " + std::string(exceptionString) + "\n";
      }
    } else {
      if (exceptionString == nullptr) {
        result = "JavaScript exception in file '" +
                 std::string(filenameString) + "' at " +
                 StringUtils::itoa(linenum) + "," + StringUtils::itoa(start) +
                 "\n";
      } else {
        result = "JavaScript exception in file '" +
                 std::string(filenameString) + "' at " +
                 StringUtils::itoa(linenum) + "," + StringUtils::itoa(start) +
                 ": " + exceptionString + "\n";
      }
    }

    TRI_Utf8ValueNFC sourceline(TRI_UNKNOWN_MEM_ZONE, message->GetSourceLine());

    if (*sourceline) {
      std::string l = *sourceline;

      result += "!" + l + "\n";

      if (1 < start) {
        l = std::string(start - 1, ' ');
      } else {
        l = "";
      }

      // in fact, we observed start being greater than end sometimes...
      // this does not make sense and seems to be a bug in V8, but it happens
      // so we need to work around this
      if (end >= start) {
        l += std::string((size_t)(end - start + 1), '^');
      } else {
        l = "^";
      }

      result += "!" + l + "\n";
    }

    TRI_Utf8ValueNFC stacktrace(TRI_UNKNOWN_MEM_ZONE, tryCatch->StackTrace());

    if (*stacktrace && stacktrace.length() > 0) {
      result += "stacktrace: " + std::string(*stacktrace) + "\n";
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an exception and stacktrace
////////////////////////////////////////////////////////////////////////////////

void TRI_LogV8Exception(v8::Isolate* isolate, v8::TryCatch* tryCatch) {
  v8::HandleScope handle_scope(isolate);

  TRI_Utf8ValueNFC exception(TRI_UNKNOWN_MEM_ZONE, tryCatch->Exception());
  char const* exceptionString = *exception;
  v8::Handle<v8::Message> message = tryCatch->Message();

  // V8 didn't provide any extra information about this error; just print the
  // exception.
  if (message.IsEmpty()) {
    if (exceptionString == nullptr) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "JavaScript exception";
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "JavaScript exception: "
                                              << exceptionString;
    }
  } else {
    TRI_Utf8ValueNFC filename(TRI_UNKNOWN_MEM_ZONE,
                              message->GetScriptResourceName());
    char const* filenameString = *filename;
    // if ifdef is not used, the compiler will complain about linenum being
    // unused
    int linenum = message->GetLineNumber();
    int start = message->GetStartColumn() + 1;
    int end = message->GetEndColumn();

    if (filenameString == nullptr) {
      if (exceptionString == nullptr) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "JavaScript exception";
      } else {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "JavaScript exception: "
                                                << exceptionString;
      }
    } else {
      if (exceptionString == nullptr) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "JavaScript exception in file '" << filenameString << "' at "
            << linenum << "," << start;
      } else {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "JavaScript exception in file '" << filenameString << "' at "
            << linenum << "," << start << ": " << exceptionString;
      }
    }

    TRI_Utf8ValueNFC sourceline(TRI_UNKNOWN_MEM_ZONE, message->GetSourceLine());

    if (*sourceline) {
      std::string l = *sourceline;

      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "!" << l;

      if (1 < start) {
        l = std::string(start - 1, ' ');
      } else {
        l = "";
      }

      l += std::string((size_t)(end - start + 1), '^');

      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "!" << l;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file into the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteGlobalJavaScriptFile(v8::Isolate* isolate, char const* filename,
                                     bool stripShebang) {
  return LoadJavaScriptFile(isolate, filename, stripShebang, true, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads all files from a directory into the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteGlobalJavaScriptDirectory(v8::Isolate* isolate,
                                          char const* path) {
  return LoadJavaScriptDirectory(isolate, path, false, true, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes all files from a directory in a local context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteLocalJavaScriptDirectory(v8::Isolate* isolate,
                                         char const* path) {
  return LoadJavaScriptDirectory(isolate, path, false, true, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseJavaScriptFile(v8::Isolate* isolate, char const* filename,
                             bool stripShebang) {
  return LoadJavaScriptFile(isolate, filename, stripShebang, false, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a string within a V8 context, optionally print the result
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ExecuteJavaScriptString(
    v8::Isolate* isolate, v8::Handle<v8::Context> context,
    v8::Handle<v8::String> const source, v8::Handle<v8::String> const name,
    bool printResult) {
  v8::EscapableHandleScope scope(isolate);

  v8::Handle<v8::Value> result;
  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);

  // compilation failed, print errors that happened during compilation
  if (script.IsEmpty()) {
    return scope.Escape<v8::Value>(result);
  }

  // compilation succeeded, run the script
  result = script->Run();

  if (result.IsEmpty()) {
    return scope.Escape<v8::Value>(result);
  }

  // if all went well and the result wasn't undefined then print the returned
  // value
  if (printResult && !result->IsUndefined()) {
    v8::TryCatch tryCatch;

    v8::Handle<v8::String> printFuncName = TRI_V8_ASCII_STRING("print");
    v8::Handle<v8::Function> print =
        v8::Handle<v8::Function>::Cast(context->Global()->Get(printFuncName));

    if (print->IsFunction()) {
      v8::Handle<v8::Value> arguments[] = {result};
      print->Call(print, 1, arguments);

      if (tryCatch.HasCaught()) {
        if (tryCatch.CanContinue()) {
          TRI_LogV8Exception(isolate, &tryCatch);
        } else {
          TRI_GET_GLOBALS();
          v8g->_canceled = true;
          return scope.Escape<v8::Value>(v8::Undefined(isolate));
        }
      }
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "no output function defined in Javascript context";
    }
  }

  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error in a javascript object, based on error number only
////////////////////////////////////////////////////////////////////////////////

void TRI_CreateErrorObject(v8::Isolate* isolate, int errorNumber) {
  v8::HandleScope scope(isolate);
  CreateErrorObject(isolate, errorNumber, TRI_errno_string(errorNumber));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error in a javascript object, based on arangodb::result
////////////////////////////////////////////////////////////////////////////////
void TRI_CreateErrorObject(v8::Isolate* isolate, arangodb::Result const& res){
  TRI_CreateErrorObject(isolate, res.errorNumber(), res.errorMessage(), false);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error in a javascript object
////////////////////////////////////////////////////////////////////////////////

void TRI_CreateErrorObject(v8::Isolate* isolate, int errorNumber,
                           std::string const& message, bool autoPrepend) {
  v8::HandleScope scope(isolate);

  try {
    // does string concatenation, so we must wrap this in a try...catch block
    if (autoPrepend && message.empty()) {
        CreateErrorObject(
            isolate, errorNumber,
            message + ": " + std::string(TRI_errno_string(errorNumber)));
    } else {
      CreateErrorObject(isolate, errorNumber, message);
    }
  } catch (...) {
    // we cannot do anything about this here, but no C++ exception must
    // escape the C++ bindings called by V8
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes a v8 object
////////////////////////////////////////////////////////////////////////////////

void TRI_normalize_V8_Obj(v8::FunctionCallbackInfo<v8::Value> const& args,
                          v8::Handle<v8::Value> obj) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  v8::String::Value str(obj);
  size_t str_len = str.length();
  if (str_len > 0) {
    UErrorCode errorCode = U_ZERO_ERROR;
    Normalizer2 const* normalizer =
        Normalizer2::getInstance(nullptr, "nfc", UNORM2_COMPOSE, errorCode);

    if (U_FAILURE(errorCode)) {
      TRI_V8_RETURN(TRI_V8_PAIR_STRING((char*)*str, (int)str_len));
    }

    UnicodeString result = normalizer->normalize(
        UnicodeString((UChar*)*str, (int32_t)str_len), errorCode);

    if (U_FAILURE(errorCode)) {
      TRI_V8_RETURN(TRI_V8_STRING_UTF16(*str, (int)str_len));
    }

    // ..........................................................................
    // Take note here: we are assuming that the ICU type UChar is two bytes.
    // There is no guarantee that this will be the case on all platforms and
    // compilers. v8 expects uint16_t (2 bytes)
    // ..........................................................................

    TRI_V8_RETURN(TRI_V8_STRING_UTF16((uint16_t const*)result.getBuffer(),
                                      result.length()));
  }

  TRI_V8_RETURN(v8::String::Empty(isolate));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the path list
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Array> static V8PathList(v8::Isolate* isolate,
                                        std::string const& modules) {
  v8::EscapableHandleScope scope(isolate);

#ifdef _WIN32
  std::vector<std::string> paths = StringUtils::split(modules, ";", '\0');
#else
  std::vector<std::string> paths = StringUtils::split(modules, ";:");
#endif

  uint32_t const n = static_cast<uint32_t>(paths.size());
  v8::Handle<v8::Array> result = v8::Array::New(isolate, n);

  for (uint32_t i = 0; i < n; ++i) {
    result->Set(i, TRI_V8_STD_STRING(paths[i]));
  }

  return scope.Escape<v8::Array>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a single garbage collection run
////////////////////////////////////////////////////////////////////////////////

static bool SingleRunGarbageCollectionV8(v8::Isolate* isolate,
                                         int idleTimeInMs) {
  isolate->LowMemoryNotification();
  bool rc = isolate->IdleNotification(idleTimeInMs);
  isolate->RunMicrotasks();
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the V8 garbage collection for at most a specifiable amount of
/// time. returns a boolean indicating whether or not the caller should attempt
/// to do more gc
////////////////////////////////////////////////////////////////////////////////

bool TRI_RunGarbageCollectionV8(v8::Isolate* isolate, double availableTime) {
  int idleTimeInMs = 1000;

  if (availableTime >= 5.0) {
    idleTimeInMs = 5000;
  }
  if (availableTime >= 10.0) {
    idleTimeInMs = 10000;
  }

  try {
    TRI_ClearObjectCacheV8(isolate);

    double const until = TRI_microtime() + availableTime;

    int totalGcTimeInMs = static_cast<int>(availableTime) * 1000;
    int gcAttempts = totalGcTimeInMs / idleTimeInMs;

    if (gcAttempts <= 0) {
      // minimum is to run the GC once
      gcAttempts = 1;
    }

    int gcTries = 0;

    while (++gcTries <= gcAttempts) {
      if (SingleRunGarbageCollectionV8(isolate, idleTimeInMs)) {
        return false;
      }
    }

    size_t i = 0;
    while (TRI_microtime() < until) {
      if (++i % 1000 == 0) {
        // garbage collection only every x iterations, otherwise we'll use too
        // much CPU
        if (++gcTries > gcAttempts ||
            SingleRunGarbageCollectionV8(isolate, idleTimeInMs)) {
          return false;
        }
      }

      usleep(1000);
    }

    return true;
  } catch (...) {
    // error caught during garbage collection
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clear the instance-local cache of wrapped objects
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearObjectCacheV8(v8::Isolate* isolate) {
  auto globals = isolate->GetCurrentContext()->Global();

  if (globals->Has(TRI_V8_ASCII_STRING("__dbcache__"))) {
    v8::Handle<v8::Object> cacheObject =
        globals->Get(TRI_V8_ASCII_STRING("__dbcache__"))->ToObject();
    if (!cacheObject.IsEmpty()) {
      v8::Handle<v8::Array> props = cacheObject->GetPropertyNames();
      for (uint32_t i = 0; i < props->Length(); i++) {
        v8::Handle<v8::Value> key = props->Get(i);
        cacheObject->Delete(key);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 utils functions inside the global variable
////////////////////////////////////////////////////////////////////////////////

extern void TRI_InitV8Env(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                          std::string const& startupPath,
                          std::string const& modules);

void TRI_InitV8Utils(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                     std::string const& startupPath,
                     std::string const& modules) {
  v8::HandleScope scope(isolate);

  // check the isolate
  TRI_v8_global_t* v8g = TRI_GetV8Globals(isolate);
  TRI_ASSERT(v8g != nullptr);

  v8::Handle<v8::FunctionTemplate> ft;
  v8::Handle<v8::ObjectTemplate> rt;

  // .............................................................................
  // generate the general error template
  // .............................................................................

  ft = v8::FunctionTemplate::New(isolate, JS_ArangoError);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoError"));

  // ArangoError is a "sub-class" of Error
  v8::Handle<v8::Function> ArangoErrorFunc = ft->GetFunction();
  v8::Handle<v8::Value> ErrorObject =
      context->Global()->Get(TRI_V8_ASCII_STRING("Error"));
  v8::Handle<v8::Value> ErrorPrototype =
      ErrorObject->ToObject()->Get(TRI_V8_ASCII_STRING("prototype"));

  ArangoErrorFunc->Get(TRI_V8_ASCII_STRING("prototype"))
      ->ToObject()
      ->SetPrototype(ErrorPrototype);

  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("ArangoError"), ArangoErrorFunc);

  rt = ft->InstanceTemplate();
  v8g->ArangoErrorTempl.Reset(isolate, rt);

  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING("FS_CHMOD"), JS_ChMod);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("FS_EXISTS"), JS_Exists);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("FS_FILESIZE"), JS_SizeFile);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("FS_GET_TEMP_FILE"),
                               JS_GetTempFile);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("FS_GET_TEMP_PATH"),
                               JS_GetTempPath);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("FS_IS_DIRECTORY"), JS_IsDirectory);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("FS_IS_FILE"), JS_IsFile);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING("FS_LIST"),
                               JS_List);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("FS_LIST_TREE"), JS_ListTree);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING("FS_MAKE_ABSOLUTE"),
                               JS_MakeAbsolute);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING("FS_MAKE_DIRECTORY"),
                               JS_MakeDirectory);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("FS_MAKE_DIRECTORY_RECURSIVE"),
      JS_MakeDirectoryRecursive);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING("FS_MOVE"),
                               JS_MoveFile);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING("FS_COPY_RECURSIVE"),
                               JS_CopyRecursive);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("FS_COPY_FILE"), JS_CopyFile);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("FS_MTIME"), JS_MTime);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("FS_REMOVE"), JS_Remove);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("FS_REMOVE_DIRECTORY"),
                               JS_RemoveDirectory);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("FS_REMOVE_RECURSIVE_DIRECTORY"),
      JS_RemoveRecursiveDirectory);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("FS_UNZIP_FILE"), JS_UnzipFile);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("FS_ZIP_FILE"), JS_ZipFile);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("FS_ADLER32"), JS_Adler32);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_APPEND"), JS_Append);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_BASE64DECODE"),
                               JS_Base64Decode);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_BASE64ENCODE"),
                               JS_Base64Encode);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_CHECK_AND_MARK_NONCE"),
                               JS_MarkNonce);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_CREATE_NONCE"),
                               JS_CreateNonce);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("SYS_DOWNLOAD"), JS_Download);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_EXECUTE"), JS_Execute);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_EXECUTE_EXTERNAL"),
                               JS_ExecuteExternal);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("SYS_EXECUTE_EXTERNAL_AND_WAIT"),
      JS_ExecuteAndWaitExternal);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("SYS_GEN_RANDOM_ALPHA_NUMBERS"),
      JS_RandomAlphaNum);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_GEN_RANDOM_NUMBERS"),
                               JS_RandomNumbers);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING("SYS_GEN_RANDOM_SALT"),
                               JS_RandomSalt);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_GETLINE"), JS_Getline);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_HMAC"), JS_HMAC);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_IS_IP"), JS_IsIP);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_SPLIT_WORDS_ICU"),
                               JS_SplitWordlist);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_KILL_EXTERNAL"),
                               JS_KillExternal);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_SUSPEND_EXTERNAL"),
                               JS_SuspendExternal);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_CONTINUE_EXTERNAL"),
                               JS_ContinueExternal);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_LOAD"), JS_Load);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING("SYS_LOG"),
                               JS_Log);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("SYS_LOG_LEVEL"), JS_LogLevel);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING("SYS_MD5"),
                               JS_Md5);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING("SYS_OPTIONS"), JS_Options);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING("SYS_OUTPUT"), JS_Output);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING("SYS_PARSE"), JS_Parse);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("SYS_PARSE_FILE"), JS_ParseFile);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("SYS_PBKDF2HS1"), JS_PBKDF2HS1);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_PBKDF2"), JS_PBKDF2);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_PROCESS_STATISTICS"),
                               JS_ProcessStatistics);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING("SYS_RAND"), JS_Rand);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_READ"), JS_Read);
  TRI_AddGlobalFunctionVocbase(isolate, 
                               TRI_V8_ASCII_STRING("SYS_READ64"), JS_Read64);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("SYS_READ_BUFFER"), JS_ReadBuffer);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_SHA1"), JS_Sha1);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_SHA224"), JS_Sha224);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_SHA256"), JS_Sha256);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_SHA384"), JS_Sha384);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_SHA512"), JS_Sha512);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_SLEEP"), JS_Sleep);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_SPRINTF"), JS_SPrintF);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_STATUS_EXTERNAL"),
                               JS_StatusExternal);
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("SYS_TEST_PORT"), JS_TestPort);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_TIME"), JS_Time);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_WAIT"), JS_Wait);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_WRITE"), JS_Write);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_DEBUG_CAN_USE_FAILAT"),
                               JS_DebugCanUseFailAt);

  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("SYS_IS_STOPPING"), JS_IsStopping);

  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("V8_TO_VPACK"), JS_V8ToVPack);

  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING("VPACK_TO_V8"), JS_VPackToV8);

  // .............................................................................
  // create the global variables
  // .............................................................................

  TRI_AddGlobalVariableVocbase(isolate, TRI_V8_ASCII_STRING("HOME"),
                               TRI_V8_STD_STRING(FileUtils::homeDirectory()));

  TRI_AddGlobalVariableVocbase(isolate, 
                               TRI_V8_ASCII_STRING("MODULES_PATH"),
                               V8PathList(isolate, modules));

  TRI_AddGlobalVariableVocbase(isolate, 
                               TRI_V8_ASCII_STRING("STARTUP_PATH"),
                               TRI_V8_STD_STRING(startupPath));

  TRI_AddGlobalVariableVocbase(isolate,
                               TRI_V8_ASCII_STRING("PATH_SEPARATOR"),
                               TRI_V8_ASCII_STRING(TRI_DIR_SEPARATOR_STR));

  TRI_AddGlobalVariableVocbase(
      isolate, TRI_V8_ASCII_STRING("VALGRIND"),
      v8::Boolean::New(isolate, (RUNNING_ON_VALGRIND > 0)));

#ifdef COVERAGE
  TRI_AddGlobalVariableVocbase(
      isolate, TRI_V8_ASCII_STRING("COVERAGE"), v8::True(isolate));
#else
  TRI_AddGlobalVariableVocbase(
      isolate, TRI_V8_ASCII_STRING("COVERAGE"), v8::False(isolate));
#endif

  TRI_AddGlobalVariableVocbase(isolate, TRI_V8_ASCII_STRING("VERSION"),
                               TRI_V8_ASCII_STRING(ARANGODB_VERSION));

  TRI_AddGlobalVariableVocbase(isolate, 
                               TRI_V8_ASCII_STRING("SYS_PLATFORM"),
                               TRI_V8_ASCII_STRING(TRI_PLATFORM));

  TRI_InitV8Env(isolate, context, startupPath, modules);
}
