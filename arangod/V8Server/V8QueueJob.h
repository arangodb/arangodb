////////////////////////////////////////////////////////////////////////////////
/// @brief V8 queue job
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_V8SERVER_V8_QUEUE_JOB_H
#define ARANGODB_V8SERVER_V8_QUEUE_JOB_H 1

#include "Dispatcher/Job.h"
#include "Basics/json.h"

struct TRI_vocbase_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                       class V8Job
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {
    class ApplicationV8;

    class V8QueueJob : public rest::Job {
      private:
        V8QueueJob (V8QueueJob const&) = delete;
        V8QueueJob& operator= (V8QueueJob const&) = delete;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new V8 queue job
////////////////////////////////////////////////////////////////////////////////

        V8QueueJob (const std::string& queue,
                    struct TRI_vocbase_s*,
                    ApplicationV8*,
                    const TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a V8 job
////////////////////////////////////////////////////////////////////////////////
        
        ~V8QueueJob ();

// -----------------------------------------------------------------------------
// --SECTION--                                                       Job methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        JobType type () const override;

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        std::string const& queue () const override;

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        status_t work () override;

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool cancel (bool running) override;

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

        void handleError (basics::Exception const& ex);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief queue name
////////////////////////////////////////////////////////////////////////////////

        std::string _queue;

////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase
////////////////////////////////////////////////////////////////////////////////

        struct TRI_vocbase_s* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 dealer
////////////////////////////////////////////////////////////////////////////////

        ApplicationV8* _v8Dealer;

////////////////////////////////////////////////////////////////////////////////
/// @brief paramaters
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* _parameters;

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel flag
////////////////////////////////////////////////////////////////////////////////

        volatile sig_atomic_t _canceled;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
