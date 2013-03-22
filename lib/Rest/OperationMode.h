////////////////////////////////////////////////////////////////////////////////
/// @brief server operation mode
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
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_REST_OPERATION_MODE_H
#define TRIAGENS_REST_OPERATION_MODE_H 1

#include "Basics/ProgramOptions.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace rest {

// -----------------------------------------------------------------------------
// --SECTION--                                               class OperationMode
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server operation modes
////////////////////////////////////////////////////////////////////////////////

    class OperationMode {
      public:

        typedef enum {
          MODE_CONSOLE,
          MODE_UNITTESTS,
          MODE_JSLINT,
          MODE_SCRIPT,
#ifdef TRI_ENABLE_MRUBY
          MODE_RUBY_CONSOLE,
#endif
          MODE_SERVER
        }
        server_operation_mode_e;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the server operation mode
////////////////////////////////////////////////////////////////////////////////

        static server_operation_mode_e determineMode (const triagens::basics::ProgramOptions& options) {
          if (options.has("console")) {
            return MODE_CONSOLE;
          }
          else if (options.has("javascript.unit-tests")) {
            return MODE_UNITTESTS;
          }
          else if (options.has("jslint")) {
            return MODE_JSLINT;
          }
          else if (options.has("javascript.script")) {
            return MODE_SCRIPT;
          }
#ifdef TRI_ENABLE_MRUBY
          else if (options.has("ruby-console")) {
            return MODE_RUBY_CONSOLE;
          }
#endif
          else {
            return MODE_SERVER;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

    };

  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
