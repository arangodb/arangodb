////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "GeneralServer/AuthenticationOptionsProvider.h"
#include "GeneralServer/GeneralServerOptionsProvider.h"
#include "GeneralServer/SslServerOptionsProvider.h"
#include "Network/NetworkOptionsProvider.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/DatabasePathOptionsProvider.h"
#include "RestServer/DumpLimitsOptionsProvider.h"
#include "RestServer/EndpointOptionsProvider.h"
#include "RestServer/FlushOptionsProvider.h"
#include "RestServer/FortuneOptionsProvider.h"
#include "RestServer/FrontendOptionsProvider.h"
#include "RestServer/InitDatabaseOptionsProvider.h"
#include "RestServer/ServerOptionsProvider.h"
#include "RestServer/TemporaryStorageOptionsProvider.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillOptionsProvider.h"
#include "RocksDBEngine/RocksDBOptionFeatureOptionsProvider.h"
#include "RocksDBEngine/RocksDBEngineOptionsProvider.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Ssl/SslServerEEOptionsProvider.h"
#endif

#ifdef USE_V8
#include "RestServer/FrontendOptionsProvider.h"
#endif

#ifdef TRI_HAVE_GETRLIMIT
#include "RestServer/FileDescriptorsOptionsProvider.h"
#endif

#include <tuple>

namespace arangodb::application_features {
class FeatureOptionProviderContainer final {
 public:
  void declareOptions(std::shared_ptr<options::ProgramOptions> programOptions);
  void validateOptions(std::shared_ptr<options::ProgramOptions> programOptions);

  template<typename ProviderType>
  auto& getOptions() const {
    return std::get<ProviderType>(_providers).options();
  }

 private:
  std::tuple<AuthenticationOptionsProvider, DatabasePathOptionsProvider,
             DumpLimitsOptionsProvider, EndpointOptionsProvider,
             FlushOptionsProvider, fortune::FortuneOptionsProvider,
             GeneralServerOptionsProvider, InitDatabaseOptionsProvider,
             NetworkOptionsProvider, RocksDBEngineOptionsProvider,
             RocksDBIndexCacheRefillOptionsProvider,
             RocksDBOptionFeatureOptionsProvider, ServerOptionsProvider,
             SslServerOptionsProvider, TemporaryStorageOptionsProvider
#ifdef USE_ENTERPRISE
             ,
             enterprise::SslServerEEOptionsProvider
#endif
#ifdef USE_V8
             ,
             FrontendOptionsProvider
#endif
#ifdef TRI_HAVE_GETRLIMIT
             ,
             file_descriptors::FileDescriptorsOptionsProvider
#endif
             >
      _providers{};
};
}  // namespace arangodb::application_features
