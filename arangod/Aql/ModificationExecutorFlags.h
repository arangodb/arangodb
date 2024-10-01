////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Christoph Uhde
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace arangodb::aql {

struct BoolWrapper {
  explicit BoolWrapper(bool b) { _value = b; }
  operator bool() const noexcept { return _value; }
  bool _value;
};

struct ProducesResults : BoolWrapper {
  explicit ProducesResults(bool b) : BoolWrapper(b) {}
};
struct ConsultAqlWriteFilter : BoolWrapper {
  explicit ConsultAqlWriteFilter(bool b) : BoolWrapper(b) {}
};
struct IgnoreErrors : BoolWrapper {
  explicit IgnoreErrors(bool b) : BoolWrapper(b) {}
};
struct DoCount : BoolWrapper {
  explicit DoCount(bool b) : BoolWrapper(b) {}
};
struct IsReplace : BoolWrapper {
  explicit IsReplace(bool b) : BoolWrapper(b) {}
};
struct IgnoreDocumentNotFound : BoolWrapper {
  explicit IgnoreDocumentNotFound(bool b) : BoolWrapper(b) {}
};

}  // namespace arangodb::aql
