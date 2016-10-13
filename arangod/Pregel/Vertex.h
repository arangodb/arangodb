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
    
    struct Message {
        Message(VPackSlice slice);
        
        int64_t _value; // demo
    };
    
  //template <typename T>
  class MessageIterator {
  public:
      MessageIterator() : _size(0) {}
      
      typedef MessageIterator iterator;
      typedef const MessageIterator const_iterator;
      
      explicit MessageIterator(VPackSlice slice) : _slice(slice) {
          if (_slice.isNull() || _slice.isNone()) _size = 0;
          else if (_slice.isArray()) _size = _slice.length();
          else _size = 1;
      }
      
      iterator begin() {
          return MessageIterator(_slice);
      }
      const_iterator begin() const {
          return MessageIterator(_slice);
      }
      iterator end() {
          auto it = MessageIterator(_slice);
          it._position = it._size;
          return it;
      }
      const_iterator end() const {
          auto it = MessageIterator(_slice);
          it._position = it._size;
          return it;
      }
      Message operator*() const {
          if (_slice.isArray()) {
              return Message(_slice.at(_position));
          } else {
              return Message(_slice);
          }
      }
      
      // prefix ++
      MessageIterator& operator++() {
          ++_position;
          return *this;
      }
      
      // postfix ++
      MessageIterator operator++(int) {
          MessageIterator result(*this);
          ++(*this);
          return result;
      }
      
      bool operator!=(MessageIterator const& other) const {
          return _position != other._position;
      }
      
      size_t size() const {
          return _size;
      }
      
  private:
      VPackSlice _slice;
      size_t _position = 0;
      size_t _size = 1;
  };
  
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
