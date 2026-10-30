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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ApiVersion.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <velocypack/Builder.h>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
class GeneralRequest;
class GeneralResponse;

namespace rest {
class RestHandler;

class RestHandlerFactory {
  RestHandlerFactory(RestHandlerFactory const&) = delete;
  RestHandlerFactory& operator=(RestHandlerFactory const&) = delete;

 public:
  // handler creator
  typedef std::shared_ptr<RestHandler> (*create_fptr)(
      application_features::ApplicationServer&, GeneralRequest*,
      GeneralResponse*, void* data);

  RestHandlerFactory();

  // creates a new handler; returns nullptr if no route matches
  std::shared_ptr<RestHandler> createHandler(
      application_features::ApplicationServer&, std::unique_ptr<GeneralRequest>,
      std::unique_ptr<GeneralResponse>) const;

  // adds a path and constructor to the factory
  void addHandler(std::string const& path, create_fptr,
                  std::initializer_list<ApiVersion> apiVersions,
                  void* data = nullptr);

  // adds a prefix path and constructor to the factory
  void addPrefixHandler(std::string const& path, create_fptr,
                        std::initializer_list<ApiVersion> apiVersions,
                        void* data = nullptr);

  // make the factory read-only (i.e. no new handlers can be added)
  void seal();

 private:
  using HandlerMap =
      std::unordered_map<std::string, std::pair<create_fptr, void*>>;

  struct VersionRoutes {
    ApiVersion version;
    HandlerMap constructors;
    // sorted by length descending once sealed
    std::vector<std::string> prefixes;
  };

  VersionRoutes* find(ApiVersion version);
  VersionRoutes const* find(ApiVersion version) const;

  // There are only ever a handful of versions, so a linear scan is enough.
  std::vector<VersionRoutes> _routes;

  // whether or not handlers can be added (sealed = false)
  bool _sealed;
};
}  // namespace rest
}  // namespace arangodb
