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

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"
#include "Maskings/Collection.h"
#include "Maskings/ParseResult.h"

namespace arangodb {
namespace basics {
class StringBuffer;
}
namespace maskings {
class Maskings;

struct MaskingsResult {
  enum StatusCode : int {
    VALID,
    CANNOT_PARSE_FILE,
    CANNOT_READ_FILE,
    ILLEGAL_DEFINITION
  };

  MaskingsResult(StatusCode s, std::string const& m)
      : status(s), message(m), maskings(nullptr) {}
  explicit MaskingsResult(std::unique_ptr<Maskings>&& m)
      : status(StatusCode::VALID), maskings(std::move(m)) {}

  StatusCode status;
  std::string message;
  std::unique_ptr<Maskings> maskings;
};

class Maskings {
 public:
  static MaskingsResult fromFile(std::string const&);

 public:
  bool shouldDumpStructure(std::string const& name);
  bool shouldDumpData(std::string const& name);
  void mask(std::string const& name, basics::StringBuffer const& data,
            basics::StringBuffer& result);

  uint64_t randomSeed() const noexcept { return _randomSeed; }

 private:
  ParseResult<Maskings> parse(VPackSlice const&);
  VPackValue maskedItem(Collection& collection, std::vector<std::string>& path,
                        std::string& buffer, VPackSlice const& data);
  void addMaskedArray(Collection& collection, VPackBuilder& builder,
                      std::vector<std::string>& path, VPackSlice const& data);
  void addMaskedObject(Collection& collection, VPackBuilder& builder,
                       std::vector<std::string>& path, VPackSlice const& data);
  void addMasked(Collection& collection, VPackBuilder& builder, VPackSlice const& data);
  void addMasked(Collection& collection, basics::StringBuffer& data, VPackSlice const& slice);

 private:
  std::map<std::string, Collection> _collections;
  bool _hasDefaultCollection = false;
  Collection _defaultCollection;
  uint64_t _randomSeed = 0;
};

}  // namespace maskings
}  // namespace arangodb

#endif
