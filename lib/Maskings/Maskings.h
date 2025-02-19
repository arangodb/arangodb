////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Maskings/Collection.h"
#include "Maskings/ParseResult.h"

namespace arangodb::maskings {
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

  bool shouldDumpStructure(std::string const& name);
  bool shouldDumpData(std::string const& name);
  void mask(std::string const& name, velocypack::Slice data,
            velocypack::Builder& builder) const;

  uint64_t randomSeed() const noexcept { return _randomSeed; }

 private:
  ParseResult<Maskings> parse(velocypack::Slice def);
  void maskedItem(Collection const& collection,
                  std::vector<std::string_view>& path, velocypack::Slice data,
                  velocypack::Builder& out, std::string& buffer) const;
  void addMaskedArray(Collection const& collection,
                      std::vector<std::string_view>& path,
                      velocypack::Slice data, velocypack::Builder& builder,
                      std::string& buffer) const;
  void addMaskedObject(Collection const& collection,
                       std::vector<std::string_view>& path,
                       velocypack::Slice data, velocypack::Builder& builder,
                       std::string& buffer) const;
  void addMasked(Collection const& collection, VPackBuilder& out,
                 velocypack::Slice data) const;

 private:
  std::map<std::string, Collection> _collections;
  bool _hasDefaultCollection = false;
  Collection _defaultCollection;
  uint64_t _randomSeed = 0;
};

}  // namespace arangodb::maskings
