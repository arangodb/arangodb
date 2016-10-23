////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "Job.h"

#include "GeneralServer/RestHandler.h"

using namespace arangodb;
using namespace arangodb::rest;

Job::Job(std::function<void(std::shared_ptr<RestHandler>)> callback)
    : _server(nullptr), _handler(nullptr), _callback(callback) {}

Job::Job(rest::GeneralServer* server, std::shared_ptr<RestHandler> handler,
         std::function<void(std::shared_ptr<RestHandler>)> callback)
    : _server(server), _handler(std::move(handler)), _callback(callback) {}

// trival, but needs definition of RestHandler
Job::~Job() {}
