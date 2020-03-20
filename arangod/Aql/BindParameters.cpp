////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/debugging.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

#include <utility>

using namespace arangodb::aql;

/// @brief create a hash value for the bind parameters
uint64_t BindParameters::hash() const {
  if (_builder == nullptr) {
    return 0xdeadbeef;
  }
  // we can get away with the fast hash function here, as we're only using the
  // hash for quickly checking whether we've seen the same set of parameters
  return _builder->slice().hash();
}

/// @brief process the parameters
void BindParameters::process() {
  if (_processed || _builder == nullptr) {
    return;
  }

  VPackSlice const slice = _builder->slice();
  if (slice.isNone()) {
    return;
  }

  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID);
  }

  for (auto it : VPackObjectIterator(slice, false)) {
    auto const key(it.key.copyString());
    VPackSlice const value(it.value);

    if (value.isNone()) {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, key.c_str());
    }

    if (key[0] == '@' && !value.isString()) {
      // collection bind parameter
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, key.c_str());
    }

    _parameters.try_emplace(std::move(key), value, false);
  }

  _processed = true;
}

/// @brief strip collection name prefixes from the parameters
/// the values must be a VelocyPack array
void BindParameters::stripCollectionNames(VPackSlice const& keys,
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

BindParameters::BindParameters()
    : _parameters(), _processed(false) {}

BindParameters::BindParameters(std::shared_ptr<arangodb::velocypack::Builder> builder)
    : _builder(std::move(builder)), _parameters(), _processed(false) {}

BindParametersType& BindParameters::get() {
  process();
  return _parameters;
}

std::shared_ptr<arangodb::velocypack::Builder> BindParameters::builder() const {
  return _builder;
}
