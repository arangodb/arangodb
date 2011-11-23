////////////////////////////////////////////////////////////////////////////////
/// @brief logger stream
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

#include "LoggerStream.h"

#include <Basics/Logger.h>

using namespace triagens::basics;

// -----------------------------------------------------------------------------
// helper
// -----------------------------------------------------------------------------

namespace {
  void computeInfo (LoggerData::Info& info) {
    if (info.severity == TRI_LOG_SEVERITY_UNKNOWN) {
      switch (info.category) {
        case TRI_LOG_CATEGORY_FATAL:
          info.severity = TRI_LOG_SEVERITY_EXCEPTION;
          break;
        case TRI_LOG_CATEGORY_ERROR:
          info.severity = TRI_LOG_SEVERITY_EXCEPTION;
          break;
        case TRI_LOG_CATEGORY_WARNING:
          info.severity = TRI_LOG_SEVERITY_EXCEPTION;
          break;

        case TRI_LOG_CATEGORY_REQUEST_IN_START:
          info.severity = TRI_LOG_SEVERITY_TECHNICAL;
          break;
        case TRI_LOG_CATEGORY_REQUEST_IN_END:
          info.severity = TRI_LOG_SEVERITY_TECHNICAL;
          break;
        case TRI_LOG_CATEGORY_REQUEST_OUT_START:
          info.severity = TRI_LOG_SEVERITY_TECHNICAL;
          break;
        case TRI_LOG_CATEGORY_REQUEST_OUT_END:
          info.severity = TRI_LOG_SEVERITY_TECHNICAL;
          break;
        case TRI_LOG_CATEGORY_HEARTBEAT:
          info.severity = TRI_LOG_SEVERITY_TECHNICAL;
          break;

        case TRI_LOG_CATEGORY_MODULE_IN_START:
          info.severity = TRI_LOG_SEVERITY_DEVELOPMENT;
          break;
        case TRI_LOG_CATEGORY_MODULE_IN_END:
          info.severity = TRI_LOG_SEVERITY_DEVELOPMENT;
          break;
        case TRI_LOG_CATEGORY_FUNCTION_IN_START:
          info.severity = TRI_LOG_SEVERITY_DEVELOPMENT;
          break;
        case TRI_LOG_CATEGORY_FUNCTION_IN_END:
          info.severity = TRI_LOG_SEVERITY_DEVELOPMENT;
          break;
        case TRI_LOG_CATEGORY_STEP:
          info.severity = TRI_LOG_SEVERITY_DEVELOPMENT;
          break;
        case TRI_LOG_CATEGORY_LOOP:
          info.severity = TRI_LOG_SEVERITY_DEVELOPMENT;
          break;
        case TRI_LOG_CATEGORY_HEARTPULSE:
          info.severity = TRI_LOG_SEVERITY_DEVELOPMENT;
          break;

        default:
          info.severity = TRI_LOG_SEVERITY_UNKNOWN;
      }
    }
  }
}

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // LoggerStream
    // -----------------------------------------------------------------------------

    LoggerStream::LoggerStream () :
      stream(new stringstream()),
      info() {
    }



    LoggerStream::LoggerStream (LoggerData::Info const& info) :
      stream(new stringstream()), info(info) {
    }



    LoggerStream::LoggerStream (LoggerStream const& copy) :
      stream(new stringstream()), info(copy.info) {
      stream->str(copy.stream->str());
    }



    LoggerStream::~LoggerStream () {
      computeInfo(info);

      if (stream != 0) {
        Logger::output(static_cast<stringstream*> (stream)->str(), info);

        delete stream;
      }
    }



    LoggerStream& LoggerStream::operator<< (ostream& (*fptr) (ostream&)) {
      if (stream != 0) {
        *stream << fptr;
      }

      return *this;
    }



    LoggerStream& LoggerStream::operator<< (TRI_log_level_e level) {
      info.level = level;

      return *this;
    }



    LoggerStream& LoggerStream::operator<< (TRI_log_category_e category) {
      info.category = category;

      return *this;
    }



    LoggerStream& LoggerStream::operator<< (TRI_log_severity_e severity) {
      info.severity = severity;

      return *this;
    }



    LoggerStream& LoggerStream::operator<< (LoggerData::Functional const& functional) {
      info.functional = functional;

      return *this;
    }



    LoggerStream& LoggerStream::operator<< (LoggerData::MessageIdentifier const& messageIdentifier) {
      info.messageIdentifier = messageIdentifier;

      return *this;
    }



    LoggerStream& LoggerStream::operator<< (LoggerData::Peg const& peg) {
      info.peg = peg;

      return *this;
    }



    LoggerStream& LoggerStream::operator<< (LoggerData::Task const& task) {
      info.task = task;

      return *this;
    }



    LoggerStream& LoggerStream::operator<< (LoggerData::Position const& position) {
      info.position = position;

      return *this;
    }



    LoggerStream& LoggerStream::operator<< (LoggerData::Measure const& measure) {
      info.measure = measure;

      return *this;
    }



    LoggerStream& LoggerStream::operator<< (LoggerData::Extra const& extra) {
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



    LoggerStream& LoggerStream::operator<< (LoggerData::UserIdentifier const& userIdentifier) {
      info.userIdentifier = userIdentifier;

      return *this;
    }
  }
}
