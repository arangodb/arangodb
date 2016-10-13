////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "WorkerContext.h"
#include "InMessageCache.h"

using namespace arangodb;
using namespace arangodb::pregel;

WorkerContext::WorkerContext(unsigned int en) : _executionNumber(en) {
    _readCache = new InMessageCache();
    _writeCache = new InMessageCache();
}

WorkerContext::~WorkerContext() {
    LOG(ERR) << "dafuq";
}

void WorkerContext::swapIncomingCaches() {
    InMessageCache *t = _readCache;
    _readCache = _writeCache;
    _writeCache = t;
}
