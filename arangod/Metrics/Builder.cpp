////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "Metrics/Builder.h"

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

}  // namespace arangodb::metrics
