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

#include <string>
#include <cstdint>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#ifndef ARANGODB_PREGEL_AGGREGATOR_H
#define ARANGODB_PREGEL_AGGREGATOR_H 1

namespace arangodb {
namespace pregel {
    
    template<class T>
    class Aggregator {
    public:
        
        Aggregator(const Aggregator&) = delete;
        Aggregator& operator=(const Aggregator&) = delete;
        
        virtual T const& getValue() const = 0;
        virtual void reduce(T const& otherValue) = 0;
        std::string name() {return _name;}
        
    protected:
        Aggregator(std::string const& name) : _name(name) {}
        
    private:
        std::string _name;
    };
    
    class MinIntegerAggregator : public Aggregator<int64_t> {
        MinIntegerAggregator(std::string const& name, int64_t init) : Aggregator(name), _value(init) {}
        
        int64_t const& getValue() const override {
            return _value;
        };
        void reduce(int64_t const& otherValue) override {
            if (otherValue < _value) _value = otherValue;
        };
    private:
        int64_t _value;
    };
}
}
#endif
