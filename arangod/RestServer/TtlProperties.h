#pragma once

#include <cstdint>

namespace arangodb {
class Result;
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

struct TtlProperties {
  static constexpr uint64_t minFrequency = 1 * 1000;  // milliseconds
  uint64_t frequency = 30 * 1000;                     // milliseconds
  uint64_t maxTotalRemoves = 1000000;
  uint64_t maxCollectionRemoves = 100000;

  void toVelocyPack(velocypack::Builder& out, bool isActive) const;
  Result fromVelocyPack(velocypack::Slice const& properties);
};

}  // namespace arangodb