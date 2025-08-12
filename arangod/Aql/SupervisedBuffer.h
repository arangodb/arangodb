#pragma once


#include "Aql/ResourceMonitor.h"
#include "Aql/ResourceUsageScope.h"

#include <vector>



namespace arangodb {

template<typename Byte = uint8_t>
class SupervisedBuffer {
 public:

  explicit SupervisedBuffer(ResourceUsageScope& scope)
      : _usageScope(&scope), _resourceMonitor(nullptr) {}


  explicit SupervisedBuffer(ResourceMonitor& monitor)
      : _usageScope(nullptr), _resourceMonitor(&monitor) {}


  SupervisedBuffer() : _usageScope(nullptr), _resourceMonitor(nullptr) {}

  void appendByte(Byte byte) {
    reserve(_data.size() + 1);
    _data.push_back(byte);
  }

  void append(Byte const* data, size_t length) {
    reserve(_data.size() + length);
    _data.insert(_data.end(), data, data + length);
  }


  void reserve(size_t amountBytes) {
    size_t currentCapacity = _data.capacity();
    if (amountBytes > currentCapacity) {
      if (_usageScope) {
        _usageScope->increase(n - oldCap);
      } else if (_resourceMonitor) {
        _resourceMonitor->increaseMemoryUsage(n - oldCap);
      }
      _data.reserve(amountBytes);
    }
  }




  void steal() {
    if (_usageScope) {
      _usageScope->steal();
    }
  }

  const Byte* data() const noexcept { return _data.data(); }
  size_t size() const noexcept { return _data.size(); }
  size_t capacity() const noexcept { return _data.capacity(); }

 private:
  std::vector<Byte> _data;
  ResourceUsageScope* _usageScope;
  ResourceMonitor* _resourceMonitor;
};
}  // namespace arangodb
