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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

// NOTE: this files exists primarily to include these algorithm specfic structs
// in the
// cpp files to do template specialization

#ifndef ARANGODB_PREGEL_ALGO_DMID_MESSAGE_F_H
#define ARANGODB_PREGEL_ALGO_DMID_MESSAGE_F_H 1

#include "Pregel/Graph.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/MessageFormat.h"

namespace arangodb {
namespace pregel {

struct DMIDMessageFormat : public MessageFormat<DMIDMessage> {
  DMIDMessageFormat() {}
  void unwrapValue(VPackSlice s, DMIDMessage& message) const override {
    VPackArrayIterator array(s);
    message.senderId.shard = (PregelShard)((*array).getUInt());
    message.senderId.key = (*(++array)).copyString();
    message.leaderId.shard = (PregelShard)(*array).getUInt();
    message.leaderId.key = (*(++array)).copyString();
    message.weight = (*(++array)).getNumber<float>();
  }
  void addValue(VPackBuilder& arrayBuilder, DMIDMessage const& message) const override {
    arrayBuilder.openArray();
    arrayBuilder.add(VPackValue(message.senderId.shard));
    arrayBuilder.add(VPackValuePair(message.senderId.key.data(),
                                    message.senderId.key.size(),
                                    VPackValueType::String));
    arrayBuilder.add(VPackValue(message.leaderId.shard));
    arrayBuilder.add(VPackValuePair(message.leaderId.key.data(),
                                    message.leaderId.key.size(),
                                    VPackValueType::String));
    arrayBuilder.add(VPackValue(message.weight));
    arrayBuilder.close();
  }
};
}  // namespace pregel
}  // namespace arangodb
#endif
