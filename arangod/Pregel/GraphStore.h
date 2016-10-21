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

#ifndef ARANGODB_PREGEL_GRAPH_STATE_H
#define ARANGODB_PREGEL_GRAPH_STATE_H 1

#include <cstdio>
#include <cstdint>

namespace arangodb {
namespace pregel {
    
    struct EdgeEntry {
    private:
        size_t _dataLength;
        size_t _vertexIDSize;
        char _vertexID[1];
    public:
        EdgeEntry() : _vertexIDSize(0), _dataLength(0) {}
        ~EdgeEntry() {}
        
        size_t getDataLength() {return _dataLength;}
        size_t getSize() {return sizeof(EdgeEntry) + _dataLength;}
        std::string getToID() {
            return std::string(_vertexID, _vertexIDSize);
        }
    };
    
    template<typename E>
    class EdgeIterator {
    private:
        void *_begin, *_end, *_current;
        EdgeEntry *_entry = nullptr;
    public:
        typedef EdgeIterator iterator;
        typedef const EdgeIterator const_iterator;
        EdgeIterator(void *beginAddr, endAddr) : _begin(beginAddr), _end(endAddr), _current(_begin) {}
        
        iterator begin() {
            return EdgeIterator(_begin, _end);
        }
        const_iterator begin() const {
            return EdgeIterator(_begin, _end);
        }
        iterator end() {
            auto it = EdgeIterator(_begin, _end);
            it._current = it._end;
            return it;
        }
        const_iterator end() const {
            auto it = EdgeIterator(_begin, _end);
            it._current = it._end;
            return it;
        }
        
        // prefix ++
        EdgeIterator& operator++() {
            EdgeEntry *entry = static_cast<EdgeEntry>(_current);
            _current = _current + entry->getSize();
            return *this;
        }
        
        EdgeEntry* operator*() const {
            return _current != _end ? static_cast<EdgeEntry>(_current) : nullptr;
        }
    };
    
    struct VertexIndexEntry {
    private:
        size_t _vertexDataOffset;
        size_t _edgeDataOffset;
        bool _active;
        size_t _vertexIDSize;
        char _vertexID[1];
    public:
        VertexIndexEntry() : _vertexDataOffset(0), _edgeDataOffset(0), _active(true), _vertexIDSize(0) {}
        ~VertexIndexEntry() {}
        
        inline size_t getVertexDataOffset() {return _vertexDataOffset;}
        inline size_t getEdgeDataOffset() {return _edgeDataOffset;}
        inline size_t getSize() {return sizeof(VertexIndexEntry) + _vertexIDSize;}
        inline bool getActive() {
            return _active;
        }
        std::string getVertexID() {
            return std::string(_vertexID, _vertexIDSize);
        };
    };
    
    template<typename V>
    class VertexIterator {
    private:
        void *_begin, *_end;
    public:
        VertexIterator() {}
    };
    
    template<typename V>
    class Vertex;
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief carry graph data for a worker job
    ////////////////////////////////////////////////////////////////////////////////
    template<typename V, typename E>
    class GraphStore {
        friend class VertexIterator<V>;
        friend class EdgeIterator<E>;
    private:
        int _indexFd, _vertexFd, _edgeFd;
        void *_indexMapping, *_vertexMapping, *_edgeMapping;
        size_t _indexSize, _vertexSize, _edgeSize;
        
    public:
        GraphStore() {}
        
        VertexIterator<V> vertexIterator();
        EdgeIterator<E> edgeIterator(Vertex<V> const* v);
    };

}
}
#endif
