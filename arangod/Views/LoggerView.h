////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VIEWS_LOGGER_VIEW_H
#define ARANGOD_VIEWS_LOGGER_VIEW_H 1

#include "Basics/Common.h"
#include "Logger/Logger.h"
#include "VocBase/ViewImplementation.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
class PhysicalView;
class Result;

class LoggerView final : public ViewImplementation {
 public:
  static std::string type;
  static std::unique_ptr<ViewImplementation> creator(
      LogicalView*, arangodb::velocypack::Slice const&);

 private:
  struct ConstructionGuard {
    ConstructionGuard() {}
  };

 public:
  LoggerView(ConstructionGuard const& guard, LogicalView* logical,
             arangodb::velocypack::Slice const& info);
  ~LoggerView() = default;

  arangodb::Result updateProperties(arangodb::velocypack::Slice const& slice,
                                    bool doSync);
  arangodb::Result persistProperties() noexcept;

  /// @brief export properties
  void getPropertiesVPack(velocypack::Builder&) const;

  /// @brief opens an existing view
  void open(bool ignoreErrors);

  void drop();

 protected:
  LogicalView* _logicalView;
  arangodb::LogLevel _level;
};

typedef std::function<std::unique_ptr<ViewImplementation>(
    LogicalView*, arangodb::velocypack::Slice const&)>
    ViewCreator;

}  // namespace arangodb

#endif
