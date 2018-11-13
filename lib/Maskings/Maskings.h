////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_MASKINGS_MASKINGS_H
#define ARANGODB_MASKINGS_MASKINGS_H 1

#include "Basics/Common.h"

#include "Maskings/Collection.h"

namespace arangodb {
namespace maskings {
class Maskings;

struct MaskingsResult {
  enum StatusCode : int {
    VALID,
    CANNOT_PARSE_FILE,
    CANNOT_READ_FILE
  };

  StatusCode status;
  std::unique_ptr<Maskings> maskings;
};

class Maskings {
 public:
  static MaskingsResult fromFile(std::string const&);

 private:
  std::map<std::string, Collection> _collections;
};

}  // namespace maskings
}  // namespace arangodb

#endif
