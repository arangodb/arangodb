////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_HTTP_SERVER_HTTP_HANDLER_FACTORY_H
#define ARANGOD_HTTP_SERVER_HTTP_HANDLER_FACTORY_H 1

#include "Basics/Common.h"

#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Rest/HttpResponse.h"

namespace arangodb {
class GeneralRequest;
class GeneralResponse;
struct ConnectionInfo;

namespace rest {
class RestHandler;

class RestHandlerFactory {
  RestHandlerFactory(RestHandlerFactory const&) = delete;
  RestHandlerFactory& operator=(RestHandlerFactory const&) = delete;

 public:
  // handler creator
  typedef RestHandler* (*create_fptr)(GeneralRequest*, GeneralResponse*,
                                      void* data);

  // context handler
  typedef bool (*context_fptr)(GeneralRequest*, void*);

 public:
  // cppcheck-suppress *
  RestHandlerFactory(context_fptr, void*);

 public:
  // sets maintenance mode
  static void setMaintenance(bool);

  // checks maintenance mode
  static bool isMaintenance();

 public:
  // set request context, wrapper method
  bool setRequestContext(GeneralRequest*);

  // creates a new handler
  RestHandler* createHandler(std::unique_ptr<GeneralRequest>,
                             std::unique_ptr<GeneralResponse>);

  // adds a path and constructor to the factory
  void addHandler(std::string const& path, create_fptr, void* data = nullptr);

  // adds a prefix path and constructor to the factory
  void addPrefixHandler(std::string const& path, create_fptr,
                        void* data = nullptr);

  // adds a path and constructor to the factory
  void addNotFoundHandler(create_fptr);

 private:
  // set context callback
  context_fptr _setContext;

  // set context data
  void* _contextData;

  // list of constructors
  std::unordered_map<std::string, std::pair<create_fptr, void*>> _constructors;

  // list of prefix handlers
  std::vector<std::string> _prefixes;

  // constructor for a not-found handler
  create_fptr _notFound;

 private:
  static std::atomic<bool> _maintenanceMode;
};
}
}

#endif
