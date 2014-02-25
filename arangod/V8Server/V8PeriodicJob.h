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

#ifndef TRIAGENS_V8SERVER_V8PERIODIC_JOB_H
#define TRIAGENS_V8SERVER_V8PERIODIC_JOB_H 1

#include "Dispatcher/Job.h"
#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                               class V8PeriodicJob
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {
    class ApplicationV8;

    class V8PeriodicJob : public rest::Job {
      private:
        V8PeriodicJob (V8PeriodicJob const&);
        V8PeriodicJob& operator= (V8PeriodicJob const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new V8 job
////////////////////////////////////////////////////////////////////////////////

        V8PeriodicJob (TRI_vocbase_t*,
                       ApplicationV8*,
                       const string& module,
                       const string& func,
                       const string& parameter);

// -----------------------------------------------------------------------------
// --SECTION--                                                       Job methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        JobType type ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        const string& queue ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        status_t work ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void cleanup ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool beginShutdown ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void handleError (basics::TriagensError const& ex);

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

        ApplicationV8* _v8Dealer;

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
