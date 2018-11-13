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

#include "Maskings.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/FileUtils.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::maskings;

MaskingsResult Maskings::fromFile(std::string const& filename) {
  std::string definition;

  try {
    definition = basics::FileUtils::slurp(filename);
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::CONFIG)
        << "cannot read maskings file '" << filename << "': " << e.what();

    return MaskingsResult{MaskingsResult::CANNOT_READ_FILE, nullptr};
  }

  LOG_TOPIC(DEBUG, Logger::CONFIG) << "found maskings file '" << filename;

  if (definition.empty()) {
    LOG_TOPIC(ERR, Logger::CONFIG)
        << "maskings file '" << filename << "' is empty";
    return MaskingsResult{MaskingsResult::CANNOT_READ_FILE, nullptr};
  }

  try {
    std::shared_ptr<VPackBuilder> parsed =
        velocypack::Parser::fromJson(definition);
  } catch (velocypack::Exception const& e) {
    LOG_TOPIC(ERR, Logger::CONFIG)
        << "cannot parse maskings file '" << filename << "': " << e.what()
        << ". file content: " << definition;

    return MaskingsResult{MaskingsResult::CANNOT_PARSE_FILE, nullptr};
  }

  std::unique_ptr<Maskings> maskings;

  return MaskingsResult{MaskingsResult::VALID, std::move(maskings)};
}
