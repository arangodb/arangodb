////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif


#include "Basics/logging.h"
#include "Scheduler/PeriodicTask.h"

#include "ApplicationAgency.h"

using namespace std;
using namespace arangodb::basics;
using namespace arangodb::rest;

ApplicationAgency::ApplicationAgency()
    : ApplicationFeature("agency"),
      _reportInterval(0.0),
      _nrStandardThreads(0) {}


ApplicationAgency::~ApplicationAgency() { /*delete _dispatcher;*/ }


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of used threads
////////////////////////////////////////////////////////////////////////////////

size_t ApplicationAgency::numberOfThreads() {
  return _nrStandardThreads /* + _nrAQLThreads */;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the processor affinity
////////////////////////////////////////////////////////////////////////////////

void ApplicationAgency::setupOptions(
    std::map<std::string, ProgramOptionsDescription>& options) {
  options["Server Options:help-admin"]("dispatcher.report-interval",
                                       &_reportInterval,
                                       "dispatcher report interval");
}


bool ApplicationAgency::prepare() {
  if (_disabled) {
    return true;
  }
  return true;
}


bool ApplicationAgency::start() {
  if (_disabled) {
    return true;
  }
  return true;
}


bool ApplicationAgency::open() { return true; }


void ApplicationAgency::close() {
  if (_disabled) {
    return;
  }
}


void ApplicationAgency::stop() {
  if (_disabled) {
    return;
  }
}



