
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

#include "SkipResult.h"

#include "Cluster/ResultT.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

SkipResult::SkipResult() {}

SkipResult::SkipResult(SkipResult const& other) : _skipped{other._skipped} {}

auto SkipResult::getSkipCount() const noexcept -> size_t { return _skipped; }

auto SkipResult::didSkip(size_t skipped) -> void { _skipped += skipped; }

auto SkipResult::nothingSkipped() const noexcept -> bool {
  return _skipped == 0;
}

auto SkipResult::toVelocyPack(VPackBuilder& builder) const noexcept -> void {
  builder.add(VPackValue(_skipped));
}

auto SkipResult::fromVelocyPack(VPackSlice slice) -> ResultT<SkipResult> {
  if (!slice.isInteger()) {
    auto message = std::string{
        "When deserializating AqlExecuteResult: When reading skipped: "
        "Unexpected type "};
    message += slice.typeName();
    return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
  }
  try {
    SkipResult res;
    res.didSkip(slice.getNumber<std::size_t>());
    return res;
  } catch (velocypack::Exception const& ex) {
    auto message = std::string{
        "When deserializating AqlExecuteResult: When reading skipped: "};
    message += ex.what();
    return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
  }
}

auto arangodb::aql::operator+=(SkipResult& a, SkipResult const& b) noexcept -> SkipResult& {
  a.didSkip(b.getSkipCount());
  return a;
}

auto arangodb::aql::operator==(SkipResult const& a, SkipResult const& b) noexcept -> bool {
  return a.getSkipCount() == b.getSkipCount();
}