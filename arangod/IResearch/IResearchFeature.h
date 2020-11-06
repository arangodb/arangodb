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

struct IResearchAsync;
class IResearchLink;
class ResourceMutex;

////////////////////////////////////////////////////////////////////////////////
/// @enum ThreadGroup
/// @brief there are 2 thread groups for execution of asynchronous maintenance
///        jobs.
////////////////////////////////////////////////////////////////////////////////
enum class ThreadGroup : size_t { _0 = 0, _1 };

bool isFilter(arangodb::aql::Function const& func) noexcept;
bool isScorer(arangodb::aql::Function const& func) noexcept;

////////////////////////////////////////////////////////////////////////////////
/// @class IResearchFeature
////////////////////////////////////////////////////////////////////////////////
class IResearchFeature final : public application_features::ApplicationFeature {
 public:
  explicit IResearchFeature(arangodb::application_features::ApplicationServer& server);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief schedule an asynchronous task for execution
  /// @param id thread group to handle the execution
  /// @param fn the function to execute
  /// @param delay how log to sleep in before the next
  //////////////////////////////////////////////////////////////////////////////
  void queue(ThreadGroup id,
             std::chrono::steady_clock::duration delay,
             std::function<void()>&& fn);

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
  std::shared_ptr<IResearchAsync> _async;
  std::atomic<bool> _running;
  size_t _consolidationThreads;
  size_t _commitThreads;
  uint64_t _threads;
  uint64_t _threadsLimit;
  std::map<std::type_index, std::shared_ptr<arangodb::IndexTypeFactory>> _factories;
};

}  // namespace iresearch
}  // namespace arangodb

#endif
