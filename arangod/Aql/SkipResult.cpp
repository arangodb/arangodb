
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
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

SkipResult::SkipResult() {}

SkipResult::SkipResult(SkipResult const& other) : _skipped{other._skipped} {}

auto SkipResult::getSkipCount() const noexcept -> size_t {
  TRI_ASSERT(!_skipped.empty());
  return _skipped.back();
}

auto SkipResult::didSkip(size_t skipped) -> void {
  TRI_ASSERT(!_skipped.empty());
  _skipped.back() += skipped;
}

auto SkipResult::didSkipSubquery(size_t skipped, size_t depth) -> void {
  TRI_ASSERT(!_skipped.empty());
  TRI_ASSERT(_skipped.size() > depth + 1);
  size_t index = _skipped.size() - depth - 2;
  size_t& localSkip = _skipped.at(index);
  localSkip += skipped;
}

auto SkipResult::getSkipOnSubqueryLevel(size_t depth) const -> size_t {
  TRI_ASSERT(!_skipped.empty());
  TRI_ASSERT(_skipped.size() > depth);
  return _skipped.at(_skipped.size() - 1 - depth);
}

auto SkipResult::nothingSkipped() const noexcept -> bool {
  TRI_ASSERT(!_skipped.empty());
  return std::all_of(_skipped.begin(), _skipped.end(),
                     [](size_t const& e) -> bool { return e == 0; });
}

auto SkipResult::toVelocyPack(VPackBuilder& builder) const noexcept -> void {
  VPackArrayBuilder guard(&builder);
  TRI_ASSERT(!_skipped.empty());
  for (auto const& s : _skipped) {
    builder.add(VPackValue(s));
  }
}

auto SkipResult::fromVelocyPack(VPackSlice slice) -> arangodb::ResultT<SkipResult> {
  if (!slice.isArray()) {
    auto message = std::string{
        "When deserializating AqlExecuteResult: When reading skipped: "
        "Unexpected type "};
    message += slice.typeName();
    return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
  }
  if (slice.isEmptyArray()) {
    auto message = std::string{
        "When deserializating AqlExecuteResult: When reading skipped: "
        "Got an empty list of skipped values."};
    return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
  }
  try {
    SkipResult res;
    auto it = VPackArrayIterator(slice);
    while (it.valid()) {
      auto val = it.value();
      if (!val.isInteger()) {
        auto message = std::string{
            "When deserializating AqlExecuteResult: When reading skipped: "
            "Unexpected type "};
        message += slice.typeName();
        return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
      }
      if (!it.isFirst()) {
        res.incrementSubquery();
      }
      res.didSkip(val.getNumber<std::size_t>());
      ++it;
    }
    return {res};
  } catch (velocypack::Exception const& ex) {
    auto message = std::string{
        "When deserializating AqlExecuteResult: When reading skipped: "};
    message += ex.what();
    return Result(TRI_ERROR_TYPE_ERROR, std::move(message));
  }
}

auto SkipResult::incrementSubquery() -> void { _skipped.emplace_back(0); }
auto SkipResult::decrementSubquery() -> void {
  TRI_ASSERT(!_skipped.empty());
  _skipped.pop_back();
  TRI_ASSERT(!_skipped.empty());
}
auto SkipResult::subqueryDepth() const noexcept -> size_t {
  TRI_ASSERT(!_skipped.empty());
  return _skipped.size();
}

auto SkipResult::reset() -> void {
  for (size_t i = 0; i < _skipped.size(); ++i) {
    _skipped[i] = 0;
  }
}

auto SkipResult::merge(SkipResult const& other, bool excludeTopLevel) noexcept -> void {
  _skipped.reserve(other.subqueryDepth());
  while (other.subqueryDepth() > subqueryDepth()) {
    incrementSubquery();
  }
  TRI_ASSERT(other._skipped.size() <= _skipped.size());
  for (size_t i = 0; i < other._skipped.size(); ++i) {
    if (excludeTopLevel && i + 1 == other._skipped.size()) {
      // Do not copy top level
      continue;
    }
    _skipped[i] += other._skipped[i];
  }
}

auto SkipResult::mergeOnlyTopLevel(SkipResult const& other) noexcept -> void {
  _skipped.reserve(other.subqueryDepth());
  while (other.subqueryDepth() > subqueryDepth()) {
    incrementSubquery();
  }
  _skipped.back() += other._skipped.back();
}

auto SkipResult::operator+=(SkipResult const& b) noexcept -> SkipResult& {
  didSkip(b.getSkipCount());
  return *this;
}

auto SkipResult::operator==(SkipResult const& b) const noexcept -> bool {
  if (_skipped.size() != b._skipped.size()) {
    return false;
  }
  for (size_t i = 0; i < _skipped.size(); ++i) {
    if (_skipped[i] != b._skipped[i]) {
      return false;
    }
  }
  return true;
}

auto SkipResult::operator!=(SkipResult const& b) const noexcept -> bool {
  return !(*this == b);
}
namespace arangodb::aql {
std::ostream& operator<<(std::ostream& stream, arangodb::aql::SkipResult const& result) {
  VPackBuilder temp;
  result.toVelocyPack(temp);
  stream << temp.toJson();
  return stream;
}
}  // namespace arangodb::aql
