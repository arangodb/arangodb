#include "AlgorithmSpecification.h"
#include "Utils.h"

#include <cctype>

using namespace arangodb;
using namespace arangodb::pregel3;

auto getStringProp(VPackSlice const slice, char const* propertyName)
    -> ResultT<std::string> {
  std::string propName(propertyName);
  TRI_ASSERT(!propName.empty());
  if (!slice.hasKey(propertyName)) {
    return ResultT<std::string>::error(
        TRI_ERROR_BAD_PARAMETER,
        "Algorithm specification must have a " + propName + " field.");
  }
  VPackSlice propertyValue = slice.get(propertyName);
  if (!propertyValue.isString()) {
    propName[0] = static_cast<char>(toupper(propName[0]));
    return ResultT<std::string>::error(TRI_ERROR_BAD_PARAMETER,
                                       propName + " should be a string.");
  }
  return propertyValue.copyString();
}

auto AlgorithmSpecification::fromVelocyPack(VPackSlice slice)
    -> ResultT<AlgorithmSpecification> {
  AlgorithmSpecification algorithmSpecification;

  if (!slice.isObject()) {
    return ResultT<AlgorithmSpecification>::error(
        ErrorCode(TRI_ERROR_BAD_PARAMETER),
        "Algorithm specification must be an object.");
  }

  {
    auto res = getStringProp(slice, Utils::algorithmName);
    if (res.fail()) {
      return ResultT<AlgorithmSpecification>{res.result()};
    } else {
      algorithmSpecification.algName = res.get();
    }
  }

  {
    auto res = getStringProp(slice, Utils::capacityProp);
    if (res.fail()) {
      return ResultT<AlgorithmSpecification>{res.result()};
    } else {
      algorithmSpecification.capacityProp = res.get();
    }
  }

  {
    auto res = getStringProp(slice, Utils::defaultCapacity);
    if (res.fail()) {
      return ResultT<AlgorithmSpecification>{res.result()};
    } else {
      algorithmSpecification.defaultCapacity = res.get();
    }
  }

  {
    auto res = getStringProp(slice, Utils::sourceVertexId);
    if (res.fail()) {
      return ResultT<AlgorithmSpecification>{res.result()};
    } else {
      algorithmSpecification.sourceVertexId = res.get();
    }
  }

  {
    auto res = getStringProp(slice, Utils::targetVertexId);
    if (res.fail()) {
      return ResultT<AlgorithmSpecification>{res.result()};
    } else {
      algorithmSpecification.targetVertexId = res.get();
    }
  }
  return algorithmSpecification;
}
