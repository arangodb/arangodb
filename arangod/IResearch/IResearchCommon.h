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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_COMMON_H
#define ARANGOD_IRESEARCH__IRESEARCH_COMMON_H 1

#include "Logger/LogTopic.h"
#include "VocBase/LogicalDataSource.h"

namespace arangodb {
namespace iresearch {

arangodb::LogicalDataSource::Type const& dataSourceType();
arangodb::LogTopic& logTopic();

static auto& DATA_SOURCE_TYPE = dataSourceType();
static auto& TOPIC = logTopic();

} // iresearch
} // arangodb

#endif