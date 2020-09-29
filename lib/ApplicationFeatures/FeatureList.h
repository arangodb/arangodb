////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_LIB_FEATURE_LIST_H
#define ARANGODB_LIB_FEATURE_LIST_H 1

#include <initializer_list>
#include <functional>
#include <cstddef>

#include "Basics/TypeInfo.h"

#include <frozen/map.h>

namespace arangodb {

template <typename... T>
class TypeList {
 public:
  constexpr static auto Length = sizeof...(T);

  static constexpr std::initializer_list<std::pair<TypeInfo::TypeId, size_t>> toList() {
    return toListImpl(std::make_integer_sequence<size_t, Length>());
  }

 private:
  template<size_t... Idx>
  using Indices = std::integer_sequence<size_t, Length>;

  template<size_t... Idx>
  static constexpr std::initializer_list<std::pair<TypeInfo::TypeId, size_t>> toListImpl(
      std::integer_sequence<size_t, Idx...>) {
    return { std::pair<TypeInfo::TypeId, size_t>{ Type<T>::id(), Idx }... };
  }
};

// the features
class DaemonFeature;
class SupervisorFeature;
class PrivilegeFeature;
class V8SecurityFeature;
class V8DealerFeature;
class V8PlatformFeature;
class HttpEndpointProvider;
class MetricsFeature;
class ConfigFeature;
class VersionFeature;
class RandomFeature;
class LoggerFeature;
class ShellColorsFeature;
class ShutdownFeature;
class ViewTypesFeature;
class ClusterFeature;
class ClusterUpgradeFeature;
class SslFeature;
class TempFeature;
class ConsoleFeature;
class ActionFeature;
class AgencyFeature;
class AqlFeature;
class AuthenticationFeature;
class BootstrapFeature;
class CacheManagerFeature;
class CheckVersionFeature;
class DatabaseFeature;
class DatabasePathFeature;
class EndpointFeature;
class EngineSelectorFeature;
class EnvironmentFeature;
class FileDescriptorsFeature;
class FlushFeature;
class FortuneFeature;
class FoxxQueuesFeature;
class FrontendFeature;
class GeneralServerFeature;
class GreetingsFeature;
class InitDatabaseFeature;
class MaintenanceFeature;
class MaxMapCountFeature;
class NetworkFeature;
class NonceFeature;
class QueryRegistryFeature;
class ReplicationFeature;
class ReplicationMetricsFeature;
class ReplicationTimeoutFeature;
class RocksDBOptionFeature;
class RocksDBRecoveryManager;
class SchedulerFeature;
class ScriptFeature;
class ServerFeature;
class ServerIdFeature;
class ServerSecurityFeature;
class ShardingFeature;
class StatisticsFeature;
class StorageEngineFeature;
class SystemDatabaseFeature;
class TtlFeature;
class UpgradeFeature;
class LanguageCheckFeature;
class LanguageFeature;
class LockfileFeature;
class LogBufferFeature;
class ClusterEngine;
class RocksDBEngine;

#ifdef _WIN32
class WindowsServiceFeature;
#endif

#ifdef USE_ENTERPRISE
setupServerEE(server); // FIXME
#else
class SslServerFeature;
#endif

class V8ShellFeature;
class BenchFeature;
class ShellFeature;
class RestoreFeature;
class ExportFeature;
class ImportFeature;
class DumpFeature;
class VPackFeature;

namespace application_features {

// the Phases
class AgencyFeaturePhase;
class CommunicationFeaturePhase;
class AqlFeaturePhase;
class BasicFeaturePhaseServer;
class ClusterFeaturePhase;
class DatabaseFeaturePhase;
class FinalFeaturePhase;
class FoxxFeaturePhase;
class GreetingsFeaturePhase;
class ServerFeaturePhase;
class V8FeaturePhase;

class V8ShellFeaturePhase;
class BasicFeaturePhaseClient;

} // application_features

namespace iresearch {
class IResearchAnalyzerFeature;
class IResearchFeature;
} // iresearch

namespace transaction {
class ManagerFeature;
} // transaction

namespace aql {
class AqlFunctionFeature;
class OptimizerRulesFeature;
} // aql

namespace pregel {
class PregelFeature;
} // pregel

using FeatureList = TypeList<
  application_features::AgencyFeaturePhase,
  application_features::CommunicationFeaturePhase,
  application_features::AqlFeaturePhase,
  application_features::BasicFeaturePhaseServer,
  application_features::ClusterFeaturePhase,
  application_features::DatabaseFeaturePhase,
  application_features::FinalFeaturePhase,
  application_features::FoxxFeaturePhase,
  application_features::GreetingsFeaturePhase,
  application_features::ServerFeaturePhase,
  application_features::V8FeaturePhase,
  arangodb::ActionFeature,
  arangodb::AgencyFeature,
  arangodb::AqlFeature,
  arangodb::AuthenticationFeature,
  arangodb::BootstrapFeature,
  arangodb::CacheManagerFeature,
  arangodb::CheckVersionFeature,
  arangodb::ClusterFeature,
  arangodb::ClusterUpgradeFeature,
  arangodb::ConfigFeature,
  arangodb::ConsoleFeature,
  arangodb::DatabaseFeature,
  arangodb::DatabasePathFeature,
  arangodb::EndpointFeature,
  arangodb::HttpEndpointProvider,
  arangodb::EngineSelectorFeature,
  arangodb::EnvironmentFeature,
  arangodb::FileDescriptorsFeature,
  arangodb::FlushFeature,
  arangodb::FortuneFeature,
  arangodb::FoxxQueuesFeature,
  arangodb::FrontendFeature,
  arangodb::GeneralServerFeature,
  arangodb::GreetingsFeature,
  arangodb::InitDatabaseFeature,
  arangodb::LanguageCheckFeature,
  arangodb::LanguageFeature,
  arangodb::LockfileFeature,
  arangodb::LogBufferFeature,
  arangodb::LoggerFeature,
  arangodb::MaintenanceFeature,
  arangodb::MaxMapCountFeature,
  arangodb::MetricsFeature,
  arangodb::NetworkFeature,
  arangodb::NonceFeature,
  arangodb::PrivilegeFeature,
  arangodb::QueryRegistryFeature,
  arangodb::RandomFeature,
  arangodb::ReplicationFeature,
  arangodb::ReplicationMetricsFeature,
  arangodb::ReplicationTimeoutFeature,
  arangodb::RocksDBOptionFeature,
  arangodb::RocksDBRecoveryManager,
  arangodb::SchedulerFeature,
  arangodb::ScriptFeature,
  arangodb::ServerFeature,
  arangodb::ServerIdFeature,
  arangodb::ServerSecurityFeature,
  arangodb::ShardingFeature,
  arangodb::ShellColorsFeature,
  arangodb::ShutdownFeature,
  arangodb::SslFeature,
  arangodb::StatisticsFeature,
  arangodb::StorageEngineFeature,
  arangodb::SystemDatabaseFeature,
  arangodb::TempFeature,
  arangodb::TtlFeature,
  arangodb::UpgradeFeature,
  arangodb::V8DealerFeature,
  arangodb::V8PlatformFeature,
  arangodb::V8SecurityFeature,
  arangodb::VersionFeature,
  arangodb::ViewTypesFeature,
  arangodb::DaemonFeature,
  arangodb::SupervisorFeature,
#ifdef _WIN32
  arangodb::WindowsServiceFeature,
#endif
#ifdef USE_ENTERPRISE
  setupServerEE(server); // FIXME
#else
  arangodb::SslServerFeature,
#endif
  arangodb::ClusterEngine,
  arangodb::RocksDBEngine,
  iresearch::IResearchAnalyzerFeature,
  iresearch::IResearchFeature,
  transaction::ManagerFeature,
  aql::AqlFunctionFeature,
  aql::OptimizerRulesFeature,
  pregel::PregelFeature,
  arangodb::V8ShellFeature,
  application_features::V8ShellFeaturePhase,
  application_features::BasicFeaturePhaseClient,
  arangodb::BenchFeature,
  arangodb::ShellFeature,
  arangodb::RestoreFeature,
  arangodb::ExportFeature,
  arangodb::ImportFeature,
  arangodb::DumpFeature,
  arangodb::VPackFeature
>;

namespace application_features {

struct TypeIdLess {
  constexpr bool operator()(TypeInfo::TypeId lhs, TypeInfo::TypeId rhs) const noexcept {
    return lhs().name() < rhs().name();
  }
};

using FeatureMap = ::frozen::map<TypeInfo::TypeId, size_t, FeatureList::Length, TypeIdLess>;

static constexpr FeatureMap FEATURE_MAP{FeatureList::toList()};

constexpr std::pair<size_t, bool> getIndex(TypeInfo::TypeId type) noexcept {
  auto it = FEATURE_MAP.find(type);
  return it == FEATURE_MAP.end()
    ? std::make_pair(FEATURE_MAP.size(), false)
    : std::make_pair(it->second, true);
}
template<typename T>
constexpr size_t getIndex() noexcept {
  constexpr auto res = getIndex(Type<T>::id());
  static_assert(res.second, "Invalid application feature");
  return res.first;
}
constexpr bool hasFeature(TypeInfo::TypeId type) noexcept {
  return getIndex(type).second;
}
}

} // arangodb

#endif // ARANGODB_LIB_FEATURE_LIST_H
