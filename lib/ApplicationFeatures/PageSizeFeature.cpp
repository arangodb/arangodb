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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "PageSizeFeature.h"
#include "Logger/Logger.h"
#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

using namespace arangodb::basics;

namespace arangodb {

size_t PageSizeFeature::PageSize = 0;

PageSizeFeature::PageSizeFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "PageSize") {
  setOptional(false);
  startsAfter("GreetingsPhase");
}

void PageSizeFeature::prepare() {
  PageSize = static_cast<size_t>(getpagesize());
  LOG_TOPIC("c6b86", TRACE, arangodb::Logger::FIXME) << "page size is " << PageSize;
}

}  // namespace arangodb
