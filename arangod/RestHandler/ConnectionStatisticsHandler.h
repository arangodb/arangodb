////////////////////////////////////////////////////////////////////////////////
/// @brief statistics handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_REST_HANDLER_CONNECTION_STATISTICS_HANDLER_H
#define TRIAGENS_REST_HANDLER_CONNECTION_STATISTICS_HANDLER_H 1

#include "RestHandler/StatisticsBaseHandler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                         class RestDocumentHandler
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace arango {

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics handler
////////////////////////////////////////////////////////////////////////////////

    class ConnectionStatisticsHandler : public StatisticsBaseHandler {

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new handler
////////////////////////////////////////////////////////////////////////////////

        ConnectionStatisticsHandler (triagens::rest::HttpRequest*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                     StatisticsBaseHandler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
///
/// @RESTHEADER{GET /_admin/connection-statistics,reads the connection statistics}
///
/// @REST{GET /_admin/connection-statistics?granularity=@FA{granularity}&figures=@FA{figures}&length=@FA{length}}
///
/// The call returns statistics about the current and past requests. The
/// following parameter control which information is returned.
///
/// - @FA{granularity}: use @LIT{minutes} for a granularity of minutes,
///   @LIT{hours} for hours, and @LIT{days} for days. The default is
///   @LIT{minutes}.
///
/// - @FA{figures}: a list of figures, comma-separated. Possible figures are
///   @LIT{httpConnections}. You can use @LIT{all} to get all figures. The default
///   is @LIT{httpConnections}.
///
/// - @FA{length}: If you want a time series, the maximal length of the series
///   as integer. You can use @LIT{all} to get all available information. You can
///   use @LIT{current} to get the latest interval.
///
/// The returned statistics objects contains information of the request figures.
///
/// - @LIT{resolution}: the resolution in seconds aka granularity. The length of
///   the time intervals.
///
/// - @LIT{start}: a list of time stamps in seconds since 1970-01-01. Each entry
///   marks the start of an interval for which the figures were computed. The length
///   of the interval is given by @LIT{resolution}.
///
/// - @LIT{length}: the number of returned intervals.
///
/// - @LIT{totalLength}: the number of available intervals.
///
/// - @LIT{httpConnections}: the number of opened http connections
///   during the interval.
///
/// - @LIT{httpDuration}: the distribution of the duration of the
///   closed http connections during the interval.
///
/// If @FA{length} is @LIT{current} the figures for the current interval are returned.
///
/// @EXAMPLES
///
/// A time-series:
///
/// @EXAMPLE{connection-statistics,connection statistics as time series}
///
/// The current figures:
///
/// @EXAMPLE{connection-statistics-current,current connection statistics}
////////////////////////////////////////////////////////////////////////////////

        void compute (TRI_statistics_granularity_e, size_t length);
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
