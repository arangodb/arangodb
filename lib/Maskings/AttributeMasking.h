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

#ifndef ARANGODB_MASKINGS_ATTRIBUTE_MASKING_H
#define ARANGODB_MASKINGS_ATTRIBUTE_MASKING_H 1

#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Maskings/MaskingFunction.h"
#include "Maskings/ParseResult.h"
#include "Maskings/Path.h"

namespace arangodb {
namespace maskings {
void InstallMaskings();
  
class AttributeMasking {
 public:
  static ParseResult<AttributeMasking> parse(Maskings*, VPackSlice const&);
  static void installMasking(std::string const& name, ParseResult<AttributeMasking> (* func)(Path, Maskings*, VPackSlice const&)) {
    _maskings[name] = func;
  }

 public:
  AttributeMasking() = default;

  AttributeMasking(Path const& path, MaskingFunction* func) : _path(path) {
    _func.reset(func);
  }

  bool match(std::vector<std::string> const&) const;

  MaskingFunction* func() const { return _func.get(); }

 private:
  static std::unordered_map<std::string, ParseResult<AttributeMasking> (*)(Path, Maskings*, VPackSlice const&)> _maskings;

 private:
  Path _path;
  std::shared_ptr<MaskingFunction> _func;
};
}  // namespace maskings
}  // namespace arangodb

#endif
