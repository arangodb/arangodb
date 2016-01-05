////////////////////////////////////////////////////////////////////////////////
/// @brief barrier for synchronization
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
/// @author Copyright 2013-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_BARRIER_H
#define ARANGODB_BASICS_BARRIER_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"

namespace triagens {
namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                                           Barrier
// -----------------------------------------------------------------------------

class Barrier {
  // -----------------------------------------------------------------------------
  // --SECTION--                                        constructors /
  // destructors
  // -----------------------------------------------------------------------------

 public:
  Barrier(Barrier const&) = delete;
  Barrier& operator=(Barrier const&) = delete;

  explicit Barrier(size_t);

  ~Barrier();

  // -----------------------------------------------------------------------------
  // --SECTION--                                                  public
  // functions
  // -----------------------------------------------------------------------------

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief join a single task. reduces the number of waiting tasks and wakes
  /// up the barrier's synchronize() routine
  ////////////////////////////////////////////////////////////////////////////////

  void join();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief wait for all tasks to join
  ////////////////////////////////////////////////////////////////////////////////

  void synchronize();

  // -----------------------------------------------------------------------------
  // --SECTION--                                                 private
  // variables
  // -----------------------------------------------------------------------------

 private:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief condition variable
  ////////////////////////////////////////////////////////////////////////////////

  triagens::basics::ConditionVariable _condition;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief number of non-joined tasks
  ////////////////////////////////////////////////////////////////////////////////

  size_t _missing;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       BarrierTask
// -----------------------------------------------------------------------------

class BarrierTask {
  // -----------------------------------------------------------------------------
  // --SECTION--                                        constructors /
  // destructors
  // -----------------------------------------------------------------------------

 public:
  BarrierTask() = delete;
  BarrierTask(BarrierTask const&) = delete;
  BarrierTask& operator=(BarrierTask const&) = delete;

  explicit BarrierTask(Barrier* barrier) : _barrier(barrier) {}

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief destructor. joins the underlying barrier
  ////////////////////////////////////////////////////////////////////////////////

  ~BarrierTask() { _barrier->join(); }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                 private
  // variables
  // -----------------------------------------------------------------------------

 private:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the underlying barrier
  ////////////////////////////////////////////////////////////////////////////////

  Barrier* _barrier;
};

}  // namespace triagens::basics
}  // namespace triagens

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|//
// --SECTION--\\|/// @\\}"
// End:
