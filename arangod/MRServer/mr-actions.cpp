////////////////////////////////////////////////////////////////////////////////
/// @brief mruby actions
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "mr-actions.h"

#include "Actions/actions.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "BasicsC/conversions.h"
#include "BasicsC/tri-strings.h"
#include "Logger/Logger.h"
#include "MRServer/ApplicationMR.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "VocBase/vocbase.h"

#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/hash.h"
#include "mruby/string.h"
#include "mruby/variable.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static HttpResponse* ExecuteActionVocbase (TRI_vocbase_t* vocbase,
                                           mrb_state* mrb,
                                           TRI_action_t const* action,
                                           mrb_value callback,
                                           HttpRequest* request);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Actions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief global MRuby dealer
////////////////////////////////////////////////////////////////////////////////

ApplicationMR* GlobalMRDealer = 0;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 class mr_action_t
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoActions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief action description for MRuby
////////////////////////////////////////////////////////////////////////////////

class mr_action_t : public TRI_action_t {
  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

    mr_action_t (set<string> const& contexts)
      : TRI_action_t(contexts) {
      _type = "RUBY";
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

    ~mr_action_t () {
      // TODO cleanup
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates callback for a context
////////////////////////////////////////////////////////////////////////////////

    void createCallback (mrb_state* mrb, mrb_value callback) {
      WRITE_LOCKER(_callbacksLock);

      _callbacks[mrb] = callback;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates callback for a context
////////////////////////////////////////////////////////////////////////////////

    HttpResponse* execute (TRI_vocbase_t* vocbase, HttpRequest* request) {
      ApplicationMR::MRContext* context = GlobalMRDealer->enterContext();
      mrb_state* mrb = context->_mrb;

      READ_LOCKER(_callbacksLock);

      map< mrb_state*, mrb_value >::iterator i = _callbacks.find(mrb);

      if (i == _callbacks.end()) {
        LOGGER_WARNING("no callback function for Ruby action '" << _url.c_str() << "'");
        return new HttpResponse(HttpResponse::NOT_FOUND);
      }

      mrb_value callback = i->second;

      HttpResponse* response = ExecuteActionVocbase(vocbase, mrb, this, callback, request);

      GlobalMRDealer->exitContext(context);

      return response;
    }

  private:

////////////////////////////////////////////////////////////////////////////////
/// @brief callback dictionary
////////////////////////////////////////////////////////////////////////////////

    map< mrb_state*, mrb_value > _callbacks;

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
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief float value
////////////////////////////////////////////////////////////////////////////////

double MR_float (mrb_state* mrb, mrb_value val) {
  switch (mrb_type(val)) {
    case MRB_TT_FIXNUM:
      return (double) mrb_fixnum(val);

    case MRB_TT_FLOAT:
      return mrb_float(val);
      break;

    default:
      return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief string  value
////////////////////////////////////////////////////////////////////////////////

char const* MR_string (mrb_state* mrb, mrb_value val) {
  return RSTRING_PTR(val);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

static HttpResponse* ExecuteActionVocbase (TRI_vocbase_t* vocbase,
                                           mrb_state* mrb,
                                           TRI_action_t const* action,
                                           mrb_value callback,
                                           HttpRequest* request) {
  MR_state_t* mrs;
  mrb_sym id;
  mrb_sym bodyId;
  mrb_value key;
  mrb_value val;

  mrs = (MR_state_t*) mrb->ud;

  // setup the request
  mrb_value req = mrb_class_new_instance(mrb, 0, 0, mrs->_arangoRequest);

  // setup the response
  mrb_value res = mrb_class_new_instance(mrb, 0, 0, mrs->_arangoResponse);

  // copy suffixes
  vector<string> const& suffix = request->suffix();
  mrb_value suffixArray = mrb_ary_new_capa(mrb, suffix.size());

  uint32_t index = 0;

  for (size_t s = action->_urlParts;  s < suffix.size();  ++s, ++index) {
    string const& str = suffix[s];
    val = mrb_str_new(mrb, str.c_str(), str.size());

    mrb_ary_set(mrb, suffixArray, index, val);
  }

  id = mrb_intern(mrb, "@suffix");
  mrb_iv_set(mrb, req, id, suffixArray);

  // copy header fields
  map<string, string> const& headers = request->headers();
  map<string, string>::const_iterator iter = headers.begin();

  mrb_value headerFields = mrb_hash_new_capa(mrb, headers.size());

  for (; iter != headers.end(); ++iter) {
    string const& f = iter->first;
    string const& s = iter->second;

    key = mrb_str_new(mrb, f.c_str(), f.size());
    val = mrb_str_new(mrb, s.c_str(), s.size());


    mrb_hash_set(mrb, headerFields, key, val);
  }

  id = mrb_intern(mrb, "@headers");
  mrb_iv_set(mrb, req, id, headerFields);

  // copy request type
  id = mrb_intern(mrb, "@request_type");
  bodyId = mrb_intern(mrb, "@body");

  switch (request->requestType()) {
    case HttpRequest::HTTP_REQUEST_POST:
      mrb_iv_set(mrb, req, id, mrb_str_new_cstr(mrb, "POST"));
      mrb_iv_set(mrb, req, bodyId, mrb_str_new(mrb, request->body(), request->bodySize()));
      break;

    case HttpRequest::HTTP_REQUEST_PUT:
      mrb_iv_set(mrb, req, id, mrb_str_new_cstr(mrb, "PUT"));
      mrb_iv_set(mrb, req, bodyId, mrb_str_new(mrb, request->body(), request->bodySize()));
      break;

    case HttpRequest::HTTP_REQUEST_DELETE:
      mrb_iv_set(mrb, req, id, mrb_str_new_cstr(mrb, "DELETE"));
      break;

    case HttpRequest::HTTP_REQUEST_HEAD:
      mrb_iv_set(mrb, req, id, mrb_str_new_cstr(mrb, "DELETE"));
      break;

    default:
      mrb_iv_set(mrb, req, id, mrb_str_new_cstr(mrb, "GET"));
      break;
  }

  // copy request parameter
  map<string, string> values = request->values();
  mrb_value parametersArray = mrb_hash_new_capa(mrb, values.size());

  for (map<string, string>::iterator i = values.begin();  i != values.end();  ++i) {
    string const& k = i->first;
    string const& v = i->second;

    map<string, TRI_action_parameter_type_e>::const_iterator p = action->_parameters.find(k);

    if (p == action->_parameters.end()) {
      key = mrb_str_new(mrb, k.c_str(), k.size());
      val = mrb_str_new(mrb, v.c_str(), v.size());

      mrb_hash_set(mrb, parametersArray, key, val);
    }
    else {
      TRI_action_parameter_type_e const& ap = p->second;

      switch (ap) {
        case TRI_ACT_COLLECTION:
        case TRI_ACT_COLLECTION_NAME:
        case TRI_ACT_COLLECTION_ID:
        case TRI_ACT_STRING: {
          key = mrb_str_new(mrb, k.c_str(), k.size());
          val = mrb_str_new(mrb, v.c_str(), v.size());

          mrb_hash_set(mrb, parametersArray, key, val);
          break;
        }

        case TRI_ACT_NUMBER: {
          key = mrb_str_new(mrb, k.c_str(), k.size());
          // TODO: val is assigned and then re-assigned. Is this intentional??
          val = mrb_str_new(mrb, v.c_str(), v.size());
          val = mrb_float_value(TRI_DoubleString(v.c_str()));

          mrb_hash_set(mrb, parametersArray, key, val);
          break;
        }
      }
    }
  }

  id = mrb_intern(mrb, "@parameters");
  mrb_iv_set(mrb, req, id, parametersArray);

  // execute the callback
  mrb_value args[2];
  args[0] = req;
  args[1] = res;

  id = mrb_intern(mrb, "service");
  mrb_funcall_argv(mrb, callback, id, 2, args);

  if (mrb->exc) {
    TRI_LogRubyException(mrb, mrb->exc);
    mrb->exc = 0;
    return new HttpResponse(HttpResponse::SERVER_ERROR);
  }

  // set status code
  id = mrb_intern(mrb, "@status");
  val = mrb_iv_get(mrb, res, id);
  HttpResponse::HttpResponseCode code = HttpResponse::OK;

  if (! mrb_nil_p(val)) {
    code = (HttpResponse::HttpResponseCode) MR_float(mrb, val);
  }

  // generate response
  HttpResponse* response = new HttpResponse(code);

  // set content type
  id = mrb_intern(mrb, "@content_type");
  val = mrb_iv_get(mrb, res, id);

  if (! mrb_nil_p(val)) {
    response->setContentType(MR_string(mrb, val));
  }

  id = mrb_intern(mrb, "@body");
  val = mrb_iv_get(mrb, res, id);

  if (! mrb_nil_p(val)) {
    response->body().appendText(MR_string(mrb, val));
  }

  return response;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    ruby functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief defines an action
////////////////////////////////////////////////////////////////////////////////

static mrb_value MR_Mount (mrb_state* mrb, mrb_value self) {
  char* s;
  size_t l;
  mrb_value cl;
  struct RClass* rcl;

  mrb_get_args(mrb, "so", &s, &l, &cl);

  // extract the mount point
  if (s == NULL) {
    return mrb_false_value();
  }

  // extract the class template
  rcl = mrb_class_ptr(cl);

  if (rcl == 0) {
    return mrb_false_value();
  }

  // create an action with the given options
  set<string> contexts;
  contexts.insert("api");
  contexts.insert("admin");

  mr_action_t* action = new mr_action_t(contexts);

  // store an action with the given name
  TRI_action_t* result = TRI_DefineActionVocBase(s, action);

  // and define the callback
  if (result != 0) {
    action = dynamic_cast<mr_action_t*>(result);

    if (action != 0) {
      mrb_value callback = mrb_class_new_instance(mrb, 0, 0, rcl);

      action->createCallback(mrb, callback);
      return mrb_false_value();
    }
    else {
      LOGGER_ERROR("cannot create callback for MRuby action");
      return mrb_true_value();
    }
  }
  else {
    LOGGER_ERROR("cannot define MRuby action");
    return mrb_false_value();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  module functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief init mruby utilities
////////////////////////////////////////////////////////////////////////////////

void TRI_InitMRActions (mrb_state* mrb, triagens::arango::ApplicationMR* applicationMR) {
  MR_state_t* mrs;
  struct RClass *rcl;
  struct RClass *arango;

  mrs = (MR_state_t*) mrb->ud;
  arango = mrb_define_module(mrb, "Arango");

  GlobalMRDealer = applicationMR;

  // .............................................................................
  // HttpServer
  // .............................................................................

  rcl = mrb_define_class_under(mrb, arango, "HttpServer", mrb->object_class);

  mrb_define_class_method(mrb, rcl, "mount", MR_Mount, ARGS_REQ(2));

  // .............................................................................
  // HttpRequest
  // .............................................................................

  // TODO: rcl is assigned and then re-assigned directly. Is this intentional?
  rcl = mrs->_arangoRequest = mrb_define_class_under(mrb, arango, "HttpRequest", mrb->object_class);

  // .............................................................................
  // HttpResponse
  // .............................................................................

  rcl = mrs->_arangoResponse = mrb_define_class_under(mrb, arango, "HttpResponse", mrb->object_class);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
