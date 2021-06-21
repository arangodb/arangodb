////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_SERIALIZABLE_H
#define VELOCYPACK_SERIALIZABLE_H 1

#include <memory>

#include "velocypack/velocypack-common.h"

namespace arangodb {
namespace velocypack {
class Builder;

class Serializable {
 public:
  virtual void toVelocyPack(Builder&) const = 0;

  // convenience method
  std::shared_ptr<Builder> toVelocyPack() const;

  virtual ~Serializable() = default;
};

struct Serialize {
  Serialize(Serializable const& sable) : _sable(sable) {}
  Serializable const& _sable;
};

}
}

#endif
