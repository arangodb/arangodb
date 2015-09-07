////////////////////////////////////////////////////////////////////////////////
/// @brief index watermarks
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "IndexWatermarks.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                            struct IndexWatermarks
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                               static initializers
// -----------------------------------------------------------------------------

double IndexWatermarks::DefaultInitialFillFactor  = 0.5;
double IndexWatermarks::DefaultLowWatermark       = 0.0;
double IndexWatermarks::DefaultHighWatermark      = 0.0;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void IndexWatermarks::SetDefaults (IndexWatermarks const& other) {
  auto initialFillFactor = other.initialFillFactor;

  if (initialFillFactor < 0.05) {
    initialFillFactor = 0.05;
  }
  else if (initialFillFactor > 0.90) {
    initialFillFactor = 0.90;
  }

  DefaultInitialFillFactor = initialFillFactor;


  auto lowWatermark = other.lowWatermark;

  if (lowWatermark < 0.0) {
    lowWatermark = 0.0;
  }
  else if (lowWatermark > 0.95) {
    lowWatermark = 0.95;
  }

  DefaultLowWatermark = lowWatermark;


  auto highWatermark = other.highWatermark;

  if (highWatermark < 0.0) {
    highWatermark = 0.0;
  }
  else if (highWatermark > 0.95) {
    highWatermark = 0.95;
  }

  if (lowWatermark >= highWatermark) {
    highWatermark = lowWatermark + 0.01;
  }

  DefaultHighWatermark = highWatermark;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
