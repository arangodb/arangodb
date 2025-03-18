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

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "RestServer/arangod.h"
#include "Containers/BoundedList.h"
#include "Rest/CommonDefines.h"
#include "Inspection/Transformers.h"
#include "Metrics/LogScale.h"
#include "Metrics/HistogramBuilder.h"

namespace arangodb {

// Define a struct for the LogScale used in the histogram
struct ApiCallTimeScale {
  static metrics::LogScale<double> scale() { return {2, 0, 16000.0, 9}; }
};

// Declare metrics
DECLARE_HISTOGRAM(arangodb_api_recording_call_time, ApiCallTimeScale,
                  "Execution time histogram for API recording calls [ns]");

struct ApiCallRecord {
  std::chrono::system_clock::time_point timeStamp;
  arangodb::rest::RequestType requestType;
  std::string path;
  std::string database;

  ApiCallRecord(arangodb::rest::RequestType requestType, std::string_view path,
                std::string_view database)
      : timeStamp(std::chrono::system_clock::now()),
        requestType(requestType),
        path(path),
        database(database) {}

  size_t memoryUsage() const noexcept;
};

// Inspector for ApiCallRecord to allow serialization using the Inspection
// library
template<class Inspector>
auto inspect(Inspector& f, ApiCallRecord& record) {
  return f.object(record).fields(
      f.field("timeStamp", record.timeStamp)
          .transformWith(arangodb::inspection::TimeStampTransformer{}),
      f.field("requestType", record.requestType), f.field("path", record.path),
      f.field("database", record.database));
}

class ApiRecordingFeature : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "ApiRecording"; }

  explicit ApiRecordingFeature(Server& server);
  ~ApiRecordingFeature() override;

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;

  void recordAPICall(arangodb::rest::RequestType requestType,
                     std::string_view path, std::string_view database);

  // Iterates over API call records from newest to oldest, invoking the given
  // callback function for each record. Thread-safe.
  template<typename F>
  requires std::is_invocable_v<F, ApiCallRecord const&>
  void doForApiCallRecords(F&& callback) const {
    if (_apiCallRecord) {
      _apiCallRecord->forItems(std::forward<F>(callback));
    }
  }

 private:
  // Cleanup thread function
  void cleanupLoop();

  // Whether or not to record recent API calls
  bool _enabled{true};

  // Memory limit for one list of ApiCallRecords:
  size_t _memoryPerApiRecordList{100000};

  // Number of ApiCallRecord lists in ring buffer:
  size_t _numberOfApiRecordLists{256};

  /// record of recent api calls:
  std::unique_ptr<arangodb::BoundedList<ApiCallRecord>> _apiCallRecord;

  // Flag to control the cleanup thread
  std::atomic<bool> _stopCleanupThread{false};

  // The cleanup thread itself
  std::jthread _cleanupThread;

  // Metrics for measuring recordAPICall performance
  metrics::Histogram<metrics::LogScale<double>>& _recordApiCallTimes;
};

}  // namespace arangodb
