////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log garbage collection thread
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_WAL_COLLECTOR_THREAD_H
#define TRIAGENS_WAL_COLLECTOR_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "VocBase/voc-types.h"

struct TRI_datafile_s;
struct TRI_df_marker_s;
struct TRI_server_s;

namespace triagens {
  namespace wal {

    class LogfileManager;
    class Logfile;
    
// -----------------------------------------------------------------------------
// --SECTION--                                             class CollectorThread
// -----------------------------------------------------------------------------

    class CollectorThread : public basics::Thread {

////////////////////////////////////////////////////////////////////////////////
/// @brief CollectorThread
////////////////////////////////////////////////////////////////////////////////

      private:
        CollectorThread (CollectorThread const&) = delete;
        CollectorThread& operator= (CollectorThread const&) = delete;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the collector thread
////////////////////////////////////////////////////////////////////////////////

        CollectorThread (LogfileManager*,
                         struct TRI_server_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the collector thread
////////////////////////////////////////////////////////////////////////////////

        ~CollectorThread ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   public typedefs
// -----------------------------------------------------------------------------

      public:
       
////////////////////////////////////////////////////////////////////////////////
/// @brief typedef key => document marker 
////////////////////////////////////////////////////////////////////////////////

        typedef std::unordered_map<std::string, struct TRI_df_marker_s const*> DocumentOperationsType;

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for structural operation (attributes, shapes) markers
////////////////////////////////////////////////////////////////////////////////

        typedef std::vector<struct TRI_df_marker_s const*> OperationsType;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the collector thread
////////////////////////////////////////////////////////////////////////////////

        void stop ();

////////////////////////////////////////////////////////////////////////////////
/// @brief signal the thread that there is something to do
////////////////////////////////////////////////////////////////////////////////

        void signal ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief main loop
////////////////////////////////////////////////////////////////////////////////

        void run ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief perform collection of a logfile (if any)
////////////////////////////////////////////////////////////////////////////////

        bool collectLogfiles ();

////////////////////////////////////////////////////////////////////////////////
/// @brief perform removal of a logfile (if any)
////////////////////////////////////////////////////////////////////////////////

        bool removeLogfiles ();

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to handle one marker during collection
////////////////////////////////////////////////////////////////////////////////

        static bool ScanMarker (struct TRI_df_marker_s const*,
                                void*,
                                struct TRI_datafile_s*,
                                bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief collect one logfile
////////////////////////////////////////////////////////////////////////////////

        int collect (Logfile*);

////////////////////////////////////////////////////////////////////////////////
/// @brief transfer markers into a collection
////////////////////////////////////////////////////////////////////////////////

        int transferMarkers (TRI_voc_cid_t,
                             TRI_voc_tick_t,
                             OperationsType const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the logfile manager
////////////////////////////////////////////////////////////////////////////////

        LogfileManager* _logfileManager;

////////////////////////////////////////////////////////////////////////////////
/// @brief pointer to the server
////////////////////////////////////////////////////////////////////////////////

        struct TRI_server_s* _server;

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable for the collector thread
////////////////////////////////////////////////////////////////////////////////

        basics::ConditionVariable _condition;

////////////////////////////////////////////////////////////////////////////////
/// @brief stop flag
////////////////////////////////////////////////////////////////////////////////
        
        volatile sig_atomic_t _stop;

////////////////////////////////////////////////////////////////////////////////
/// @brief wait interval for the collector thread when idle
////////////////////////////////////////////////////////////////////////////////

        static uint64_t const Interval;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
