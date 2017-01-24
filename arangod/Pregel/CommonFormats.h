////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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


// NOTE: this files exists primarily to include these algorithm specfic structs in the
// cpp files to do template specialization

#ifndef ARANGODB_PREGEL_COMMON_MFORMATS_H
#define ARANGODB_PREGEL_COMMON_MFORMATS_H 1

#include "Pregel/GraphFormat.h"
#include "Pregel/MessageFormat.h"
#include "Pregel/Graph.h"


namespace arangodb {
namespace pregel {
  
struct SCCValue {
  std::vector<PregelID> parents;
  uint64_t vertexID;
  uint64_t color;
};

template<typename T>
struct SenderMessage {
  SenderMessage() {}
  SenderMessage(PregelID const& pid, T const& val)
    : pregelId(pid), value(val) {}
  
  PregelID pregelId;
  T value;
};
  
template <typename T>
struct SenderMessageFormat : public MessageFormat<SenderMessage<T>> {
  static_assert(std::is_arithmetic<T>::value, "Message type must be numeric");
  SenderMessageFormat() {}
  void unwrapValue(VPackSlice s, SenderMessage<T>& senderVal) const override {
    VPackArrayIterator array(s);
    senderVal.pregelId.shard = (*array).getUInt();
    senderVal.pregelId.key = (*(++array)).copyString();
    senderVal.value = (*(++array)).getNumber<T>();
  }
  void addValue(VPackBuilder& arrayBuilder, SenderMessage<T> const& senderVal) const override {
    arrayBuilder.openArray();
    arrayBuilder.add(VPackValue(senderVal.pregelId.shard));
    arrayBuilder.add(VPackValue(senderVal.pregelId.key));
    arrayBuilder.add(VPackValue(senderVal.value));
    arrayBuilder.close();
  }
};
  
}
}
#endif
