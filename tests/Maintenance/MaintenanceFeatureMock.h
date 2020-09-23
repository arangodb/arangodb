////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <thread>

#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"

#include "ApplicationFeatures/ApplicationServer.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

//
// structure used to store expected states of action properties
//
struct Expected {
  int _id;
  int _result;
  int _state;
  int _progress;
};

typedef std::vector<Expected> ExpectedVec_t;

//
// TestProgressHandler lets us know once ApplicationServer is ready
//
class TestProgressHandler
    : public arangodb::application_features::ApplicationServer::ProgressHandler {
 public:
  TestProgressHandler() {
    _serverReady = false;

    using std::placeholders::_1;
    _state = std::bind(&TestProgressHandler::StateChange, this, _1);

    using std::placeholders::_2;
    _feature = std::bind(&TestProgressHandler::FeatureChange, this, _1, _2);
  }

  void StateChange(arangodb::application_features::ApplicationServer::State newState) {
    if (arangodb::application_features::ApplicationServer::State::IN_WAIT == newState) {
      CONDITION_LOCKER(clock, _serverReadyCond);
      _serverReady = true;
      _serverReadyCond.broadcast();
    }
  }

  void FeatureChange(arangodb::application_features::ApplicationServer::State newState,
                     std::string const&) {}

  arangodb::basics::ConditionVariable _serverReadyCond;
  std::atomic_bool _serverReady;

};  // class TestProgressHandler

using namespace arangodb::maintenance;

//
// TestFeature wraps MaintenanceFeature to all test specific action objects
//  by overriding the actionFactory() virtual function.  Two versions:
//  1. default constructor for non-threaded actions
//  2. constructor with ApplicationServer pointer for threaded actions
//
class TestMaintenanceFeature : public arangodb::MaintenanceFeature {
 public:
  TestMaintenanceFeature(arangodb::application_features::ApplicationServer& as)
      : arangodb::MaintenanceFeature(as) {
    // force activation of the feature, even in agency/single-server mode
    // (the catch tests use single-server mode)
    _forceActivation = true;

    // begin with no threads to allow queue validation
    _maintenanceThreadsMax = 0;
    as.addReporter(_progressHandler);
    initializeMetrics();
  }

  virtual ~TestMaintenanceFeature() = default;

  void validateOptions(std::shared_ptr<arangodb::options::ProgramOptions> options) override {}

  void setSecondsActionsBlock(uint32_t seconds) {
    _secondsActionsBlock = seconds;
  }

  /// @brief set thread count, then activate the threads via start().  One time usage only.
  ///   Code waits until background ApplicationServer known to have fully started.
  void setMaintenanceThreadsMax(uint32_t threads) {
    CONDITION_LOCKER(clock, _progressHandler._serverReadyCond);
    while (!_progressHandler._serverReady) {
      _progressHandler._serverReadyCond.wait();
    }  // while

    _maintenanceThreadsMax = threads;
    start();
  }  // setMaintenanceThreadsMax

  virtual arangodb::Result addAction(std::shared_ptr<arangodb::maintenance::Action> action,
                                     bool executeNow = false) override {
    _recentAction = action;
    return MaintenanceFeature::addAction(action, executeNow);
  }

  virtual arangodb::Result addAction(std::shared_ptr<arangodb::maintenance::ActionDescription> const& description,
                                     bool executeNow = false) override {
    return MaintenanceFeature::addAction(description, executeNow);
  }

  bool verifyRegistryState(ExpectedVec_t& expected) {
    bool good(true);

    VPackBuilder registryBuilder(toVelocyPack());
    VPackArrayIterator registry(registryBuilder.slice());

    auto action = registry.begin();
    auto check = expected.begin();

    for (; registry.end() != action && expected.end() != check; ++action, ++check) {
      VPackSlice id = (*action).get("id");
      if (!(id.isInteger() && id.getInt() == check->_id)) {
        std::cerr << "Id mismatch: action has " << id.getInt() << " expected "
                  << check->_id << std::endl;
        good = false;
      }  // if

      VPackSlice result = (*action).get("result");
      if (!(result.isInteger() && check->_result == result.getInt())) {
        std::cerr << "Result mismatch: action has " << result.getInt()
                  << " expected " << check->_result << std::endl;
        good = false;
      }  // if

      VPackSlice state = (*action).get("state");
      if (!(state.isInteger() && check->_state == state.getInt())) {
        std::cerr << "State mismatch: action has " << state.getInt()
                  << " expected " << check->_state << std::endl;
        good = false;
      }  // if

      VPackSlice progress = (*action).get("progress");
      if (!(progress.isInteger() && check->_progress == progress.getInt())) {
        std::cerr << "Progress mismatch: action has " << progress.getInt()
                  << " expected " << check->_progress << std::endl;
        good = false;
      }  // if
    }    // for

    return good;

  }  // verifyRegistryState

  /// @brief poll registry until all actions finish
  void waitRegistryComplete() {
    bool again;

    do {
      again = false;
      std::this_thread::sleep_for(std::chrono::seconds(1));

      VPackBuilder registryBuilder(toVelocyPack());
      VPackArrayIterator registry(registryBuilder.slice());
      for (auto action : registry) {
        VPackSlice state = action.get("state");
        again = again || (COMPLETE != state.getInt() && FAILED != state.getInt());
      }  // for
    } while (again);
  }  // waitRegistryComplete

 public:
  std::shared_ptr<Action> _recentAction;
  TestProgressHandler _progressHandler;

};  // TestMaintenanceFeature
