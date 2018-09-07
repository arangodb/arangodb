////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "AgencyStrings.h"

#include <string>

using namespace arangodb::consensus;

std::string const arangodb::consensus::DATABASES = "Databases";
std::string const arangodb::consensus::COLLECTIONS = "Collections";
std::string const arangodb::consensus::VERSION = "Version";

std::string const arangodb::consensus::CURRENT = "Current"; 
std::string const arangodb::consensus::CURRENT_VERSION = CURRENT + VERSION;
std::string const arangodb::consensus::CURRENT_COLLECTIONS = CURRENT + "/" + COLLECTIONS + "/";
std::string const arangodb::consensus::CURRENT_DATABASES = CURRENT + "/" + DATABASES + "/";

std::string const arangodb::consensus::PLAN = "Plan"; 
std::string const arangodb::consensus::PLAN_VERSION = PLAN + VERSION; 
std::string const arangodb::consensus::PLAN_COLLECTIONS = PLAN + "/" + COLLECTIONS + "/"; 
std::string const arangodb::consensus::PLAN_DATABASES = PLAN + "/" + DATABASES + "/";


