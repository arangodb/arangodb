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

#ifndef ARANGODB_PREGEL_JOBMAPPING_H
#define ARANGODB_PREGEL_JOBMAPPING_H 1

#include <map>

namespace arangodb {
namespace pregel {
  
  class Conductor;
  class Worker;
  
  class JobMapping {
  public:
    
    
    static JobMapping* instance() {
      return &_Instance;
    };
    
    int createExecutionNumber();
    void addExecution(Conductor *exec, int executionNumber);
    Conductor* conductor(int executionNumber);
    Worker* worker(int executionNumber);

    
  private:
    std::map<int, Conductor*> _conductors;
    std::map<int, Worker*> _workers;

    JobMapping() {};
    JobMapping(const JobMapping &c) {};
    static JobMapping _Instance;
  };
}
}

#endif
