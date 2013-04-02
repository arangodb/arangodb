////////////////////////////////////////////////////////////////////////////////
/// @brief V8 action functions
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

#include "v8-actions.h"

#include "Actions/actions.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/strings.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/ApplicationV8.h"
#include "V8Server/v8-vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static HttpResponse* ExecuteActionVocbase (TRI_vocbase_t* vocbase,
                                           v8::Isolate* isolate,
                                           TRI_action_t const* action,
                                           v8::Handle<v8::Function> callback,
                                           HttpRequest* request);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Actions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief global V8 dealer
////////////////////////////////////////////////////////////////////////////////

ApplicationV8* GlobalV8Dealer = 0;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 class v8_action_t
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Actions
/// @{
////////////////////////////////////////////////////////////////////////////////

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

    void createCallback (v8::Isolate* isolate, v8::Handle<v8::Function> callback) {
      WRITE_LOCKER(_callbacksLock);

      map< v8::Isolate*, v8::Persistent<v8::Function> >::iterator i = _callbacks.find(isolate);

      if (i != _callbacks.end()) {
        i->second.Dispose();
      }

      _callbacks[isolate] = v8::Persistent<v8::Function>::New(callback);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates callback for a context
////////////////////////////////////////////////////////////////////////////////

    HttpResponse* execute (TRI_vocbase_t* vocbase, HttpRequest* request) {
      ApplicationV8::V8Context* context = GlobalV8Dealer->enterContext(false);

      // note: the context might be 0 in case of shut-down
      if (context == 0) {
        // it is safe to return 0 as the caller checks for a 0 return value
        return 0;
      }

      READ_LOCKER(_callbacksLock);

      map< v8::Isolate*, v8::Persistent<v8::Function> >::iterator i = _callbacks.find(context->_isolate);

      if (i == _callbacks.end()) {
        LOGGER_WARNING("no callback function for JavaScript action '" << _url.c_str() << "'");

        GlobalV8Dealer->exitContext(context);

        return new HttpResponse(HttpResponse::NOT_FOUND);
      }

      HttpResponse* response = ExecuteActionVocbase(vocbase, context->_isolate, this, i->second, request);

      GlobalV8Dealer->exitContext(context);

      return response;
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Actions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the action options "parameters" field of type string
////////////////////////////////////////////////////////////////////////////////

static void ParseActionOptionsParameter (TRI_v8_global_t* v8g,
                                         TRI_action_t* action,
                                         string const& key,
                                         string const& parameter) {
  TRI_action_parameter_type_e p;

  if (parameter == "collection") {
    p = TRI_ACT_COLLECTION;
  }
  else if (parameter == "collection-name") {
    p = TRI_ACT_COLLECTION_NAME;
  }
  else if (parameter == "collection-identifier") {
    p = TRI_ACT_COLLECTION_ID;
  }
  else if (parameter == "number") {
    p = TRI_ACT_NUMBER;
  }
  else if (parameter == "string") {
    p = TRI_ACT_STRING;
  }
  else {
    LOG_ERROR("unknown parameter type '%s', falling back to string", parameter.c_str());
    p = TRI_ACT_STRING;
  }

  action->_parameters[key] = p;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the action options "parameters" field
////////////////////////////////////////////////////////////////////////////////

static void ParseActionOptionsParameter (TRI_v8_global_t* v8g,
                                         TRI_action_t* action,
                                         string const& key,
                                         v8::Handle<v8::Value> parameter) {
  if (parameter->IsString() || parameter->IsStringObject()) {
    ParseActionOptionsParameter(v8g, action, key, TRI_ObjectToString(parameter));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the action options "parameters" field
////////////////////////////////////////////////////////////////////////////////

static void ParseActionOptionsParameters (TRI_v8_global_t* v8g,
                                          TRI_action_t* action,
                                          v8::Handle<v8::Object> parameters) {
  v8::Handle<v8::Array> keys = parameters->GetOwnPropertyNames();
  uint32_t len = keys->Length();

  for (uint32_t i = 0;  i < len;  ++i) {
    v8::Handle<v8::Value> key = keys->Get(i);

    ParseActionOptionsParameter(v8g, action, TRI_ObjectToString(key), parameters->Get(key));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the action options
////////////////////////////////////////////////////////////////////////////////

static void ParseActionOptions (TRI_v8_global_t* v8g,
                                TRI_action_t* action,
                                v8::Handle<v8::Object> options) {

  // check "parameters" field
  if (options->Has(v8g->ParametersKey)) {
    v8::Handle<v8::Value> parameters = options->Get(v8g->ParametersKey);

    if (parameters->IsObject()) {
      ParseActionOptionsParameters(v8g, action, parameters->ToObject());
    }
  }

  // check the "prefix" field
  if (options->Has(v8g->PrefixKey)) {
    action->_isPrefix = TRI_ObjectToBoolean(options->Get(v8g->PrefixKey));
  }
  else {
    action->_isPrefix = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

static HttpResponse* ExecuteActionVocbase (TRI_vocbase_t* vocbase,
                                           v8::Isolate* isolate,
                                           TRI_action_t const* action,
                                           v8::Handle<v8::Function> callback,
                                           HttpRequest* request) {
  TRI_v8_global_t* v8g;

  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  v8g = (TRI_v8_global_t*) isolate->GetData();

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

  string const& fullUrl = request->fullUrl();
  req->Set(v8g->UrlKey, v8::String::New(fullUrl.c_str(), fullUrl.size()));

  // copy prefix
  string path = request->prefix();

  req->Set(v8g->PrefixKey, v8::String::New(path.c_str()));

  // copy suffix
  v8::Handle<v8::Array> suffixArray = v8::Array::New();
  vector<string> const& suffix = request->suffix();

  uint32_t index = 0;
  char const* sep = "";

  for (size_t s = action->_urlParts;  s < suffix.size();  ++s) {
    suffixArray->Set(index++, v8::String::New(suffix[s].c_str(), suffix[s].size()));

    path += sep + suffix[s];
    sep = "/";
  }

  req->Set(v8g->SuffixKey, suffixArray);

  // copy full path
  req->Set(v8g->PathKey, v8::String::New(path.c_str()));

  // copy header fields
  v8::Handle<v8::Object> headerFields = v8::Object::New();

  map<string, string> const& headers = request->headers();
  map<string, string>::const_iterator iter = headers.begin();

  for (; iter != headers.end(); ++iter) {
    headerFields->Set(v8::String::New(iter->first.c_str()), v8::String::New(iter->second.c_str()));
  }

  req->Set(v8g->HeadersKey, headerFields);

  // copy request type
  switch (request->requestType()) {
    case HttpRequest::HTTP_REQUEST_POST:
      req->Set(v8g->RequestTypeKey, v8g->PostConstant);
      req->Set(v8g->RequestBodyKey, v8::String::New(request->body()));
      break;

    case HttpRequest::HTTP_REQUEST_PUT:
      req->Set(v8g->RequestTypeKey, v8g->PutConstant);
      req->Set(v8g->RequestBodyKey, v8::String::New(request->body()));
      break;

    case HttpRequest::HTTP_REQUEST_PATCH:
      req->Set(v8g->RequestTypeKey, v8g->PatchConstant);
      req->Set(v8g->RequestBodyKey, v8::String::New(request->body()));
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

  for (map<string, string>::iterator i = values.begin();  i != values.end();  ++i) {
    string const& k = i->first;
    string const& v = i->second;

    map<string, TRI_action_parameter_type_e>::const_iterator p = action->_parameters.find(k);

    if (p == action->_parameters.end()) {
      valuesObject->Set(v8::String::New(k.c_str()), v8::String::New(v.c_str()));
    }
    else {
      TRI_action_parameter_type_e const& ap = p->second;

      switch (ap) {
        case TRI_ACT_COLLECTION: {
          if (! v.empty()) {
            char ch = v[0];
            TRI_vocbase_col_t const* collection = 0;

            if ('0' < ch && ch <= '9') {
              collection = TRI_LookupCollectionByIdVocBase(vocbase, TRI_UInt64String(v.c_str()));
            }
            else {
              collection = TRI_LookupCollectionByNameVocBase(vocbase, v.c_str());
            }

            if (collection != 0) {
              valuesObject->Set(v8::String::New(k.c_str()), TRI_WrapCollection(collection));
            }
          }

          break;
        }

        case TRI_ACT_COLLECTION_NAME: {
          TRI_vocbase_col_t const* collection = TRI_LookupCollectionByNameVocBase(vocbase, v.c_str());

          if (collection != 0) {
            valuesObject->Set(v8::String::New(k.c_str()), TRI_WrapCollection(collection));
          }

          break;
        }

        case TRI_ACT_COLLECTION_ID: {
          TRI_vocbase_col_t const* collection = TRI_LookupCollectionByIdVocBase(
            vocbase,
            TRI_UInt64String(v.c_str()));

          if (collection != 0) {
            valuesObject->Set(v8::String::New(k.c_str()), TRI_WrapCollection(collection));
          }

          break;
        }

        case TRI_ACT_NUMBER:
          valuesObject->Set(v8::String::New(k.c_str()), v8::Number::New(TRI_DoubleString(v.c_str())));
          break;

        case TRI_ACT_STRING: {
          valuesObject->Set(v8::String::New(k.c_str()), v8::String::New(v.c_str()));
          break;
        }
      }
    }
  }

  // copy request array parameter (a[]=1&a[]=2&...)
  map<string, vector<char const*>* > arrayValues = request->arrayValues();

  for (map<string, vector<char const*>* >::iterator i = arrayValues.begin();  i != arrayValues.end();  ++i) {
    string const& k = i->first;
    vector<char const*>* v = i->second;

    v8::Handle<v8::Array> list = v8::Array::New();

    for (size_t i = 0; i < v->size(); ++i) {
      list->Set(i, v8::String::New(v->at(i)));
    }

    valuesObject->Set(v8::String::New(k.c_str()), list);
  }

  req->Set(v8g->ParametersKey, valuesObject);

  // execute the callback
  v8::Handle<v8::Object> res = v8::Object::New();
  v8::Handle<v8::Value> args[2] = { req, res };

  callback->Call(callback, 2, args);

  // convert the result
  if (tryCatch.HasCaught()) {
    string msg = TRI_StringifyV8Exception(&tryCatch);

    HttpResponse* response = new HttpResponse(HttpResponse::SERVER_ERROR);
    response->body().appendText(msg);
    return response;
  }

  else {
    HttpResponse::HttpResponseCode code = HttpResponse::OK;

    if (res->Has(v8g->ResponseCodeKey)) {
      // Windows has issues with converting from a double to an enumeration type
      code = (HttpResponse::HttpResponseCode) ((int) (TRI_ObjectToDouble(res->Get(v8g->ResponseCodeKey))));
    }

    HttpResponse* response = new HttpResponse(code);

    if (res->Has(v8g->ContentTypeKey)) {
      response->setContentType(TRI_ObjectToString(res->Get(v8g->ContentTypeKey)));
    }

    // .............................................................................
    // body
    // .............................................................................

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
          v8::Handle<v8::Value> transformator = transformations->Get(v8::Integer::New(i));
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

    // .............................................................................
    // body from file
    // .............................................................................

    else if (res->Has(v8g->BodyFromFileKey)) {
      TRI_Utf8ValueNFC filename(TRI_UNKNOWN_MEM_ZONE, res->Get(v8g->BodyFromFileKey));
      size_t length;
      char* content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, *filename, &length);

      if (content != 0) {
        response->body().appendText(content, length);
        TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);
      }
      else {
        string msg = string("cannot read file '") + *filename + "': " + TRI_last_error();

        response->body().appendText(msg.c_str());
        response->setResponseCode(HttpResponse::SERVER_ERROR);
      }
    }

    // .............................................................................
    // headers
    // .............................................................................

    if (res->Has(v8g->HeadersKey)) {
      v8::Handle<v8::Value> val = res->Get(v8g->HeadersKey);
      v8::Handle<v8::Object> v8Headers = val.As<v8::Object>();

      if (v8Headers->IsObject()) {
        v8::Handle<v8::Array> props = v8Headers->GetPropertyNames();

        for (uint32_t i = 0; i < props->Length(); i++) {
          v8::Handle<v8::Value> key = props->Get(v8::Integer::New(i));
          response->setHeader(TRI_ObjectToString(key), TRI_ObjectToString(v8Headers->Get(key)));
        }
      }
    }

    return response;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Actions
/// @{
////////////////////////////////////////////////////////////////////////////////

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
      LOGGER_ERROR("cannot create callback for V8 action");
    }
  }
  else {
    LOGGER_ERROR("cannot define V8 action");
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
    TRI_V8_TYPE_ERROR(scope, "<defition> must be a UTF-8 function definition");
  }

  string def = *utf8def;

  // and pass it to the V8 contexts
  GlobalV8Dealer->addGlobalContextMethod(def);

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Actions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 actions function inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Actions (v8::Handle<v8::Context> context, ApplicationV8* applicationV8) {
  v8::HandleScope scope;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  assert(v8g != 0);

  GlobalV8Dealer = applicationV8;

  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(context, "SYS_DEFINE_ACTION", JS_DefineAction);
  TRI_AddGlobalFunctionVocbase(context, "SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION", JS_ExecuteGlobalContextFunction);

  // .............................................................................
  // keys
  // .............................................................................

  v8g->BodyKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("body"));
  v8g->BodyFromFileKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("bodyFromFile"));
  v8g->ContentTypeKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("contentType"));
  v8g->HeadersKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("headers"));
  v8g->ParametersKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("parameters"));
  v8g->PathKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("path"));
  v8g->PrefixKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("prefix"));
  v8g->RequestBodyKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("requestBody"));
  v8g->RequestTypeKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("requestType"));
  v8g->ResponseCodeKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("responseCode"));
  v8g->SuffixKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("suffix"));
  v8g->TransformationsKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("transformations"));
  v8g->UrlKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("url"));
  v8g->UserKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("user"));

  v8g->DeleteConstant = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("DELETE"));
  v8g->GetConstant = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("GET"));
  v8g->HeadConstant = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("HEAD"));
  v8g->OptionsConstant = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("OPTIONS"));
  v8g->PatchConstant = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("PATCH"));
  v8g->PostConstant = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("POST"));
  v8g->PutConstant = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("PUT"));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
