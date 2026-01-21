////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "TtlProperties.h"

#include "Basics/Exceptions.h"
#include "Basics/debugging.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {

void TtlProperties::toVelocyPack(velocypack::Builder& builder,
                                 bool isActive) const {
  builder.openObject();
  builder.add("frequency", velocypack::Value(frequency));
  builder.add("maxTotalRemoves", velocypack::Value(maxTotalRemoves));
  builder.add("maxCollectionRemoves", velocypack::Value(maxCollectionRemoves));
  // this attribute is hard-coded to false since v3.8, and will be removed later
  builder.add("onlyLoadedCollections", velocypack::Value(false));
  builder.add("active", velocypack::Value(isActive));
  builder.close();
}

Result TtlProperties::fromVelocyPack(velocypack::Slice const& slice) {
  if (!slice.isObject()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "expecting object for properties");
  }

  try {
    uint64_t frequency = this->frequency;
    uint64_t maxTotalRemoves = this->maxTotalRemoves;
    uint64_t maxCollectionRemoves = this->maxCollectionRemoves;

    if (slice.hasKey("frequency")) {
      if (!slice.get("frequency").isNumber()) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "expecting numeric value for frequency");
      }
      frequency = slice.get("frequency").getNumericValue<uint64_t>();
      TRI_IF_FAILURE("allow-low-ttl-frequency") {
        // for faster js tests we want to allow lower frequency values
      }
      else {
        if (frequency < TtlProperties::minFrequency) {
          return Result(TRI_ERROR_BAD_PARAMETER, "too low value for frequency");
        }
      }
    }
    if (slice.hasKey("maxTotalRemoves")) {
      if (!slice.get("maxTotalRemoves").isNumber()) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "expecting numeric value for maxTotalRemoves");
      }
      maxTotalRemoves = slice.get("maxTotalRemoves").getNumericValue<uint64_t>();
    }
    if (slice.hasKey("maxCollectionRemoves")) {
      if (!slice.get("maxCollectionRemoves").isNumber()) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "expecting numeric value for maxCollectionRemoves");
      }
      maxCollectionRemoves =
          slice.get("maxCollectionRemoves").getNumericValue<uint64_t>();
    }

    this->frequency = frequency;
    this->maxTotalRemoves = maxTotalRemoves;
    this->maxCollectionRemoves = maxCollectionRemoves;

    return Result();
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  }
}

}  // namespace arangodb
