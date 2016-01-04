////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for jobs
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Martin Schoenert
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Job.h"

#include "Dispatcher/Dispatcher.h"

using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

namespace {
std::atomic_uint_fast64_t NEXT_JOB_ID(
    static_cast<uint64_t>(TRI_microtime() * 100000.0));
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a job
////////////////////////////////////////////////////////////////////////////////

Job::Job(string const &name)
    : _jobId(NEXT_JOB_ID.fetch_add(1, memory_order_seq_cst)),
      _name(name),
      _queuePosition((size_t)-1) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

Job::~Job() {}

// -----------------------------------------------------------------------------
// --SECTION--                                            virtual public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the queue name to use
////////////////////////////////////////////////////////////////////////////////

size_t Job::queue() const { return Dispatcher::STANDARD_QUEUE; }

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
