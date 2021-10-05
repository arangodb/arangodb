////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/BindParameters.h"
#include "Basics/Exceptions.h"
#include "Basics/ResourceUsage.h"
#include "Basics/debugging.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

#include <utility>

using namespace arangodb::aql;

BindParameters::BindParameters(ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor), 
      _processed(false) {}

BindParameters::BindParameters(ResourceMonitor& resourceMonitor,
                               std::shared_ptr<arangodb::velocypack::Builder> builder)
    : _resourceMonitor(resourceMonitor),
      _builder(std::move(builder)), 
      _processed(false) {
  process();
}
    
BindParameters::~BindParameters() {
  std::size_t memoryUsed = 0;
  for (auto const& it : _parameters) {
    memoryUsed += memoryUsage(it.first, it.second.first);
  }
  _resourceMonitor.decreaseMemoryUsage(memoryUsed);
}

/// @brief create a hash value for the bind parameters
uint64_t BindParameters::hash() const {
  if (_builder == nullptr) {
    return 0xdeadbeef;
  }
  // we can get away with the fast hash function here, as we're only using the
  // hash for quickly checking whether we've seen the same set of parameters
  return _builder->slice().hash();
}

/// @brief mark a bind parameter as "used", and return its value.
/// will return VPackSlice::noneSlice() if the bind parameter does not exist!
VPackSlice BindParameters::markUsed(std::string const& name) noexcept {
  TRI_ASSERT(_processed);

  auto it = _parameters.find(name);
  if (it == _parameters.end()) {
    return VPackSlice::noneSlice();
  }

  // mark the bind parameter as being used
  (*it).second.second = true;
        
  // return parameter value
  TRI_ASSERT(!(*it).second.first.isNone());
  return(*it).second.first;
}
  
/// @brief run a visitor function on all bind parameters
void BindParameters::visit(std::function<void(std::string const& key, arangodb::velocypack::Slice value, bool used)> const& visitor) const {
  for (auto const& it : _parameters) {
    visitor(it.first, it.second.first, it.second.second);
  }
}

/// @brief strip collection name prefixes from the parameters
/// the values must be a VelocyPack array
void BindParameters::stripCollectionNames(VPackSlice keys,
                                          std::string const& collectionName,
                                          VPackBuilder& result) {
  char const* c = collectionName.c_str();

  TRI_ASSERT(keys.isArray());
  result.openArray();
  for (VPackSlice element : VPackArrayIterator(keys)) {
    if (element.isString()) {
      VPackValueLength l;
      char const* s = element.getString(l);
      auto p = static_cast<char const*>(memchr(s, '/', static_cast<size_t>(l)));
      if (p != nullptr && strncmp(s, c, p - s) == 0) {
        // key begins with collection name + '/', now strip it in place for
        // further comparisons
        result.add(VPackValue(
         std::string(p + 1, static_cast<size_t>(l - static_cast<std::ptrdiff_t>(p - s) - 1))));
        continue;
      }
    }
    result.add(element);
  }
  result.close();
}

std::shared_ptr<arangodb::velocypack::Builder> BindParameters::builder() const {
  return _builder;
}

/// @brief process the parameters
void BindParameters::process() {
  if (_builder == nullptr || _builder->slice().isNone()) {
    _processed = true;
  }
  
  if (_processed) {
    return;
  }
    
  TRI_ASSERT(_builder != nullptr);
  VPackSlice slice = _builder->slice();
  TRI_ASSERT(!slice.isNone());

  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID);
  }

  for (auto it : VPackObjectIterator(slice, false)) {
    std::string key(it.key.copyString());
    TRI_ASSERT(!key.empty());

    VPackSlice value(it.value);

    if (value.isNone()) {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, key.c_str());
    }

    if (key[0] == '@' && !value.isString()) {
      // collection bind parameter
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, key.c_str());
    }

    ResourceUsageScope guard(_resourceMonitor, memoryUsage(key, value));

    _parameters.try_emplace(std::move(key), value, false);

    // now we are responsible for tracking the memory usage
    guard.steal();
  }

  _processed = true;
}
    
std::size_t BindParameters::memoryUsage(std::string const& key, VPackSlice value) const noexcept {
  return 32 + key.size() + value.byteSize();
}
