////////////////////////////////////////////////////////////////////////////////
/// @brief V8 action functions
///
/// @file
///
/// DISCLAIMER
///
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-actions.h"

#include "Actions/actions.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/ApplicationScheduler.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/ApplicationV8.h"
#include "V8Server/V8PeriodicTask.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/server.h"

#ifdef TRI_ENABLE_CLUSTER

#include "Cluster/ClusterComm.h"
#include "Cluster/ServerState.h"

#endif

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
/// @brief global VocBase
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* GlobalVocbase = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief global V8 dealer
////////////////////////////////////////////////////////////////////////////////

ApplicationV8* GlobalV8Dealer = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief global scheduler
////////////////////////////////////////////////////////////////////////////////

Scheduler* GlobalScheduler = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief global dispatcher
////////////////////////////////////////////////////////////////////////////////

Dispatcher* GlobalDispatcher = 0;

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

    v8_action_t (set<string> const& contexts)
      : TRI_action_t(contexts) {
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
        i->second.Dispose(isolate);
      }

      _callbacks[isolate] = v8::Persistent<v8::Function>::New(isolate, callback);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the callback for a request
////////////////////////////////////////////////////////////////////////////////

    TRI_action_result_t execute (TRI_vocbase_t* vocbase,
                                 HttpRequest* request) {
      TRI_action_result_t result;

      // determine whether we should force a re-initialistion of the engine in development mode
      bool allowEngineReset;
      extern bool allowUseDatabaseInRESTActions;

      allowEngineReset = false;

      string const& fullUrl = request->fullUrl();

      // only URLs starting with /dev will trigger an engine reset
      if (fullUrl.find("/dev/") == 0) {
        allowEngineReset = true;
      }

      ApplicationV8::V8Context* context = GlobalV8Dealer->enterContext(
        vocbase,
        request,
        ! allowEngineReset,
        allowUseDatabaseInRESTActions);

      // note: the context might be 0 in case of shut-down
      if (context == 0) {
        return result;
      }

      // locate the callback
      READ_LOCKER(_callbacksLock);

      map< v8::Isolate*, v8::Persistent<v8::Function> >::iterator i = _callbacks.find(context->_isolate);

      if (i == _callbacks.end()) {
        LOG_WARNING("no callback function for JavaScript action '%s'", _url.c_str());

        GlobalV8Dealer->exitContext(context);

        result.isValid = true;
        result.response = new HttpResponse(HttpResponse::NOT_FOUND);

        return result;
      }

      // and execute it
      result = ExecuteActionVocbase(vocbase, context->_isolate, this, i->second, request);

      GlobalV8Dealer->exitContext(context);

      return result;
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

static void ParseActionOptions (TRI_v8_global_t* v8g,
                                TRI_action_t* action,
                                v8::Handle<v8::Object> options) {

  // check the "prefix" field
  if (options->Has(v8g->PrefixKey)) {
    action->_isPrefix = TRI_ObjectToBoolean(options->Get(v8g->PrefixKey));
  }
  else {
    action->_isPrefix = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add cookie
////////////////////////////////////////////////////////////////////////////////

static void AddCookie (TRI_v8_global_t const* v8g,
                       HttpResponse* response,
                       v8::Handle<v8::Object> data) {

  string name;
  string value;
  int lifeTimeSeconds = 0;
  string path = "/";
  string domain = "";
  bool secure = false;
  bool httpOnly = false;

  if (data->Has(v8g->NameKey)) {
    v8::Handle<v8::Value> v = data->Get(v8g->NameKey);
    name = TRI_ObjectToString(v);
  }
  else {
    // something is wrong here
    return;
  }
  if (data->Has(v8g->ValueKey)) {
    v8::Handle<v8::Value> v = data->Get(v8g->ValueKey);
    value = TRI_ObjectToString(v);
  }
  else {
    // something is wrong here
    return;
  }
  if (data->Has(v8g->LifeTimeKey)) {
    v8::Handle<v8::Value> v = data->Get(v8g->LifeTimeKey);
    lifeTimeSeconds = TRI_ObjectToInt64(v);
  }
  if (data->Has(v8g->PathKey) && ! data->Get(v8g->PathKey)->IsUndefined()) {
    v8::Handle<v8::Value> v = data->Get(v8g->PathKey);
    path = TRI_ObjectToString(v);
  }
  if (data->Has(v8g->DomainKey) && ! data->Get(v8g->DomainKey)->IsUndefined()) {
    v8::Handle<v8::Value> v = data->Get(v8g->DomainKey);
    domain = TRI_ObjectToString(v);
  }
  if (data->Has(v8g->SecureKey)) {
    v8::Handle<v8::Value> v = data->Get(v8g->SecureKey);
    secure = TRI_ObjectToBoolean(v);
  }
  if (data->Has(v8g->HttpOnlyKey)) {
    v8::Handle<v8::Value> v = data->Get(v8g->HttpOnlyKey);
    httpOnly = TRI_ObjectToBoolean(v);
  }

  response->setCookie(name, value, lifeTimeSeconds, path, domain, secure, httpOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a C++ HttpRequest to a V8 request object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> RequestCppToV8 ( TRI_v8_global_t const* v8g,
                                               HttpRequest* request) {
  v8::HandleScope scope;
  // setup the request
  v8::Handle<v8::Object> req = v8::Object::New();

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

  if (user.empty()) {
    req->Set(v8g->UserKey, v8::Null());
  }
  else {
    req->Set(v8g->UserKey, v8::String::New(user.c_str(), user.size()));
  }

  // create database attribute
  string const& database = request->databaseName();
  assert(! database.empty());

  req->Set(v8g->DatabaseKey, v8::String::New(database.c_str(), database.size()));

  // set the full url
  string const& fullUrl = request->fullUrl();
  req->Set(v8g->UrlKey, v8::String::New(fullUrl.c_str(), fullUrl.size()));

  // set the protocol
  string const& protocol = request->protocol();
  req->Set(v8g->ProtocolKey, v8::String::New(protocol.c_str(), protocol.size()));

  // set the connection info
  const ConnectionInfo& info = request->connectionInfo();

  v8::Handle<v8::Object> serverArray = v8::Object::New();
  serverArray->Set(v8g->AddressKey, v8::String::New(info.serverAddress.c_str(), info.serverAddress.size()));
  serverArray->Set(v8g->PortKey, v8::Number::New(info.serverPort));
  req->Set(v8g->ServerKey, serverArray);

  v8::Handle<v8::Object> clientArray = v8::Object::New();
  clientArray->Set(v8g->AddressKey, v8::String::New(info.clientAddress.c_str(), info.clientAddress.size()));
  clientArray->Set(v8g->PortKey, v8::Number::New(info.clientPort));
  req->Set(v8g->ClientKey, clientArray);

  // copy prefix
  string path = request->prefix();

  req->Set(v8g->PrefixKey, v8::String::New(path.c_str(), path.size()));

  // copy header fields
  v8::Handle<v8::Object> headerFields = v8::Object::New();

  map<string, string> const& headers = request->headers();
  map<string, string>::const_iterator iter = headers.begin();

  for (; iter != headers.end(); ++iter) {
    headerFields->Set(v8::String::New(iter->first.c_str(),
                      iter->first.size()),
                      v8::String::New(iter->second.c_str(),
                      iter->second.size()));
  }

  req->Set(v8g->HeadersKey, headerFields);

  // copy request type
  switch (request->requestType()) {
    case HttpRequest::HTTP_REQUEST_POST:
      req->Set(v8g->RequestTypeKey, v8g->PostConstant);
      req->Set(v8g->RequestBodyKey, v8::String::New(request->body(),
               request->bodySize()));
      break;

    case HttpRequest::HTTP_REQUEST_PUT:
      req->Set(v8g->RequestTypeKey, v8g->PutConstant);
      req->Set(v8g->RequestBodyKey, v8::String::New(request->body(),
               request->bodySize()));
      break;

    case HttpRequest::HTTP_REQUEST_PATCH:
      req->Set(v8g->RequestTypeKey, v8g->PatchConstant);
      req->Set(v8g->RequestBodyKey, v8::String::New(request->body(),
               request->bodySize()));
      break;

    case HttpRequest::HTTP_REQUEST_OPTIONS:
      req->Set(v8g->RequestTypeKey, v8g->OptionsConstant);
      break;

    case HttpRequest::HTTP_REQUEST_DELETE:
      req->Set(v8g->RequestTypeKey, v8g->DeleteConstant);
      break;

    case HttpRequest::HTTP_REQUEST_HEAD:
      req->Set(v8g->RequestTypeKey, v8g->HeadConstant);
      break;

    case HttpRequest::HTTP_REQUEST_GET:
    default:
      req->Set(v8g->RequestTypeKey, v8g->GetConstant);
      break;
  }

  // copy request parameter
  v8::Handle<v8::Object> valuesObject = v8::Object::New();
  map<string, string> values = request->values();

  for (map<string, string>::iterator i = values.begin();
       i != values.end();  ++i) {
    string const& k = i->first;
    string const& v = i->second;

    valuesObject->Set(v8::String::New(k.c_str(), k.size()),
                      v8::String::New(v.c_str(), v.size()));
  }

  // copy request array parameter (a[]=1&a[]=2&...)
  map<string, vector<char const*>* > arrayValues = request->arrayValues();

  for (map<string, vector<char const*>* >::iterator i = arrayValues.begin();
       i != arrayValues.end();  ++i) {
    string const& k = i->first;
    vector<char const*>* v = i->second;

    v8::Handle<v8::Array> list = v8::Array::New();

    for (size_t i = 0; i < v->size(); ++i) {
      list->Set(i, v8::String::New(v->at(i)));
    }

    valuesObject->Set(v8::String::New(k.c_str(), k.size()), list);
  }

  req->Set(v8g->ParametersKey, valuesObject);

  // copy cookies
  v8::Handle<v8::Object> cookiesObject = v8::Object::New();

  map<string, string> const& cookies = request->cookieValues();
  iter = cookies.begin();

  for (; iter != cookies.end(); ++iter) {
    cookiesObject->Set(v8::String::New(iter->first.c_str(),
                       iter->first.size()),
                       v8::String::New(iter->second.c_str(),
                       iter->second.size()));
  }

  req->Set(v8g->CookiesKey, cookiesObject);

  // determine API compatibility version
  int32_t compatibility = request->compatibility();
  req->Set(v8g->CompatibilityKey, v8::Integer::New(compatibility));

  return scope.Close(req);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a C++ HttpRequest to a V8 request object
////////////////////////////////////////////////////////////////////////////////

static HttpResponse* ResponseV8ToCpp (TRI_v8_global_t const* v8g,
                                      v8::Handle<v8::Object> res) {
  HttpResponse::HttpResponseCode code = HttpResponse::OK;

  if (res->Has(v8g->ResponseCodeKey)) {
    // Windows has issues with converting from a double to an enumeration type
    code = (HttpResponse::HttpResponseCode)
           ((int) (TRI_ObjectToDouble(res->Get(v8g->ResponseCodeKey))));
  }

  HttpResponse* response = new HttpResponse(code);

  if (res->Has(v8g->ContentTypeKey)) {
    response->setContentType(
        TRI_ObjectToString(res->Get(v8g->ContentTypeKey)));
  }

  // .........................................................................
  // body
  // .........................................................................

  if (res->Has(v8g->BodyKey)) {
    // check if we should apply result transformations
    // transformations turn the result from one type into another
    // a Javascript action can request transformations by
    // putting a list of transformations into the res.transformations
    // array, e.g. res.transformations = [ "base64encode" ]
    v8::Handle<v8::Value> val = res->Get(v8g->TransformationsKey);

    if (val->IsArray()) {
      string out(TRI_ObjectToString(res->Get(v8g->BodyKey)));
      v8::Handle<v8::Array> transformations = val.As<v8::Array>();

      for (uint32_t i = 0; i < transformations->Length(); i++) {
        v8::Handle<v8::Value> transformator
              = transformations->Get(v8::Integer::New(i));
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
      response->body().appendText(TRI_ObjectToString(res->Get(v8g->BodyKey)));
    }
  }

  // .........................................................................
  // body from file
  // .........................................................................

  else if (res->Has(v8g->BodyFromFileKey)) {
    TRI_Utf8ValueNFC filename(TRI_UNKNOWN_MEM_ZONE,
                              res->Get(v8g->BodyFromFileKey));
    size_t length;
    char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *filename, &length);

    if (content != 0) {
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

  if (res->Has(v8g->HeadersKey)) {
    v8::Handle<v8::Value> val = res->Get(v8g->HeadersKey);
    v8::Handle<v8::Object> v8Headers = val.As<v8::Object>();

    if (v8Headers->IsObject()) {
      v8::Handle<v8::Array> props = v8Headers->GetPropertyNames();

      for (uint32_t i = 0; i < props->Length(); i++) {
        v8::Handle<v8::Value> key = props->Get(v8::Integer::New(i));
        response->setHeader(TRI_ObjectToString(key),
                            TRI_ObjectToString(v8Headers->Get(key)));
      }
    }
  }

  // .........................................................................
  // cookies
  // .........................................................................

  if (res->Has(v8g->CookiesKey)) {
    v8::Handle<v8::Value> val = res->Get(v8g->CookiesKey);
    v8::Handle<v8::Object> v8Cookies = val.As<v8::Object>();

    if (v8Cookies->IsArray()) {
      v8::Handle<v8::Array> v8Array = v8Cookies.As<v8::Array>();

      for (uint32_t i = 0; i < v8Array->Length(); i++) {
        v8::Handle<v8::Value> v8Cookie = v8Array->Get(i);
        if (v8Cookie->IsObject()) {
          AddCookie(v8g, response, v8Cookie.As<v8::Object>());
        }
      }
    }
    else if (v8Cookies->IsObject()) {
      // one cookie
      AddCookie(v8g, response, v8Cookies);
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
  TRI_v8_global_t const* v8g;

  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  v8g = (TRI_v8_global_t const*) isolate->GetData();

  v8::Handle<v8::Object> req = RequestCppToV8(v8g, request);

  // copy suffix, which comes from the action:
  string path = request->prefix();
  v8::Handle<v8::Array> suffixArray = v8::Array::New();
  vector<string> const& suffix = request->suffix();

  uint32_t index = 0;
  char const* sep = "";

  for (size_t s = action->_urlParts;  s < suffix.size();  ++s) {
    suffixArray->Set(index++, v8::String::New(suffix[s].c_str(),
                     suffix[s].size()));

    path += sep + suffix[s];
    sep = "/";
  }

  req->Set(v8g->SuffixKey, suffixArray);

  // copy full path
  req->Set(v8g->PathKey, v8::String::New(path.c_str(), path.size()));

  // execute the callback
  v8::Handle<v8::Object> res = v8::Object::New();
  v8::Handle<v8::Value> args[2] = { req, res };

  callback->Call(callback, 2, args);

  // convert the result
  result.isValid = true;

  if (tryCatch.HasCaught()) {
    v8::Handle<v8::Value> exception = tryCatch.Exception();
    bool isSleepAndRequeue = v8g->SleepAndRequeueFuncTempl->HasInstance(exception);

    if (isSleepAndRequeue) {
      result.requeue = true;
      result.sleep = TRI_ObjectToDouble(exception->ToObject()->Get(v8g->SleepKey));
    }
    else {
      string msg = TRI_StringifyV8Exception(&tryCatch);

      HttpResponse* response = new HttpResponse(HttpResponse::SERVER_ERROR);
      response->body().appendText(msg);

      result.response = response;
    }
  }

  else {
    result.response = ResponseV8ToCpp(v8g, res);
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

static v8::Handle<v8::Value> JS_DefineAction (v8::Arguments const& argv) {
  v8::Isolate* isolate;

  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  isolate = v8::Isolate::GetCurrent();
  v8g = (TRI_v8_global_t*) isolate->GetData();

  if (argv.Length() != 4) {
    TRI_V8_EXCEPTION_USAGE(scope, "defineAction(<name>, <callback>, <parameter>, <contexts>)");
  }

  // extract the action name
  TRI_Utf8ValueNFC utf8name(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*utf8name == 0) {
    TRI_V8_TYPE_ERROR(scope, "<name> must be an UTF-8 string");
  }

  string name = *utf8name;

  // extract the action callback
  if (! argv[1]->IsFunction()) {
    TRI_V8_TYPE_ERROR(scope, "<callback> must be a function");
  }

  v8::Handle<v8::Function> callback = v8::Handle<v8::Function>::Cast(argv[1]);

  // extract the options
  v8::Handle<v8::Object> options;

  if (argv[2]->IsObject()) {
    options = argv[2]->ToObject();
  }
  else {
    options = v8::Object::New();
  }

  // extract the contexts
  set<string> contexts;

  if (! argv[3]->IsArray()) {
    TRI_V8_TYPE_ERROR(scope, "<contexts> must be a list of contexts");
  }

  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(argv[3]);

  uint32_t n = array->Length();

  for (uint32_t j = 0; j < n; ++j) {
    v8::Handle<v8::Value> item = array->Get(j);

    contexts.insert(TRI_ObjectToString(item));
  }

  // create an action with the given options
  v8_action_t* action = new v8_action_t(contexts);
  ParseActionOptions(v8g, action, options);

  // store an action with the given name
  TRI_action_t* result = TRI_DefineActionVocBase(name, action);

  // and define the callback
  if (result != 0) {
    action = dynamic_cast<v8_action_t*>(result);

    if (action != 0) {
      action->createCallback(isolate, callback);
    }
    else {
      LOG_ERROR("cannot create callback for V8 action");
    }
  }
  else {
    LOG_ERROR("cannot define V8 action");
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief eventually executes a function in all contexts
///
/// @FUN{internal.executeGlobalContextFunction(@FA{function-definition})}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExecuteGlobalContextFunction (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "executeGlobalContextFunction(<function-definition>)");
  }

  // extract the action name
  v8::String::Utf8Value utf8def(argv[0]);

  if (*utf8def == 0) {
    TRI_V8_TYPE_ERROR(scope, "<definition> must be a UTF-8 function definition");
  }

  string def = *utf8def;

  // and pass it to the V8 contexts
  GlobalV8Dealer->addGlobalContextMethod(def);

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function to test sharding
///
/// @FUN{internal.defineAction(@FA{name}, @FA{callback}, @FA{parameter})}
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_CLUSTER

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

static v8::Handle<v8::Value> JS_ClusterTest (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = (TRI_v8_global_t*)
                         v8::Isolate::GetCurrent()->GetData();

  if (argv.Length() != 9) {
    TRI_V8_EXCEPTION_USAGE(scope,
      "SYS_CLUSTER_TEST(<req>, <res>, <dest>, <path>, <clientTransactionID>, "
      "<headers>, <body>, <timeout>, <asyncMode>)");
  }

  if (ServerState::instance()->getRole() != ServerState::ROLE_COORDINATOR) {
    TRI_V8_EXCEPTION_INTERNAL(scope,"request works only in coordinator role");
  }

  ClusterComm* cc = ClusterComm::instance();

  if (cc == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL,
                             "clustercomm object not found");
  }

  // Now get the arguments to form our request:
  triagens::rest::HttpRequest::HttpRequestType reqType
    = triagens::rest::HttpRequest::HTTP_REQUEST_GET;
  if (argv[0]->IsObject()) {
    v8::Handle<v8::Object> obj = argv[0].As<v8::Object>();
    v8::Handle<v8::Value> meth = obj->Get(v8::String::New("requestType"));
    if (meth->IsString()) {
      TRI_Utf8ValueNFC UTF8(TRI_UNKNOWN_MEM_ZONE, meth);
      string methstring = *UTF8;
      reqType = triagens::rest::HttpRequest::translateMethod(methstring);
    }
  }

  string destination = TRI_ObjectToString(argv[2]);
  if (destination == "") {
    destination = "shard:shardBlubb";
  }

  string path = TRI_ObjectToString(argv[3]);
  if (path == "") {
    path = "/_admin/version";
  }

  string clientTransactionId = TRI_ObjectToString(argv[4]);
  if (clientTransactionId == "") {
    clientTransactionId = StringUtils::itoa(TRI_NewTickServer());
  }

  map<string, string>* headerFields = new map<string, string>;
  if (argv[5]->IsObject()) {
    v8::Handle<v8::Object> obj = argv[5].As<v8::Object>();
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

  string body = TRI_ObjectToString(argv[6]);

  double timeout = TRI_ObjectToDouble(argv[7]);
  if (timeout == 0.0) {
    timeout = 24*3600.0;
  }

  bool asyncMode = TRI_ObjectToBoolean(argv[8]);

  ClusterCommResult const* res;

  v8::Handle<v8::Object> r = v8::Object::New();

  if (asyncMode) {
    res = cc->asyncRequest(clientTransactionId,TRI_NewTickServer(),destination,
                         reqType, path, &body, false, headerFields,
                         new CallbackTest("Hello Callback"), timeout);

    if (res == 0) {
      TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL,
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
        TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL,
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
      r->Set(v8::String::New("errorMsg"),v8::String::New("out of memory"));
      LOG_DEBUG("JS_ClusterTest: out of memory");
    }
    else if (res->status == CL_COMM_TIMEOUT) {
      r->Set(v8::String::New("timeout"),v8::BooleanObject::New(true));
      LOG_DEBUG("JS_ClusterTest: timeout");
    }
    else if (res->status == CL_COMM_ERROR) {
      if (res->result && res->result->isComplete()) {
        v8::Handle<v8::Object> details = v8::Object::New();
        details->Set(v8::String::New("code"),
                  v8::Number::New(res->result->getHttpReturnCode()));
        details->Set(v8::String::New("message"),
                  v8::String::New(res->result->getHttpReturnMessage().c_str()));
        details->Set(v8::String::New("body"),
                v8::String::New(res->result->getBody().str().c_str(),
                res->result->getBody().str().length()));

        r->Set(v8::String::New("details"), details);
        r->Set(v8g->ErrorMessageKey,
               v8::String::New("got bad HTTP response"));
      }
      else {
        r->Set(v8g->ErrorMessageKey,
               v8::String::New("got no HTTP response, DBserver seems gone"));
      }
      LOG_DEBUG("JS_ClusterTest: communications error");
    }
    else if (res->status == CL_COMM_DROPPED) {
      // Note that this can basically not happen
      r->Set(v8::String::New("errorMessage"),
             v8::String::New("request dropped whilst waiting for answer"));
      LOG_DEBUG("JS_ClusterTest: dropped");
    }
    else {   // Everything is OK
      // The headers:
      v8::Handle<v8::Object> h = v8::Object::New();
      map<string,string> headers = res->answer->headers();
      map<string,string>::iterator i;
      for (i = headers.begin(); i != headers.end(); ++i) {
        h->Set(v8::String::New(i->first.c_str()),
               v8::String::New(i->second.c_str()));
      }
      r->Set(v8::String::New("headers"), h);

      // The body:
      if (0 != res->answer->body()) {
        r->Set(v8::String::New("body"), v8::String::New(res->answer->body(),
                                                    res->answer->bodySize()));
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

    if (0 == res) {
      r->Set(v8::String::New("errorMsg"),v8::String::New("out of memory"));
      LOG_DEBUG("JS_ClusterTest: out of memory");
    }
    else if (res->status == CL_COMM_TIMEOUT) {
      r->Set(v8::String::New("timeout"),v8::BooleanObject::New(true));
      LOG_DEBUG("JS_ClusterTest: timeout");
    }
    else if (res->status == CL_COMM_ERROR) {
      r->Set(v8::String::New("errorMessage"),
             v8::String::New("could not send request, DBServer gone"));
      LOG_DEBUG("JS_ClusterTest: communications error");
    }
    else if (res->status == CL_COMM_DROPPED) {
      // Note that this can basically not happen
      r->Set(v8::String::New("errorMessage"),
             v8::String::New("request dropped whilst waiting for answer"));
      LOG_DEBUG("JS_ClusterTest: dropped");
    }
    else {   // Everything is OK
      // The headers:
      v8::Handle<v8::Object> h = v8::Object::New();
      map<string,string> headers = res->result->getHeaderFields();
      map<string,string>::iterator i;
      for (i = headers.begin(); i != headers.end(); ++i) {
        h->Set(v8::String::New(i->first.c_str()),
               v8::String::New(i->second.c_str()));
      }
      r->Set(v8::String::New("headers"), h);

      // The body:
      string theBody = res->result->getBody().str();
      r->Set(v8::String::New("body"), v8::String::New(theBody.c_str(),
                                                      theBody.size()));
      LOG_DEBUG("JS_ClusterTest: success");

    }
  }

  delete res;

  return scope.Close(r);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a periodic task
///
/// @FUN{internal.definePeriodic(@FA{offset}, @FA{period}, @FA{module}, @FA{funcname})}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DefinePeriodic (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 5) {
    TRI_V8_EXCEPTION_USAGE(scope, "definePeriodic(<offset>, <period>, <module>, <funcname>, <string-parameter>)");
  }

  // offset in seconds into period
  double offset = TRI_ObjectToDouble(argv[0]);

  // period in seconds
  double period = TRI_ObjectToDouble(argv[1]);

  if (period <= 0.0) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "<period> must be positive");
  }

  // extract the module name
  TRI_Utf8ValueNFC moduleName(TRI_UNKNOWN_MEM_ZONE, argv[2]);

  if (*moduleName == 0) {
    TRI_V8_TYPE_ERROR(scope, "<module> must be an UTF-8 string");
  }

  string module = *moduleName;

  // extract the function name
  TRI_Utf8ValueNFC funcName(TRI_UNKNOWN_MEM_ZONE, argv[3]);

  if (*funcName == 0) {
    TRI_V8_TYPE_ERROR(scope, "<funcname> must be an UTF-8 string");
  }

  string func = *funcName;

  // extract the parameter
  TRI_Utf8ValueNFC parameterString(TRI_UNKNOWN_MEM_ZONE, argv[4]);

  if (*parameterString == 0) {
    TRI_V8_TYPE_ERROR(scope, "<parameter> must be an UTF-8 string");
  }

  string parameter = *parameterString;

  // create a new periodic task
  if (GlobalScheduler != 0 && GlobalDispatcher != 0) {
    V8PeriodicTask* task = new V8PeriodicTask(
      GlobalVocbase,
      GlobalV8Dealer,
      GlobalScheduler,
      GlobalDispatcher,
      offset,
      period,
      module,
      func,
      parameter);

    GlobalScheduler->registerTask(task);
  }

  return scope.Close(v8::Undefined());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 actions function inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Actions (v8::Handle<v8::Context> context,
                        TRI_vocbase_t* vocbase,
                        ApplicationScheduler* scheduler,
                        ApplicationDispatcher* dispatcher,
                        ApplicationV8* applicationV8) {
  v8::HandleScope scope;

  GlobalVocbase = vocbase;
  GlobalV8Dealer = applicationV8;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  /* TRI_v8_global_t* v8g = */ TRI_CreateV8Globals(isolate);

  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(context, "SYS_DEFINE_ACTION", JS_DefineAction);
  TRI_AddGlobalFunctionVocbase(context, "SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION", JS_ExecuteGlobalContextFunction);

#ifdef TRI_ENABLE_CLUSTER
  TRI_AddGlobalFunctionVocbase(context, "SYS_CLUSTER_TEST", JS_ClusterTest);
#endif

  // we need a scheduler and a dispatcher to define periodic tasks
  GlobalScheduler = scheduler->scheduler();
  GlobalDispatcher = dispatcher->dispatcher();

  if (GlobalScheduler != 0 && GlobalDispatcher != 0) {
    TRI_AddGlobalFunctionVocbase(context, "SYS_DEFINE_PERIODIC", JS_DefinePeriodic);
  }
  else {
    LOG_ERROR("cannot initialise definePeriodic, scheduler or dispatcher unknown");
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
