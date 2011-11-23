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

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Logging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a prefix
////////////////////////////////////////////////////////////////////////////////

LoggerInfo& LoggerInfo::operator<< (string const& text) {
  info.prefix += text;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a peg
////////////////////////////////////////////////////////////////////////////////

LoggerInfo& LoggerInfo::operator<< (LoggerData::Peg const& peg) {
  info.peg = peg;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a task
////////////////////////////////////////////////////////////////////////////////

LoggerInfo& LoggerInfo::operator<< (LoggerData::Task const& task) {
  info.task = task;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches an extra
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief catches an user identifier
////////////////////////////////////////////////////////////////////////////////

LoggerInfo& LoggerInfo::operator<< (LoggerData::UserIdentifier const& userIdentifier) {
  info.userIdentifier = userIdentifier;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief catches a position
////////////////////////////////////////////////////////////////////////////////

LoggerInfo& LoggerInfo::operator<< (LoggerData::Position const& position) {
  info.position = position;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:

