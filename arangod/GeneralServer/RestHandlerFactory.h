////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Basics/Common.h"
#include "RestServer/arangod.h"

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
  typedef std::shared_ptr<RestHandler> (*create_fptr)(ArangodServer&,
                                                      GeneralRequest*,
                                                      GeneralResponse*,
                                                      void* data);

  RestHandlerFactory();

  // creates a new handler
  std::shared_ptr<RestHandler> createHandler(
      ArangodServer&, std::unique_ptr<GeneralRequest>,
      std::unique_ptr<GeneralResponse>) const;

  // adds a path and constructor to the factory
  void addHandler(std::string const& path, create_fptr, void* data = nullptr);

  // adds a prefix path and constructor to the factory
  void addPrefixHandler(std::string const& path, create_fptr,
                        void* data = nullptr);

  // make the factory read-only (i.e. no new handlers can be added)
  void seal();

 private:
  // list of constructors
  std::unordered_map<std::string, std::pair<create_fptr, void*>> _constructors;

  // list of prefix handlers
  std::vector<std::string> _prefixes;

  // whether or not handlers can be added (sealed = false)
  bool _sealed;
};
}  // namespace rest
}  // namespace arangodb
