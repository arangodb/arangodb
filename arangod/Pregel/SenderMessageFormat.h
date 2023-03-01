///////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Pregel/MessageFormat.h"

namespace arangodb::pregel {
template<typename T>
struct SenderMessageFormat : public MessageFormat<SenderMessage<T>> {
  static_assert(std::is_arithmetic<T>::value, "Message type must be numeric");
  SenderMessageFormat() = default;
  void unwrapValue(VPackSlice s, SenderMessage<T>& senderVal) const override {
    VPackArrayIterator array(s);
    senderVal.senderId.shard =
        PregelShard(static_cast<PregelShard::value_type>((*array).getUInt()));
    senderVal.senderId.key = (*(++array)).copyString();
    senderVal.value = (*(++array)).getNumber<T>();
  }
  void addValue(VPackBuilder& arrayBuilder,
                SenderMessage<T> const& senderVal) const override {
    arrayBuilder.openArray();
    arrayBuilder.add(VPackValue(senderVal.senderId.shard));
    arrayBuilder.add(VPackValuePair(senderVal.senderId.key.data(),
                                    senderVal.senderId.key.size(),
                                    VPackValueType::String));
    arrayBuilder.add(VPackValue(senderVal.value));
    arrayBuilder.close();
  }
};
}  // namespace arangodb::pregel
