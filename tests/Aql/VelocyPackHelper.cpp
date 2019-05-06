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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "VelocyPackHelper.h"

using namespace arangodb;
using namespace arangodb::tests::aql;

VPackBufferPtr arangodb::tests::aql::vpackFromJsonString(char const* c) {
  velocypack::Options options;
  options.checkAttributeUniqueness = true;
  velocypack::Parser parser(&options);
  parser.parse(c);

  std::shared_ptr<velocypack::Builder> builder = parser.steal();

  return builder->steal();
}

VPackBufferPtr arangodb::tests::aql::operator"" _vpack(const char* json, size_t) {
  return vpackFromJsonString(json);
}
