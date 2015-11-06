////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_DUMP_H
#define VELOCYPACK_DUMP_H 1

#include <string>

#include "velocypack/velocypack-common.h"
#include "velocypack/Buffer.h"
#include "velocypack/Dumper.h"

namespace arangodb {
  namespace velocypack {

    // some alias types for easier usage
    typedef Dumper<CharBuffer, false> BufferDumper;
    typedef Dumper<std::string, false> StringDumper;
    typedef Dumper<std::string, true>  StringPrettyDumper;

  }  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
