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
#include <velocypack/Iterator.h>
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
  public:
    std::string edgeId;
    std::string toId;
    
    int value;// demo
  };
  
  class Vertex {
    friend class Worker;
  public:
    //typedef std::iterator<std::forward_iterator_tag, VPackSlice> MessageIterator;
    Vertex(VPackSlice document);
    ~Vertex();
    void compute(int gss, VPackArrayIterator const &messages, OutMessageCache* const cache);
    
    VertexActivationState state() {return _activationState;}
    //std::vector<VPackSlice> messages() {return _messages;}
    
    std::vector<Edge*> _edges;
    std::string documentId;

  protected:
    void voteHalt() {_activationState = VertexActivationState::STOPPED;}
    //void sendMessage(VPackBuilder &message) {_messages.push_back(message.slice());}

  private:
    VertexActivationState _activationState;
    //std::vector<VPackSlice> _messages;
    
    int _vertexState;// demo
  };

}
}
#endif
