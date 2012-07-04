////////////////////////////////////////////////////////////////////////////////
/// @brief request statistics
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_STATISTICS_REQUEST_STATISTICS_H
#define TRIAGENS_STATISTICS_REQUEST_STATISTICS_H 1

#include "Statistics/statistics.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

#ifdef __cplusplus

namespace triagens {
  namespace basics {
    class VariantArray;
  }
}

#endif

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief request granularity
////////////////////////////////////////////////////////////////////////////////

enum TRI_request_statistics_granularity_e {
  TRI_REQUEST_STATISTICS_SECONDS,
  TRI_REQUEST_STATISTICS_MINUTES,
  TRI_REQUEST_STATISTICS_HOURS,
  TRI_REQUEST_STATISTICS_DAYS
};

////////////////////////////////////////////////////////////////////////////////
/// @brief request statistics
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_request_statistics_s {
  void* _next;

  double _readStart;
  double _readEnd;
  double _queueStart;
  double _queueEnd;
  double _requestStart;
  double _requestEnd;
  double _writeStart;
  double _writeEnd;

  double _receivedBytes;
  double _sentBytes;

  bool _tooLarge;
  bool _executeError;
}
TRI_request_statistics_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Statistics
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a new statistics block
////////////////////////////////////////////////////////////////////////////////

TRI_request_statistics_t* TRI_AcquireRequestStatistics (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseRequestStatistics (TRI_request_statistics_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the request statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateRequestStatistics (double now);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a statistics list
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus

triagens::basics::VariantArray* TRI_RequestStatistics (TRI_request_statistics_granularity_e granularity,
                                                       size_t limit,
                                                       bool showTotalTime,
                                                       bool showQueueTime,
                                                       bool showRequestTime,
                                                       bool showBytesSent,
                                                       bool showBytesReceived);

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief module init function
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseRequestStatistics (void);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
