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

#include "RestStatus.h"

#include "Logger/Logger.h"

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                    static members
// -----------------------------------------------------------------------------

RestStatus const RestStatus::DONE(new RestStatusElement(RestStatusElement::State::DONE));

RestStatus const RestStatus::FAIL(new RestStatusElement(RestStatusElement::State::FAIL));

RestStatus const RestStatus::QUEUE(new RestStatusElement(RestStatusElement::State::QUEUED));

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void RestStatusElement::printTree() const {
  std::string s = "TREE: ";
  std::string sep = "";
  RestStatusElement const* element = this;

  while (element != nullptr) {
    s += sep;

    switch (element->_state) {
      case State::DONE:
        s += "DONE";
        break;

      case State::FAIL:
        s += "FAILED";
        break;

      case State::WAIT_FOR:
        s += "WAIT_FOR";
        break;

      case State::QUEUED:
        s += "QUEUED";
        break;

      case State::THEN:
        s += "THEN";
        break;
    }

    sep = " -> ";

    element = element->_previous.get();
  }

  LOG_TOPIC(INFO, arangodb::Logger::FIXME) << s;
}

void RestStatus::printTree() const {
  if (_element != nullptr) {
    _element->printTree();
  } else {
    LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "TREE: EMPTY";
  }
}
