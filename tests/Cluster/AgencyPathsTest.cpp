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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Agency/AgencyPaths.h"
#include "Basics/StringUtils.h"

#include <memory>
#include <vector>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::cluster;
using namespace arangodb::cluster::paths;

// We don't want any class in the hierarchy to be publicly constructible.
// However, we can only check for specific constructors.
#define CONSTRUCTIBLE_MESSAGE "This class should not be publicly constructible!"
// Turn autoformat off here, to allow for easy multiline editing!
// clang-format off

// First, default constructors
static_assert(!std::is_default_constructible<Root>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersKnown>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersKnown::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersKnown::Server::RebootId>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::FoxxmasterQueueupdate>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ShardsCopied>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Foxxmaster>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersRegistered>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersRegistered::Version>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersRegistered::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersRegistered::Server::Timestamp>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersRegistered::Server::Engine>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersRegistered::Server::Endpoint>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersRegistered::Server::Host>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersRegistered::Server::VersionString>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersRegistered::Server::AdvertisedEndpoint>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ServersRegistered::Server::Version>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::NewServers>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::AsyncReplication>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Coordinators>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Coordinators::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Version>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Lock>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Singles>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::DbServers>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::DbServers::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Collections>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Collections::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Collections::Database::Collection::Shard>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Collections::Database::Collection::Shard::Servers>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Collections::Database::Collection::Shard::Indexes>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Collections::Database::Collection::Shard::FailoverCandidates>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Collections::Database::Collection::Shard::ErrorNum>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Collections::Database::Collection::Shard::ErrorMessage>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Collections::Database::Collection::Shard::Error>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Databases>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Databases::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Databases::Database::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Databases::Database::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Databases::Database::Server::Name>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Databases::Database::Server::ErrorNum>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Databases::Database::Server::Id>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Databases::Database::Server::Error>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::Databases::Database::Server::ErrorMessage>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ReplicatedLogs>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ReplicatedLogs::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ReplicatedLogs::Database::Log>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus::Participant>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus::Participant::Term>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus::Participant::SpearHead>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Views>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Views::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::AsyncReplication>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Coordinators>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Coordinators::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Version>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Lock>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Singles>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::DbServers>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::DbServers::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::WaitForSync>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::Type>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::Status>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::Shards>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::Shards::Shard>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::StatusString>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::ShardingStrategy>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::ShardKeys>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::ReplicatedLogs>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::ReplicatedLogs::Shard>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::ReplicationFactor>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::NumberOfShards>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::KeyOptions>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::KeyOptions::Type>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::KeyOptions::AllowUserKeys>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::IsSystem>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::Name>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::Indexes>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::IsSmart>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::Id>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::DistributeShardsLike>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::Deleted>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::WriteConcern>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::CacheEnabled>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Collections::Database::Collection::IsBuilding>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Databases>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Databases::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Databases::Database::Name>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::Databases::Database::Id>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::Id>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::ParticipantsConfig>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::ParticipantsConfig::Generation>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::ParticipantsConfig::Participants>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::ParticipantsConfig::Participants::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Term>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Config>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Config::WaitForSync>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Config::WriteConcern>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Config::ReplicationFactor>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Leader>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Leader::ServerId>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Leader::RebootId>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::State>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::State::Timestamp>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::State::Mode>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Shards>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::DbServers>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::Server::SyncTime>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::Server::Timestamp>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::Server::SyncStatus>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::Server::LastAckedTime>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::Server::Host>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::Server::Engine>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::Server::Version>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::Server::Status>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::Server::ShortName>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Supervision::Health::Server::Endpoint>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::ToDo>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::ToBeCleanedServers>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::Pending>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::NumberOfDBServers>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::LatestDbServerId>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::Failed>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::CleanedServers>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::LatestCoordinatorId>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::MapUniqueToShortId>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::MapUniqueToShortId::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::MapUniqueToShortId::Server::TransactionId>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::MapUniqueToShortId::Server::ShortName>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::FailedServers>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::NumberOfCoordinators>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::Finished>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::Version>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Target::Lock>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::SystemCollectionsCreated>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Sync>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Sync::UserVersion>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Sync::ServerStates>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Sync::Problems>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Sync::HeartbeatIntervalMs>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Sync::LatestId>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Bootstrap>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Cluster>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Agency>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::Agency::Definition>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_default_constructible<Root::Arango::InitDone>::value, CONSTRUCTIBLE_MESSAGE);

// Exclude these on windows, because the constructor is made public there.
#ifndef _WIN32

