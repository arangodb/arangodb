////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CLUSTER_AGENCY_CALLBACK_H
#define ARANGODB_CLUSTER_AGENCY_CALLBACK_H

#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <functional>
#include <memory>

#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionVariable.h"

namespace arangodb {
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
  AgencyCallback(application_features::ApplicationServer& server, std::string const&,
                 std::function<bool(velocypack::Slice const&)> const&, bool needsValue,
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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Set if local (b:true) or remote
  //////////////////////////////////////////////////////////////////////////////
  void local(bool b);
  bool local() const;
  

  //////////////////////////////////////////////////////////////////////////////
  /// @brief private members
  //////////////////////////////////////////////////////////////////////////////

 private:
  application_features::ApplicationServer& _server;
  std::unique_ptr<AgencyComm> _agency;
  std::function<bool(velocypack::Slice const&)> const _cb;
  std::shared_ptr<velocypack::Builder> _lastData;
  bool const _needsValue;
  /// @brief this flag is set if there was an attempt to signal the callback's
  /// condition variable - this is necessary to catch all signals that happen
  /// before the caller is going into the wait state, i.e. to prevent this
  ///  1) register callback
  ///  2a) execute callback
  ///  2b) execute callback signaling
  ///  3) caller going into condition.wait() (and not woken up)
  /// this variable is protected by the condition variable! 
  bool _wasSignaled;

  /// Determined when registered in registery. Default: true 
  bool _local;

  // execute callback with current value data:
  bool execute(std::shared_ptr<velocypack::Builder>);
  // execute callback without any data:
  bool executeEmpty();

  // Compare last value and newly read one and call execute if the are
  // different:
  void checkValue(std::shared_ptr<velocypack::Builder>, bool forceCheck);
};

}  // namespace arangodb

#endif
