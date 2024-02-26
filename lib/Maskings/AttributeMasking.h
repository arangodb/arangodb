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

#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

#include "Maskings/MaskingFunction.h"
#include "Maskings/ParseResult.h"
#include "Maskings/Path.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace arangodb {
namespace maskings {
void InstallMaskings();

class AttributeMasking {
 public:
  static ParseResult<AttributeMasking> parse(Maskings*, velocypack::Slice def);
  static void installMasking(std::string const& name,
                             ParseResult<AttributeMasking> (*func)(
                                 Path, Maskings*, velocypack::Slice)) {
    _maskings[name] = func;
  }

 public:
  AttributeMasking() = default;

  AttributeMasking(Path path, std::shared_ptr<MaskingFunction> func)
      : _path(std::move(path)), _func(std::move(func)) {}

  bool match(std::vector<std::string_view> const&) const;

  MaskingFunction* func() const { return _func.get(); }

 private:
  static std::unordered_map<std::string,
                            ParseResult<AttributeMasking> (*)(
                                Path, Maskings*, velocypack::Slice)>
      _maskings;

 private:
  Path _path;
  std::shared_ptr<MaskingFunction> _func;
};
}  // namespace maskings
}  // namespace arangodb
