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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_FEATURE_H
#define ARANGODB_PREGEL_FEATURE_H 1

#include <unordered_map>
#include "Basics/Common.h"
#include "ApplicationFeatures/ApplicationFeature.h"


namespace arangodb {
namespace pregel {
  
  class Conductor;
  class Worker;
  
    class PregelFeature final
    : public application_features::ApplicationFeature {
  public:
    explicit PregelFeature(application_features::ApplicationServer* server);
        ~PregelFeature();
        
    static PregelFeature* instance();
    
    void beginShutdown() override final;
    
    unsigned int createExecutionNumber();
    void addExecution(Conductor* const exec, unsigned int executionNumber);
    Conductor* conductor(int32_t executionNumber);
    
    void addWorker(Worker* const worker, unsigned int executionNumber);
    Worker* worker(unsigned int executionNumber);
    
      void cleanup(unsigned int executionNumber);
      void cleanupAll();
      
  private:
    std::unordered_map<unsigned int, Conductor*> _conductors;
    std::unordered_map<unsigned int, Worker*> _workers;
  };
}
}

#endif
