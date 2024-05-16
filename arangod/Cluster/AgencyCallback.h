////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <memory>

#include "Basics/ConditionVariable.h"
#include "RestServer/arangod.h"
#include "Agency/AgencyCommon.h"

namespace arangodb {
class AgencyComm;
class AgencyCache;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace application_features {
class ApplicationServer;
}

////////////////////////////////////////////////////////////////////////////////
/// class AgencyCallback
///
/// This class encapsulates an agency observer that has been registered
/// with the agency. One specifies a callback function that is called
/// for every incoming HTTP request from the agency. A mutex ensures that
/// this callback function is only executed in one thread at a time (see
/// below for more details).
///
/// Furthermore, if needsValue == true, the latest value of the key
/// which the callback observes is kept and updated with every HTTP
/// request received from the agency, and handed to the callback
/// function. If needsValue == false then a None Slice is handed in
/// instead.
///
/// If an initial value should be kept even before the first agency callback
/// has happened, then needInitialValue must be set to true. In this case
/// the callback function is already called once at object creation.
///
/// Usually, with needsValue == true one would like to wait until a certain
/// condition is met with respect to the value. The callback is only called
/// for new values, such that one can check this condition in the callback
/// function.
///
/// To assist code that wants to wait for something which is discovered
/// in the callback function (for example a certain value of the
/// observed key), this class maintains a condition variable, which is
/// signaled whenever the callback function has been called. To avoid
/// missing signals, the above mentioned mutex is the one of the condition
/// variable and the callback function is always called under this mutex
/// and the signal is sent while the mutex is still held. Thus, the following
/// pseudocode does not miss events:
///
/// create AgencyCallback object with a callback function and register it
/// TRI_DEFER(unregister callback)
/// {
///   acquire mutex of condition variable
///   while true:
///     check if a callback has produced the termination event: if so: OK
///     if overall patience lost: leave with error
///     wait for condition variable with a timeout
/// }
///
/// In this way, the mutex of the condition variable can at the same time
/// organize mutual exclusion of the callback function and the checking of
/// the termination condition in the main thread.
/// The wait for condition variable can conveniently be done with the
/// method executeByCallbackOrTimeout below.

class AgencyCallback {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief ctor
  //////////////////////////////////////////////////////////////////////////////

 public:
  using CallbackType =
      std::function<bool(velocypack::Slice, consensus::index_t)>;

  AgencyCallback(ApplicationServer& server, AgencyCache& agencyCache,
                 std::string key, CallbackType, bool needsValue,
                 bool needsInitialValue = true);
  [[deprecated(
      "Avoid this constructor to get rid of the ArangodServer "
      "dependency")]] AgencyCallback(ArangodServer& server, std::string const&,
                                     std::function<
                                         bool(velocypack::Slice const&)> const&,
                                     bool needsValue,
                                     bool needsInitialValue = true);
  [[deprecated(
      "Avoid this constructor to get rid of the ArangodServer "
      "dependency")]] AgencyCallback(ArangodServer& server, std::string key,
                                     CallbackType, bool needsValue,
                                     bool needsInitialValue = true);

  std::string const key;
  arangodb::basics::ConditionVariable _cv;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief refetch the value, and call the callback function with it,
  /// this is called whenever an HTTP request is received from the agency
  /// (see RestAgencyCallbacksHandler and AgencyCallbackRegistry). If the
  /// forceCheck flag is set, a check is initiated even if the value has
  /// not changed. This is needed in case other outside conditions could
  /// have changed (like a Plan change).
  //////////////////////////////////////////////////////////////////////////////

  void refetchAndUpdate(bool needToAcquireMutex, bool forceCheck);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief wait until a callback is received or a timeout has happened
  ///
  /// @return true => if we got woken up after maxTimeout
  ///         false => if someone else ringed the condition variable
  //////////////////////////////////////////////////////////////////////////////

  bool executeByCallbackOrTimeout(double);

  bool needsInitialValue() const noexcept { return _needsInitialValue; }

 private:
  // execute callback with current value data:
  bool execute(velocypack::Slice data, consensus::index_t raftIndex);

  // execute callback without any data:
  bool executeEmpty();

  // Compare last value and newly read one and call execute if the are
  // different:
  void checkValue(std::shared_ptr<velocypack::Builder>,
                  consensus::index_t raftIndex, bool forceCheck);

 private:
  ApplicationServer& _server;
  AgencyCache& _agencyCache;
  CallbackType const _cb;
  std::shared_ptr<velocypack::Builder> _lastData;
  bool const _needsValue;
  bool const _needsInitialValue;
  /// @brief this flag is set if there was an attempt to signal the callback's
  /// condition variable - this is necessary to catch all signals that happen
  /// before the caller is going into the wait state, i.e. to prevent this
  ///  1) register callback
  ///  2a) execute callback
  ///  2b) execute callback signaling
  ///  3) caller going into condition.wait() (and not woken up)
  /// this variable is protected by the condition variable!
  bool _wasSignaled{false};

  /// This index keeps track of which raft index was seen last. It ensures a
  /// monotonic view on the observed value.
  consensus::index_t _lastSeenIndex{0};
};

}  // namespace arangodb
