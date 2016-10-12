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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Utils.h"
#include "Basics/StringUtils.h"
#include "VocBase/vocbase.h"

using namespace arangodb::pregel;

std::string const Utils::apiPrefix = "/_api/pregel/";


std::string const Utils::nextGSSPath = "nextGSS";
std::string const Utils::finishedGSSPath = "finishedGSS";
std::string const Utils::messagesPath = "messages";
std::string const Utils::writeResultsPath = "writeResults";

std::string const Utils::executionNumberKey = "extn";
std::string const Utils::vertexCollectionKey = "vertexCollection";
std::string const Utils::vertexShardsListKey = "vertexShards";
std::string const Utils::edgeShardsListKey = "edgeShards";
std::string const Utils::resultShardKey = "resultShard";
std::string const Utils::coordinatorIdKey = "coordinatorId";
std::string const Utils::algorithmKey = "algorithm";
std::string const Utils::globalSuperstepKey = "gss";
std::string const Utils::messagesKey = "msgs";
std::string const Utils::senderKey = "sender";
std::string const Utils::doneKey = "done";

std::string Utils::baseUrl(std::string dbName) {
  return  "/_db/" + basics::StringUtils::urlEncode(dbName) + Utils::apiPrefix;
}
