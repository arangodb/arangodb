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
/// @author Andrey Abramov
/// @author Vasily Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_FEATURE_H
#define ARANGOD_IRESEARCH__IRESEARCH_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/voc-types.h"

namespace arangodb {
struct IndexTypeFactory;

namespace aql {

struct Function;

}  // namespace aql

namespace iresearch {

class IResearchLink; // forward declaration
class ResourceMutex;  // forward declaration

bool isFilter(arangodb::aql::Function const& func) noexcept;
bool isScorer(arangodb::aql::Function const& func) noexcept;

class IResearchFeature final : public application_features::ApplicationFeature {
 public:
  explicit IResearchFeature(arangodb::application_features::ApplicationServer& server);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief execute an asynchronous task
  /// @note each task will be invoked by its first of timeout or 'asyncNotify()'
  /// @param mutex a mutex to check/prevent resource deallocation (nullptr ==
  /// not required)
  /// @param fn the function to execute
  ///           @param timeoutMsec how log to sleep in msec before the next
  ///           iteration (0 == sleep until previously set timeout or until
  ///           notification if first run)
  ///           @param timeout the timeout has been reached (false == triggered
  ///           by notification)
  ///           @return continue/reschedule
  //////////////////////////////////////////////////////////////////////////////
  void async(std::shared_ptr<ResourceMutex> const& mutex,
             std::function<bool(size_t& timeoutMsec, bool timeout)>&& fn);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief notify all currently running async tasks
  //////////////////////////////////////////////////////////////////////////////
  void asyncNotify() const;

  void beginShutdown() override;
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  static std::string const& name();
  void prepare() override;
  void start() override;
  void stop() override;
  void unprepare() override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;

  template <typename Engine, typename std::enable_if<std::is_base_of<StorageEngine, Engine>::value, int>::type = 0>
  IndexTypeFactory& factory();

 private:
  class Async;  // forward declaration

  std::shared_ptr<Async> _async;  // object managing async jobs (never null!!!)
  std::atomic<bool> _running;
  uint64_t _threads;
  uint64_t _threadsLimit;
  std::map<std::type_index, std::shared_ptr<arangodb::IndexTypeFactory>> _factories;
};

}  // namespace iresearch
}  // namespace arangodb

#endif
