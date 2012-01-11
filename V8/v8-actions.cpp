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

#include <map>

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "BasicsC/conversions.h"
#include "BasicsC/logging.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

#include "V8/v8-utils.h"
#include "V8/v8-vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Actions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief parameter type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_ACT_STRING,
  TRI_ACT_NUMBER,
  TRI_ACT_COLLECTION,
  TRI_ACT_COLLECTION_ID
}
action_parameter_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief parameter definition
////////////////////////////////////////////////////////////////////////////////

typedef struct action_parameter_s {
  action_parameter_s ()
    : _name(),
      _type(TRI_ACT_NUMBER) {
  }

  string _name;
  action_parameter_type_e _type;
}
action_parameter_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief action options definition
////////////////////////////////////////////////////////////////////////////////

typedef struct action_options_s {
  action_options_s ()
    : _parameters() {
  }

  map<string, action_parameter_t*> _parameters;
}
action_options_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief action definition
////////////////////////////////////////////////////////////////////////////////

typedef struct action_s {
  private:
    action_s (action_s const&);
    action_s& operator= (action_s const&);

  public:
    action_s ()
      : _name(),
        _callback(),
        _options() {
    }

    string _name;
    v8::Persistent<v8::Function> _callback;
    action_options_t* _options;
}
action_t;

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
                                         action_options_t* ao,
                                         string const& key,
                                         string const& parameter) {
  if (ao->_parameters.find(key) != ao->_parameters.end()) {
    action_parameter_t* p = ao->_parameters[key];

    delete p;
  }

  action_parameter_t* p = new action_parameter_t;

  p->_name = key;

  if (parameter == "collection") {
    p->_type = TRI_ACT_COLLECTION;
  }
  else if (parameter == "collection-identifier") {
    p->_type = TRI_ACT_COLLECTION_ID;
  }
  else if (parameter == "number") {
    p->_type = TRI_ACT_NUMBER;
  }
  else if (parameter == "string") {
    p->_type = TRI_ACT_STRING;
  }
  else {
    LOG_ERROR("unknown parameter type '%s', falling back to string", parameter.c_str());
    p->_type = TRI_ACT_STRING;
  }

  ao->_parameters[key] = p;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the action options "parameters" field
////////////////////////////////////////////////////////////////////////////////

static void ParseActionOptionsParameter (TRI_v8_global_t* v8g,
                                         action_options_t* ao,
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
                                          action_options_t* ao,
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

static action_options_t* ParseActionOptions (TRI_v8_global_t* v8g,
                                             v8::Handle<v8::Object> options) {
  action_options_t* ao;

  ao = new action_options_t;

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
/// @brief defines a new action helper
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> DefineActionHelper (v8::Arguments const& argv, TRI_action_category_e category) {
  v8::HandleScope scope;

  if (argv.Length() < 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: defineAction(<name>, <callback>[, <options>])")));
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

  // extract the action callback
  if (! argv[1]->IsFunction()) {
    return scope.Close(v8::ThrowException(v8::String::New("<callback> must be a function")));
  }

  v8::Handle<v8::Function> callback = v8::Handle<v8::Function>::Cast(argv[1]);

  // extract the options
  v8::Handle<v8::Object> options;

  if (argv.Length() == 3 && argv[2]->IsObject()) {
    options = argv[2]->ToObject();
  }
  else {
    options = v8::Object::New();
  }

  // store an action with the given name
  TRI_CreateActionVocBase(category, name, callback, options);

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new action
///
/// @FUN{defineAction(@FA{name}, @FA{callback})}
///
/// Defines an action named @FA{name}, which might contain slashes. The action
/// can be accessed using the path @c /_action/name. The callback function
/// is executed as soon as a request is received.
///
/// @FA{callback}(@FA{request}, @FA{response})
///
/// The request arguments contains a description of the request. A request
/// parameter @c foo is accessible as @c request.foo.
///
/// The callback must define the @FA{response}.
///
/// - @FA{response}.responseCode: the response code
/// - @FA{response}.contentType: the content type of the response
/// - @FA{response}.body: the body of the response
///
/// @FUN{defineAction(@FA{request}, @FA{response}, @FA{options})}
///
/// Normally the paramaters are passed to the callback as strings. You can
/// use the @FA{options}, to force a converstion of the parameter to
///
/// - @c "collection"
/// - @c "collection-identifier"
/// - @c "number"
/// - @c "string"
///
/// @verbinclude action1
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DefineAction (v8::Arguments const& argv) {
  return DefineActionHelper(argv, TRI_ACT_CATEGORY_USER);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new system action
///
/// @FUN{defineSystemAction(@FA{name}, @FA{callback})}
///
/// @FUN{defineSystemAction(@FA{request}, @FA{response}, @FA{options})}
///
/// This functions works like @FN{defineAction} but defines a system action
/// instead of a user action. A system action is only available in the
/// administration interface and the access path is @c /_action/name.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DefineSystemAction (v8::Arguments const& argv) {
  return DefineActionHelper(argv, TRI_ACT_CATEGORY_SYSTEM);
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

void TRI_CreateActionVocBase (TRI_action_category_e category,
                              string const& name,
                              v8::Handle<v8::Function> callback,
                              v8::Handle<v8::Object> options) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // parse the options
  action_options_t* ao = ParseActionOptions(v8g, options);

  // lock the actions map
  WRITE_LOCKER(v8g->ActionsLock);

  // which map to use
  map<string, void*>* actions = 0;

  switch (category) {
    case TRI_ACT_CATEGORY_USER:
      actions = &(v8g->Actions);
      break;

    case TRI_ACT_CATEGORY_SYSTEM:
      actions = &(v8g->SystemActions);
      break;
  }

  // check if we already know the action
  map<string, void*>::iterator i = actions->find(name);

  if (i != actions->end()) {
    action_t* cb = (action_t*) i->second;

    cb->_callback.Dispose();

    delete cb->_options;
    delete cb;
  }

  // create a new action info
  action_t* cb = new action_t;

  cb->_name = name;
  cb->_callback = v8::Persistent<v8::Function>::New(callback);
  cb->_options = ao;

  LOG_DEBUG("created action '%s' for category %d", name.c_str(), (int) category);

  (*actions)[name] = (void*) cb;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

HttpResponse* TRI_ExecuteActionVocBase (TRI_vocbase_t* vocbase,
                                        TRI_action_category_e category,
                                        string const& name,
                                        HttpRequest* request) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  READ_LOCKER(v8g->ActionsLock);

  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  // which map to use
  map<string, void*>* actions = 0;

  switch (category) {
    case TRI_ACT_CATEGORY_USER:
      actions = &(v8g->Actions);
      break;

    case TRI_ACT_CATEGORY_SYSTEM:
      actions = &(v8g->SystemActions);
      break;
  }

  // check if we know a callback
  map<string, void*>::iterator i = actions->find(name);

  if (i == actions->end()) {
    LOG_DEBUG("unknown action '%s' for category %d", name.c_str(), (int) category);
    return new HttpResponse(HttpResponse::NOT_FOUND);
  }

  action_t* cb = (action_t*) i->second;

  // that up the request
  v8::Handle<v8::Object> req = v8::Object::New();

  map<string, string> values = request->values();

  for (map<string, string>::iterator i = values.begin();  i != values.end();  ++i) {
    string const& k = i->first;
    string const& v = i->second;

    map<string, action_parameter_t*>::iterator p = cb->_options->_parameters.find(k);

    if (p == cb->_options->_parameters.end()) {
      req->Set(v8::Handle<v8::String>(v8::String::New(k.c_str())),
               v8::Handle<v8::String>(v8::String::New(v.c_str())));
    }
    else {
      action_parameter_t* ap = p->second;

      switch (ap->_type) {
        case TRI_ACT_COLLECTION: {
          TRI_vocbase_col_t const* collection = TRI_FindCollectionByNameVocBase(vocbase, v.c_str(), false);

          if (collection == 0) {
            return new HttpResponse(HttpResponse::NOT_FOUND);
          }

          req->Set(v8::Handle<v8::String>(v8::String::New(k.c_str())),
                   TRI_WrapCollection(collection));

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

          req->Set(v8::Handle<v8::String>(v8::String::New(k.c_str())),
                   TRI_WrapCollection(collection));

          break;
        }

        case TRI_ACT_NUMBER:
          req->Set(v8::Handle<v8::String>(v8::String::New(k.c_str())),
                   v8::Handle<v8::Number>(v8::Number::New(TRI_DoubleString(v.c_str()))));
          break;

        case TRI_ACT_STRING: {
          req->Set(v8::Handle<v8::String>(v8::String::New(k.c_str())),
                   v8::Handle<v8::String>(v8::String::New(v.c_str())));
          break;
        }
      }
    }
  }

  // execute the callback
  v8::Handle<v8::Object> res = v8::Object::New();
  v8::Handle<v8::Value> args[2] = { req, res };

  cb->_callback->Call(cb->_callback, 2, args);

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

    if (res->Has(v8g->ContentType)) {
      response->setContentType(TRI_ObjectToString(res->Get(v8g->ContentType)));
    }

    if (res->Has(v8g->BodyKey)) {
      response->body().appendText(TRI_ObjectToString(res->Get(v8g->BodyKey)));
    }

    return response;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 actions function inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Actions (v8::Handle<v8::Context> context) {
  v8::HandleScope scope;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  if (v8g == 0) {
    v8g = new TRI_v8_global_t;
    isolate->SetData(v8g);
  }

  // .............................................................................
  // create the global functions
  // .............................................................................

  context->Global()->Set(v8::String::New("defineAction"),
                         v8::FunctionTemplate::New(JS_DefineAction)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("defineSystemAction"),
                         v8::FunctionTemplate::New(JS_DefineSystemAction)->GetFunction(),
                         v8::ReadOnly);

  // .............................................................................
  // keys
  // .............................................................................

  v8g->BodyKey = v8::Persistent<v8::String>::New(v8::String::New("body"));
  v8g->ContentType = v8::Persistent<v8::String>::New(v8::String::New("contentType"));
  v8g->ParametersKey = v8::Persistent<v8::String>::New(v8::String::New("parameters"));
  v8g->ResponseCodeKey = v8::Persistent<v8::String>::New(v8::String::New("responseCode"));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
