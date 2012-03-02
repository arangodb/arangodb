////////////////////////////////////////////////////////////////////////////////
/// @brief V8 action functions
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

#include "v8-actions.h"

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/StringUtils.h"
#include "BasicsC/conversions.h"
#include "BasicsC/logging.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Actions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief actions
////////////////////////////////////////////////////////////////////////////////

static map< string, TRI_action_t* > Actions;

////////////////////////////////////////////////////////////////////////////////
/// @brief actions lock
////////////////////////////////////////////////////////////////////////////////

static ReadWriteLock ActionsLock;

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
/// @brief parses the action options "parameters" field of type string
////////////////////////////////////////////////////////////////////////////////

static void ParseActionOptionsParameter (TRI_v8_global_t* v8g,
<<<<<<< HEAD
<<<<<<< HEAD
                                         TRI_action_options_t ao,
=======
                                         TRI_action_options_t* ao,
>>>>>>> unfinished actions cleanup
=======
                                         TRI_action_options_t ao,
>>>>>>> more action cleanup
                                         string const& key,
                                         string const& parameter) {
  TRI_action_parameter_t p;

  p._name = key;

  if (parameter == "collection") {
    p._type = TRI_ACT_COLLECTION;
  }
  else if (parameter == "collection-identifier") {
    p._type = TRI_ACT_COLLECTION_ID;
  }
  else if (parameter == "number") {
    p._type = TRI_ACT_NUMBER;
  }
  else if (parameter == "string") {
    p._type = TRI_ACT_STRING;
  }
  else {
    LOG_ERROR("unknown parameter type '%s', falling back to string", parameter.c_str());
    p._type = TRI_ACT_STRING;
  }

  ao._parameters[key] = p;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the action options "parameters" field
////////////////////////////////////////////////////////////////////////////////

static void ParseActionOptionsParameter (TRI_v8_global_t* v8g,
<<<<<<< HEAD
<<<<<<< HEAD
                                         TRI_action_options_t ao,
=======
                                         TRI_action_options_t* ao,
>>>>>>> unfinished actions cleanup
=======
                                         TRI_action_options_t ao,
>>>>>>> more action cleanup
                                         string const& key,
                                         v8::Handle<v8::Value> parameter) {
  if (parameter->IsString() || parameter->IsStringObject()) {
    ParseActionOptionsParameter(v8g, ao, key, TRI_ObjectToString(parameter));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the action options "parameters" field
////////////////////////////////////////////////////////////////////////////////

static void ParseActionOptionsParameters (TRI_v8_global_t* v8g,
                                          TRI_action_options_t ao,
                                          v8::Handle<v8::Object> parameters) {
  v8::Handle<v8::Array> keys = parameters->GetOwnPropertyNames();
  uint32_t len = keys->Length();

  for (uint32_t i = 0;  i < len;  ++i) {
    v8::Handle<v8::Value> key = keys->Get(i);

    ParseActionOptionsParameter(v8g, ao, TRI_ObjectToString(key), parameters->Get(key));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the action options
////////////////////////////////////////////////////////////////////////////////

static TRI_action_options_t ParseActionOptions (TRI_v8_global_t* v8g,
                                                v8::Handle<v8::Object> options) {
  TRI_action_options_t ao;

  // check "parameter" field
  if (options->Has(v8g->ParametersKey)) {
    v8::Handle<v8::Value> parameters = options->Get(v8g->ParametersKey);

    if (parameters->IsObject()) {
      ParseActionOptionsParameters(v8g, ao, parameters->ToObject());
    }
  }

  // and return the result
  return ao;
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
/// @FUN{defineSystemAction(@FA{name}, @FA{queue}, @FA{callback}, @FA{parameter})}
///
/// Possible queues are:
/// - "CLIENT"
/// - "SYSTEM"
/// - "MONITORING"
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DefineAction (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  if (argv.Length() != 4) {
<<<<<<< HEAD
<<<<<<< HEAD
    return scope.Close(v8::ThrowException(v8::String::New("usage: SYS_DEFINE_ACTION(<name>, <queue>, <callback>, <parameter>)")));
=======
    return scope.Close(v8::ThrowException(v8::String::New("usage: defineAction(<name>, <queue>, <callback>, <parameter>)")));
>>>>>>> unfinished actions cleanup
=======
    return scope.Close(v8::ThrowException(v8::String::New("usage: SYS_DEFINE_ACTION(<name>, <queue>, <callback>, <parameter>)")));
>>>>>>> actions are now working again
  }

  // extract the action name
  v8::String::Utf8Value utf8name(argv[0]);

  if (*utf8name == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<name> must be an UTF8 name")));
  }

  string name = *utf8name;

  if (name.empty()) {
    return scope.Close(v8::ThrowException(v8::String::New("<name> must be non-empty")));
  }

  // extract the action queue
  v8::String::Utf8Value utf8queue(argv[1]);

  if (*utf8queue == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<queue> must be an UTF8 name")));
  }

  string queue = *utf8queue;

  if (queue != "CLIENT" && queue != "SYSTEM" && queue != "MONITORING") {
    return scope.Close(v8::ThrowException(v8::String::New("<queue> must CLIENT, SYSTEM, MONITORING")));
  }

  // extract the action callback
  if (! argv[2]->IsFunction()) {
    return scope.Close(v8::ThrowException(v8::String::New("<callback> must be a function")));
  }

  v8::Handle<v8::Function> callback = v8::Handle<v8::Function>::Cast(argv[2]);

  // extract the options
  v8::Handle<v8::Object> options;

  if (argv[3]->IsObject()) {
<<<<<<< HEAD
<<<<<<< HEAD
    options = argv[3]->ToObject();
=======
    options = argv[2]->ToObject();
>>>>>>> unfinished actions cleanup
=======
    options = argv[3]->ToObject();
>>>>>>> actions are now working again
  }
  else {
    options = v8::Object::New();
  }

  TRI_action_options_t ao = ParseActionOptions(v8g, options);

  // store an action with the given name
  TRI_CreateActionVocBase(name, queue, ao, callback);

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
/// @brief creates a new action
////////////////////////////////////////////////////////////////////////////////

void TRI_CreateActionVocBase (string const& name,
                              string const& queue,
                              TRI_action_options_t ao,
                              v8::Handle<v8::Function> callback) {

  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  WRITE_LOCKER(ActionsLock);
  WRITE_LOCKER(v8g->ActionsLock);

<<<<<<< HEAD
<<<<<<< HEAD
  string url = name;

  while (! url.empty() && url[0] == '/') {
    url = url.substr(1);
  }

  // create a new action and store the callback function
  if (Actions.find(url) == Actions.end()) {
    TRI_action_t* action = new TRI_action_t;

    action->_url = url;
    action->_urlParts = StringUtils::split(url, "/").size();
    action->_queue = queue;
    action->_options = ao;

    Actions[url] = action;
  }

  // check if we already know an callback
  map< string, v8::Persistent<v8::Function> >::iterator i = v8g->Actions.find(url);

  if (i != v8g->Actions.end()) {
    v8::Persistent<v8::Function> cb = i->second;

    cb.Dispose();
  }

  v8g->Actions[url] = v8::Persistent<v8::Function>::New(callback);

  // some debug output
  LOG_DEBUG("created action '%s' for queue %s", url.c_str(), queue.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an action
////////////////////////////////////////////////////////////////////////////////

TRI_action_t const* TRI_LookupActionVocBase (triagens::rest::HttpRequest* request) {
  READ_LOCKER(ActionsLock);

  // check if we know a callback
  vector<string> suffix = request->suffix();
    
  // find longest prefix
  while (true) {
    string name = StringUtils::join(suffix, '/');
    map<string, TRI_action_t*>::iterator i = Actions.find(name);

    if (i != Actions.end()) {
      return i->second;
    }

    if (suffix.empty()) {
      break;
    }

    suffix.pop_back();
  }

=======
=======
  // create a new action and store the callback function
  if (Actions.find(name) == Actions.end()) {
    TRI_action_t* action = new TRI_action_t;

    action->_url = name;
    action->_urlParts = StringUtils::split(name, "/").size();
    action->_queue = queue;
    action->_options = ao;

    Actions[name] = action;
  }

>>>>>>> actions are now working again
  // check if we already know an callback
  map< string, v8::Persistent<v8::Function> >::iterator i = v8g->Actions.find(name);

  if (i != v8g->Actions.end()) {
    v8::Persistent<v8::Function> cb = i->second;

    cb.Dispose();
  }

  v8g->Actions[name] = v8::Persistent<v8::Function>::New(callback);

  // some debug output
  LOG_DEBUG("created action '%s' for queue %s", name.c_str(), queue.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an action
////////////////////////////////////////////////////////////////////////////////

TRI_action_t const* TRI_LookupActionVocBase (triagens::rest::HttpRequest* request) {
  READ_LOCKER(ActionsLock);

  // check if we know a callback
  vector<string> suffix = request->suffix();
    
  // find longest prefix
  while (true) {
    string name = StringUtils::join(suffix, '/');
    map<string, TRI_action_t*>::iterator i = Actions.find(name);

    if (i != Actions.end()) {
      return i->second;
    }

    if (suffix.empty()) {
      break;
    }

    suffix.pop_back();
  }

>>>>>>> unfinished actions cleanup
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

HttpResponse* TRI_ExecuteActionVocBase (TRI_vocbase_t* vocbase,
                                        TRI_action_t const* action,
                                        HttpRequest* request) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  READ_LOCKER(ActionsLock);
  READ_LOCKER(v8g->ActionsLock);

  v8::HandleScope scope;
  v8::TryCatch tryCatch;

<<<<<<< HEAD
<<<<<<< HEAD
  map< string, v8::Persistent<v8::Function> >::iterator i = v8g->Actions.find(action->_url);
=======
  map< string, v8::Persistent<v8::Function> >::iterator i = v8g->Actions.find(action._url);
>>>>>>> unfinished actions cleanup
=======
  map< string, v8::Persistent<v8::Function> >::iterator i = v8g->Actions.find(action->_url);
>>>>>>> more action cleanup

  if (i == v8g->Actions.end()) {
    LOG_DEBUG("no callback for action '%s'", action->_url.c_str());
    return new HttpResponse(HttpResponse::NOT_FOUND);
  }

  v8::Persistent<v8::Function> cb = i->second;

  // setup the request
  v8::Handle<v8::Object> req = v8::Object::New();

  // Example:  
  //      {
<<<<<<< HEAD
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
  //        "requestBody" : "... only for PUT and POST ..."
=======
  //        "_suffix":[
  //          "suffix1",
  //          "suffix2"
  //        ],
  //        "_headers":
  //            {
  //              "accept":"text/html",
  //              "accept-encoding":"gzip, deflate",
  //              "accept-language":"de-de,en-us;q=0.7,en;q=0.3",
  //              "user-agent":"Mozilla/5.0"
  //            },
  //        "_requestType":"GET",
  //        "_requestBody":"... only for PUT and POST ...",
>>>>>>> unfinished actions cleanup
  //      } 
  
  // copy suffix 
  v8::Handle<v8::Array> suffixArray = v8::Array::New();
  vector<string> const& suffix = request->suffix();

  uint32_t index = 0;

<<<<<<< HEAD
<<<<<<< HEAD
  for (size_t s = action->_urlParts;  s < suffix.size();  ++s) {
    suffixArray->Set(index++, v8::String::New(suffix[s].c_str()));
  }

  req->Set(v8g->SuffixKey, suffixArray);
=======
  for (size_t s = action._offset;  s < suffix.size();  ++s) {
=======
  for (size_t s = action->_urlParts;  s < suffix.size();  ++s) {
>>>>>>> more action cleanup
    v8SuffixArray->Set(index++, v8::String::New(suffix[s].c_str()));
  }

  req->Set(v8g->SuffixKey, v8SuffixArray);
>>>>>>> unfinished actions cleanup
  
  // copy header fields
  v8::Handle<v8::Object> headerFields = v8::Object::New();

  map<string, string> const& headers = request->headers();
  map<string, string>::const_iterator iter = headers.begin();

  for (; iter != headers.end(); ++iter) {
    headerFields->Set(v8::String::New(iter->first.c_str()), v8::String::New(iter->second.c_str()));
  }

<<<<<<< HEAD
  req->Set(v8g->HeadersKey, headerFields);  
=======
  req->Set(v8g->HeaderKey, headerFields);  
>>>>>>> unfinished actions cleanup
  
  // copy request type
  switch (request->requestType()) {
    case HttpRequest::HTTP_REQUEST_POST: 
      req->Set(v8g->RequestTypeKey, v8g->PostConstant);
      req->Set(v8g->RequestBodyKey, v8::String::New(request->body().c_str()));
      break;

    case HttpRequest::HTTP_REQUEST_PUT:
      req->Set(v8g->RequestTypeKey, v8g->PutConstant);
      req->Set(v8g->RequestBodyKey, v8::String::New(request->body().c_str()));
      break;

    case HttpRequest::HTTP_REQUEST_DELETE: 
      req->Set(v8g->RequestTypeKey, v8g->DeleteConstant);
      break;

    case HttpRequest::HTTP_REQUEST_HEAD: 
      req->Set(v8g->RequestTypeKey, v8g->HeadConstant);
      break;

    default:
      req->Set(v8g->RequestTypeKey, v8g->GetConstant);
  }
  
  // copy request parameter
<<<<<<< HEAD
  v8::Handle<v8::Array> parametersArray = v8::Array::New();
=======
>>>>>>> unfinished actions cleanup
  map<string, string> values = request->values();

  for (map<string, string>::iterator i = values.begin();  i != values.end();  ++i) {
    string const& k = i->first;
    string const& v = i->second;

    map<string, TRI_action_parameter_t>::const_iterator p = action->_options._parameters.find(k);

<<<<<<< HEAD
<<<<<<< HEAD
    if (p == action->_options._parameters.end()) {
      parametersArray->Set(v8::String::New(k.c_str()), v8::String::New(v.c_str()));
=======
    if (p == cb->_options->_parameters.end()) {
=======
    if (p == action->_options._parameters.end()) {
>>>>>>> more action cleanup
      req->Set(v8::String::New(k.c_str()), v8::String::New(v.c_str()));
>>>>>>> unfinished actions cleanup
    }
    else {
      TRI_action_parameter_t const& ap = p->second;

      switch (ap._type) {
        case TRI_ACT_COLLECTION: {
          TRI_vocbase_col_t const* collection = TRI_FindCollectionByNameVocBase(vocbase, v.c_str(), false);

          if (collection == 0) {
            return new HttpResponse(HttpResponse::NOT_FOUND);
          }

<<<<<<< HEAD
          parametersArray->Set(v8::String::New(k.c_str()), TRI_WrapCollection(collection));
=======
          req->Set(v8::String::New(k.c_str()), TRI_WrapCollection(collection));
>>>>>>> unfinished actions cleanup

          break;
        }

        case TRI_ACT_COLLECTION_ID: {
          TRI_ReadLockReadWriteLock(&vocbase->_lock);

          TRI_vocbase_col_t const* collection = TRI_LookupCollectionByIdVocBase(
            vocbase,
            TRI_UInt64String(v.c_str()));

          TRI_ReadUnlockReadWriteLock(&vocbase->_lock);

          if (collection == 0) {
            return new HttpResponse(HttpResponse::NOT_FOUND);
          }

<<<<<<< HEAD
          parametersArray->Set(v8::String::New(k.c_str()), TRI_WrapCollection(collection));
=======
          req->Set(v8::String::New(k.c_str()), TRI_WrapCollection(collection));
>>>>>>> unfinished actions cleanup

          break;
        }

        case TRI_ACT_NUMBER:
<<<<<<< HEAD
          parametersArray->Set(v8::String::New(k.c_str()), v8::Number::New(TRI_DoubleString(v.c_str())));
          break;

        case TRI_ACT_STRING: {
          parametersArray->Set(v8::String::New(k.c_str()), v8::String::New(v.c_str()));
=======
          req->Set(v8::String::New(k.c_str()), v8::Number::New(TRI_DoubleString(v.c_str())));
          break;

        case TRI_ACT_STRING: {
          req->Set(v8::String::New(k.c_str()), v8::String::New(v.c_str()));
>>>>>>> unfinished actions cleanup
          break;
        }
      }
    }
  }

  req->Set(v8g->ParametersKey, parametersArray);  

  // execute the callback
  v8::Handle<v8::Object> res = v8::Object::New();
  v8::Handle<v8::Value> args[2] = { req, res };

  cb->Call(cb, 2, args);

  // convert the result
  if (tryCatch.HasCaught()) {
    string msg = TRI_ReportV8Exception(&tryCatch);

    HttpResponse* response = new HttpResponse(HttpResponse::SERVER_ERROR);
    response->body().appendText(msg);
    return response;
  }

  else {
    HttpResponse::HttpResponseCode code = HttpResponse::OK;

    if (res->Has(v8g->ResponseCodeKey)) {
      code = (HttpResponse::HttpResponseCode) TRI_ObjectToDouble(res->Get(v8g->ResponseCodeKey));
    }

    HttpResponse* response = new HttpResponse(code);

    if (res->Has(v8g->ContentTypeKey)) {
      response->setContentType(TRI_ObjectToString(res->Get(v8g->ContentTypeKey)));
    }

    if (res->Has(v8g->BodyKey)) {
      response->body().appendText(TRI_ObjectToString(res->Get(v8g->BodyKey)));
    }

    if (res->Has(v8g->HeadersKey)) {
      v8::Handle<v8::Value> val = res->Get(v8g->HeadersKey);
      v8::Handle<v8::Object> v8Headers = val.As<v8::Object>();

      if (v8Headers->IsObject()) {
        v8::Handle<v8::Array> props = v8Headers->GetPropertyNames();

        for (size_t i = 0; i < props->Length(); i++) {          
          v8::Handle<v8::Value> key = props->Get(v8::Integer::New(i));
          response->setHeader(TRI_ObjectToString(key), TRI_ObjectToString(v8Headers->Get(key)));
        }
      }
    }

    return response;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 actions function inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Actions (v8::Handle<v8::Context> context, char const* actionQueue) {
  v8::HandleScope scope;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  if (v8g == 0) {
    v8g = new TRI_v8_global_t;
    isolate->SetData(v8g);
  }

  // .............................................................................
  // create the global constants
  // .............................................................................

<<<<<<< HEAD
  context->Global()->Set(v8::String::New("SYS_ACTION_QUEUE"),
                         v8::String::New(actionQueue),
                         v8::ReadOnly);

  // .............................................................................
  // create the global functions
  // .............................................................................

  context->Global()->Set(v8::String::New("SYS_DEFINE_ACTION"),
                         v8::FunctionTemplate::New(JS_DefineAction)->GetFunction(),
                         v8::ReadOnly);

=======
  context->Global()->Set(v8::String::New("SYS_DEFINE_ACTION"),
                         v8::FunctionTemplate::New(JS_DefineAction)->GetFunction(),
                         v8::ReadOnly);

>>>>>>> unfinished actions cleanup
  // .............................................................................
  // keys
  // .............................................................................

  v8g->BodyKey = v8::Persistent<v8::String>::New(v8::String::New("body"));
  v8g->ContentTypeKey = v8::Persistent<v8::String>::New(v8::String::New("contentType"));
<<<<<<< HEAD
  v8g->HeadersKey = v8::Persistent<v8::String>::New(v8::String::New("headers"));
  v8g->ParametersKey = v8::Persistent<v8::String>::New(v8::String::New("parameters"));
  v8g->RequestBodyKey = v8::Persistent<v8::String>::New(v8::String::New("requestBody"));
  v8g->RequestTypeKey = v8::Persistent<v8::String>::New(v8::String::New("requestType"));
  v8g->ResponseCodeKey = v8::Persistent<v8::String>::New(v8::String::New("responseCode"));
  v8g->SuffixKey = v8::Persistent<v8::String>::New(v8::String::New("suffix"));
=======
  v8g->HeaderKey = v8::Persistent<v8::String>::New(v8::String::New("_header"));
  v8g->HeadersKey = v8::Persistent<v8::String>::New(v8::String::New("headers"));
  v8g->ParametersKey = v8::Persistent<v8::String>::New(v8::String::New("parameters"));
  v8g->RequestBodyKey = v8::Persistent<v8::String>::New(v8::String::New("_requestBody"));
  v8g->RequestTypeKey = v8::Persistent<v8::String>::New(v8::String::New("_requestType"));
  v8g->ResponseCodeKey = v8::Persistent<v8::String>::New(v8::String::New("responseCode"));
  v8g->SuffixKey = v8::Persistent<v8::String>::New(v8::String::New("_suffix"));
>>>>>>> unfinished actions cleanup

  v8g->DeleteConstant = v8::Persistent<v8::String>::New(v8::String::New("DELETE"));
  v8g->GetConstant = v8::Persistent<v8::String>::New(v8::String::New("GET"));
  v8g->HeadConstant = v8::Persistent<v8::String>::New(v8::String::New("HEAD"));
  v8g->PostConstant = v8::Persistent<v8::String>::New(v8::String::New("POST"));
  v8g->PutConstant = v8::Persistent<v8::String>::New(v8::String::New("PUT"));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
