////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Inspection/VPack.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include <Basics/Exceptions.h>
#include <Basics/StaticStrings.h>
#include <Basics/application-exit.h>
#include <Basics/debugging.h>
#include <Basics/overload.h>
#include <Logger/LogMacros.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include "LogStatus.h"

using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

auto QuickLogStatus::getCurrentTerm() const noexcept -> std::optional<LogTerm> {
  if (role == ParticipantRole::kUnconfigured) {
    return std::nullopt;
  }
  return term;
}

auto QuickLogStatus::getLocalStatistics() const noexcept
    -> std::optional<LogStatistics> {
  if (role == ParticipantRole::kUnconfigured) {
    return std::nullopt;
  }
  return local;
}

auto LogStatus::getCurrentTerm() const noexcept -> std::optional<LogTerm> {
  return std::visit(
      overload{
          [&](replicated_log::UnconfiguredStatus) -> std::optional<LogTerm> {
            return std::nullopt;
          },
          [&](replicated_log::LeaderStatus const& s) -> std::optional<LogTerm> {
            return s.term;
          },
          [&](replicated_log::FollowerStatus const& s)
              -> std::optional<LogTerm> { return s.term; },
      },
      _variant);
}

auto LogStatus::getLocalStatistics() const noexcept
    -> std::optional<LogStatistics> {
  return std::visit(
      overload{
          [&](replicated_log::UnconfiguredStatus const& s)
              -> std::optional<LogStatistics> { return std::nullopt; },
          [&](replicated_log::LeaderStatus const& s)
              -> std::optional<LogStatistics> { return s.local; },
          [&](replicated_log::FollowerStatus const& s)
              -> std::optional<LogStatistics> { return s.local; },
      },
      _variant);
}

auto LogStatus::fromVelocyPack(VPackSlice slice) -> LogStatus {
  auto role = slice.get("role");
  if (role.isEqualString(StaticStrings::Leader)) {
    return LogStatus{velocypack::deserialize<LeaderStatus>(slice)};
  } else if (role.isEqualString(StaticStrings::Follower)) {
    return LogStatus{velocypack::deserialize<FollowerStatus>(slice)};
  } else {
    return LogStatus{velocypack::deserialize<UnconfiguredStatus>(slice)};
  }
}

LogStatus::LogStatus(UnconfiguredStatus status) noexcept : _variant(status) {}
LogStatus::LogStatus(LeaderStatus status) noexcept
    : _variant(std::move(status)) {}
LogStatus::LogStatus(FollowerStatus status) noexcept
    : _variant(std::move(status)) {}

auto LogStatus::getVariant() const noexcept -> VariantType const& {
  return _variant;
}

auto LogStatus::toVelocyPack(velocypack::Builder& builder) const -> void {
  std::visit([&](auto const& s) { velocypack::serialize(builder, s); },
             _variant);
}

auto LogStatus::asLeaderStatus() const noexcept -> LeaderStatus const* {
  return std::get_if<LeaderStatus>(&_variant);
}
auto LogStatus::asFollowerStatus() const noexcept -> FollowerStatus const* {
  return std::get_if<FollowerStatus>(&_variant);
}

namespace {
inline constexpr std::string_view kSupervision = "supervision";
inline constexpr std::string_view kLeaderId = "leaderId";
}  // namespace

auto GlobalStatus::toVelocyPack(velocypack::Builder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  builder.add(VPackValue(kSupervision));
  supervision.toVelocyPack(builder);
  {
    VPackObjectBuilder ob2(&builder, StaticStrings::Participants);
    for (auto const& [id, status] : participants) {
      builder.add(VPackValue(id));
      status.toVelocyPack(builder);
    }
  }
  builder.add(VPackValue("specification"));
  specification.toVelocyPack(builder);
  if (leaderId.has_value()) {
    builder.add(kLeaderId, VPackValue(*leaderId));
  }
}

auto GlobalStatus::fromVelocyPack(VPackSlice slice) -> GlobalStatus {
  GlobalStatus status;
  auto sup = slice.get(kSupervision);
  TRI_ASSERT(!sup.isNone())
      << "expected " << kSupervision << " key in GlobalStatus";
  status.supervision = SupervisionStatus::fromVelocyPack(sup);
  status.specification =
      Specification::fromVelocyPack(slice.get("specification"));
  for (auto [key, value] :
       VPackObjectIterator(slice.get(StaticStrings::Participants))) {
    auto id = ParticipantId{key.copyString()};
    auto stat = ParticipantStatus::fromVelocyPack(value);
    status.participants.emplace(std::move(id), stat);
  }
  if (auto leaderId = slice.get(kLeaderId); !leaderId.isNone()) {
    status.leaderId = leaderId.copyString();
  }
  return status;
}

void GlobalStatusConnection::toVelocyPack(
    arangodb::velocypack::Builder& b) const {
  velocypack::ObjectBuilder ob(&b);
  b.add(StaticStrings::ErrorCode, VPackValue(error));
  if (!errorMessage.empty()) {
    b.add(StaticStrings::ErrorMessage, VPackValue(errorMessage));
  }
}

auto GlobalStatusConnection::fromVelocyPack(arangodb::velocypack::Slice slice)
    -> GlobalStatusConnection {
  auto code = ErrorCode(slice.get(StaticStrings::ErrorCode).extract<int>());
  auto message = std::string{};
  if (auto ms = slice.get(StaticStrings::ErrorMessage); !ms.isNone()) {
    message = ms.copyString();
  }
  return GlobalStatusConnection{.error = code, .errorMessage = message};
}