// Second, constructors with parent
static_assert(!std::is_constructible<Root::Arango, Root>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersKnown, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersKnown::Server, Root::Arango::Current::ServersKnown>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersKnown::Server::RebootId, Root::Arango::Current::ServersKnown::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::FoxxmasterQueueupdate, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ShardsCopied, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Foxxmaster, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersRegistered, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersRegistered::Version, Root::Arango::Current::ServersRegistered>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersRegistered::Server, Root::Arango::Current::ServersRegistered>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersRegistered::Server::Timestamp, Root::Arango::Current::ServersRegistered::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersRegistered::Server::Engine, Root::Arango::Current::ServersRegistered::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersRegistered::Server::Endpoint, Root::Arango::Current::ServersRegistered::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersRegistered::Server::Host, Root::Arango::Current::ServersRegistered::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersRegistered::Server::VersionString, Root::Arango::Current::ServersRegistered::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersRegistered::Server::AdvertisedEndpoint, Root::Arango::Current::ServersRegistered::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersRegistered::Server::Version, Root::Arango::Current::ServersRegistered::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::NewServers, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::AsyncReplication, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Coordinators, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Coordinators::Server, Root::Arango::Current::Coordinators>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Version, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Lock, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Singles, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::DbServers, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::DbServers::Server, Root::Arango::Current::DbServers>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections::Database, Root::Arango::Current::Collections>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections::Database::Collection, Root::Arango::Current::Collections::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections::Database::Collection::Shard, Root::Arango::Current::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections::Database::Collection::Shard::Servers, Root::Arango::Current::Collections::Database::Collection::Shard>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections::Database::Collection::Shard::Indexes, Root::Arango::Current::Collections::Database::Collection::Shard>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections::Database::Collection::Shard::FailoverCandidates, Root::Arango::Current::Collections::Database::Collection::Shard>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections::Database::Collection::Shard::ErrorNum, Root::Arango::Current::Collections::Database::Collection::Shard>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections::Database::Collection::Shard::ErrorMessage, Root::Arango::Current::Collections::Database::Collection::Shard>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections::Database::Collection::Shard::Error, Root::Arango::Current::Collections::Database::Collection::Shard>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Databases, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Databases::Database, Root::Arango::Current::Databases>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Databases::Database::Server, Root::Arango::Current::Databases::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Databases::Database::Server::Name, Root::Arango::Current::Databases::Database::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Databases::Database::Server::ErrorNum, Root::Arango::Current::Databases::Database::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Databases::Database::Server::Id, Root::Arango::Current::Databases::Database::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Databases::Database::Server::Error, Root::Arango::Current::Databases::Database::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Databases::Database::Server::ErrorMessage, Root::Arango::Current::Databases::Database::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ReplicatedLogs, Root::Arango::Current>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ReplicatedLogs::Database, Root::Arango::Current::ReplicatedLogs>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ReplicatedLogs::Database::Log, Root::Arango::Current::ReplicatedLogs::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus, Root::Arango::Current::ReplicatedLogs::Database::Log>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus::Participant, Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus::Participant::Term, Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus::Participant>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus::Participant::SpearHead, Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus::Participant>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Views, Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Views::Database, Root::Arango::Plan::Views>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::AsyncReplication, Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Coordinators, Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Coordinators::Server, Root::Arango::Plan::Coordinators>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Version, Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Lock, Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Singles, Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::DbServers, Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::DbServers::Server, Root::Arango::Plan::DbServers>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections, Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database, Root::Arango::Plan::Collections>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection, Root::Arango::Plan::Collections::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::WaitForSync, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::Type, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::Status, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::Shards, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::Shards::Shard, Root::Arango::Plan::Collections::Database::Collection::Shards>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::StatusString, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::ShardingStrategy, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::ShardKeys, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::ReplicatedLogs, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::ReplicatedLogs::Shard, Root::Arango::Plan::Collections::Database::Collection::ReplicatedLogs>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::ReplicationFactor, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::NumberOfShards, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::KeyOptions, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::KeyOptions::Type, Root::Arango::Plan::Collections::Database::Collection::KeyOptions>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::KeyOptions::AllowUserKeys, Root::Arango::Plan::Collections::Database::Collection::KeyOptions>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::IsSystem, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::Name, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::Indexes, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::IsSmart, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::Id, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::DistributeShardsLike, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::Deleted, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::WriteConcern, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::CacheEnabled, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::IsBuilding, Root::Arango::Plan::Collections::Database::Collection>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Databases, Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Databases::Database, Root::Arango::Plan::Databases>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Databases::Database::Name, Root::Arango::Plan::Databases::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Databases::Database::Id, Root::Arango::Plan::Databases::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs, Root::Arango::Plan>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database, Root::Arango::Plan::ReplicatedLogs>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log, Root::Arango::Plan::ReplicatedLogs::Database>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::Id, Root::Arango::Plan::ReplicatedLogs::Database::Log>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm, Root::Arango::Plan::ReplicatedLogs::Database::Log>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Term, Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Config, Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Config::WaitForSync, Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Config>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Config::WriteConcern, Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Config>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Config::ReplicationFactor, Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Config>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Leader, Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Leader::ServerId, Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Leader>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Leader::RebootId, Root::Arango::Plan::ReplicatedLogs::Database::Log::CurrentTerm::Leader>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::ParticipantsConfig::Generation, Root::Arango::Plan::ReplicatedLogs::Database::Log::ParticipantsConfig>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::ParticipantsConfig::Participants, Root::Arango::Plan::ReplicatedLogs::Database::Log::ParticipantsConfig>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::ParticipantsConfig::Participants::Server, Root::Arango::Plan::ReplicatedLogs::Database::Log::ParticipantsConfig::Participants>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::State, Root::Arango::Supervision>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::State::Timestamp, Root::Arango::Supervision::State>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::State::Mode, Root::Arango::Supervision::State>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Shards, Root::Arango::Supervision>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::DbServers, Root::Arango::Supervision>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health, Root::Arango::Supervision>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::Server, Root::Arango::Supervision::Health>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::Server::SyncTime, Root::Arango::Supervision::Health::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::Server::Timestamp, Root::Arango::Supervision::Health::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::Server::SyncStatus, Root::Arango::Supervision::Health::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::Server::LastAckedTime, Root::Arango::Supervision::Health::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::Server::Host, Root::Arango::Supervision::Health::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::Server::Engine, Root::Arango::Supervision::Health::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::Server::Version, Root::Arango::Supervision::Health::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::Server::Status, Root::Arango::Supervision::Health::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::Server::ShortName, Root::Arango::Supervision::Health::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::Server::Endpoint, Root::Arango::Supervision::Health::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::ToDo, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::ToBeCleanedServers, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::Pending, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::NumberOfDBServers, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::LatestDbServerId, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::Failed, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::CleanedServers, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::LatestCoordinatorId, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::MapUniqueToShortId, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::MapUniqueToShortId::Server, Root::Arango::Target::MapUniqueToShortId>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::MapUniqueToShortId::Server::TransactionId, Root::Arango::Target::MapUniqueToShortId::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::MapUniqueToShortId::Server::ShortName, Root::Arango::Target::MapUniqueToShortId::Server>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::FailedServers, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::NumberOfCoordinators, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::Finished, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::Version, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::Lock, Root::Arango::Target>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::SystemCollectionsCreated, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Sync, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Sync::UserVersion, Root::Arango::Sync>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Sync::ServerStates, Root::Arango::Sync>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Sync::Problems, Root::Arango::Sync>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Sync::HeartbeatIntervalMs, Root::Arango::Sync>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Sync::LatestId, Root::Arango::Sync>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Bootstrap, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Cluster, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Agency, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Agency::Definition, Root::Arango::Agency>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::InitDone, Root::Arango>::value, CONSTRUCTIBLE_MESSAGE);

