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
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "RbacFeature.h"

#include "Auth/Rbac/BackendImpl.h"
#include "Auth/Rbac/ServiceImpl.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Network/NetworkFeature.h"

namespace arangodb {

std::atomic<RbacFeature*> RbacFeature::INSTANCE{nullptr};

RbacFeature::RbacFeature(application_features::ApplicationServer& server)
    : application_features::ApplicationFeature{server, *this} {
  setOptional(true);
  startsAfter<NetworkFeature>();
  startsAfter<AuthenticationFeature>();
  startsBefore<GeneralServerFeature>();
}

RbacFeature::~RbacFeature() = default;

void RbacFeature::prepare() {
  auto* auth = AuthenticationFeature::instance();
  TRI_ASSERT(auth != nullptr);

  auto endpoint = auth->externalRBACservice();
  if (endpoint.empty()) {
    LOG_TOPIC("5a0e2", DEBUG, Logger::AUTHORIZATION)
        << "External RBAC service not configured, RBAC is disabled";
    INSTANCE.store(this, std::memory_order_release);
    return;
  }

  auto* pool = server().getFeature<NetworkFeature>().pool();
  TRI_ASSERT(pool != nullptr);

  auto backend =
      std::make_unique<rbac::BackendImpl>(*pool, std::string{endpoint});
  _service = std::make_unique<rbac::ServiceImpl>(std::move(backend));

  LOG_TOPIC("66f4a", INFO, Logger::AUTHORIZATION)
      << "External RBAC service enabled, endpoint: " << endpoint;

  INSTANCE.store(this, std::memory_order_release);
}

void RbacFeature::unprepare() {
  INSTANCE.store(nullptr, std::memory_order_release);
  _service.reset();
}

RbacFeature* RbacFeature::instance() noexcept {
  return INSTANCE.load(std::memory_order_acquire);
}

rbac::Service* RbacFeature::service() const noexcept {
  return _service.get();
}

}  // namespace arangodb
