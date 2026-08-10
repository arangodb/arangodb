
#include "TtlProperties.h"

#include "Basics/debugging.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
void TtlProperties::toVelocyPack(VPackBuilder& builder, bool isActive) const {
  builder.openObject();
  builder.add("frequency", VPackValue(frequency));
  builder.add("maxTotalRemoves", VPackValue(maxTotalRemoves));
  builder.add("maxCollectionRemoves", VPackValue(maxCollectionRemoves));
  // this attribute is hard-coded to false since v3.8, and will be removed later
  builder.add("onlyLoadedCollections", VPackValue(false));
  builder.add("active", VPackValue(isActive));
  builder.close();
}

Result TtlProperties::fromVelocyPack(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "expecting object for properties");
  }

  try {
    uint64_t _frequency = this->frequency;
    uint64_t _maxTotalRemoves = this->maxTotalRemoves;
    uint64_t _maxCollectionRemoves = this->maxCollectionRemoves;

    if (slice.hasKey("frequency")) {
      if (!slice.get("frequency").isNumber()) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "expecting numeric value for frequency");
      }
      _frequency = slice.get("frequency").getNumericValue<uint64_t>();
      TRI_IF_FAILURE("allow-low-ttl-frequency") {
        // for faster js tests we want to allow lower frequency values
      }
      else {
        if (_frequency < TtlProperties::minFrequency) {
          return Result(TRI_ERROR_BAD_PARAMETER, "too low value for frequency");
        }
      }
    }
    if (slice.hasKey("maxTotalRemoves")) {
      if (!slice.get("maxTotalRemoves").isNumber()) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "expecting numeric value for maxTotalRemoves");
      }
      _maxTotalRemoves =
          slice.get("maxTotalRemoves").getNumericValue<uint64_t>();
    }
    if (slice.hasKey("maxCollectionRemoves")) {
      if (!slice.get("maxCollectionRemoves").isNumber()) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "expecting numeric value for maxCollectionRemoves");
      }
      _maxCollectionRemoves =
          slice.get("maxCollectionRemoves").getNumericValue<uint64_t>();
    }

    this->frequency = _frequency;
    this->maxTotalRemoves = _maxTotalRemoves;
    this->maxCollectionRemoves = _maxCollectionRemoves;

    return Result();
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  }
}

}  // namespace arangodb
