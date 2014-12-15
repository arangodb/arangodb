////////////////////////////////////////////////////////////////////////////////
/// @brief V8 action functions
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

#include "v8-actions.h"
#include "Actions/actions.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/json.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/ApplicationV8.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ServerState.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static TRI_action_result_t ExecuteActionVocbase (TRI_vocbase_t* vocbase,
                                                 v8::Isolate* isolate,
                                                 TRI_action_t const* action,
                                                 v8::Handle<v8::Function> callback,
                                                 HttpRequest* request);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief global V8 dealer
////////////////////////////////////////////////////////////////////////////////

static ApplicationV8* GlobalV8Dealer = nullptr;

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 class v8_action_t
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief action description for V8
////////////////////////////////////////////////////////////////////////////////

class v8_action_t : public TRI_action_t {
  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

    v8_action_t ()
      : TRI_action_t(),
        _callbacks(),
        _callbacksLock() {
      _type = "JAVASCRIPT";
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates callback for a context
////////////////////////////////////////////////////////////////////////////////

    void createCallback (v8::Isolate* isolate,
                         v8::Handle<v8::Function> callback) {
      WRITE_LOCKER(_callbacksLock);

      map< v8::Isolate*, v8::Persistent<v8::Function> >::iterator i = _callbacks.find(isolate);

      if (i != _callbacks.end()) {
        i->second.Reset();
      }

      _callbacks[isolate].Reset(isolate, callback);
    }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

    TRI_action_result_t execute (TRI_vocbase_t* vocbase,
                                 HttpRequest* request,
                                 Mutex* dataLock,
                                 void** data) {
      TRI_action_result_t result;

      // determine whether we should force a re-initialistion of the engine in development mode
      bool allowEngineReset = false;

      // allow use datase execution in rest calls
      extern bool ALLOW_USE_DATABASE_IN_REST_ACTIONS;
      bool allowUseDatabaseInRestActions = ALLOW_USE_DATABASE_IN_REST_ACTIONS;

      if (_allowUseDatabase) {
        allowUseDatabaseInRestActions = true;
      }

      // only URLs starting with /dev will trigger an engine reset
      string const& fullUrl = request->fullUrl();

      if (fullUrl.find("/dev/") == 0) {
        allowEngineReset = true;
      }

      ApplicationV8::V8Context* context = GlobalV8Dealer->enterContext(
        "STANDARD",
        vocbase,
        ! allowEngineReset,
        allowUseDatabaseInRestActions);

      // note: the context might be 0 in case of shut-down
      if (context == nullptr) {
        return result;
      }

      // locate the callback
      READ_LOCKER(_callbacksLock);

      {
        map< v8::Isolate*, v8::Persistent<v8::Function> >::iterator i = _callbacks.find(context->isolate);

        if (i == _callbacks.end()) {
          LOG_WARNING("no callback function for JavaScript action '%s'", _url.c_str());

          GlobalV8Dealer->exitContext(context);

          result.isValid = true;
          result.response = new HttpResponse(HttpResponse::NOT_FOUND, request->compatibility());

          return result;
        }

        // and execute it
        {
          MUTEX_LOCKER(*dataLock);

          if (*data != 0) {
            result.canceled = true;

            GlobalV8Dealer->exitContext(context);

            return result;
          }

          *data = (void*) context->isolate;
        }
        v8::HandleScope scope(context->isolate);
        auto localFunction = v8::Local<v8::Function>::New(context->isolate, i->second);
        result = ExecuteActionVocbase(vocbase, context->isolate, this, localFunction, request);

        {
          MUTEX_LOCKER(*dataLock);
          *data = 0;
        }
      }
      GlobalV8Dealer->exitContext(context);

      return result;
    }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

    bool cancel (Mutex* dataLock, void** data) {
      {
        MUTEX_LOCKER(*dataLock);

        // either we have not yet reached the execute above or we are already done
        if (*data == 0) {
          *data = (void*) 1; // mark as canceled
        }

        // data is set, cancel the execution
        else {
          if (! v8::V8::IsExecutionTerminating((v8::Isolate*) *data)) {
            v8::V8::TerminateExecution((v8::Isolate*) *data);
          }
        }
      }

      return true;
    }

  private:

////////////////////////////////////////////////////////////////////////////////
/// @brief callback dictionary
////////////////////////////////////////////////////////////////////////////////

    map< v8::Isolate*, v8::Persistent<v8::Function> > _callbacks;

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for the callback dictionary
////////////////////////////////////////////////////////////////////////////////

    ReadWriteLock _callbacksLock;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the action options
////////////////////////////////////////////////////////////////////////////////

static void ParseActionOptions (v8::Isolate* isolate,
                                TRI_v8_global_t* v8g,
                                TRI_action_t* action,
                                v8::Handle<v8::Object> options) {
  
  TRI_GET_GLOBAL_STRING(PrefixKey);
  // check the "prefix" field
  if (options->Has(PrefixKey)) {
    action->_isPrefix = TRI_ObjectToBoolean(options->Get(PrefixKey));
  }
  else {
    action->_isPrefix = false;
  }

  // check the "allowUseDatabase" field
  TRI_GET_GLOBAL_STRING(AllowUseDatabaseKey);
  if (options->Has(AllowUseDatabaseKey)) {
    action->_allowUseDatabase = TRI_ObjectToBoolean(options->Get(AllowUseDatabaseKey));
  }
  else {
    action->_allowUseDatabase = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add cookie
////////////////////////////////////////////////////////////////////////////////

static void AddCookie (v8::Isolate* isolate,
                       TRI_v8_global_t const* v8g,
                       HttpResponse* response,
                       v8::Handle<v8::Object> data) {

  string name;
  string value;
  int lifeTimeSeconds = 0;
  string path = "/";
  string domain = "";
  bool secure = false;
  bool httpOnly = false;

  TRI_GET_GLOBAL_STRING(NameKey);
  if (data->Has(NameKey)) {
    v8::Handle<v8::Value> v = data->Get(NameKey);
    name = TRI_ObjectToString(v);
  }
  else {
    // something is wrong here
    return;
  }
  TRI_GET_GLOBAL_STRING(ValueKey);
  if (data->Has(ValueKey)) {
    v8::Handle<v8::Value> v = data->Get(ValueKey);
    value = TRI_ObjectToString(v);
  }
  else {
    // something is wrong here
    return;
  }
  TRI_GET_GLOBAL_STRING(LifeTimeKey);
  if (data->Has(LifeTimeKey)) {
    v8::Handle<v8::Value> v = data->Get(LifeTimeKey);
    lifeTimeSeconds = (int) TRI_ObjectToInt64(v);
  }
  TRI_GET_GLOBAL_STRING(PathKey);
  if (data->Has(PathKey) && ! data->Get(PathKey)->IsUndefined()) {
    v8::Handle<v8::Value> v = data->Get(PathKey);
    path = TRI_ObjectToString(v);
  }
  TRI_GET_GLOBAL_STRING(DomainKey);
  if (data->Has(DomainKey) && ! data->Get(DomainKey)->IsUndefined()) {
    v8::Handle<v8::Value> v = data->Get(DomainKey);
    domain = TRI_ObjectToString(v);
  }
  TRI_GET_GLOBAL_STRING(SecureKey);
  if (data->Has(SecureKey)) {
    v8::Handle<v8::Value> v = data->Get(SecureKey);
    secure = TRI_ObjectToBoolean(v);
  }
  TRI_GET_GLOBAL_STRING(HttpOnlyKey);
  if (data->Has(HttpOnlyKey)) {
    v8::Handle<v8::Value> v = data->Get(HttpOnlyKey);
    httpOnly = TRI_ObjectToBoolean(v);
  }

  response->setCookie(name, value, lifeTimeSeconds, path, domain, secure, httpOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a C++ HttpRequest to a V8 request object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> RequestCppToV8 (v8::Isolate* isolate,
                                              TRI_v8_global_t const* v8g,
                                              HttpRequest* request) {
  // setup the request
  v8::Handle<v8::Object> req = v8::Object::New(isolate);

  // Example:
  //      {
  //        path : "/full/path/suffix1/suffix2",
  //
  //        prefix : "/full/path",
  //
  //        "suffix" : [
  //          "suffix1",
  //          "suffix2"
  //        ],
  //
  //        "parameters" : {
  //          "init" : "true"
  //        },
  //
  //        "headers" : {
  //          "accept" : "text/html",
  //          "accept-encoding" : "gzip, deflate",
  //          "accept-language" : "de-de,en-us;q=0.7,en;q=0.3",
  //          "user-agent" : "Mozilla/5.0"
  //        },
  //
  //        "cookies" : {
  //          "ARANGODB_SESSION_ID" : "0cwuzusd23nw3qiwui84uwqwqw23e"
  //        },
  //
  //        "requestType" : "GET",
  //        "requestBody" : "... only for PUT and POST ...",
  //        "user" : "authenticatedUser"
  //      }

  // create user or null
  string const& user = request->user();

  TRI_GET_GLOBAL_STRING(UserKey);
  if (user.empty()) {
    req->Set(UserKey, v8::Null(isolate));
  }
  else {
    req->Set(UserKey, TRI_V8_STD_STRING(user));
  }

  // create database attribute
  string const& database = request->databaseName();
  TRI_ASSERT(! database.empty());

  TRI_GET_GLOBAL_STRING(DatabaseKey);
  req->Set(DatabaseKey, TRI_V8_STD_STRING(database));

  // set the full url
  string const& fullUrl = request->fullUrl();
  TRI_GET_GLOBAL_STRING(UrlKey);
  req->Set(UrlKey, TRI_V8_STD_STRING(fullUrl));

  // set the protocol
  string const& protocol = request->protocol();
  TRI_GET_GLOBAL_STRING(ProtocolKey);
  req->Set(ProtocolKey, TRI_V8_STD_STRING(protocol));

  // set the connection info
  const ConnectionInfo& info = request->connectionInfo();

  v8::Handle<v8::Object> serverArray = v8::Object::New(isolate);
  TRI_GET_GLOBAL_STRING(AddressKey);
  serverArray->Set(AddressKey, TRI_V8_STD_STRING(info.serverAddress));
  TRI_GET_GLOBAL_STRING(PortKey);
  serverArray->Set(PortKey, v8::Number::New(isolate, info.serverPort));
  TRI_GET_GLOBAL_STRING(ServerKey);
  req->Set(ServerKey, serverArray);
  
  TRI_GET_GLOBAL_STRING(PortTypeKey);
  req->ForceSet(PortTypeKey, TRI_V8_STD_STRING(info.portType()), static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum));

  v8::Handle<v8::Object> clientArray = v8::Object::New(isolate);
  clientArray->Set(AddressKey, TRI_V8_STD_STRING(info.clientAddress));
  clientArray->Set(PortKey, v8::Number::New(isolate, info.clientPort));
  TRI_GET_GLOBAL_STRING(ClientKey);
  req->Set(ClientKey, clientArray);

  req->Set(TRI_V8_ASCII_STRING("internals"), v8::External::New(isolate, request));

  // copy prefix
  string path = request->prefix();
  TRI_GET_GLOBAL_STRING(PrefixKey);
  req->Set(PrefixKey, TRI_V8_STD_STRING(path));

  // copy header fields
  v8::Handle<v8::Object> headerFields = v8::Object::New(isolate);

  map<string, string> const& headers = request->headers();
  map<string, string>::const_iterator iter = headers.begin();

  for (; iter != headers.end(); ++iter) {
    headerFields->Set(TRI_V8_STD_STRING(iter->first),
                      TRI_V8_STD_STRING(iter->second));
  }

  TRI_GET_GLOBAL_STRING(HeadersKey);
  req->Set(HeadersKey, headerFields);
  TRI_GET_GLOBAL_STRING(RequestTypeKey);
  TRI_GET_GLOBAL_STRING(RequestBodyKey);

  // copy request type
  switch (request->requestType()) {
  case HttpRequest::HTTP_REQUEST_POST: {
      TRI_GET_GLOBAL_STRING(PostConstant);
      req->Set(RequestTypeKey, PostConstant);
      req->Set(RequestBodyKey, TRI_V8_PAIR_STRING(request->body(),
                                                  (int) request->bodySize()));
      break;
  }

  case HttpRequest::HTTP_REQUEST_PUT: {
      TRI_GET_GLOBAL_STRING(PutConstant);
      req->Set(RequestTypeKey, PutConstant);
      req->Set(RequestBodyKey, TRI_V8_PAIR_STRING(request->body(),
                                                  (int) request->bodySize()));
      break;
  }

  case HttpRequest::HTTP_REQUEST_PATCH: {
      TRI_GET_GLOBAL_STRING(PatchConstant);
      req->Set(RequestTypeKey, PatchConstant);
      req->Set(RequestBodyKey, TRI_V8_PAIR_STRING(request->body(),
                                                  (int) request->bodySize()));
      break;
  }
  case HttpRequest::HTTP_REQUEST_OPTIONS: {
      TRI_GET_GLOBAL_STRING(OptionsConstant);
      req->Set(RequestTypeKey, OptionsConstant);
      break;
  }
  case HttpRequest::HTTP_REQUEST_DELETE: {
      TRI_GET_GLOBAL_STRING(DeleteConstant);
      req->Set(RequestTypeKey, DeleteConstant);
      break;
  }
  case HttpRequest::HTTP_REQUEST_HEAD: {
      TRI_GET_GLOBAL_STRING(HeadConstant);
      req->Set(RequestTypeKey, HeadConstant);
      break;
  }
  case HttpRequest::HTTP_REQUEST_GET: {
    default:
      TRI_GET_GLOBAL_STRING(GetConstant);
      req->Set(RequestTypeKey, GetConstant);
      break;
  }
  }

  // copy request parameter
  v8::Handle<v8::Object> valuesObject = v8::Object::New(isolate);
  map<string, string> values = request->values();

  for (map<string, string>::iterator i = values.begin();
       i != values.end();
       ++i) {
    valuesObject->Set(TRI_V8_STD_STRING(i->first), TRI_V8_STD_STRING(i->second));
  }

  // copy request array parameter (a[]=1&a[]=2&...)
  map<string, vector<char const*>* > arrayValues = request->arrayValues();

  for (map<string, vector<char const*>* >::iterator i = arrayValues.begin();
       i != arrayValues.end();  ++i) {
    string const& k = i->first;
    vector<char const*>* v = i->second;

    v8::Handle<v8::Array> list = v8::Array::New(isolate);

    for (size_t i = 0; i < v->size(); ++i) {
      list->Set((uint32_t) i, TRI_V8_ASCII_STRING(v->at(i)));
    }

    valuesObject->Set(TRI_V8_STD_STRING(k), list);
  }

  TRI_GET_GLOBAL_STRING(ParametersKey);
  req->Set(ParametersKey, valuesObject);

  // copy cookies
  v8::Handle<v8::Object> cookiesObject = v8::Object::New(isolate);

  map<string, string> const& cookies = request->cookieValues();
  iter = cookies.begin();

  for (; iter != cookies.end(); ++iter) {
    cookiesObject->Set(TRI_V8_STD_STRING(iter->first),
                       TRI_V8_STD_STRING(iter->second));
  }

  TRI_GET_GLOBAL_STRING(CookiesKey);
  req->Set(CookiesKey, cookiesObject);

  // determine API compatibility version
  int32_t compatibility = request->compatibility();
  TRI_GET_GLOBAL_STRING(CompatibilityKey);
  req->Set(CompatibilityKey, v8::Integer::New(isolate, compatibility));

  return req;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a C++ HttpRequest to a V8 request object
////////////////////////////////////////////////////////////////////////////////

static HttpResponse* ResponseV8ToCpp (v8::Isolate* isolate,
                                      TRI_v8_global_t const* v8g,
                                      v8::Handle<v8::Object> const res,
                                      uint32_t compatibility) {
  HttpResponse::HttpResponseCode code = HttpResponse::OK;

  TRI_GET_GLOBAL_STRING(ResponseCodeKey);
  if (res->Has(ResponseCodeKey)) {
    // Windows has issues with converting from a double to an enumeration type
    code = (HttpResponse::HttpResponseCode)
           ((int) (TRI_ObjectToDouble(res->Get(ResponseCodeKey))));
  }

  HttpResponse* response = new HttpResponse(code, compatibility);

  TRI_GET_GLOBAL_STRING(ContentTypeKey);
  if (res->Has(ContentTypeKey)) {
    response->setContentType(TRI_ObjectToString(res->Get(ContentTypeKey)));
  }

  // .........................................................................
  // body
  // .........................................................................

  TRI_GET_GLOBAL_STRING(BodyKey);
  TRI_GET_GLOBAL_STRING(BodyFromFileKey);
  TRI_GET_GLOBAL_STRING(HeadersKey);
  TRI_GET_GLOBAL_STRING(CookiesKey);
  if (res->Has(BodyKey)) {
    // check if we should apply result transformations
    // transformations turn the result from one type into another
    // a Javascript action can request transformations by
    // putting a list of transformations into the res.transformations
    // array, e.g. res.transformations = [ "base64encode" ]
    TRI_GET_GLOBAL_STRING(TransformationsKey);
    v8::Handle<v8::Value> val = res->Get(TransformationsKey);

    if (val->IsArray()) {
      TRI_GET_GLOBAL_STRING(BodyKey);
      string out(TRI_ObjectToString(res->Get(BodyKey)));
      v8::Handle<v8::Array> transformations = val.As<v8::Array>();

      for (uint32_t i = 0; i < transformations->Length(); i++) {
        v8::Handle<v8::Value> transformator
          = transformations->Get(v8::Integer::New(isolate, i));
        string name = TRI_ObjectToString(transformator);

        // check available transformations
        if (name == "base64encode") {
          // base64-encode the result
          out = StringUtils::encodeBase64(out);
          // set the correct content-encoding header
          response->setHeader("content-encoding", "base64");
        }
        else if (name == "base64decode") {
          // base64-decode the result
          out = StringUtils::decodeBase64(out);
          // set the correct content-encoding header
          response->setHeader("content-encoding", "binary");
        }
      }

      response->body().appendText(out);
    }
    else {
      TRI_GET_GLOBAL_STRING(BodyKey);
      v8::Handle<v8::Value> b = res->Get(BodyKey);
      if (V8Buffer::hasInstance(isolate, b)) {
        // body is a Buffer
        auto obj = b.As<v8::Object>();
        response->body().appendText(V8Buffer::data(obj), V8Buffer::length(obj));
      }
      else {
        // treat body as a string
        response->body().appendText(TRI_ObjectToString(res->Get(BodyKey)));
      }
    }
  }

  // .........................................................................
  // body from file
  // .........................................................................

  else if (res->Has(BodyFromFileKey)) {
    TRI_Utf8ValueNFC filename(TRI_UNKNOWN_MEM_ZONE,
                              res->Get(BodyFromFileKey));
    size_t length;
    char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *filename, &length);

    if (content != nullptr) {
      response->body().appendText(content, length);
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);
    }
    else {
      string msg = string("cannot read file '") + *filename + "': " +
                   TRI_last_error();

      response->body().appendText(msg.c_str(), msg.size());
      response->setResponseCode(HttpResponse::SERVER_ERROR);
    }
  }

  // .........................................................................
  // headers
  // .........................................................................

  if (res->Has(HeadersKey)) {
    v8::Handle<v8::Value> val = res->Get(HeadersKey);
    v8::Handle<v8::Object> v8Headers = val.As<v8::Object>();

    if (v8Headers->IsObject()) {
      v8::Handle<v8::Array> props = v8Headers->GetPropertyNames();

      for (uint32_t i = 0; i < props->Length(); i++) {
        v8::Handle<v8::Value> key = props->Get(v8::Integer::New(isolate, i));
        response->setHeader(TRI_ObjectToString(key),
                            TRI_ObjectToString(v8Headers->Get(key)));
      }
    }
  }

  // .........................................................................
  // cookies
  // .........................................................................

  if (res->Has(CookiesKey)) {
    v8::Handle<v8::Value> val = res->Get(CookiesKey);
    v8::Handle<v8::Object> v8Cookies = val.As<v8::Object>();

    if (v8Cookies->IsArray()) {
      v8::Handle<v8::Array> v8Array = v8Cookies.As<v8::Array>();

      for (uint32_t i = 0; i < v8Array->Length(); i++) {
        v8::Handle<v8::Value> v8Cookie = v8Array->Get(i);
        if (v8Cookie->IsObject()) {
          AddCookie(isolate, v8g, response, v8Cookie.As<v8::Object>());
        }
      }
    }
    else if (v8Cookies->IsObject()) {
      // one cookie
      AddCookie(isolate, v8g, response, v8Cookies);
    }
  }

  return response;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

static TRI_action_result_t ExecuteActionVocbase (TRI_vocbase_t* vocbase,
                                                 v8::Isolate* isolate,
                                                 TRI_action_t const* action,
                                                 v8::Handle<v8::Function> callback,
                                                 HttpRequest* request) {
  TRI_action_result_t result;

  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  v8::Handle<v8::Object> req = RequestCppToV8(isolate, v8g, request);

  // copy suffix, which comes from the action:
  string path = request->prefix();
  v8::Handle<v8::Array> suffixArray = v8::Array::New(isolate);
  vector<string> const& suffix = request->suffix();

  uint32_t index = 0;
  char const* sep = "";

  for (size_t s = action->_urlParts;  s < suffix.size();  ++s) {
    suffixArray->Set(index++, TRI_V8_STD_STRING(suffix[s]));

    path += sep + suffix[s];
    sep = "/";
  }

  TRI_GET_GLOBAL_STRING(SuffixKey);
  req->Set(SuffixKey, suffixArray);

  // copy full path
  TRI_GET_GLOBAL_STRING(PathKey);
  req->Set(PathKey, TRI_V8_STD_STRING(path));

  // create the response object
  v8::Handle<v8::Object> res = v8::Object::New(isolate);

  // register request & response in the context
  v8g->_currentRequest  = req;
  v8g->_currentResponse = res;

  // execute the callback
  v8::Handle<v8::Value> args[2] = { req, res };

  callback->Call(callback, 2, args);

  // invalidate request / response objects
  v8g->_currentRequest  = v8::Undefined(isolate);
  v8g->_currentResponse = v8::Undefined(isolate);

  // convert the result
  result.isValid = true;

  if (v8g->_canceled) {
    result.isValid = false;
    result.canceled = true;
  }

  else if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      v8::Handle<v8::Value> exception = tryCatch.Exception();
      TRI_GET_GLOBAL(SleepAndRequeueFuncTempl, v8::FunctionTemplate);
      bool isSleepAndRequeue = SleepAndRequeueFuncTempl->HasInstance(exception);

      if (isSleepAndRequeue) {
        result.requeue = true;
        TRI_GET_GLOBAL_STRING(SleepKey);
        result.sleep = TRI_ObjectToDouble(exception->ToObject()->Get(SleepKey));
      }
      else {
        string msg = TRI_StringifyV8Exception(isolate, &tryCatch);

        HttpResponse* response = new HttpResponse(HttpResponse::SERVER_ERROR, request->compatibility());
        response->body().appendText(msg);

        result.response = response;
      }
    }
    else {
      v8g->_canceled = true;
      result.isValid = false;
      result.canceled = true;
    }
  }

  else {
    result.response = ResponseV8ToCpp(isolate, v8g, res, request->compatibility());
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new action
///
/// @FUN{internal.defineAction(@FA{name}, @FA{callback}, @FA{parameter})}
////////////////////////////////////////////////////////////////////////////////

static void JS_DefineAction (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();

  if (args.Length() != 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("defineAction(<name>, <callback>, <parameter>)");
  }

  // extract the action name
  TRI_Utf8ValueNFC utf8name(TRI_UNKNOWN_MEM_ZONE, args[0]);

  if (*utf8name == 0) {
    TRI_V8_THROW_TYPE_ERROR("<name> must be an UTF-8 string");
  }

  string name = *utf8name;

  // extract the action callback
  if (! args[1]->IsFunction()) {
    TRI_V8_THROW_TYPE_ERROR("<callback> must be a function");
  }

  v8::Handle<v8::Function> callback = v8::Handle<v8::Function>::Cast(args[1]);

  // extract the options
  v8::Handle<v8::Object> options;

  if (args[2]->IsObject()) {
    options = args[2]->ToObject();
  }
  else {
    options = v8::Object::New(isolate);
  }

  // create an action with the given options
  v8_action_t* action = new v8_action_t();
  ParseActionOptions(isolate, v8g, action, options);

  // store an action with the given name
  TRI_action_t* result = TRI_DefineActionVocBase(name, action);

  // and define the callback
  if (result != nullptr) {
    action = dynamic_cast<v8_action_t*>(result);

    if (action != nullptr) {
      action->createCallback(isolate, callback);
    }
    else {
      LOG_ERROR("cannot create callback for V8 action");
    }
  }
  else {
    LOG_ERROR("cannot define V8 action");
  }

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief eventually executes a function in all contexts
///
/// @FUN{internal.executeGlobalContextFunction(@FA{function-definition})}
////////////////////////////////////////////////////////////////////////////////

static void JS_ExecuteGlobalContextFunction (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("executeGlobalContextFunction(<function-type>)");
  }

  // extract the action name
  v8::String::Utf8Value utf8def(args[0]);

  if (*utf8def == 0) {
    TRI_V8_THROW_TYPE_ERROR("<definition> must be a UTF-8 function definition");
  }

  string const def = *utf8def;

  // and pass it to the V8 contexts
  if (! GlobalV8Dealer->addGlobalContextMethod(def)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid action definition");
  }

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current request
///
/// @FUN{internal.getCurrentRequest()}
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCurrentRequest (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getCurrentRequest()");
  }

  TRI_V8_RETURN(v8g->_currentRequest);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the raw body of the current request
///
/// @FUN{internal.rawRequestBody()}
////////////////////////////////////////////////////////////////////////////////

static void JS_RawRequestBody (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("rawRequestBody(req)");
  }

  v8::Handle<v8::Value> current = args[0];
  if (current->IsObject()) {
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(current);
    v8::Handle<v8::Value> property = obj->Get(TRI_V8_ASCII_STRING("internals"));
    if (property->IsExternal()) {
      v8::Handle<v8::External> e = v8::Handle<v8::External>::Cast(property);
      auto request = static_cast<triagens::rest::HttpRequest*>(e->Value());

      if (request != nullptr) {
        V8Buffer* buffer = V8Buffer::New(isolate, request->body(), request->bodySize());

        TRI_V8_RETURN(buffer->_handle);
      }
    }
  }

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the raw body of the current request
///
/// @FUN{internal.rawRequestBody()}
////////////////////////////////////////////////////////////////////////////////

static void JS_RequestParts (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("requestParts(req)");
  }

  v8::Handle<v8::Value> current = args[0];
  if (current->IsObject()) {
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(current);
    v8::Handle<v8::Value> property = obj->Get(TRI_V8_ASCII_STRING("internals"));
    if (property->IsExternal()) {
      v8::Handle<v8::External> e = v8::Handle<v8::External>::Cast(property);
      auto request = static_cast<triagens::rest::HttpRequest*>(e->Value());

      char const* beg = request->body();
      char const* end = beg + request->bodySize();

      while (beg < end && (*beg == '\r' || *beg == '\n' || *beg == ' ')) {
        ++beg;
      }

      // find delimiter
      char const* ptr = beg;
      while (ptr < end && *ptr == '-') {
        ++ptr;
      }

      while (ptr < end && *ptr != '\r' && *ptr != '\n') {
        ++ptr;
      }
      if (ptr == beg) {
        // oops
        TRI_V8_THROW_EXCEPTION_PARAMETER("request is no multipart request"); 
      }

      std::string const delimiter(beg, ptr - beg);
      if (ptr < end && *ptr == '\r') {
        ++ptr;
      }
      if (ptr < end && *ptr == '\n') {
        ++ptr;
      }

      std::vector<std::pair<char const*, size_t>> parts;

      while (ptr < end) {
        char const* p = TRI_IsContainedMemory(ptr, end - ptr, delimiter.c_str(), delimiter.size());
        if (p == nullptr || p + delimiter.size() + 2 >= end || p - 2 <= ptr) {
          TRI_V8_THROW_EXCEPTION_PARAMETER("bad request data"); 
        }

        char const* q = p;
        if (*(q - 1) == '\n') {
          --q;
        }
        if (*(q - 1) == '\r') {
          --q;
        }
       
        parts.push_back(std::make_pair(ptr, q - ptr));
        ptr = p + delimiter.size();
        if (*ptr == '-' && *(ptr + 1) == '-') {
          // eom
          break;
        }
        if (*ptr == '\r') {
          ++ptr;
        }
        if (ptr < end && *ptr == '\n') {
          ++ptr;
        }
      }

      v8::Handle<v8::Array> result = v8::Array::New(isolate);

      uint32_t j = 0;
      for (auto& part : parts) {
        v8::Handle<v8::Object> headersObject = v8::Object::New(isolate);
     
        auto ptr = part.first;
        auto end = part.first + part.second;
        char const* data = nullptr;

        while (ptr < end) {
          while (ptr < end && *ptr == ' ') {
            ++ptr;
          }
          if (ptr < end && (*ptr == '\r' || *ptr == '\n')) {
            // end of headers
            if (*ptr == '\r') {
              ++ptr;
            }
            if (ptr < end && *ptr == '\n') {
              ++ptr;
            }
            data = ptr;
            break;
          }

          // header line
          char const* eol = TRI_IsContainedMemory(ptr, end - ptr, "\r\n", 2);
          if (eol == nullptr) {
            eol = TRI_IsContainedMemory(ptr, end - ptr, "\n", 1);
          }
          if (eol == nullptr) {
            TRI_V8_THROW_EXCEPTION_PARAMETER("bad request data"); 
          }
          char const* colon = TRI_IsContainedMemory(ptr, end - ptr, ":", 1);
          if (colon == nullptr) {
            TRI_V8_THROW_EXCEPTION_PARAMETER("bad request data"); 
          }
          char const* p = colon; 
          while (p > ptr && *(p - 1) == ' ') {
            --p; 
          }
          ++colon;
          while (colon < eol && *colon == ' ') {
            ++colon;     
          }
          char const* q = eol;
          while (q > ptr && *(q - 1) == ' ') {
            --q; 
          }

          headersObject->Set(TRI_V8_PAIR_STRING(ptr, (int) (p - ptr)), TRI_V8_PAIR_STRING(colon, (int) (eol - colon)));

          ptr = eol;
          if (*ptr == '\r') {
            ++ptr;
          }
          if (ptr < end && *ptr == '\n') {
            ++ptr;
          }
        }

        if (data == nullptr) {
          TRI_V8_THROW_EXCEPTION_PARAMETER("bad request data"); 
        }

        v8::Handle<v8::Object> partObject = v8::Object::New(isolate);
        partObject->Set(TRI_V8_ASCII_STRING("headers"), headersObject);
 
        V8Buffer* buffer = V8Buffer::New(isolate, data, end - data);
        auto localHandle = v8::Local<v8::Object>::New(isolate, buffer->_handle);

        partObject->Set(TRI_V8_ASCII_STRING("data"), localHandle); 
        
        result->Set(j++, partObject);
      }

      TRI_V8_RETURN(result);
    }
  }

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current response
///
/// @FUN{internal.getCurrentRequest()}
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCurrentResponse (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getCurrentResponse()");
  }

  TRI_V8_RETURN(v8g->_currentResponse);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function to test sharding
///
/// @FUN{internal.defineAction(@FA{name}, @FA{callback}, @FA{parameter})}
////////////////////////////////////////////////////////////////////////////////

class CallbackTest : public ClusterCommCallback {
    string _msg;

  public:
    CallbackTest(string msg) : _msg(msg) {}
    virtual ~CallbackTest() {}
    virtual bool operator() (ClusterCommResult* res) {
      LOG_DEBUG("ClusterCommCallback called on operation %llu",
                (unsigned long long) res->operationID);
      LOG_DEBUG("Message: %s", _msg.c_str());
      return false;  // Keep it in the queue
    }
};

static void JS_ClusterTest (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();

  if (args.Length() != 9) {
    TRI_V8_THROW_EXCEPTION_USAGE(
      "SYS_CLUSTER_TEST(<req>, <res>, <dest>, <path>, <clientTransactionID>, "
      "<headers>, <body>, <timeout>, <asyncMode>)");
  }

  if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("request works only in coordinator role");
  }

  ClusterComm* cc = ClusterComm::instance();

  if (cc == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                             "clustercomm object not found");
  }

  // Now get the arguments to form our request:
  triagens::rest::HttpRequest::HttpRequestType reqType
    = triagens::rest::HttpRequest::HTTP_REQUEST_GET;
  if (args[0]->IsObject()) {
    v8::Handle<v8::Object> obj = args[0].As<v8::Object>();
    v8::Handle<v8::Value> meth = obj->Get(TRI_V8_ASCII_STRING("requestType"));
    if (meth->IsString()) {
      TRI_Utf8ValueNFC UTF8(TRI_UNKNOWN_MEM_ZONE, meth);
      string methstring = *UTF8;
      reqType = triagens::rest::HttpRequest::translateMethod(methstring);
    }
  }

  string destination = TRI_ObjectToString(args[2]);
  if (destination == "") {
    destination = "shard:shardBlubb";
  }

  string path = TRI_ObjectToString(args[3]);
  if (path == "") {
    path = "/_admin/version";
  }

  string clientTransactionId = TRI_ObjectToString(args[4]);
  if (clientTransactionId == "") {
    clientTransactionId = StringUtils::itoa(TRI_NewTickServer());
  }

  map<string, string>* headerFields = new map<string, string>;
  if (args[5]->IsObject()) {
    v8::Handle<v8::Object> obj = args[5].As<v8::Object>();
    v8::Handle<v8::Array> props = obj->GetOwnPropertyNames();
    uint32_t i;
    for (i = 0; i < props->Length(); ++i) {
      v8::Handle<v8::Value> prop = props->Get(i);
      v8::Handle<v8::Value> val = obj->Get(prop);
      string propstring = TRI_ObjectToString(prop);
      string valstring = TRI_ObjectToString(val);
      if (propstring != "") {
        headerFields->insert(pair<string,string>(propstring, valstring));
      }
    }
  }

  string body = TRI_ObjectToString(args[6]);

  double timeout = TRI_ObjectToDouble(args[7]);
  if (timeout == 0.0) {
    timeout = 24 * 3600.0;
  }

  bool asyncMode = TRI_ObjectToBoolean(args[8]);

  ClusterCommResult const* res;

  v8::Handle<v8::Object> r = v8::Object::New(isolate);

  if (asyncMode) {
    res = cc->asyncRequest(clientTransactionId,TRI_NewTickServer(),destination,
                         reqType, path, &body, false, headerFields,
                         new CallbackTest("Hello Callback"), timeout);

    if (res == nullptr) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                               "couldn't queue async request");
    }

    LOG_DEBUG("JS_ClusterTest: request has been submitted");

    OperationID opID = res->operationID;
    delete res;

    ClusterCommOpStatus status;

    // Wait until the request has actually been sent:
    while (true) {
      res = cc->enquire(opID);
      if (res == 0) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "couldn't enquire operation");
      }
      status = res->status;
      delete res;
      if (status >= CL_COMM_SENT) {
        break;
      }
      LOG_DEBUG("JS_ClusterTest: request not yet sent");

      usleep(50000);
    }

    LOG_DEBUG("JS_ClusterTest: request has been sent, status: %d",status);

    res = cc->wait("", 0, opID, "");

    if (0 == res) {
      r->Set(TRI_V8_ASCII_STRING("errorMsg"), TRI_V8_ASCII_STRING("out of memory"));
      LOG_DEBUG("JS_ClusterTest: out of memory");
    }
    else if (res->status == CL_COMM_TIMEOUT) {
      r->Set(TRI_V8_ASCII_STRING("timeout"), v8::BooleanObject::New(true));
      LOG_DEBUG("JS_ClusterTest: timeout");
    }
    else if (res->status == CL_COMM_ERROR) {
      if (res->result && res->result->isComplete()) {
        v8::Handle<v8::Object> details = v8::Object::New(isolate);
        details->Set(TRI_V8_ASCII_STRING("code"),
                     v8::Number::New(isolate,
                     res->result->getHttpReturnCode()));
        details->Set(TRI_V8_ASCII_STRING("message"),
                     TRI_V8_STD_STRING(res->result->getHttpReturnMessage()));
        details->Set(TRI_V8_ASCII_STRING("body"),
                     TRI_V8_STD_STRING(res->result->getBody()));
        TRI_GET_GLOBAL_STRING(ErrorMessageKey);
        r->Set(TRI_V8_ASCII_STRING("details"), details);
        r->Set(ErrorMessageKey, TRI_V8_ASCII_STRING("got bad HTTP response"));
      }
      else {
        TRI_GET_GLOBAL_STRING(ErrorMessageKey);
        r->Set(ErrorMessageKey,
               TRI_V8_ASCII_STRING("got no HTTP response, DBserver seems gone"));
      }
      LOG_DEBUG("JS_ClusterTest: communications error");
    }
    else if (res->status == CL_COMM_DROPPED) {
      // Note that this can basically not happen
      r->Set(TRI_V8_ASCII_STRING("errorMessage"),
             TRI_V8_ASCII_STRING("request dropped whilst waiting for answer"));
      LOG_DEBUG("JS_ClusterTest: dropped");
    }
    else {   // Everything is OK
      // The headers:
      v8::Handle<v8::Object> h = v8::Object::New(isolate);
      map<string,string> headers = res->answer->headers();
      map<string,string>::iterator i;
      for (i = headers.begin(); i != headers.end(); ++i) {
        h->Set(TRI_V8_STD_STRING(i->first),
               TRI_V8_STD_STRING(i->second));
      }
      r->Set(TRI_V8_ASCII_STRING("headers"), h);

      // The body:
      if (nullptr != res->answer->body()) {
        r->Set(TRI_V8_ASCII_STRING("body"),
               TRI_V8_PAIR_STRING(res->answer->body(),
                                  (int) res->answer->bodySize()));
      }
      LOG_DEBUG("JS_ClusterTest: success");
    }
  }
  else {   // synchronous mode
    res = cc->syncRequest(clientTransactionId, TRI_NewTickServer(),destination,
                          reqType, path, body, *headerFields, timeout);
    delete headerFields;
    if (res != 0) {
      LOG_DEBUG("JS_ClusterTest: request has been sent synchronously, "
                "status: %d",res->status);
    }

    if (nullptr == res) {
      r->Set(TRI_V8_ASCII_STRING("errorMsg"), TRI_V8_ASCII_STRING("out of memory"));
      LOG_DEBUG("JS_ClusterTest: out of memory");
    }
    else if (res->status == CL_COMM_TIMEOUT) {
      r->Set(TRI_V8_ASCII_STRING("timeout"),v8::BooleanObject::New(true));
      LOG_DEBUG("JS_ClusterTest: timeout");
    }
    else if (res->status == CL_COMM_ERROR) {
      r->Set(TRI_V8_ASCII_STRING("errorMessage"),
             TRI_V8_ASCII_STRING("could not send request, DBServer gone"));
      LOG_DEBUG("JS_ClusterTest: communications error");
    }
    else if (res->status == CL_COMM_DROPPED) {
      // Note that this can basically not happen
      r->Set(TRI_V8_ASCII_STRING("errorMessage"),
             TRI_V8_ASCII_STRING("request dropped whilst waiting for answer"));
      LOG_DEBUG("JS_ClusterTest: dropped");
    }
    else {   // Everything is OK
      // The headers:
      v8::Handle<v8::Object> h = v8::Object::New(isolate);
      map<string,string> headers = res->result->getHeaderFields();
      map<string,string>::iterator i;
      for (i = headers.begin(); i != headers.end(); ++i) {
        h->Set(TRI_V8_STD_STRING(i->first), TRI_V8_STD_STRING(i->second));
      }
      r->Set(TRI_V8_ASCII_STRING("headers"), h);

      // The body:
      StringBuffer& theBody = res->result->getBody();
      r->Set(TRI_V8_ASCII_STRING("body"), TRI_V8_STD_STRING(theBody));
      LOG_DEBUG("JS_ClusterTest: success");

    }
  }

  delete res;

  TRI_V8_RETURN(r);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 actions function inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Actions (v8::Isolate* isolate,
                        v8::Handle<v8::Context> context,
                        TRI_vocbase_t* vocbase,
                        ApplicationV8* applicationV8) {
  v8::HandleScope scope(isolate);

  GlobalV8Dealer = applicationV8;


  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_DEFINE_ACTION"), JS_DefineAction);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION"), JS_ExecuteGlobalContextFunction);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_GET_CURRENT_REQUEST"), JS_GetCurrentRequest);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_GET_CURRENT_RESPONSE"), JS_GetCurrentResponse);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_CLUSTER_TEST"), JS_ClusterTest, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_RAW_REQUEST_BODY"), JS_RawRequestBody, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_REQUEST_PARTS"), JS_RequestParts, true);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
