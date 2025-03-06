////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ApiRecordingFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Parameters.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "Metrics/MetricsFeature.h"

using namespace arangodb::options;

namespace arangodb {

size_t ApiCallRecord::memoryUsage() const noexcept {
  return sizeof(ApiCallRecord) + path.size() + database.size();
}

ApiRecordingFeature::ApiRecordingFeature(Server& server)
    : ArangodFeature{server, *this},
      _recordApiCallTimes(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_api_recording_call_time{})) {
  setOptional(false);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

ApiRecordingFeature::~ApiRecordingFeature() {
  // Ensure cleanup thread is stopped if not already
  _stopCleanupThread.store(true, std::memory_order_relaxed);
  if (_cleanupThread.joinable()) {
    _cleanupThread.join();
  }
}

void ApiRecordingFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addOption(
      "--server.api-call-recording",
      "Record recent API calls for debugging purposes (default: true).",
      new BooleanParameter(&_enabled),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon,
                                          arangodb::options::Flags::Command));

  options->addOption(
      "--server.memory-per-api-call-list",
      "Memory limit for a list of ApiCallRecords. Exceeding this limit results "
      "in a new list beeing created.",
      new UInt64Parameter(&_memoryPerApiRecordList, 1, 1000, 1000000000),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon,
                                          arangodb::options::Flags::Command));

  options->addOption(
      "--server.number-of-api-call-lists",
      "Number of lists of api call records in ring buffer.",
      new UInt64Parameter(&_numberOfApiRecordLists, 1, 3, 1000),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon,
                                          arangodb::options::Flags::Command));
}

void ApiRecordingFeature::prepare() {
  // Initialize the API call record list if enabled
  if (_enabled) {
    _apiCallRecord = std::make_unique<BoundedList<ApiCallRecord>>(
        _memoryPerApiRecordList, _numberOfApiRecordLists);
  }
}

void ApiRecordingFeature::start() {
  // Start the cleanup thread if enabled
  if (_enabled) {
    _stopCleanupThread.store(false, std::memory_order_relaxed);
    _cleanupThread = std::jthread([this] { cleanupLoop(); });
#ifdef TRI_HAVE_SYS_PRCTL_H
    pthread_setname_np(_cleanupThread.native_handle(), "ApiRecordCleanup");
#endif
  }
}

void ApiRecordingFeature::stop() {
  // Stop and join the cleanup thread
  _stopCleanupThread.store(true, std::memory_order_relaxed);
  if (_cleanupThread.joinable()) {
    _cleanupThread.join();
  }
}

void ApiRecordingFeature::recordAPICall(arangodb::rest::RequestType requestType,
                                        std::string_view path,
                                        std::string_view database) {
  if (!_enabled || !_apiCallRecord) {
    return;
  }

  // Start timing
  auto start = std::chrono::steady_clock::now();

  // Existing implementation
  _apiCallRecord->prepend(ApiCallRecord(requestType, path, database));

  // End timing and record metrics
  auto end = std::chrono::steady_clock::now();
  int64_t elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

  // Record in histogram (seconds)
  _recordApiCallTimes.count(static_cast<double>(elapsed));
}

void ApiRecordingFeature::cleanupLoop() {
  while (!_stopCleanupThread.load(std::memory_order_relaxed)) {
    // Get the trash and measure the time
    auto start = std::chrono::steady_clock::now();
    size_t count = _apiCallRecord->clearTrash();

    auto duration = std::chrono::steady_clock::now() - start;
    auto nanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(duration);

    if (count > 0) {
      LOG_TOPIC("53626", TRACE, Logger::MEMORY)
          << "Cleaned up " << count << " API call record lists in "
          << nanoseconds.count() << " nanoseconds";
    }

    // Sleep for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

}  // namespace arangodb
