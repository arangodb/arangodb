////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, execution statistics
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_EXECUTION_STATS_H
#define ARANGODB_AQL_EXECUTION_STATS_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"

namespace triagens {
  namespace aql {

    struct ExecutionStats {

      ExecutionStats ();

////////////////////////////////////////////////////////////////////////////////
/// @brief instanciate the statistics from JSON
////////////////////////////////////////////////////////////////////////////////

      ExecutionStats (triagens::basics::Json const& jsonStats);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the statistics to JSON
////////////////////////////////////////////////////////////////////////////////

      triagens::basics::Json toJson () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief create empty statistics for JSON
////////////////////////////////////////////////////////////////////////////////

      static triagens::basics::Json toJsonStatic ();

////////////////////////////////////////////////////////////////////////////////
/// @brief sumarize two sets of ExecutionStats      
////////////////////////////////////////////////////////////////////////////////

      void add (ExecutionStats const& summand) {
        writesExecuted += summand.writesExecuted;
        writesIgnored  += summand.writesIgnored;
        scannedFull    += summand.scannedFull;
        scannedIndex   += summand.scannedIndex;
        fullCount      += summand.fullCount;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief sumarize the delta of two other sets of ExecutionStats to us
////////////////////////////////////////////////////////////////////////////////

      void addDelta (ExecutionStats const& lastStats, ExecutionStats const& newStats) {
        writesExecuted += newStats.writesExecuted - lastStats.writesExecuted;
        writesIgnored  += newStats.writesIgnored  - lastStats.writesIgnored;
        scannedFull    += newStats.scannedFull    - lastStats.scannedFull;
        scannedIndex   += newStats.scannedIndex   - lastStats.scannedIndex;
        fullCount      += newStats.fullCount      - lastStats.fullCount;
      }


////////////////////////////////////////////////////////////////////////////////
/// @brief number of successfully executed write operations
////////////////////////////////////////////////////////////////////////////////

      int64_t writesExecuted;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of ignored write operations (ignored due to errors)
////////////////////////////////////////////////////////////////////////////////

      int64_t writesIgnored;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of documents scanned (full-collection scan)
////////////////////////////////////////////////////////////////////////////////

      int64_t scannedFull; 

////////////////////////////////////////////////////////////////////////////////
/// @brief number of documents scanned (using indexes scan)
////////////////////////////////////////////////////////////////////////////////

      int64_t scannedIndex; 

////////////////////////////////////////////////////////////////////////////////
/// @brief total number of results, before applying last limit
////////////////////////////////////////////////////////////////////////////////

      int64_t fullCount; 

    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
