////////////////////////////////////////////////////////////////////////////////
/// @brief mruby actions
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "mr-actions.h"

#include "Actions/actions.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "BasicsC/strings.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "VocBase/vocbase.h"

#include "mruby/class.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

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

    mr_action_t ()  {
      _type = "RUBY";
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

    ~mr_action_t () {
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates callback for a context
////////////////////////////////////////////////////////////////////////////////

    void createCallback (void* context, void* callback) {
      WRITE_LOCKER(_callbacksLock);

      _callbacks[context] = * (mrb_value*) callback;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates callback for a context
////////////////////////////////////////////////////////////////////////////////

    HttpResponse* execute (TRI_vocbase_t* vocbase, void* context, HttpRequest* request) {
      mrb_value callback;

      LOGGER_FATAL << "STATE EXEC: " << context;

      {
        READ_LOCKER(_callbacksLock);

        map< void*, mrb_value >::iterator i = _callbacks.find(context);

        if (i == _callbacks.end()) {
          LOGGER_WARNING << "no callback function for Ruby action '" << _url.c_str() << "'";
          return new HttpResponse(HttpResponse::NOT_FOUND);
        }

        callback = i->second;
      }

      return new HttpResponse(HttpResponse::SERVER_ERROR);
    }

  private:

////////////////////////////////////////////////////////////////////////////////
/// @brief callback dictionary
////////////////////////////////////////////////////////////////////////////////

    map< void*, mrb_value > _callbacks;

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

  LOGGER_FATAL << "STATE MOUNT: " << (void*) mrb;

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
  mr_action_t* action = new mr_action_t;

  // store an action with the given name
  TRI_action_t* result = TRI_DefineActionVocBase(s, action);

  // and define the callback
  if (result != 0) {
    mrb_value callback = mrb_class_new_instance(mrb, 0, 0, rcl);

    result->createCallback((void*) mrb, (void*) &callback);

    return mrb_true_value();
  }
  else {
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

void TRI_InitMRActions (MR_state_t* mrs) {
  struct RClass *rcl;
  struct RClass *arango;

  arango = mrb_define_module(&mrs->_mrb, "Arango");

  // .............................................................................
  // HttpServer
  // .............................................................................

  rcl = mrb_define_class_under(&mrs->_mrb, arango, "HttpServer", mrs->_mrb.object_class);
  
  mrb_define_class_method(&mrs->_mrb, rcl, "mount", MR_Mount, ARGS_REQ(2));

  // .............................................................................
  // HttpRequest
  // .............................................................................

  rcl = mrs->_arangoRequest = mrb_define_class_under(&mrs->_mrb, arango, "HttpRequest", mrs->_mrb.object_class);

  // .............................................................................
  // HttpResponse
  // .............................................................................

  rcl = mrs->_arangoResponse = mrb_define_class_under(&mrs->_mrb, arango, "HttpResponse", mrs->_mrb.object_class);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