void GlobalStatus::ParticipantStatus::Response::toVelocyPack(
    arangodb::velocypack::Builder& b) const {
  if (std::holds_alternative<LogStatus>(value)) {
    std::get<LogStatus>(value).toVelocyPack(b);
  } else {
    TRI_ASSERT(std::holds_alternative<velocypack::UInt8Buffer>(value));
    auto slice = VPackSlice(std::get<velocypack::UInt8Buffer>(value).data());
    b.add(slice);
  }
}

auto GlobalStatus::ParticipantStatus::Response::fromVelocyPack(
    arangodb::velocypack::Slice s)
    -> GlobalStatus::ParticipantStatus::Response {
  if (s.hasKey("role")) {
    return Response{.value = LogStatus::fromVelocyPack(s)};
  } else {
    auto buffer = velocypack::UInt8Buffer(s.byteSize());
    buffer.append(s.start(), s.byteSize());
    return Response{.value = buffer};
  }
}

void GlobalStatus::ParticipantStatus::toVelocyPack(
    arangodb::velocypack::Builder& b) const {
  velocypack::ObjectBuilder ob(&b);
  b.add(VPackValue("connection"));
  connection.toVelocyPack(b);
  if (response.has_value()) {
    b.add(VPackValue("response"));
    response->toVelocyPack(b);
  }
}

auto GlobalStatus::ParticipantStatus::fromVelocyPack(
    arangodb::velocypack::Slice s) -> GlobalStatus::ParticipantStatus {
  auto connection = GlobalStatusConnection::fromVelocyPack(s.get("connection"));
  auto response = std::optional<Response>{};
  if (auto rs = s.get("response"); !rs.isNone()) {
    response = Response::fromVelocyPack(rs);
  }
  return ParticipantStatus{.connection = std::move(connection),
                           .response = std::move(response)};
}

void GlobalStatus::SupervisionStatus::toVelocyPack(
    arangodb::velocypack::Builder& b) const {
  velocypack::ObjectBuilder ob(&b);
  b.add(VPackValue("connection"));
  connection.toVelocyPack(b);
  if (response.has_value()) {
    b.add(VPackValue("response"));
    velocypack::serialize(b, response);
  }
}

auto GlobalStatus::SupervisionStatus::fromVelocyPack(
    arangodb::velocypack::Slice s) -> GlobalStatus::SupervisionStatus {
  auto connection = GlobalStatusConnection::fromVelocyPack(s.get("connection"));
  auto const response =
      std::invoke([&s]() -> std::optional<agency::LogCurrentSupervision> {
        if (auto rs = s.get("response"); !rs.isNone()) {
          return velocypack::deserialize<agency::LogCurrentSupervision>(rs);
        }
        return std::nullopt;
      });
  return SupervisionStatus{.connection = std::move(connection),
                           .response = std::move(response)};
}

auto replicated_log::to_string(GlobalStatus::SpecificationSource source)
    -> std::string_view {
  switch (source) {
    case GlobalStatus::SpecificationSource::kLocalCache:
      return "LocalCache";
    case GlobalStatus::SpecificationSource::kRemoteAgency:
      return "RemoteAgency";
    default:
      return "(unknown)";
  }
}

auto replicated_log::to_string(ParticipantRole role) noexcept
    -> std::string_view {
  switch (role) {
    case ParticipantRole::kUnconfigured:
      return "Unconfigured";
    case ParticipantRole::kLeader:
      return "Leader";
    case ParticipantRole::kFollower:
      return "Follower";
  }
  LOG_TOPIC("e3242", ERR, Logger::REPLICATION2)
      << "Unhandled participant role: "
      << static_cast<std::underlying_type_t<decltype(role)>>(role);
  TRI_ASSERT(false);
  return "(unknown status code)";
}

void GlobalStatus::Specification::toVelocyPack(
    arangodb::velocypack::Builder& b) const {
  velocypack::ObjectBuilder ob(&b);
  b.add(VPackValue("plan"));
  velocypack::serialize(b, plan);
  b.add("source", VPackValue(to_string(source)));
}

auto GlobalStatus::Specification::fromVelocyPack(arangodb::velocypack::Slice s)
    -> Specification {
  auto plan =
      velocypack::deserialize<agency::LogPlanSpecification>(s.get("plan"));
  auto source = GlobalStatus::SpecificationSource::kLocalCache;
  if (s.get("source").isEqualString("RemoteAgency")) {
    source = GlobalStatus::SpecificationSource::kRemoteAgency;
  }
  return Specification{.source = source, .plan = std::move(plan)};
}

auto ParticipantRoleStringTransformer::toSerialized(ParticipantRole source,
                                                    std::string& target) const
    -> arangodb::inspection::Status {
  target = to_string(source);
  return {};
}

auto ParticipantRoleStringTransformer::fromSerialized(
    std::string const& source, ParticipantRole& target) const
    -> arangodb::inspection::Status {
  if (source == "Unconfigured") {
    target = ParticipantRole::kUnconfigured;
  } else if (source == "Leader") {
    target = ParticipantRole::kLeader;
  } else if (source == "Follower") {
    target = ParticipantRole::kFollower;
  } else {
    return inspection::Status{"Invalid participant role name: " +
                              std::string{source}};
  }
  return {};
}