// Third, constructors for dynamic components with parent and an additional string where applicable
static_assert(!std::is_constructible<Root::Arango::Current::ServersKnown::Server, Root::Arango::Current::ServersKnown, ServerID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ServersRegistered::Server, Root::Arango::Current::ServersRegistered, ServerID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Coordinators::Server, Root::Arango::Current::Coordinators, ServerID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::DbServers::Server, Root::Arango::Current::DbServers, ServerID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections::Database, Root::Arango::Current::Collections, DatabaseID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections::Database::Collection, Root::Arango::Current::Collections::Database, CollectionID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Collections::Database::Collection::Shard, Root::Arango::Current::Collections::Database::Collection, ShardID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Databases::Database, Root::Arango::Current::Databases, DatabaseID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::Databases::Database::Server, Root::Arango::Current::Databases::Database, ServerID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ReplicatedLogs::Database, Root::Arango::Current::ReplicatedLogs, std::string>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ReplicatedLogs::Database::Log, Root::Arango::Current::ReplicatedLogs::Database, std::string>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus::Participant, Root::Arango::Current::ReplicatedLogs::Database::Log::LocalStatus, ServerID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Views::Database, Root::Arango::Plan::Views, DatabaseID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Coordinators::Server, Root::Arango::Plan::Coordinators, ServerID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::DbServers::Server, Root::Arango::Plan::DbServers, ServerID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database, Root::Arango::Plan::Collections, DatabaseID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection, Root::Arango::Plan::Collections::Database, CollectionID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::ReplicatedLogs::Shard, Root::Arango::Plan::Collections::Database::Collection::ReplicatedLogs, ShardID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Collections::Database::Collection::Shards::Shard, Root::Arango::Plan::Collections::Database::Collection::Shards, ShardID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::Databases::Database, Root::Arango::Plan::Databases, DatabaseID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Supervision::Health::Server, Root::Arango::Supervision::Health, ServerID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Target::MapUniqueToShortId::Server, Root::Arango::Target::MapUniqueToShortId, ServerID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database, Root::Arango::Plan::ReplicatedLogs, DatabaseID>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log, Root::Arango::Plan::ReplicatedLogs::Database, std::string>::value, CONSTRUCTIBLE_MESSAGE);
static_assert(!std::is_constructible<Root::Arango::Plan::ReplicatedLogs::Database::Log::ParticipantsConfig::Participants::Server, Root::Arango::Plan::ReplicatedLogs::Database::Log::ParticipantsConfig::Participants, ServerID>::value, CONSTRUCTIBLE_MESSAGE);

