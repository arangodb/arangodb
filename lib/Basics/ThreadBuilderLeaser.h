#pragma once

#include "Basics/Exceptions.h"

#include <vector>
#include "velocypack/Builder.h"

namespace {
constexpr auto max_builders_per_thread = size_t{16};
}

namespace arangodb {

struct ThreadBuilderLeaser {
  struct Lease {
    ~Lease() {
      // put builder on builders vector unless capacity is reached
      if (_builder != nullptr) {
        current.returnBuilder(std::move(_builder));
      }
    }
    Lease(Lease&&) = default;
    auto operator=(Lease&&) -> Lease& = default;

    friend struct ThreadBuilderLeaser;

    auto builder() const -> velocypack::Builder* { return _builder.get(); }

   private:
    Lease(std::unique_ptr<velocypack::Builder>&& builder)
        : _builder(std::move(builder)) {
      TRI_ASSERT(_builder != nullptr);
    };

    std::unique_ptr<velocypack::Builder> _builder;
  };

  auto leaseBuilder() -> Lease {
    if (!_builders.empty()) {
      TRI_ASSERT(_builders.back() != nullptr);
      auto builder = std::move(_builders.back());
      builder->clear();
      TRI_ASSERT(builder != nullptr);
      _builders.pop_back();
      return Lease(std::move(builder));
    } else {
      return Lease(std::make_unique<velocypack::Builder>());
    }
  }

  static thread_local ThreadBuilderLeaser current;

  friend struct Lease;

 private:
  auto returnBuilder(std::unique_ptr<velocypack::Builder>&& builder) -> void {
    TRI_ASSERT(builder != nullptr);
    if (_builders.size() < max_builders_per_thread) {
      _builders.push_back(std::move(builder));
    }
  }

  std::vector<std::unique_ptr<velocypack::Builder>> _builders;
};

}  // namespace arangodb
