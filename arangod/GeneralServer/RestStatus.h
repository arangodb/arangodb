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

#ifndef ARANGOD_HTTP_SERVER_REST_STATUS_H
#define ARANGOD_HTTP_SERVER_REST_STATUS_H 1

#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 RestStatusElement
// -----------------------------------------------------------------------------

namespace arangodb {
class RestStatus;

class RestStatusElement {
  friend class RestStatus;

 public:
  enum class State { DONE, FAIL, QUEUED, THEN, WAIT_FOR };

 public:
  explicit RestStatusElement(State status)
      : _state(status), _previous(nullptr) {
    TRI_ASSERT(_state != State::THEN);
  }

  RestStatusElement(State status, std::shared_ptr<RestStatusElement> previous,
                    std::function<std::shared_ptr<RestStatus>()> callback)
      : _state(status), _previous(previous), _callThen(callback) {}

  RestStatusElement(State status, std::shared_ptr<RestStatusElement> previous)
      : _state(status), _previous(previous) {}

  RestStatusElement(State status, std::function<void(std::function<void()>)> callback)
      : _state(status), _previous(nullptr), _callWaitFor(callback) {}

 public:
  std::shared_ptr<RestStatusElement> previous() const { return _previous; }
  bool isLeaf() const { return _previous == nullptr; }
  State state() const { return _state; }

 public:
  std::shared_ptr<RestStatus> callThen() { return _callThen(); }
  void callWaitFor(std::function<void()> next) { _callWaitFor(next); }
  void printTree() const;

 private:
  State _state;
  std::shared_ptr<RestStatusElement> _previous;
  std::function<std::shared_ptr<RestStatus>()> _callThen;
  std::function<void(std::function<void()>)> _callWaitFor;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        RestStatus
// -----------------------------------------------------------------------------

class RestStatus {
 public:
  static RestStatus const DONE;
  static RestStatus const FAIL;
  static RestStatus const QUEUE;

  static RestStatus WAIT_FOR(std::function<void(std::function<void()>)> callback) {
    return RestStatus(new RestStatusElement(RestStatusElement::State::WAIT_FOR, callback));
  }

 public:
  explicit RestStatus(RestStatusElement* element) : _element(element) {}

  RestStatus(RestStatus const& that) = default;
  RestStatus(RestStatus&& that) = default;

  ~RestStatus() = default;

 public:
  template <typename FUNC>
  auto then(FUNC callback) const ->
      typename std::enable_if<std::is_void<decltype(callback())>::value, RestStatus>::type {
    return RestStatus(
        new RestStatusElement(RestStatusElement::State::THEN, _element, [callback]() {
          callback();
          return std::shared_ptr<RestStatus>(nullptr);
        }));
  }

  template <typename FUNC>
  auto then(FUNC callback) const ->
      typename std::enable_if<std::is_class<decltype(callback())>::value, RestStatus>::type {
    return RestStatus(
        new RestStatusElement(RestStatusElement::State::THEN, _element, [callback]() {
          return std::shared_ptr<RestStatus>(new RestStatus(callback()));
        }));
  }

  RestStatus done() {
    return RestStatus(new RestStatusElement(RestStatusElement::State::DONE, _element));
  }

 public:
  std::shared_ptr<RestStatusElement> element() { return _element; }

  bool isLeaf() const { return _element->isLeaf(); }

  bool isFailed() const {
    return _element->_state == RestStatusElement::State::FAIL;
  }

 public:
  void printTree() const;

 private:
  std::shared_ptr<RestStatusElement> _element;
};
}  // namespace arangodb

#endif
