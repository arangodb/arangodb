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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_SERVER_FLUSH_FEATURE_H
#define ARANGODB_REST_SERVER_FLUSH_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/ReadWriteLock.h"

namespace arangodb {

class FlushThread;
class FlushTransaction;

class FlushFeature final
    : public application_features::ApplicationFeature {
 public:
  typedef std::unique_ptr<
      FlushTransaction, std::function<void(FlushTransaction*)>
  > FlushTransactionPtr;

  typedef std::function<FlushTransactionPtr()> FlushCallback;

  explicit FlushFeature(
    application_features::ApplicationServer& server
  );

  void collectOptions(
      std::shared_ptr<options::ProgramOptions> options) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void beginShutdown() override;
  void stop() override;
  void unprepare() override;

  static bool isRunning() { return _isRunning.load(); }

  /// @brief register the callback, using ptr as key
  void registerCallback(void* ptr, FlushFeature::FlushCallback const& cb);

  /// @brief unregister the callback, by ptr
  /// if the callback is unknown, returns false.
  bool unregisterCallback(void* ptr);

  /// @brief executes all callbacks. the order in which they are executed is undefined
  void executeCallbacks();

 private:
  uint64_t _flushInterval;
  std::unique_ptr<FlushThread> _flushThread;
  static std::atomic<bool> _isRunning;
  basics::ReadWriteLock _threadLock;

  basics::ReadWriteLock _callbacksLock;
  std::unordered_map<void*, FlushCallback> _callbacks;
};

}

#endif
