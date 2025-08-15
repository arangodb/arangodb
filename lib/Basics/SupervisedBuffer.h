#pragma once

#include "velocypack/Buffer.h"
#include "Basics/ResourceMonitor.h"
#include "Basics/ResourceUsage.h"

namespace arangodb {

class SupervisedBuffer : public arangodb::velocypack::Buffer<uint8_t> {
 public:
  SupervisedBuffer() = default;

  explicit SupervisedBuffer(ResourceMonitor& monitor)
      : _usageScope(std::make_unique<ResourceUsageScope>(monitor)),
        _usageScopeRaw(_usageScope.get()) {}

 protected:
  void grow(ValueLength length) override {
    auto currentCapacity = this->capacity();
    velocypack::Buffer<uint8_t>::grow(length);
    auto newCapacity = this->capacity();
    if (_usageScope && newCapacity > currentCapacity) {
      _usageScope->increase(newCapacity - currentCapacity);
    }
  }

  uint8_t* steal() noexcept override {
    uint8_t* ptr = velocypack::Buffer<uint8_t>::steal();
    if (_usageScope) {  // assume it exists without checking?
      _usageScope->steal();
    }
    return ptr;
  }

 private:
  std::unique_ptr<ResourceUsageScope> _usageScope;
  ResourceUsageScope* _usageScopeRaw{nullptr};
};

}  // namespace arangodb