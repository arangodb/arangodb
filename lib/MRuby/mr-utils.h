////////////////////////////////////////////////////////////////////////////////
/// @brief mruby utilities
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_MRUBY_MR__UTILS_H
#define ARANGODB_MRUBY_MR__UTILS_H 1

#include "Basics/Common.h"

#include "Basics/json.h"

#include "mruby.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief mruby state info
////////////////////////////////////////////////////////////////////////////////

typedef struct MR_state_s {
  struct RClass* _arangoError;
  struct RClass* _arangoRequest;
  struct RClass* _arangoResponse;

  mrb_value _errorSym;
  mrb_value _codeSym;
  mrb_value _errorNumSym;
  mrb_value _errorMessageSym;
}
MR_state_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t into a V8 object
////////////////////////////////////////////////////////////////////////////////

mrb_value MR_ObjectJson (mrb_state* mrb, TRI_json_t const* json);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a new context
////////////////////////////////////////////////////////////////////////////////

mrb_state* MR_OpenShell (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes context
////////////////////////////////////////////////////////////////////////////////

void MR_CloseShell (mrb_state*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a ArangoError
////////////////////////////////////////////////////////////////////////////////

mrb_value MR_ArangoError (mrb_state* mrb, int errNum, char const* errMessage);

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an exception and stacktrace
////////////////////////////////////////////////////////////////////////////////

void TRI_LogRubyException (mrb_state*, struct RObject*);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a file in the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteRubyFile (mrb_state*, char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes all files from a directory in the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteRubyDirectory (mrb_state*, char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a string within a V8 context, optionally print the result
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteRubyString (mrb_state*,
                            char const* script,
                            char const* name,
                            bool printResult,
                            mrb_value* result);

// -----------------------------------------------------------------------------
// --SECTION--                                                  module functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief init utilities
////////////////////////////////////////////////////////////////////////////////

void TRI_InitMRUtils (mrb_state* mrb);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
