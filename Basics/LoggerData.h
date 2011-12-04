////////////////////////////////////////////////////////////////////////////////
/// @brief logger data
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Copyright 2007-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_LOGGER_DATA_H
#define TRIAGENS_BASICS_LOGGER_DATA_H 1

#include <Basics/Common.h>

#include <Basics/logging.h>
#include <Basics/Thread.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace basics {
    namespace LoggerData {

////////////////////////////////////////////////////////////////////////////////
/// @brief the application name
////////////////////////////////////////////////////////////////////////////////

      struct ApplicationName {
        public:
          ApplicationName ();

        public:
          string name;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the facility name
////////////////////////////////////////////////////////////////////////////////

      struct Facility {
        public:
          Facility ();

        public:
          string name;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the hostname
////////////////////////////////////////////////////////////////////////////////

      struct HostName {
        public:
          HostName ();

        public:
          string name;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the message identifier, automatically generated
////////////////////////////////////////////////////////////////////////////////

      struct MessageIdentifier {
        public:
          MessageIdentifier ();

        public:
          string name;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the proc and thread identifiers, automatically generated
////////////////////////////////////////////////////////////////////////////////

      struct ProcessIdentifier {
        public:
          ProcessIdentifier ();

        public:
          Thread::pid_t process;
          Thread::tpid_t threadProcess;
          Thread::tid_t thread;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the fucntional name
////////////////////////////////////////////////////////////////////////////////

      struct Functional {
        public:
          Functional (string const& name = "");

        public:
          string name;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the peg name
////////////////////////////////////////////////////////////////////////////////

      struct Peg {
        public:
          Peg (string const& name = "");

        public:
          string name;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the task name
////////////////////////////////////////////////////////////////////////////////

      struct Task {
        public:
          Task (string const& name = "");

        public:
          string name;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief position information
////////////////////////////////////////////////////////////////////////////////

      struct Position {
        public:
          Position (string const& function = "", string const& file = "", int line = 0);

        public:
          string function;
          string file;
          int line;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the measure unit
////////////////////////////////////////////////////////////////////////////////

      enum unit_e {
        UNIT_SECONDS,
        UNIT_MILLI_SECONDS,
        UNIT_MICRO_SECONDS,
        UNIT_NANO_SECONDS,

        UNIT_BYTE,
        UNIT_KILO_BYTE,
        UNIT_MEGA_BYTE,
        UNIT_GIGA_BYTE,

        UNIT_LESS
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the measure
////////////////////////////////////////////////////////////////////////////////

      struct Measure {
        public:
          Measure (double measure = 0.0, unit_e unit = UNIT_LESS);

        public:
          double value;
          unit_e unit;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the extra attribute
////////////////////////////////////////////////////////////////////////////////

      struct Extra {
        public:
          Extra (size_t pos, string const& name);
          Extra (string const& name = "");

        public:
          static size_t const npos;

        public:
          size_t position;
          string name;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the user identifier
////////////////////////////////////////////////////////////////////////////////

      struct UserIdentifier {
        public:
          UserIdentifier (string const& name = "");

        public:
          string user;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the info block
////////////////////////////////////////////////////////////////////////////////

      struct Info {
        Info();

        static ApplicationName applicationName;
        static Facility facility;
        static HostName hostName;

        MessageIdentifier messageIdentifier;

        TRI_log_level_e level;
        TRI_log_category_e category;
        TRI_log_severity_e severity;

        Functional functional;

        Peg peg;
        Task task;
        Position position;

        Measure measure;
        vector<Extra> extras;

        UserIdentifier userIdentifier;
        ProcessIdentifier processIdentifier;

        string prefix;
      };
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
