////////////////////////////////////////////////////////////////////////////////
/// @brief statistics handler
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
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "StatisticsBaseHandler.h"

#include "Basics/StringUtils.h"
#include "Rest/HttpRequest.h"

using namespace triagens::arango;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new handler
////////////////////////////////////////////////////////////////////////////////

StatisticsBaseHandler::StatisticsBaseHandler (triagens::rest::HttpRequest* request)
  : RestBaseHandler(request) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool StatisticsBaseHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Handler::status_e StatisticsBaseHandler::execute () {
  TRI_statistics_granularity_e granularity = TRI_STATISTICS_MINUTES;

  int length = -1;

  // .............................................................................
  // extract the granularity to show
  // .............................................................................

  bool found;
  string gran = StringUtils::tolower(_request->value("granularity", found));

  if (found) {
    if (gran == "minute" || gran == "minutes") {
      granularity = TRI_STATISTICS_MINUTES;
    }
    else if (gran == "hour" || gran == "hours") {
      granularity = TRI_STATISTICS_HOURS;
    }
    else if (gran == "day" || gran == "days") {
      granularity = TRI_STATISTICS_DAYS;
    }
  }
  else {
    granularity = TRI_STATISTICS_MINUTES;
  }

  // .............................................................................
  // extract the length
  // .............................................................................

  string l = StringUtils::tolower(_request->value("length", found));

  if (found) {
    if (l == "current") {
      length = 0;
    }
    else if (l == "all" || l == "*") {
      length = -1;
    }
    else {
      length = StringUtils::int32(l);
    }
  }
  else {
    length = -1;
  }

  // .............................................................................
  // compute the statistics
  // .............................................................................

  compute(granularity, length);
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:


