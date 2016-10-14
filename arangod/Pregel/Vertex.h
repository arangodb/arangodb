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
#include "VocBase/vocbase.h"

#include "Message.h"
#include "InMessageCache.h"

#include <velocypack/velocypack-aliases.h>

#ifndef ARANGODB_PREGEL_VERTEX_H
#define ARANGODB_PREGEL_VERTEX_H 1
namespace arangodb {
namespace pregel {
  
  enum VertexActivationState {
    ACTIVE,
    STOPPED
  };
  
  class OutMessageCache;

  struct Edge {
      Edge(VPackSlice data);
    VPackSlice _data;
      
    //protected: virtual void loadData() = 0;
      
    int64_t _value;// demo
  };
  
  class Vertex {
    friend class Worker;
    friend class WorkerJob;

  public:
    //typedef std::iterator<std::forward_iterator_tag, VPackSlice> MessageIterator;
    Vertex(VPackSlice document);
    ~Vertex();
    void compute(int gss, MessageIterator const &messages, OutMessageCache* const cache);
    
    VertexActivationState state() {return _activationState;}
    std::vector<Edge> _edges;

  protected:
    void voteHalt() {_activationState = VertexActivationState::STOPPED;}
    int64_t _vertexState;// demo
    VPackSlice _data;

  private:
    VertexActivationState _activationState = VertexActivationState::ACTIVE;
  };
}
}
#endif
