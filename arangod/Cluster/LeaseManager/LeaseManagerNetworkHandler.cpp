////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "LeaseManagerNetworkHandler.h"

#include "Futures/Future.h"
#include "Cluster/LeaseManager/AbortLeaseInformation.h"
#include "Cluster/LeaseManager/AbortLeaseInformationInspector.h"
#include "Network/Methods.h"
#include "Cluster/ServerState.h"

using namespace arangodb::cluster;

LeaseManagerNetworkHandler::LeaseManagerNetworkHandler(
    network::ConnectionPool* pool)
    : _pool{pool} {}

auto LeaseManagerNetworkHandler::abortIds(
    ServerID const& server, std::vector<LeaseId> const& leasedFrom,
    std::vector<LeaseId> const& leasedTo) const noexcept
    -> futures::Future<Result> {
  static auto path = "/_admin/leases";
  VPackBufferUInt8 buffer;
  auto info = AbortLeaseInformation{
      .server = {.serverId = ServerState::instance()->getId(),
                 .rebootId = ServerState::instance()->getRebootId()},
      .leasedFrom = leasedFrom,
      .leasedTo = leasedTo};
  {
    VPackBuilder builder(buffer);
    arangodb::velocypack::serialize(builder, info);
  }
  network::RequestOptions opts;
  auto f = network::sendRequest(_pool, "server:" + server,
                                arangodb::fuerte::RestVerb::Delete, path,
                                std::move(buffer), opts);

  return std::move(f).thenValue([](network::Response result) -> Result {
    if (result.fail() || !fuerte::statusIsSuccess(result.statusCode())) {
      return result.combinedResult();
    }
    TRI_ASSERT(result.slice().get("error").isFalse());
    return Result{};
  });
}
