////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_SKIP_RESULT_H
#define ARANGOD_AQL_SKIP_RESULT_H

// for size_t
#include <cstddef>

namespace arangodb {
template <typename T>
class ResultT;
}
namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::aql {

class SkipResult {
 public:
  static auto fromVelocyPack(velocypack::Slice) -> ResultT<SkipResult>;

  SkipResult();

  ~SkipResult() = default;

  SkipResult(SkipResult const& other);

  auto getSkipCount() const noexcept -> size_t;

  auto didSkip(size_t skipped) -> void;

  auto nothingSkipped() const noexcept -> bool;

  auto toVelocyPack(arangodb::velocypack::Builder& builder) const noexcept -> void;

 private:
  size_t _skipped{0};
};

auto operator+=(SkipResult& a, SkipResult const& b) noexcept -> SkipResult&;

auto operator==(SkipResult const& a, SkipResult const& b) noexcept -> bool;

}  // namespace arangodb::aql
#endif