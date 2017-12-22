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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "WorkMonitor.h"

using namespace arangodb;
using namespace arangodb::velocypack;

namespace arangodb {
namespace rest {
class RestHandler;
}
}
   
void WorkMonitor::run() { TRI_ASSERT(false); }

bool WorkMonitor::cancelAql(WorkDescription* desc) {
  TRI_ASSERT(false);
  return true;
}

void WorkMonitor::deleteHandler(WorkDescription*) { TRI_ASSERT(false); }

void WorkMonitor::vpackHandler(arangodb::velocypack::Builder*,
                               WorkDescription*) {
  TRI_ASSERT(false);
}

void WorkMonitor::addWorkOverview(std::shared_ptr<rest::RestHandler>,
                                  std::shared_ptr<Buffer<uint8_t>>) {
  TRI_ASSERT(false);
}

void WorkMonitor::clearAllHandlers() {
  TRI_ASSERT(false);
}

bool WorkMonitor::clearWorkDescriptions() {
  TRI_ASSERT(false);
  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  HandlerWorkStack
// -----------------------------------------------------------------------------
HandlerWorkStack::HandlerWorkStack(std::shared_ptr<arangodb::rest::RestHandler> handler)
  : _handler(handler) {
  TRI_ASSERT(false);
}

HandlerWorkStack::~HandlerWorkStack() {
  TRI_ASSERT(false);
}
