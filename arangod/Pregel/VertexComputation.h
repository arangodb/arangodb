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

#include "Basics/Common.h"
#include <cstddef>

#ifndef ARANGODB_PREGEL_COMPUTATION_H
#define ARANGODB_PREGEL_COMPUTATION_H 1
namespace arangodb {
namespace pregel {
  /*
  enum VertexActivationState {
    ACTIVE,
    STOPPED
  };*/
  
  template <typename V, typename E, typename M>
  class VertexEntry;
  
  template <typename M>
  class MessageIterator;
  
  template <typename M>
  class OutgoingCache;
  
  template <typename V, typename E, typename M>
  class VertexComputation {
    friend class Worker;
  private:
    unsigned int gss;
    OutgoingCache<M> *outgoing;
    
  public:
    
    VertexComputation() {}
    unsigned int getGlobalSuperstep() {return gss;}
    
    
    virtual void compute(VertexEntry<V> *const entry, MessageIterator<M> const& iterator)
    void sendMessage(std::string const& toValue, M const& data);
  };

}
}
#endif
