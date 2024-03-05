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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "Metrics/Builder.h"

#include "Metrics/IBatch.h"

namespace arangodb::metrics {

std::string_view Builder::name() const noexcept { return _name; }
std::string_view Builder::labels() const noexcept { return _labels; }

void Builder::addLabel(std::string_view key, std::string_view value) {
  if (!_labels.empty()) {
    _labels.push_back(',');
  }
  _labels.append(key);
  _labels.push_back('=');
  _labels.push_back('"');
  _labels.append(value);
  _labels.push_back('"');
}

void Builder::reserveSpaceForLabels(size_t bytes) { _labels.reserve(bytes); }

IBatch::~IBatch() = default;

}  // namespace arangodb::metrics
