////////////////////////////////////////////////////////////////////////////////
/// @brief logger stream
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2007-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "LoggerStream.h"

#include "Logger/Logger.h"

using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief update severity from category
////////////////////////////////////////////////////////////////////////////////

static void computeInfo (LoggerData::Info& info) {
  if (info._severity == TRI_LOG_SEVERITY_UNKNOWN) {
    switch (info._category) {
      case TRI_LOG_CATEGORY_FATAL:
        info._severity = TRI_LOG_SEVERITY_EXCEPTION;
        break;
      case TRI_LOG_CATEGORY_ERROR:
        info._severity = TRI_LOG_SEVERITY_EXCEPTION;
        break;
      case TRI_LOG_CATEGORY_WARNING:
        info._severity = TRI_LOG_SEVERITY_EXCEPTION;
        break;

      case TRI_LOG_CATEGORY_REQUEST_IN_START:
        info._severity = TRI_LOG_SEVERITY_TECHNICAL;
        break;
      case TRI_LOG_CATEGORY_REQUEST_IN_END:
        info._severity = TRI_LOG_SEVERITY_TECHNICAL;
        break;
      case TRI_LOG_CATEGORY_REQUEST_OUT_START:
        info._severity = TRI_LOG_SEVERITY_TECHNICAL;
        break;
      case TRI_LOG_CATEGORY_REQUEST_OUT_END:
        info._severity = TRI_LOG_SEVERITY_TECHNICAL;
        break;
      case TRI_LOG_CATEGORY_HEARTBEAT:
        info._severity = TRI_LOG_SEVERITY_TECHNICAL;
        break;

      case TRI_LOG_CATEGORY_MODULE_IN_START:
        info._severity = TRI_LOG_SEVERITY_DEVELOPMENT;
        break;
      case TRI_LOG_CATEGORY_MODULE_IN_END:
        info._severity = TRI_LOG_SEVERITY_DEVELOPMENT;
        break;
      case TRI_LOG_CATEGORY_FUNCTION_IN_START:
        info._severity = TRI_LOG_SEVERITY_DEVELOPMENT;
        break;
      case TRI_LOG_CATEGORY_FUNCTION_IN_END:
        info._severity = TRI_LOG_SEVERITY_DEVELOPMENT;
        break;
      case TRI_LOG_CATEGORY_STEP:
        info._severity = TRI_LOG_SEVERITY_DEVELOPMENT;
        break;
      case TRI_LOG_CATEGORY_LOOP:
        info._severity = TRI_LOG_SEVERITY_DEVELOPMENT;
        break;
      case TRI_LOG_CATEGORY_HEARTPULSE:
        info._severity = TRI_LOG_SEVERITY_DEVELOPMENT;
        break;

      default:
        info._severity = TRI_LOG_SEVERITY_UNKNOWN;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  class LoggerInfo
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new logger stream
////////////////////////////////////////////////////////////////////////////////

LoggerStream::LoggerStream () :
  _stream(new stringstream()),  _info() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new logger stream with given info
////////////////////////////////////////////////////////////////////////////////

LoggerStream::LoggerStream (LoggerData::Info const& info) :
  _stream(new stringstream()), _info(info) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy constructor
////////////////////////////////////////////////////////////////////////////////

LoggerStream::LoggerStream (LoggerStream const& copy) :
  _stream(copy._stream), _info(copy._info) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a logger stream
////////////////////////////////////////////////////////////////////////////////

LoggerStream::~LoggerStream () {
  computeInfo(_info);

  if (_stream != 0) {
    Logger::output(static_cast<stringstream*> (_stream)->str(), _info);
    delete _stream;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a special stream operator
////////////////////////////////////////////////////////////////////////////////

LoggerStream& LoggerStream::operator<< (std::ostream& (*fptr) (std::ostream&)) {
  if (_stream != 0) {
    *_stream << fptr;
  }

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a log level
////////////////////////////////////////////////////////////////////////////////

LoggerStream& LoggerStream::operator<< (TRI_log_level_e level) {
  _info._level = level;
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a category
////////////////////////////////////////////////////////////////////////////////

LoggerStream& LoggerStream::operator<< (TRI_log_category_e category) {
  _info._category = category;
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a severity
////////////////////////////////////////////////////////////////////////////////

LoggerStream& LoggerStream::operator<< (TRI_log_severity_e severity) {
  _info._severity = severity;
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a functional category
////////////////////////////////////////////////////////////////////////////////

LoggerStream& LoggerStream::operator<< (LoggerData::Functional const& functional) {
  _info._functional = functional;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a message identifier
////////////////////////////////////////////////////////////////////////////////

LoggerStream& LoggerStream::operator<< (LoggerData::MessageIdentifier const& messageIdentifier) {
  _info._messageIdentifier = messageIdentifier;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a peg
////////////////////////////////////////////////////////////////////////////////

LoggerStream& LoggerStream::operator<< (LoggerData::Peg const& peg) {
  _info._peg = peg;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a task
////////////////////////////////////////////////////////////////////////////////

LoggerStream& LoggerStream::operator<< (LoggerData::Task const& task) {
  _info._task = task;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a position
////////////////////////////////////////////////////////////////////////////////

LoggerStream& LoggerStream::operator<< (LoggerData::Position const& position) {
  _info._position = position;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a measure
////////////////////////////////////////////////////////////////////////////////

LoggerStream& LoggerStream::operator<< (LoggerData::Measure const& measure) {
  _info._measure = measure;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches an extra
////////////////////////////////////////////////////////////////////////////////

LoggerStream& LoggerStream::operator<< (LoggerData::Extra const& extra) {
  if (extra._position == LoggerData::Extra::npos) {
    _info._extras.push_back(extra);

    size_t pos = _info._extras.size() - 1;
    _info._extras[pos]._position = pos;
  }
  else {
    if (_info._extras.size() <= extra._position) {
      _info._extras.resize(extra._position + 1);
    }

    _info._extras[extra._position] = extra;
  }

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches an user identifier
////////////////////////////////////////////////////////////////////////////////

LoggerStream& LoggerStream::operator<< (LoggerData::UserIdentifier const& userIdentifier) {
  _info._userIdentifier = userIdentifier;

  return *this;
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
