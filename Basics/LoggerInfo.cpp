////////////////////////////////////////////////////////////////////////////////
/// @brief logger info
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

#include "LoggerInfo.h"

using namespace triagens::basics;
using namespace std;

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // ApplicationName
    // -----------------------------------------------------------------------------

    LoggerData::ApplicationName LoggerData::Info::applicationName;

    // -----------------------------------------------------------------------------
    // Facility
    // -----------------------------------------------------------------------------

    LoggerData::Facility LoggerData::Info::facility;

    // -----------------------------------------------------------------------------
    // HostName
    // -----------------------------------------------------------------------------

    LoggerData::HostName LoggerData::Info::hostName;

    // -----------------------------------------------------------------------------
    // ProcessIdentifier
    // -----------------------------------------------------------------------------

    LoggerData::ProcessIdentifier::ProcessIdentifier ()
      : process(Thread::currentProcessId()),
        threadProcess(Thread::currentThreadProcessId()),
        thread(Thread::currentThreadId()) {
    }

    // -----------------------------------------------------------------------------
    // Functional (log level)
    // -----------------------------------------------------------------------------

    LoggerData::Functional::Functional (string const& name)
      : name(name) {
    }

    // -----------------------------------------------------------------------------
    // Peg (hierarchy)
    // -----------------------------------------------------------------------------

    LoggerData::Peg::Peg (string const& name)
      : name(name) {
    }

    // -----------------------------------------------------------------------------
    // Task (hierarchy)
    // -----------------------------------------------------------------------------

    LoggerData::Task::Task (string const& name)
      : name(name) {
    }

    // -----------------------------------------------------------------------------
    // Position (hierarchy)
    // -----------------------------------------------------------------------------

    LoggerData::Position::Position (string const& function, string const& file, int line)
      : function(function), file(file), line(line) {
    }

    // -----------------------------------------------------------------------------
    // Measure
    // -----------------------------------------------------------------------------

    LoggerData::Measure::Measure (double measure, unit_e unit)
      : value(measure), unit(unit) {
    }

    // -----------------------------------------------------------------------------
    // Extra
    // -----------------------------------------------------------------------------

    size_t const LoggerData::Extra::npos = (size_t) -1;



    LoggerData::Extra::Extra (size_t pos, string const& name)
      : position(pos), name(name) {
    }



    LoggerData::Extra::Extra (string const& name)
      : position(npos), name(name) {
    }

    // -----------------------------------------------------------------------------
    // UserIdentifier
    // -----------------------------------------------------------------------------

    LoggerData::UserIdentifier::UserIdentifier (string const& name)
      : user(name) {
    }

    // -----------------------------------------------------------------------------
    // Info
    // -----------------------------------------------------------------------------

    LoggerData::Info::Info ()
      : level(TRI_LOG_LEVEL_FATAL),
        category(TRI_LOG_CATEGORY_WARNING),
        severity(TRI_LOG_SEVERITY_UNKNOWN) {
    }

    // -----------------------------------------------------------------------------
    // LoggerInfo
    // -----------------------------------------------------------------------------

    LoggerInfo& LoggerInfo::operator<< (string const& text) {
      info.prefix += text;

      return *this;
    }



    LoggerInfo& LoggerInfo::operator<< (LoggerData::Peg const& peg) {
      info.peg = peg;

      return *this;
    }



    LoggerInfo& LoggerInfo::operator<< (LoggerData::Task const& task) {
      info.task = task;

      return *this;
    }



    LoggerInfo& LoggerInfo::operator<< (LoggerData::Extra const& extra) {
      if (extra.position == LoggerData::Extra::npos) {
        info.extras.push_back(extra);

        size_t pos = info.extras.size() - 1;
        info.extras[pos].position = pos;
      }
      else {
        if (info.extras.size() <= extra.position) {
          info.extras.resize(extra.position + 1);
        }

        info.extras[extra.position] = extra;
      }

      return *this;
    }



    LoggerInfo& LoggerInfo::operator<< (LoggerData::UserIdentifier const& userIdentifier) {
      info.userIdentifier = userIdentifier;

      return *this;
    }



    LoggerInfo& LoggerInfo::operator<< (LoggerData::Position const& position) {
      info.position = position;

      return *this;
    }
  }
}


