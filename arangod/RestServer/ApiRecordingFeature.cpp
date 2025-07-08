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

size_t AqlQueryRecord::memoryUsage() const noexcept {
  return sizeof(AqlQueryRecord) + query.size() + database.size() +
         bindVars.byteSize();
}

ApiRecordingFeature::ApiRecordingFeature(Server& server)
    : ArangodFeature{server, *this},
      _recordApiCallTimes(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_api_recording_call_time{})),
      _recordAqlCallTimes(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_aql_recording_call_time{})) {
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
      "Whether to record recent API calls for debugging purposes.",
      new BooleanParameter(&_enabledCalls),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--server.api-recording-memory-limit",
      "Size limit for the list of API call records.",
      new UInt64Parameter(&_totalMemoryLimitCalls, 1,
                          256 * (std::size_t{1} << 10),  // Min: 256 KiB
                          256 * (std::size_t{1} << 30)   // Max: 256 GiB
                          ),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--server.aql-query-recording",
      "Whether to record recent AQL queries for debugging purposes.",
      new BooleanParameter(&_enabledQueries),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--server.aql-recording-memory-limit",
      "Size limit for the list of AQL query records.",
      new UInt64Parameter(&_totalMemoryLimitQueries, 1,
                          256 * (std::size_t{1} << 10),  // Min: 256 KiB
                          256 * (std::size_t{1} << 30)   // Max: 256 GiB
                          ),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options
      ->addOption(
          "--log.recording-api-enabled",
          "Whether the recording API is enabled (true) or not (false), or "
          "only enabled for the superuser (jwt).",
          new StringParameter(&_apiSwitch))
      .setLongDescription(R"(The `/_admin/server/api-calls` and
`/_admin/server/aql-queries` endpoints provide access to recorded API calls
and AQL queries respectively. They are referred to as the recording API.

Since this data might be sensitive depending on the context of the deployment,
these endpoints need to be properly secured. By default, the recording API is
accessible for admin users (users with administrative access to the `_system`
database). However, you can restrict it further to the superuser or disable it
altogether:

- `true`: The recording API is accessible for admin users.
- `jwt`: The recording API is accessible for the superuser only
  (authentication with JWT superuser token and empty username).
- `false`: The recording API is not accessible at all.

Whether API calls and AQL queries are recorded is independent of this option.
It is controlled by the `--server.api-call-recording` and
`--server.aql-query-recording` startup options.)");
}

void ApiRecordingFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  if (_apiSwitch == "true" || _apiSwitch == "on" || _apiSwitch == "On") {
    _apiEnabled = true;
    _apiSwitch = "true";
  } else if (_apiSwitch == "jwt" || _apiSwitch == "JWT") {
    _apiEnabled = true;
    _apiSwitch = "jwt";
  } else {
    _apiEnabled = false;
    _apiSwitch = "false";
  }
}

void ApiRecordingFeature::prepare() {
  // Calculate per-list memory limit
  _memoryPerApiRecordList = _totalMemoryLimitCalls / NUMBER_OF_API_RECORD_LISTS;
  _memoryPerAqlRecordList =
      _totalMemoryLimitQueries / NUMBER_OF_AQL_RECORD_LISTS;

  if (_enabledCalls) {
    _apiCallRecord = std::make_unique<BoundedList<ApiCallRecord>>(
        _memoryPerApiRecordList, NUMBER_OF_API_RECORD_LISTS);
  }
  if (_enabledQueries) {
    _aqlQueryRecord = std::make_unique<BoundedList<AqlQueryRecord>>(
        _memoryPerAqlRecordList, NUMBER_OF_AQL_RECORD_LISTS);
  }
}

void ApiRecordingFeature::start() {
  // Start the cleanup thread if enabled
  if (_enabledCalls || _enabledQueries) {
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
  if (!_enabledCalls || !_apiCallRecord) {
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

void ApiRecordingFeature::recordAQLQuery(
    std::string_view queryString, std::string_view database,
    velocypack::SharedSlice bindParameters) {
  if (!_enabledQueries || !_aqlQueryRecord) {
    return;
  }

  // Start timing
  auto start = std::chrono::steady_clock::now();

  _aqlQueryRecord->prepend(
      AqlQueryRecord(queryString, database, std::move(bindParameters)));

  // End timing and record metrics
  auto end = std::chrono::steady_clock::now();
  int64_t elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

  // Record in histogram (seconds)
  _recordAqlCallTimes.count(static_cast<double>(elapsed));
}

void ApiRecordingFeature::cleanupLoop() {
  // Initialize delay values
  constexpr std::chrono::milliseconds MIN_DELAY{1};
  constexpr std::chrono::milliseconds MAX_DELAY{256};
  auto currentDelay = MIN_DELAY;

  while (!_stopCleanupThread.load(std::memory_order_relaxed)) {
    // Get the trash and measure the time
    auto start = std::chrono::steady_clock::now();
    size_t apiCallCount = 0;
    size_t aqlCallCount = 0;

    if (_apiCallRecord) {
      apiCallCount = _apiCallRecord->clearTrash();
    }

    if (_aqlQueryRecord) {
      aqlCallCount = _aqlQueryRecord->clearTrash();
    }

    auto duration = std::chrono::steady_clock::now() - start;
    auto nanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(duration);

    size_t totalCount = apiCallCount + aqlCallCount;
    if (totalCount > 0) {
      if (apiCallCount > 0) {
        LOG_TOPIC("53626", TRACE, Logger::MEMORY)
            << "Cleaned up " << apiCallCount << " API call record lists in "
            << nanoseconds.count() << " nanoseconds";
      }
      if (aqlCallCount > 0) {
        LOG_TOPIC("53627", TRACE, Logger::MEMORY)
            << "Cleaned up " << aqlCallCount << " AQL query record lists in "
            << nanoseconds.count() << " nanoseconds";
      }
      // Reset delay to minimum when trash was found
      currentDelay = MIN_DELAY;
    } else {
      // Double the delay if no trash was found, up to MAX_DELAY
      currentDelay = std::min(currentDelay * 2, MAX_DELAY);
    }

    // Sleep using the calculated delay
    std::this_thread::sleep_for(currentDelay);
  }
}

}  // namespace arangodb
