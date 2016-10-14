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

#include "Basics/Common.h"
#include "VocBase/vocbase.h"
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#ifndef ARANGODB_PREGEL_MESSAGE_H
#define ARANGODB_PREGEL_MESSAGE_H 1
namespace arangodb {
namespace pregel {
    struct Message {
        Message(VPackSlice slice);
        
        int64_t _value; // demo
    };
    
    // struct IntMessage final : public Message ...
    // struct FloatMessage final : public Message ...
    // struct VelocyMessage final : public Message ...
}
}
#endif
