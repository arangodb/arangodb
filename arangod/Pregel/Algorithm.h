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
//#include "Aggregator.h"
#include "Combiner.h"
#include "VertexComputation.h"

#include <cstdint>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#ifndef ARANGODB_PREGEL_ALGORITHM_H
#define ARANGODB_PREGEL_ALGORITHM_H 1
namespace arangodb {
namespace pregel {
  
  template <typename V, typename E, typename M>
  class VertexComputation;
    
    // specify serialization, whatever
    template <typename V, typename E, typename M>
    class Algorithm {
    public:
        
        virtual bool isFixpointAlgorithm() {return false;}
        
        virtual bool preserveTransactions() {return false;}
        virtual ptrdiff_t estimatedVertexSize() = 0;
        
        virtual M unwrapMessage(VPackSlice body) = 0;
        virtual void addMessageValue(VPackBuilder &b, M const& value) = 0;
        
        virtual V* createVertexData(VPackSlice vertexDocument, intptr_t startAdress, intptr_t &endAdress) = 0;
        virtual E* createEdgeData(VPackSlice edgeDocument, V *const vertexData, intptr_t startAdress, intptr_t &endAdress) = 0;

        virtual Combiner<M> * createCombiner() = 0;
        
        virtual VertexComputation<V, E, M> * createComputation();
      //virtual void compute(int gss, VertexIndexEntry* v, V* vertexData, EdgeIterator<E> const&, MessageIterator<M> const &) = 0;
        
      //virtual void aggregate(std::string aggregatorName, VPackSlice value) = 0;
      //virtual VPackSlice aggregatedValue(std::string aggregatorName) = 0;
        
        std::string const& getName() {return _name;}
        
    protected:
        Algorithm(std::string const& name) : _name(name) {};
        std::string _name;
    };
    
    class SSSPAlgorithm : public Algorithm<int64_t, int64_t, int64_t> {
    public:
        SSSPAlgorithm() : Algorithm("SSSP") {}
    private:
      //MinIntegerAggregator _minAggregator("min", INT64_MAX);
    };
}
}
#endif
