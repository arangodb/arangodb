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

#include "RequestStatisticsHandler.h"

#include "Basics/StringUtils.h"
#include "Rest/HttpRequest.h"
#include "Variant/VariantArray.h"

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

RequestStatisticsHandler::RequestStatisticsHandler (triagens::rest::HttpRequest* request)
  : StatisticsBaseHandler(request) {
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

void RequestStatisticsHandler::compute (TRI_statistics_granularity_e granularity, size_t length) {
  bool showTotalTime = false;
  bool showQueueTime = false;
  bool showRequestTime = false;
  bool showBytesSent = false;
  bool showBytesReceived = false;

  // .............................................................................
  // extract the figures to show
  // .............................................................................

  bool found;
  string figures = StringUtils::tolower(_request->value("figures", found));

  if (found) {
    if (figures == "*" || figures == "all") {
      showTotalTime = true;
      showQueueTime = true;
      showRequestTime = true;
      showBytesSent = true;
      showBytesReceived = true;
    }
    else {
      vector<string> f = StringUtils::split(figures);

      for (vector<string>::iterator i = f.begin();  i != f.end();  ++i) {
        string const& fn = *i;

        if (fn == "totaltime") {
          showTotalTime = true;
        }
        else if (fn == "queuetime") {
          showQueueTime = true;
        }
        else if (fn == "requesttime") {
          showRequestTime = true;
        }
        else if (fn == "bytessent") {
          showBytesSent = true;
        }
        else if (fn == "bytesreceived") {
          showBytesReceived = true;
        }
        else {
          generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, "unknown figure '" + fn + "'");
          return;
        }
      }
    }

  }
  else {
    showTotalTime = true;
    showBytesSent = true;
    showBytesReceived = true;
  }

  // .............................................................................
  // compute
  // .............................................................................

  VariantArray* result = TRI_StatisticsInfo(granularity,
                                            length,
                                            showTotalTime,
                                            showQueueTime,
                                            showRequestTime,
                                            showBytesSent,
                                            showBytesReceived,
                                            false);

  generateResult(result);
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


