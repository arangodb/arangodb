////////////////////////////////////////////////////////////////////////////////
/// @brief V8 job
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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "V8Job.h"

#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/ApplicationV8.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new V8 job
////////////////////////////////////////////////////////////////////////////////

V8Job::V8Job (TRI_vocbase_t* vocbase,
              ApplicationV8* v8Dealer,
              std::string const& command,
              TRI_json_t const* parameters)
  : Job("V8 Job"),
    _vocbase(vocbase),
    _v8Dealer(v8Dealer),
    _command(command),
    _parameters(parameters),
    _canceled(0) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       Job methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job::JobType V8Job::type () {
  return READ_JOB;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

const string& V8Job::queue () {
  static const string queue = "STANDARD";
  return queue;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job::status_t V8Job::work () {
  if (_canceled) {
    return status_t(JOB_DONE);
  }

  ApplicationV8::V8Context* context
    = _v8Dealer->enterContext(_vocbase, 0, true, false);

  // note: the context might be 0 in case of shut-down
  if (context == 0) {
    return status_t(JOB_DONE);
  }

  // now execute the function within this context
  {
    v8::HandleScope scope;
    
    // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
    v8::Handle<v8::Object> current = v8::Context::GetCurrent()->Global();
    v8::Local<v8::Function> ctor = v8::Local<v8::Function>::Cast(current->Get(v8::String::New("Function")));
    
    // Invoke Function constructor to create function with the given body and no arguments
    v8::Handle<v8::Value> args[2] = { v8::String::New("params"), v8::String::New(_command.c_str(), (int) _command.size()) };
    v8::Local<v8::Object> function = ctor->NewInstance(2, args);

    v8::Handle<v8::Function> action = v8::Local<v8::Function>::Cast(function);
  
    if (action.IsEmpty()) {
      _v8Dealer->exitContext(context);
      // TODO: adjust exit code??
      return status_t(JOB_DONE);
    }
 
    v8::Handle<v8::Value> fArgs;
    if (_parameters != 0) {
      fArgs = TRI_ObjectJson(_parameters);
    }
    else {
      fArgs = v8::Undefined();
    }
    
    // call the function  
    v8::TryCatch tryCatch;
    action->Call(current, 1, &fArgs);
      
    if (tryCatch.HasCaught()) {
      if (tryCatch.CanContinue()) {
        TRI_LogV8Exception(&tryCatch);
      }
      else {
        LOG_WARNING("caught non-printable exception in periodic task");
      }
    }
  }

  _v8Dealer->exitContext(context);

  return status_t(JOB_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool V8Job::cancel (bool running) {
  if (running) {
    _canceled = 1;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void V8Job::cleanup () {
  delete this;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool V8Job::beginShutdown () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void V8Job::handleError (TriagensError const& ex) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
