////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_VOCBASE_DEFAULTS_H
#define ARANGOD_VOC_BASE_VOCBASE_DEFAULTS_H 1

#include "Basics/Common.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace velocypack {
class Builder;
}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default settings
////////////////////////////////////////////////////////////////////////////////

struct TRI_vocbase_defaults_t {
  TRI_voc_size_t defaultMaximalSize;
  bool defaultWaitForSync;
  bool requireAuthentication;
  bool requireAuthenticationUnixSockets;
  bool authenticateSystemOnly;
  bool forceSyncProperties;

  void toVelocyPack(arangodb::velocypack::Builder&) const;

  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack() const;

  void applyToVocBase(TRI_vocbase_t*) const;
};

#endif
