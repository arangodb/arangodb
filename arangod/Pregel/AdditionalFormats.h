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

#ifndef ARANGODB_PREGEL_ADDITIONAL_MFORMATS_H
#define ARANGODB_PREGEL_ADDITIONAL_MFORMATS_H 1


#include "Pregel/MessageFormat.h"
#include "Pregel/Graph.h"


namespace arangodb {
namespace pregel {

template<typename T>
struct SenderValue {
  PregelID pregelId;
  T value;
};
  
template <typename T>
struct NumberSenderFormat : public MessageFormat<SenderValue<T>> {
  static_assert(std::is_arithmetic<T>::value, "Message type must be numeric");
  NumberSenderFormat() {}
  void unwrapValue(VPackSlice s, SenderValue<T>& senderVal) const override {
    VPackArrayIterator array(s);
    senderVal.pregelId.shard = (*array).getUInt();
    senderVal.pregelId.key = (*(++array)).copyString();
    senderVal.value = (*(++array)).getNumber<T>();
  }
  void addValue(VPackBuilder& arrayBuilder, SenderValue<T> const& senderVal) const override {
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
