////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "FakeReplicatedState.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;

void replicated_state::EntrySerializer<test::DefaultEntryType>::operator()(
    streams::serializer_tag_t<test::DefaultEntryType>,
    const test::DefaultEntryType& e, velocypack::Builder& b) const {
  velocypack::ObjectBuilder ob(&b);
  b.add("key", velocypack::Value(e.key));
  b.add("value", velocypack::Value(e.value));
}

auto replicated_state::EntryDeserializer<test::DefaultEntryType>::operator()(
    streams::serializer_tag_t<test::DefaultEntryType>,
    velocypack::Slice s) const -> test::DefaultEntryType {
  auto key = s.get("key").copyString();
  auto value = s.get("value").copyString();
  return test::DefaultEntryType{.key = key, .value = value};
}
