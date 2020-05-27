////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef TESTS_AQL_EXECUTIONBLOCKPIPELINE_H
#define TESTS_AQL_EXECUTIONBLOCKPIPELINE_H

#include "Aql/ExecutionBlock.h"

#include <deque>

using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

// A wrapper to ensure correct order of deallocation of Execution Blocks
using ExecBlock = std::unique_ptr<ExecutionBlock>;
struct Pipeline {
  using PipelineStorage = std::deque<ExecBlock>;

  Pipeline() : _pipeline{} {};
  Pipeline(ExecBlock&& init) { _pipeline.emplace_back(std::move(init)); }
  Pipeline(std::deque<ExecBlock>&& init) : _pipeline(std::move(init)){};
  Pipeline(Pipeline& other) = delete;
  Pipeline(Pipeline&& other) : _pipeline(std::move(other._pipeline)){};

  Pipeline& operator=(Pipeline&& other) {
    _pipeline = std::move(other._pipeline);
    return *this;
  }

  virtual ~Pipeline() {
    for (auto&& b : _pipeline) {
      b.reset(nullptr);
    }
  };

  bool empty() const { return _pipeline.empty(); }
  void reset() { _pipeline.clear(); }

  std::deque<ExecBlock> const& get() const { return _pipeline; };
  std::deque<ExecBlock>& get() { return _pipeline; };

  Pipeline& addDependency(ExecBlock&& dependency) {
    if (!empty()) {
      _pipeline.back()->addDependency(dependency.get());
    }
    _pipeline.emplace_back(std::move(dependency));
    return *this;
  }

  Pipeline& addConsumer(ExecBlock&& consumer) {
    if (!empty()) {
      consumer->addDependency(_pipeline.front().get());
    }
    _pipeline.emplace_front(std::move(consumer));
    return *this;
  }

 private:
  PipelineStorage _pipeline;
};
}  // namespace aql
}  // namespace tests
}  // namespace arangodb
#endif
