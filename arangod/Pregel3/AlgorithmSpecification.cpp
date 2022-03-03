#include "AlgorithmSpecification.h"
#include "Utils.h"

#include <cctype>

using namespace arangodb;
using namespace arangodb::pregel3;

template<typename Type>
auto getProp(VPackSlice const slice, char const* propertyName)
    -> ResultT<Type> {
  std::string propName(propertyName);
  TRI_ASSERT(!propName.empty());
  if (!slice.hasKey(propertyName)) {
    return ResultT<Type>::error(
        TRI_ERROR_BAD_PARAMETER,
        "Algorithm specification must have a(n) " + propName + " field.");
  }
  VPackSlice propertyValue = slice.get(propertyName);
  propName[0] = static_cast<char>(toupper(propName[0]));  // for a nicer error
  if constexpr (std::is_same_v<Type, std::string>) {
    if (!propertyValue.isString()) {
      return ResultT<std::string>::error(TRI_ERROR_BAD_PARAMETER,
                                         propName + " should be a string.");
    }
    return propertyValue.copyString();
  }
  if constexpr (std::is_same_v<Type, double>) {
    if (!propertyValue.isDouble()) {
      return ResultT<double>::error(TRI_ERROR_BAD_PARAMETER,
                                    propName + " should be a double.");
    }
    return propertyValue.getDouble();
  }
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
    auto res = getProp<std::string>(slice, Utils::algorithmName);
    if (res.fail()) {
      return ResultT<AlgorithmSpecification>{res.result()};
    } else {
      algorithmSpecification.algName = res.get();
    }
  }

  if (algorithmSpecification.algName == "MinCut") {
    {
      auto res = getProp<std::string>(slice, Utils::capacityProp);
      if (res.fail()) {
        return ResultT<AlgorithmSpecification>{res.result()};
      } else {
        algorithmSpecification.capacityProp = res.get();
      }
    }

    {
      auto res = getProp<double>(slice, Utils::defaultCapacity);
      if (res.fail()) {
        return ResultT<AlgorithmSpecification>{res.result()};
      } else {
        algorithmSpecification.defaultCapacity = res.get();
      }
    }

    {
      auto res = getProp<std::string>(slice, Utils::sourceVertexId);
      if (res.fail()) {
        return ResultT<AlgorithmSpecification>{res.result()};
      } else {
        algorithmSpecification.sourceVertexId = res.get();
      }
    }

    {
      auto res = getProp<std::string>(slice, Utils::targetVertexId);
      if (res.fail()) {
        return ResultT<AlgorithmSpecification>{res.result()};
      } else {
        algorithmSpecification.targetVertexId = res.get();
      }
    }
  }
  return algorithmSpecification;
}
