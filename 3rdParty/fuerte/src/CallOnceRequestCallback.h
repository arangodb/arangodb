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
/// @author Ewout Prangsma
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef ARANGO_CXX_DRIVER_CALL_ONCE_REQUEST_CALLBACK
#define ARANGO_CXX_DRIVER_CALL_ONCE_REQUEST_CALLBACK

#include <atomic>

#include <fuerte/types.h>

namespace arangodb { namespace fuerte { inline namespace v1 { namespace impl {

// CallOnceRequestCallback is a helper that ensures that a callback is invoked
// no more than one time.
class CallOnceRequestCallback {
 public:
  CallOnceRequestCallback() : _invoked(), _cb(nullptr) {
    _invoked.clear();
  }
  explicit CallOnceRequestCallback(RequestCallback cb)
      : _invoked(), _cb(std::move(cb)) {
    _invoked.clear();
  }
  CallOnceRequestCallback& operator=(RequestCallback cb) {
    _cb = cb;
    return *this;
  }

  // Invoke the callback.
  // If the callback was already invoked, the callback is not invoked.
  inline void invoke(Error error, std::unique_ptr<Request> req,
                     std::unique_ptr<Response> resp) {
    if (!_invoked.test_and_set()) {
      assert(_cb);
      _cb(error, std::move(req), std::move(resp));
      _cb = nullptr;
    } else {
      assert(false);
    }
  }

 private:
  std::atomic_flag _invoked;
  RequestCallback _cb;
};
}}}}  // namespace arangodb::fuerte::v1::impl
#endif
