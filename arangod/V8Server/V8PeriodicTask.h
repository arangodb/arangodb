////////////////////////////////////////////////////////////////////////////////
/// @brief periodic V8 task
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

#ifndef TRIAGENS_V8SERVER_V8PERIODIC_TASK_H
#define TRIAGENS_V8SERVER_V8PERIODIC_TASK_H 1

#include "Scheduler/PeriodicTask.h"

#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              class V8PeriodicTask
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class Dispatcher;
    class Scheduler;
  }

  namespace arango {
    class ApplicationV8;

    class V8PeriodicTask : public rest::PeriodicTask {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        V8PeriodicTask (TRI_vocbase_t*,
                        ApplicationV8*,
                        rest::Scheduler*,
                        rest::Dispatcher*,
                        double _offset,
                        double _period,
                        const string& module,
                        const string& func,
                        const string& parameter);

// -----------------------------------------------------------------------------
// --SECTION--                                              PeriodicTask methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief handles the next tick
////////////////////////////////////////////////////////////////////////////////

        bool handlePeriod ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief system vocbase
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 dealer
////////////////////////////////////////////////////////////////////////////////

        arango::ApplicationV8* _v8Dealer;

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher
////////////////////////////////////////////////////////////////////////////////

        rest::Dispatcher* _dispatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief module name
////////////////////////////////////////////////////////////////////////////////

        const std::string _module;

////////////////////////////////////////////////////////////////////////////////
/// @brief function name
////////////////////////////////////////////////////////////////////////////////

        const std::string _func;

////////////////////////////////////////////////////////////////////////////////
/// @brief paramater string
////////////////////////////////////////////////////////////////////////////////

        const std::string _parameter;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
