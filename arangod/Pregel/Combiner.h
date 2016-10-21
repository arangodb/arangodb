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

#include <cstdint>

#ifndef ARANGODB_PREGEL_COMBINER_H
#define ARANGODB_PREGEL_COMBINER_H 1
namespace arangodb {
namespace pregel {
    
    // specify serialization, whatever
    template<class M>
    class Combiner {
    public:
        Combiner() {}
        
        Combiner(const Combiner&) = delete;
        Combiner& operator=(const Combiner&) = delete;
        
        virtual M combine(M const& firstValue, M const& secondValue) = 0;
    };
    
    class MinIntegerCombiner : public Combiner<int64_t> {
        MinIntegerCombiner() {}
        
        int64_t combine(int64_t const& firstValue, int64_t const& secondValue) override {
            return firstValue < secondValue ? firstValue : secondValue;
        };
    };
}
}
#endif
