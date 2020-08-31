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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_ALGORITHM_H
#define ARANGODB_PREGEL_ALGORITHM_H 1

#include <velocypack/Slice.h>
#include <cstdint>
#include <functional>

#include "Basics/Common.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/MessageCombiner.h"
#include "Pregel/MessageFormat.h"
#include "Pregel/Statistics.h"
#include "Pregel/WorkerConfig.h"
#include "Pregel/WorkerContext.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace pregel {

template <typename V, typename E, typename M>
class VertexComputation;

template <typename V, typename E, typename M>
class VertexCompensation;

class IAggregator;
class WorkerConfig;
class MasterContext;

struct IAlgorithm {
  virtual ~IAlgorithm() = default;

  // virtual bool isFixpointAlgorithm() const {return false;}

  virtual bool supportsAsyncMode() const { return false; }

  virtual bool supportsCompensation() const { return false; }

  virtual IAggregator* aggregator(std::string const& name) const {
    return nullptr;
  }

  virtual MasterContext* masterContext(arangodb::velocypack::Slice userParams) const {
    return nullptr;
  }

  // ============= Configure runtime parameters ============

  std::string const& name() const { return _name; }

 protected:
  explicit IAlgorithm(std::string const& name) : _name(name) {}

 private:
  std::string _name;
};

// specify serialization, whatever
template <typename V, typename E, typename M>
struct Algorithm : IAlgorithm {
 public:
  virtual WorkerContext* workerContext(velocypack::Slice userParams) const {
    return new WorkerContext();
  }
  virtual GraphFormat<V, E>* inputFormat() const = 0;
  virtual MessageFormat<M>* messageFormat() const = 0;
  virtual MessageCombiner<M>* messageCombiner() const { return nullptr; };
  virtual VertexComputation<V, E, M>* createComputation(WorkerConfig const*) const = 0;
  virtual VertexCompensation<V, E, M>* createCompensation(WorkerConfig const*) const {
    return nullptr;
  }
  virtual std::set<std::string> initialActiveSet() {
    return std::set<std::string>();
  }

  virtual uint32_t messageBatchSize(WorkerConfig const& config,
                                    MessageStats const& stats) const {
    if (config.localSuperstep() == 0) {
      return 500;
    } else {
      double msgsPerSec = stats.sendCount / stats.superstepRuntimeSecs;
      msgsPerSec /= config.parallelism();  // per thread
      msgsPerSec *= 0.06;
      return msgsPerSec > 250.0 ? (uint32_t)msgsPerSec : 250;
    }
  }

 protected:
  Algorithm(application_features::ApplicationServer& server, std::string const& name)
      : IAlgorithm(name), _server(server) {}
  application_features::ApplicationServer& _server;
};

template <typename V, typename E, typename M>
class SimpleAlgorithm : public Algorithm<V, E, M> {
 protected:
  std::string _sourceField, _resultField;

  SimpleAlgorithm(application_features::ApplicationServer& server,
                  std::string const& name, VPackSlice userParams)
      : Algorithm<V, E, M>(server, name) {
    arangodb::velocypack::Slice field = userParams.get("sourceField");
    _sourceField = field.isString() ? field.copyString() : "value";
    field = userParams.get("resultField");
    _resultField = field.isString() ? field.copyString() : "result";
  }
};
}  // namespace pregel
}  // namespace arangodb
#endif
