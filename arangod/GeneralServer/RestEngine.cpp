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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestEngine.h"

#include <iostream>
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

int RestEngine::asyncRun(std::shared_ptr<rest::RestHandler> handler) {
  return run(handler, false);
}

int RestEngine::syncRun(std::shared_ptr<rest::RestHandler> handler) {
  _agent = nullptr;

  _loop._ioService = nullptr;
  _loop._scheduler = nullptr;

  return run(handler, true);
}

void RestEngine::appendRestStatus(std::shared_ptr<RestStatusElement> element) {
  while (element.get() != nullptr) {
    _elements.emplace_back(element);
    element = element->previous();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

int RestEngine::run(std::shared_ptr<rest::RestHandler> handler, bool synchron) {
  while (true) {
    int res = TRI_ERROR_NO_ERROR;

    switch (_state) {
      case State::PREPARE:
        res = handler->prepareEngine();
        break;

      case State::EXECUTE:
        res = handler->executeEngine();
        if (res != TRI_ERROR_NO_ERROR) {
          handler->finalizeEngine();
        }
        break;

      case State::RUN:
        res = handler->runEngine(synchron);
        if (res != TRI_ERROR_NO_ERROR) {
          handler->finalizeEngine();
        }
        break;

      case State::WAITING:
        return synchron ? TRI_ERROR_INTERNAL : TRI_ERROR_NO_ERROR;

      case State::FINALIZE:
        res = handler->finalizeEngine();
        break;

      case State::DONE:
      case State::FAILED:
        return res;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
}
