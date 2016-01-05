////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase database defaults
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_VOCBASE__DEFAULTS_H
#define ARANGODB_VOC_BASE_VOCBASE__DEFAULTS_H 1

#include "Basics/Common.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

struct TRI_vocbase_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

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

  // -----------------------------------------------------------------------------
  // --SECTION--                                                  public
  // functions
  // -----------------------------------------------------------------------------

  void toVelocyPack(VPackBuilder&) const;

  std::shared_ptr<VPackBuilder> toVelocyPack() const;

  void applyToVocBase(TRI_vocbase_t*) const;
};

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|//
// --SECTION--\\|/// @\\}"
// End:
