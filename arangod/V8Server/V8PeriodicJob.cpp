////////////////////////////////////////////////////////////////////////////////
/// @brief periodic V8 job
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

#include "V8PeriodicJob.h"

#include "Basics/StringUtils.h"
#include "BasicsC/logging.h"
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

V8PeriodicJob::V8PeriodicJob (TRI_vocbase_t* vocbase,
                              ApplicationV8* v8Dealer,
                              const string& module,
                              const string& func,
                              const string& parameter)
  : Job("V8 Periodic Job"),
    _vocbase(vocbase),
    _v8Dealer(v8Dealer),
    _module(module),
    _func(func),
    _parameter(parameter) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       Job methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job::JobType V8PeriodicJob::type () {
  return READ_JOB;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

const string& V8PeriodicJob::queue () {
  static const string queue = "STANDARD";
  return queue;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job::status_e V8PeriodicJob::work () {
  ApplicationV8::V8Context* context
    = _v8Dealer->enterContext(_vocbase, 0, true, false);

  // note: the context might be 0 in case of shut-down
  if (context == 0) {
    return JOB_DONE;
  }

  // now execute the function within this context
  {
    v8::HandleScope scope;

    string module = StringUtils::escape(_module, "\"");
    string func = StringUtils::escape(_func, "\"");
    string parameter = StringUtils::escape(_parameter, "\"");

    string command = "(require(\"" + module + "\")[\"" + func + "\"])(\"" + parameter + "\")";

    TRI_ExecuteJavaScriptString(context->_context,
                                v8::String::New(command.c_str()),
                                v8::String::New("periodic function"),
                                true);
  }

  _v8Dealer->exitContext(context);

  return JOB_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void V8PeriodicJob::cleanup () {
  delete this;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool V8PeriodicJob::beginShutdown () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void V8PeriodicJob::handleError (triagens::basics::TriagensError const& ex) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