#endif
// clang-format on
#undef CONSTRUCTIBLE_MESSAGE

// Check the types of aliases, so we need only basic tests for them later.
static_assert(std::is_same<decltype(root()->arango()),
                           decltype(aliases::arango())>::value,
              "Aliases should have the same type as the aliased expression!");
static_assert(std::is_same<decltype(root()->arango()->plan()),
                           decltype(aliases::plan())>::value,
              "Aliases should have the same type as the aliased expression!");
static_assert(std::is_same<decltype(root()->arango()->current()),
                           decltype(aliases::current())>::value,
              "Aliases should have the same type as the aliased expression!");
static_assert(std::is_same<decltype(root()->arango()->target()),
                           decltype(aliases::target())>::value,
              "Aliases should have the same type as the aliased expression!");
static_assert(std::is_same<decltype(root()->arango()->supervision()),
                           decltype(aliases::supervision())>::value,
              "Aliases should have the same type as the aliased expression!");

class AgencyPathsTest : public ::testing::Test {
 protected:
  // Vector of {expected, actual} pairs.
  std::vector<std::pair<std::vector<std::string> const,
                        std::shared_ptr<Path const> const>> const ioPairs{
      // Turn autoformat off here, to allow for easy multiline editing!
      // clang-format off
      {{"arango"}, root()->arango()},
      {{"arango", "Plan"}, root()->arango()->plan()},
      {{"arango", "Plan", "Views"}, root()->arango()->plan()->views()},
      {{"arango", "Plan", "Views", "_system"}, root()->arango()->plan()->views()->database("_system")},
      {{"arango", "Plan", "Views", "myDb"}, root()->arango()->plan()->views()->database("myDb")},
      {{"arango", "Plan", "AsyncReplication"}, root()->arango()->plan()->asyncReplication()},
      {{"arango", "Plan", "Coordinators"}, root()->arango()->plan()->coordinators()},
      {{"arango", "Plan", "Coordinators", "CRDN-1234"}, root()->arango()->plan()->coordinators()->server("CRDN-1234")},
      {{"arango", "Plan", "Coordinators", "CRDN-5678"}, root()->arango()->plan()->coordinators()->server("CRDN-5678")},
      {{"arango", "Plan", "Version"}, root()->arango()->plan()->version()},
      {{"arango", "Plan", "Lock"}, root()->arango()->plan()->lock()},
      {{"arango", "Plan", "Singles"}, root()->arango()->plan()->singles()},
      {{"arango", "Plan", "DBServers"}, root()->arango()->plan()->dBServers()},
      {{"arango", "Plan", "DBServers", "PRMR-1234"}, root()->arango()->plan()->dBServers()->server("PRMR-1234")},
      {{"arango", "Plan", "DBServers", "PRMR-5678"}, root()->arango()->plan()->dBServers()->server("PRMR-5678")},
      {{"arango", "Plan", "Collections"}, root()->arango()->plan()->collections()},
      {{"arango", "Plan", "Collections", "_system"}, root()->arango()->plan()->collections()->database("_system")},
      {{"arango", "Plan", "Collections", "myDb"}, root()->arango()->plan()->collections()->database("myDb")},
      {{"arango", "Plan", "Collections", "_system", "12345"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")},
      {{"arango", "Plan", "Collections", "_system", "67890"}, root()->arango()->plan()->collections()->database("_system")->collection("67890")},
      {{"arango", "Plan", "Collections", "_system", "12345", "waitForSync"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->waitForSync()},
      {{"arango", "Plan", "Collections", "_system", "12345", "type"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->type()},
      {{"arango", "Plan", "Collections", "_system", "12345", "status"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->status()},
      {{"arango", "Plan", "Collections", "_system", "12345", "shards"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->shards()},
      {{"arango", "Plan", "Collections", "_system", "12345", "shards", "s123"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->shards()->shard("s123")},
      {{"arango", "Plan", "Collections", "_system", "12345", "shards", "s456"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->shards()->shard("s456")},
      {{"arango", "Plan", "Collections", "_system", "12345", "replicatedLogs"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->replicatedLogs()},
      {{"arango", "Plan", "Collections", "_system", "12345", "replicatedLogs", "s123"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->replicatedLogs()->shard("s123")},
      {{"arango", "Plan", "Collections", "_system", "12345", "statusString"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->statusString()},
      {{"arango", "Plan", "Collections", "_system", "12345", "shardingStrategy"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->shardingStrategy()},
      {{"arango", "Plan", "Collections", "_system", "12345", "shardKeys"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->shardKeys()},
      {{"arango", "Plan", "Collections", "_system", "12345", "replicationFactor"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->replicationFactor()},
      {{"arango", "Plan", "Collections", "_system", "12345", "numberOfShards"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->numberOfShards()},
      {{"arango", "Plan", "Collections", "_system", "12345", "keyOptions"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->keyOptions()},
      {{"arango", "Plan", "Collections", "_system", "12345", "keyOptions", "type"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->keyOptions()->type()},
      {{"arango", "Plan", "Collections", "_system", "12345", "keyOptions", "allowUserKeys"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->keyOptions()->allowUserKeys()},
      {{"arango", "Plan", "Collections", "_system", "12345", "isSystem"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->isSystem()},
      {{"arango", "Plan", "Collections", "_system", "12345", "name"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->name()},
      {{"arango", "Plan", "Collections", "_system", "12345", "indexes"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->indexes()},
      {{"arango", "Plan", "Collections", "_system", "12345", "isSmart"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->isSmart()},
      {{"arango", "Plan", "Collections", "_system", "12345", "id"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->id()},
      {{"arango", "Plan", "Collections", "_system", "12345", "distributeShardsLike"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->distributeShardsLike()},
      {{"arango", "Plan", "Collections", "_system", "12345", "deleted"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->deleted()},
      {{"arango", "Plan", "Collections", "_system", "12345", "writeConcern"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->writeConcern()},
      {{"arango", "Plan", "Collections", "_system", "12345", "cacheEnabled"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->cacheEnabled()},
      {{"arango", "Plan", "Collections", "_system", "12345", "isBuilding"}, root()->arango()->plan()->collections()->database("_system")->collection("12345")->isBuilding()},
      {{"arango", "Plan", "Databases"}, root()->arango()->plan()->databases()},
      {{"arango", "Plan", "Databases", "_system"}, root()->arango()->plan()->databases()->database(DatabaseID{"_system"})},
      {{"arango", "Plan", "Databases", "someDb"}, root()->arango()->plan()->databases()->database(DatabaseID{"someDb"})},
      {{"arango", "Plan", "Databases", "_system", "name"}, root()->arango()->plan()->databases()->database(DatabaseID{"_system"})->name()},
      {{"arango", "Plan", "Databases", "_system", "id"}, root()->arango()->plan()->databases()->database(DatabaseID{"_system"})->id()},
      {{"arango", "Plan", "ReplicatedLogs"}, root()->arango()->plan()->replicatedLogs()},
      {{"arango", "Plan", "ReplicatedLogs", "_system"}, root()->arango()->plan()->replicatedLogs()->database("_system")},
      {{"arango", "Plan", "ReplicatedLogs", "someDb"}, root()->arango()->plan()->replicatedLogs()->database("someDb")},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "12345"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("12345")},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "id"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->id()},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "participantsConfig"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->participantsConfig()},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "participantsConfig", "generation"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->participantsConfig()->generation()},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "participantsConfig", "participants"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->participantsConfig()->participants()},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "participantsConfig", "participants", "12345"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->participantsConfig()->participants()->server("12345")},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "participantsConfig", "participants", "98765"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->participantsConfig()->participants()->server("98765")},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "currentTerm"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->currentTerm()},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "currentTerm", "term"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->currentTerm()->term()},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "currentTerm", "config"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->currentTerm()->config()},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "currentTerm", "config", "waitForSync"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->currentTerm()->config()->waitForSync()},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "currentTerm", "config", "writeConcern"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->currentTerm()->config()->writeConcern()},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "currentTerm", "config", "replicationFactor"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->currentTerm()->config()->replicationFactor()},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "currentTerm", "leader"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->currentTerm()->leader()},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "currentTerm", "leader", "serverId"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->currentTerm()->leader()->serverId()},
      {{"arango", "Plan", "ReplicatedLogs", "_system", "98765", "currentTerm", "leader", "rebootId"}, root()->arango()->plan()->replicatedLogs()->database("_system")->log("98765")->currentTerm()->leader()->rebootId()},
      {{"arango", "Current"}, root()->arango()->current()},
      {{"arango", "Current", "ServersKnown"}, root()->arango()->current()->serversKnown()},
      {{"arango", "Current", "ServersKnown", "PRMR-1234"}, root()->arango()->current()->serversKnown()->server("PRMR-1234")},
      {{"arango", "Current", "ServersKnown", "CRDN-5678", "rebootId"}, root()->arango()->current()->serversKnown()->server("CRDN-5678")->rebootId()},
      {{"arango", "Current", "FoxxmasterQueueupdate"}, root()->arango()->current()->foxxmasterQueueupdate()},
      {{"arango", "Current", "ShardsCopied"}, root()->arango()->current()->shardsCopied()},
      {{"arango", "Current", "Foxxmaster"}, root()->arango()->current()->foxxmaster()},
      {{"arango", "Current", "ServersRegistered"}, root()->arango()->current()->serversRegistered()},
      {{"arango", "Current", "ServersRegistered", "Version"}, root()->arango()->current()->serversRegistered()->version()},
      {{"arango", "Current", "ServersRegistered", "PRMR-1234"}, root()->arango()->current()->serversRegistered()->server("PRMR-1234")},
      {{"arango", "Current", "ServersRegistered", "PRMR-5678"}, root()->arango()->current()->serversRegistered()->server("PRMR-5678")},
      {{"arango", "Current", "ServersRegistered", "PRMR-1234", "timestamp"}, root()->arango()->current()->serversRegistered()->server("PRMR-1234")->timestamp()},
      {{"arango", "Current", "ServersRegistered", "PRMR-1234", "engine"}, root()->arango()->current()->serversRegistered()->server("PRMR-1234")->engine()},
      {{"arango", "Current", "ServersRegistered", "PRMR-1234", "endpoint"}, root()->arango()->current()->serversRegistered()->server("PRMR-1234")->endpoint()},
      {{"arango", "Current", "ServersRegistered", "PRMR-1234", "host"}, root()->arango()->current()->serversRegistered()->server("PRMR-1234")->host()},
      {{"arango", "Current", "ServersRegistered", "PRMR-1234", "versionString"}, root()->arango()->current()->serversRegistered()->server("PRMR-1234")->versionString()},
      {{"arango", "Current", "ServersRegistered", "PRMR-1234", "advertisedEndpoint"}, root()->arango()->current()->serversRegistered()->server("PRMR-1234")->advertisedEndpoint()},
      {{"arango", "Current", "ServersRegistered", "PRMR-1234", "version"}, root()->arango()->current()->serversRegistered()->server("PRMR-1234")->version()},
      {{"arango", "Current", "NewServers"}, root()->arango()->current()->newServers()},
      {{"arango", "Current", "AsyncReplication"}, root()->arango()->current()->asyncReplication()},
      {{"arango", "Current", "Coordinators"}, root()->arango()->current()->coordinators()},
      {{"arango", "Current", "Coordinators", "CRDN-1234"}, root()->arango()->current()->coordinators()->server("CRDN-1234")},
      {{"arango", "Current", "Version"}, root()->arango()->current()->version()},
      {{"arango", "Current", "Lock"}, root()->arango()->current()->lock()},
      {{"arango", "Current", "Singles"}, root()->arango()->current()->singles()},
      {{"arango", "Current", "DBServers"}, root()->arango()->current()->dBServers()},
      {{"arango", "Current", "DBServers", "PRMR-1234"}, root()->arango()->current()->dBServers()->server("PRMR-1234")},
      {{"arango", "Current", "DBServers", "PRMR-5678"}, root()->arango()->current()->dBServers()->server("PRMR-5678")},
      {{"arango", "Current", "Collections"}, root()->arango()->current()->collections()},
      {{"arango", "Current", "Collections", "_system"}, root()->arango()->current()->collections()->database("_system")},
      {{"arango", "Current", "Collections", "myDb"}, root()->arango()->current()->collections()->database("myDb")},
      {{"arango", "Current", "Collections", "_system", "12345"}, root()->arango()->current()->collections()->database("_system")->collection("12345")},
      {{"arango", "Current", "Collections", "_system", "67890"}, root()->arango()->current()->collections()->database("_system")->collection("67890")},
      {{"arango", "Current", "Collections", "_system", "12345", "s123"}, root()->arango()->current()->collections()->database("_system")->collection("12345")->shard("s123")},
      {{"arango", "Current", "Collections", "_system", "12345", "s456"}, root()->arango()->current()->collections()->database("_system")->collection("12345")->shard("s456")},
      {{"arango", "Current", "Collections", "_system", "12345", "s123", "servers"}, root()->arango()->current()->collections()->database("_system")->collection("12345")->shard("s123")->servers()},
      {{"arango", "Current", "Collections", "_system", "12345", "s123", "indexes"}, root()->arango()->current()->collections()->database("_system")->collection("12345")->shard("s123")->indexes()},
      {{"arango", "Current", "Collections", "_system", "12345", "s123", "failoverCandidates"}, root()->arango()->current()->collections()->database("_system")->collection("12345")->shard("s123")->failoverCandidates()},
      {{"arango", "Current", "Collections", "_system", "12345", "s123", "errorNum"}, root()->arango()->current()->collections()->database("_system")->collection("12345")->shard("s123")->errorNum()},
      {{"arango", "Current", "Collections", "_system", "12345", "s123", "errorMessage"}, root()->arango()->current()->collections()->database("_system")->collection("12345")->shard("s123")->errorMessage()},
      {{"arango", "Current", "Collections", "_system", "12345", "s123", "error"}, root()->arango()->current()->collections()->database("_system")->collection("12345")->shard("s123")->error()},
      {{"arango", "Current", "Databases"}, root()->arango()->current()->databases()},
      {{"arango", "Current", "Databases", "_system"}, root()->arango()->current()->databases()->database("_system")},
      {{"arango", "Current", "Databases", "myDb"}, root()->arango()->current()->databases()->database("myDb")},
      {{"arango", "Current", "Databases", "_system", "PRMR-1234"}, root()->arango()->current()->databases()->database("_system")->server("PRMR-1234")},
      {{"arango", "Current", "Databases", "_system", "PRMR-5678"}, root()->arango()->current()->databases()->database("_system")->server("PRMR-5678")},
      {{"arango", "Current", "Databases", "_system", "PRMR-1234", "name"}, root()->arango()->current()->databases()->database("_system")->server("PRMR-1234")->name()},
      {{"arango", "Current", "Databases", "_system", "PRMR-1234", "errorNum"}, root()->arango()->current()->databases()->database("_system")->server("PRMR-1234")->errorNum()},
      {{"arango", "Current", "Databases", "_system", "PRMR-1234", "id"}, root()->arango()->current()->databases()->database("_system")->server("PRMR-1234")->id()},
      {{"arango", "Current", "Databases", "_system", "PRMR-1234", "error"}, root()->arango()->current()->databases()->database("_system")->server("PRMR-1234")->error()},
      {{"arango", "Current", "Databases", "_system", "PRMR-1234", "errorMessage"}, root()->arango()->current()->databases()->database("_system")->server("PRMR-1234")->errorMessage()},
      {{"arango", "Current", "ReplicatedLogs"}, root()->arango()->current()->replicatedLogs()},
      {{"arango", "Current", "ReplicatedLogs", "_system"}, root()->arango()->current()->replicatedLogs()->database("_system")},
      {{"arango", "Current", "ReplicatedLogs", "myDb"}, root()->arango()->current()->replicatedLogs()->database("myDb")},
      {{"arango", "Current", "ReplicatedLogs", "_system", "12345"}, root()->arango()->current()->replicatedLogs()->database("_system")->log("12345")},
      {{"arango", "Current", "ReplicatedLogs", "_system", "12345", "localStatus"}, root()->arango()->current()->replicatedLogs()->database("_system")->log("12345")->localStatus()},
      {{"arango", "Current", "ReplicatedLogs", "_system", "12345", "localStatus", "12345"}, root()->arango()->current()->replicatedLogs()->database("_system")->log("12345")->localStatus()->participant("12345")},
      {{"arango", "Current", "ReplicatedLogs", "_system", "12345", "localStatus", "67890"}, root()->arango()->current()->replicatedLogs()->database("_system")->log("12345")->localStatus()->participant("67890")},
      {{"arango", "Current", "ReplicatedLogs", "_system", "12345", "localStatus", "12345", "term"}, root()->arango()->current()->replicatedLogs()->database("_system")->log("12345")->localStatus()->participant("12345")->term()},
      {{"arango", "Current", "ReplicatedLogs", "_system", "12345", "localStatus", "12345", "spearHead"}, root()->arango()->current()->replicatedLogs()->database("_system")->log("12345")->localStatus()->participant("12345")->spearHead()},
      {{"arango", "Supervision"}, root()->arango()->supervision()},
      {{"arango", "Supervision", "State"}, root()->arango()->supervision()->state()},
      {{"arango", "Supervision", "State", "Timestamp"}, root()->arango()->supervision()->state()->timestamp()},
      {{"arango", "Supervision", "State", "Mode"}, root()->arango()->supervision()->state()->mode()},
      {{"arango", "Supervision", "Shards"}, root()->arango()->supervision()->shards()},
      {{"arango", "Supervision", "DBServers"}, root()->arango()->supervision()->dbServers()},
      {{"arango", "Supervision", "Health"}, root()->arango()->supervision()->health()},
      {{"arango", "Supervision", "Health", "CRDN-1234"}, root()->arango()->supervision()->health()->server("CRDN-1234")},
      {{"arango", "Supervision", "Health", "PRMR-5678", "SyncTime"}, root()->arango()->supervision()->health()->server("PRMR-5678")->syncTime()},
      {{"arango", "Supervision", "Health", "PRMR-5678", "Timestamp"}, root()->arango()->supervision()->health()->server("PRMR-5678")->timestamp()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "SyncStatus"}, root()->arango()->supervision()->health()->server("CRDN-1234")->syncStatus()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "LastAckedTime"}, root()->arango()->supervision()->health()->server("CRDN-1234")->lastAckedTime()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "Host"}, root()->arango()->supervision()->health()->server("CRDN-1234")->host()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "Engine"}, root()->arango()->supervision()->health()->server("CRDN-1234")->engine()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "Version"}, root()->arango()->supervision()->health()->server("CRDN-1234")->version()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "Status"}, root()->arango()->supervision()->health()->server("CRDN-1234")->status()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "ShortName"}, root()->arango()->supervision()->health()->server("CRDN-1234")->shortName()},
      {{"arango", "Supervision", "Health", "CRDN-1234", "Endpoint"}, root()->arango()->supervision()->health()->server("CRDN-1234")->endpoint()},
      {{"arango", "Target"}, root()->arango()->target()},
      {{"arango", "Target", "ToDo"}, root()->arango()->target()->toDo()},
      {{"arango", "Target", "ToBeCleanedServers"}, root()->arango()->target()->toBeCleanedServers()},
      {{"arango", "Target", "Pending"}, root()->arango()->target()->pending()},
      {{"arango", "Target", "NumberOfDBServers"}, root()->arango()->target()->numberOfDBServers()},
      {{"arango", "Target", "LatestDBServerId"}, root()->arango()->target()->latestDBServerId()},
      {{"arango", "Target", "Failed"}, root()->arango()->target()->failed()},
      {{"arango", "Target", "CleanedServers"}, root()->arango()->target()->cleanedServers()},
      {{"arango", "Target", "LatestCoordinatorId"}, root()->arango()->target()->latestCoordinatorId()},
      {{"arango", "Target", "MapUniqueToShortID"}, root()->arango()->target()->mapUniqueToShortID()},
      {{"arango", "Target", "MapUniqueToShortID", "PRMR-1234"}, root()->arango()->target()->mapUniqueToShortID()->server("PRMR-1234")},
      {{"arango", "Target", "MapUniqueToShortID", "CRDN-5678", "TransactionID"}, root()->arango()->target()->mapUniqueToShortID()->server("CRDN-5678")->transactionID()},
      {{"arango", "Target", "MapUniqueToShortID", "PRMR-1234", "ShortName"}, root()->arango()->target()->mapUniqueToShortID()->server("PRMR-1234")->shortName()},
      {{"arango", "Target", "FailedServers"}, root()->arango()->target()->failedServers()},
      {{"arango", "Target", "NumberOfCoordinators"}, root()->arango()->target()->numberOfCoordinators()},
      {{"arango", "Target", "Finished"}, root()->arango()->target()->finished()},
      {{"arango", "Target", "Version"}, root()->arango()->target()->version()},
      {{"arango", "Target", "Lock"}, root()->arango()->target()->lock()},
      {{"arango", "SystemCollectionsCreated"}, root()->arango()->systemCollectionsCreated()},
      {{"arango", "Sync"}, root()->arango()->sync()},
      {{"arango", "Sync", "UserVersion"}, root()->arango()->sync()->userVersion()},
      {{"arango", "Sync", "ServerStates"}, root()->arango()->sync()->serverStates()},
      {{"arango", "Sync", "Problems"}, root()->arango()->sync()->problems()},
      {{"arango", "Sync", "HeartbeatIntervalMs"}, root()->arango()->sync()->heartbeatIntervalMs()},
      {{"arango", "Sync", "LatestID"}, root()->arango()->sync()->latestId()},
      {{"arango", "Bootstrap"}, root()->arango()->bootstrap()},
      {{"arango", "Cluster"}, root()->arango()->cluster()},
      {{"arango", "Agency"}, root()->arango()->agency()},
      {{"arango", "Agency", "Definition"}, root()->arango()->agency()->definition()},
      {{"arango", "InitDone"}, root()->arango()->initDone()},
      // Aliases:
      {{"arango"}, aliases::arango()},
      {{"arango", "Plan"}, aliases::plan()},
      {{"arango", "Current"}, aliases::current()},
      {{"arango", "Target"}, aliases::target()},
      {{"arango", "Supervision"}, aliases::supervision()},
      // clang-format on
  };
};

TEST_F(AgencyPathsTest, test_path_string) {
  for (auto const& it : ioPairs) {
    auto expected = std::string{"/"} + StringUtils::join(it.first, "/");
    auto actual = it.second->str();
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(AgencyPathsTest, test_path_pathvec) {
  for (auto const& it : ioPairs) {
    auto& expected = it.first;
    auto actual = it.second->vec();
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(AgencyPathsTest, test_path_stringstream) {
  for (auto const& it : ioPairs) {
    auto expected = std::string{"/"} + StringUtils::join(it.first, "/");
    std::stringstream stream;
    stream << *it.second;
    EXPECT_EQ(expected, stream.str());
  }
}

TEST_F(AgencyPathsTest, test_path_pathtostream) {
  for (auto const& it : ioPairs) {
    auto expected = std::string{"/"} + StringUtils::join(it.first, "/");
    std::stringstream stream;
    it.second->toStream(stream);
    EXPECT_EQ(expected, stream.str());
  }
}
