////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////
#pragma once

// NOTE: This is a helper Class to make Progress.
// The following code will be implemented properly inside the Inspection Code
// to get some way of foot-gun protection.
// We can then erase the following code.

#include <velocypack/Slice.h>
#include <velocypack/HashedStringRef.h>
#include <string_view>

#include "Inspection/VPack.h"

namespace arangodb {
namespace inspection {
template<>
struct Access<arangodb::velocypack::Slice>
    : public AccessBase<arangodb::velocypack::Slice> {
  template<class Inspector>
  static auto apply(Inspector& inspector, arangodb::velocypack::Slice& target) {
    if constexpr (Inspector::isLoading) {
      target = inspector.slice();
    } else {
      inspector.builder().add(target);
    }
    return Status::Success{};
  }
};

template<>
struct Access<arangodb::velocypack::HashedStringRef>
    : public AccessBase<arangodb::velocypack::HashedStringRef> {
  template<class Inspector>
  static auto apply(Inspector& inspector,
                    arangodb::velocypack::HashedStringRef& target) {
    if constexpr (Inspector::isLoading) {
      target = inspector.slice();
    } else {
      inspector.builder().add(VPackValue(target.stringView()));
    }
    return Status::Success{};
  }
};

template<>
struct Access<std::string_view> : public AccessBase<std::string_view> {
  template<class Inspector>
  static auto apply(Inspector& inspector, std::string_view& target) {
    if constexpr (Inspector::isLoading) {
      target = inspector.slice().stringView();
    } else {
      inspector.builder().add(VPackValue(target));
    }
    return Status::Success{};
  }
};

}  // namespace inspection
}  // namespace arangodb
