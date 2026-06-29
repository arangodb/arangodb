////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Simran Spiller
////////////////////////////////////////////////////////////////////////////////

/// Stub implementations for server-side symbols referenced by AQL source files
/// compiled into arango_aql_standalone. These code paths are never reached
/// during expression-only evaluation.

#include "Basics/debugging.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Basics/ResultT.h"

// ---------- AQL function stubs ----------

#include "Aql/AqlValue.h"
#include "Aql/Functions.h"

namespace arangodb::aql::functions {

#define STUB_FUNC(name)                                       \
  AqlValue name(ExpressionContext*, AstNode const&,           \
                VPackFunctionParametersView) {                \
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, \
                                   #name " not available");   \
  }

STUB_FUNC(LevenshteinMatch)
STUB_FUNC(NgramSimilarity)
STUB_FUNC(NgramPositionalSimilarity)
STUB_FUNC(NgramMatch)
STUB_FUNC(IsSameCollection)
STUB_FUNC(Collections)
STUB_FUNC(ShardId)
STUB_FUNC(CollectionCount)
STUB_FUNC(MinHash)
STUB_FUNC(MinHashCount)
STUB_FUNC(MinHashError)
STUB_FUNC(MinHashMatch)

#undef STUB_FUNC

}  // namespace arangodb::aql::functions

// ---------- Server infrastructure stubs ----------

#include "Aql/Collections.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryExpressionContext.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Geo/GeoJson.h"
#include "Geo/ShapeContainer.h"
#include "Graph/Graph.h"
#include "Graph/GraphManager.h"
#include "Indexes/Index.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/VectorIndexFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"
#include "Transaction/SmartContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Validators.h"
#include "VocBase/VocbaseInfo.h"
#include "VocBase/vocbase.h"

// async_registry
#include "Async/Registry/promise.h"

#include "ClusterEngine/ClusterEngine.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Transaction/IgnoreNoAccessAqlTransaction.h"
#include "Enterprise/VocBase/SmartGraphSchema.h"
#endif

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// transaction::Methods
////////////////////////////////////////////////////////////////////////////////

namespace transaction {

Methods::Methods(std::shared_ptr<Context> ctx, Options const& options)
    : _transactionContext(std::move(ctx)), _mainTransaction(false) {
  if (_transactionContext != nullptr) {
    _state = _transactionContext->acquireState(options, _mainTransaction);
  }
}
Methods::~Methods() = default;
Result Methods::begin() { return {}; }
void Methods::addHint(Hints::Hint) noexcept {}
Result Methods::addCollection(DataSourceId, std::string_view,
                              AccessMode::Type) {
  return {};
}
futures::Future<DataSourceId> Methods::addCollectionAtRuntime(
    DataSourceId, std::string_view, AccessMode::Type) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
futures::Future<DataSourceId> Methods::addCollectionAtRuntime(
    std::string_view, AccessMode::Type) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
TRI_vocbase_t& Methods::vocbase() const {
  return _transactionContext->vocbase();
}
velocypack::Options const& Methods::vpackOptions() const {
  return velocypack::Options::Defaults;
}
CollectionNameResolver const* Methods::resolver() const {
  return &(_transactionContext->resolver());
}
bool Methods::isLocked(LogicalCollection*, AccessMode::Type) const {
  return false;
}
std::string Methods::extractIdString(velocypack::Slice) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
futures::Future<Result> Methods::documentFastPath(std::string const&,
                                                  velocypack::Slice,
                                                  OperationOptions const&,
                                                  velocypack::Builder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
futures::Future<Result> Methods::documentFastPathLocal(
    std::string_view, std::string_view,
    IndexIterator::DocumentCallback const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
auto Methods::documentInternal(std::string const&, velocypack::Slice,
                               OperationOptions const&, MethodsApi)
    -> futures::Future<OperationResult> {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
futures::Future<OperationResult> Methods::all(std::string const&, uint64_t,
                                              uint64_t,
                                              OperationOptions const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
futures::Future<OperationResult> Methods::any(std::string const&,
                                              OperationOptions const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
auto Methods::countInternal(std::string const&, CountType,
                            OperationOptions const&, MethodsApi)
    -> futures::Future<OperationResult> {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
OperationResult Methods::count(std::string const&, CountType,
                               OperationOptions const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// transaction::Options
////////////////////////////////////////////////////////////////////////////////

Options::Options() = default;
bool Options::isIntermediateCommitEnabled() const noexcept { return false; }
void Options::toVelocyPack(velocypack::Builder&) const {}
void Options::fromVelocyPack(velocypack::Slice) {}

////////////////////////////////////////////////////////////////////////////////
/// transaction::Context
////////////////////////////////////////////////////////////////////////////////

TransactionId Context::makeTransactionId() { return TransactionId(1); }
velocypack::Options const* Context::getVPackOptions() const noexcept {
  return &velocypack::Options::Defaults;
}

////////////////////////////////////////////////////////////////////////////////
/// transaction::SmartContext
////////////////////////////////////////////////////////////////////////////////

SmartContext::SmartContext(TRI_vocbase_t& vocbase, TransactionId,
                           std::shared_ptr<TransactionState>,
                           OperationOrigin origin)
    : Context(vocbase, origin) {}
SmartContext::~SmartContext() = default;
TransactionId SmartContext::generateId() const { return TransactionId(1); }

////////////////////////////////////////////////////////////////////////////////
/// transaction::helpers
////////////////////////////////////////////////////////////////////////////////

namespace helpers {
velocypack::Slice extractKeyFromDocument(velocypack::Slice doc) {
  return doc.get("_key");
}
velocypack::Slice extractIdFromDocument(velocypack::Slice doc) {
  return doc.get("_id");
}
std::string extractIdString(CollectionNameResolver const*, velocypack::Slice,
                            velocypack::Slice) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
std::string_view extractKeyPart(velocypack::Slice) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
velocypack::Slice extractFromFromDocument(velocypack::Slice doc) {
  return doc.get("_from");
}
velocypack::Slice extractToFromDocument(velocypack::Slice doc) {
  return doc.get("_to");
}
}  // namespace helpers

#ifdef USE_ENTERPRISE
IgnoreNoAccessAqlTransaction::IgnoreNoAccessAqlTransaction(
    std::shared_ptr<Context> const& ctx, aql::Collections const& collections,
    Options const& opts, std::unordered_set<std::string>)
    : aql::AqlTransaction(ctx, collections, opts) {}
#endif

}  // namespace transaction

////////////////////////////////////////////////////////////////////////////////
/// TransactionState
////////////////////////////////////////////////////////////////////////////////

TransactionState::TransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                                   transaction::Options const& options,
                                   transaction::OperationOrigin origin)
    : _vocbase(vocbase),
      _serverRole(ServerState::instance()->getRole()),
      _options(options),
      _id(tid),
      _operationOrigin(origin) {}
TransactionState::~TransactionState() = default;
void TransactionState::updateStatus(transaction::Status status) noexcept {
  _status = status;
}

////////////////////////////////////////////////////////////////////////////////
/// CollectionNameResolver
////////////////////////////////////////////////////////////////////////////////

CollectionNameResolver::CollectionNameResolver(TRI_vocbase_t& vocbase)
    : _vocbase(vocbase), _serverRole(ServerState::instance()->getRole()) {}
DataSourceId CollectionNameResolver::getCollectionId(std::string_view) const {
  return DataSourceId::none();
}
std::shared_ptr<LogicalDataSource> CollectionNameResolver::getDataSource(
    std::string_view) const {
  return nullptr;
}
bool CollectionNameResolver::visitCollections(
    std::function<bool(LogicalCollection&)> const&, DataSourceId) const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// ExecContext
////////////////////////////////////////////////////////////////////////////////

// ExecContext::current() needs a proper static Superuser object. We create
// a minimal one with Internal type and max privileges.
thread_local std::shared_ptr<ExecContext const> ExecContext::CURRENT = nullptr;

std::shared_ptr<ExecContext const> const ExecContext::Superuser =
    std::make_shared<ExecContext const>(
        ExecContext::ConstructorToken{}, ExecContext::Type::Internal,
        /*name*/ "", /*db*/ "", auth::Level::RW, auth::Level::RW, true);

ExecContext::ExecContext(ConstructorToken, ExecContext::Type type,
                         std::string const& user, std::string const& database,
                         auth::Level systemLevel, auth::Level dbLevel,
                         bool isAdminUser,
                         std::vector<std::string> const& roles,
                         std::string const& jwtToken)
    : _user(user),
      _database(database),
      _roles(roles),
      _jwtToken(jwtToken),
      _type(type),
      _isAdminUser(isAdminUser),
      _systemDbAuthLevel(systemLevel),
      _databaseAuthLevel(dbLevel) {}

ExecContext const& ExecContext::current() {
  if (ExecContext::CURRENT != nullptr) {
    return *ExecContext::CURRENT;
  }
  return *ExecContext::Superuser;
}

bool ExecContext::isAuthEnabled() { return false; }

////////////////////////////////////////////////////////////////////////////////
/// ServerState
////////////////////////////////////////////////////////////////////////////////

}  // namespace arangodb

static char serverStateStorage[sizeof(arangodb::ServerState)]
    __attribute__((aligned(alignof(arangodb::ServerState)))) = {};

namespace arangodb {

// Provide setRole stub so we can initialize the role to ROLE_SINGLE.
// The real implementation in ServerState.cpp also calls Logger::setRole,
// but we don't need that in client tools.
void ServerState::setRole(RoleEnum role) {
  _role.store(role, std::memory_order_release);
}

ServerState* ServerState::instance() noexcept {
  return reinterpret_cast<ServerState*>(serverStateStorage);
}
uint32_t ServerState::getShortId() const { return 0; }

}  // namespace arangodb

// Initialize ServerState role to ROLE_SINGLE so isCoordinator() etc.
// don't assert on ROLE_UNDEFINED.
static struct ServerStateInit {
  ServerStateInit() {
    arangodb::ServerState::instance()->setRole(
        arangodb::ServerState::ROLE_SINGLE);
  }
} serverStateInit;

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// VocBase / CreateDatabaseInfo
////////////////////////////////////////////////////////////////////////////////

CreateDatabaseInfo::CreateDatabaseInfo(
    application_features::ApplicationServer& server, ExecContext const& ctx)
    : _server(server), _context(ctx) {}

Result CreateDatabaseInfo::load(std::string_view, uint64_t id) {
  _id = id;
  return {};
}

}  // namespace arangodb

TRI_voc_tick_t TRI_NewServerSpecificTick() {
  static std::atomic<TRI_voc_tick_t> tick{1};
  return tick.fetch_add(1);
}

// Provide complete types for unique_ptr members in TRI_vocbase_t so the
// destructor can be instantiated. These classes are never actually used.
#include "Aql/ExecutorExpressionContext.h"
#include "Aql/QueryList.h"
#include "Aql/QueryPlanCache.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/ReplicationApplierState.h"
#include "Replication/ReplicationClients.h"
#include "Replication/InitialSyncer.h"
#include "Replication/TailingSyncer.h"
#include "RestServer/QueryRegistryFeature.h"
// ProgramOptions needs to be included for the feature method stubs
#include "ProgramOptions/ProgramOptions.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/CursorRepository.h"
#include "Utils/VersionTracker.h"
#include "VocBase/VocbaseMetrics.h"

// TRI_vocbase_t has reference members that we initialize to point at the
// ApplicationServer from CreateDatabaseInfo. _engine and _versionTracker
// are dummy references (never dereferenced in client-tool context).
static char dummyEngine[sizeof(arangodb::StorageEngine)]
    __attribute__((aligned(alignof(arangodb::StorageEngine)))) = {};
static char dummyVersionTracker[sizeof(arangodb::VersionTracker)]
    __attribute__((aligned(alignof(arangodb::VersionTracker)))) = {};

TRI_vocbase_t::TRI_vocbase_t(arangodb::CreateDatabaseInfo&& info)
    : _server(info.server()),
      _engine(*reinterpret_cast<arangodb::StorageEngine*>(dummyEngine)),
      _versionTracker(
          *reinterpret_cast<arangodb::VersionTracker*>(dummyVersionTracker)),
      _extendedNames(false),
      _info(std::move(info)) {}

TRI_vocbase_t::~TRI_vocbase_t() = default;

std::shared_ptr<arangodb::LogicalCollection> TRI_vocbase_t::lookupCollection(
    std::string_view) const noexcept {
  return nullptr;
}

namespace arangodb {  // reopen

////////////////////////////////////////////////////////////////////////////////
/// Feature stubs
////////////////////////////////////////////////////////////////////////////////

AqlFeature::~AqlFeature() = default;
bool AqlFeature::lease() noexcept { return true; }
void AqlFeature::unlease() noexcept {}
void AqlFeature::start() {}
void AqlFeature::stop() {}

// DatabaseFeature: destructor intentionally omitted —
// unique_ptr<IOHeartbeatThread> cascade.  The linker workaround handles the
// unresolved destructor entries in the vtable.  These features are never
// constructed in client tools.
void DatabaseFeature::collectOptions(std::shared_ptr<options::ProgramOptions>) {
}
void DatabaseFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {}
void DatabaseFeature::start() {}
void DatabaseFeature::stop() {}
void DatabaseFeature::prepare() {}
void DatabaseFeature::beginShutdown() {}
void DatabaseFeature::unprepare() {}
TRI_vocbase_t& DatabaseFeature::getCalculationVocbase() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
VectorIndexFeature::VectorIndexFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature(server, *this) {}
bool VectorIndexFeature::isVectorIndexEnabled() const { return false; }
void VectorIndexFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions>) {}
// For ClusterFeature, DatabaseFeature, QueryRegistryFeature,
// ReplicationApplier: their destructors trigger unique_ptr chains that pull in
// many types. Instead, use the "weak vtable" trick: define a non-virtual member
// function out-of-line. This is enough to anchor the typeinfo without needing
// the dtor. We use __attribute__((used)) to force emission. ClusterFeature:
// destructor intentionally omitted — unique_ptr cascade.
void ClusterFeature::collectOptions(std::shared_ptr<options::ProgramOptions>) {}
void ClusterFeature::validateOptions(std::shared_ptr<options::ProgramOptions>) {
}
void ClusterFeature::prepare() {}
void ClusterFeature::start() {}
void ClusterFeature::stop() {}
void ClusterFeature::beginShutdown() {}
void ClusterFeature::unprepare() {}
ClusterInfo& ClusterFeature::clusterInfo() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
std::shared_ptr<std::vector<ShardID> const> ClusterInfo::getShardList(
    std::string_view) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

// ClusterEngine::Mocking — definition is in the global stubs section below

////////////////////////////////////////////////////////////////////////////////
/// QueryCache
////////////////////////////////////////////////////////////////////////////////

namespace aql {

QueryCache::QueryCache() = default;
QueryCache::~QueryCache() = default;
QueryCache* QueryCache::instance() {
  static QueryCache cache;
  return &cache;
}
QueryCacheMode QueryCache::mode() const {
  return QueryCacheMode::CACHE_ALWAYS_OFF;
}

////////////////////////////////////////////////////////////////////////////////
/// QueryExpressionContext stubs
////////////////////////////////////////////////////////////////////////////////

void QueryExpressionContext::registerWarning(ErrorCode, std::string_view) {}
void QueryExpressionContext::registerError(ErrorCode code,
                                           std::string_view msg) {
  THROW_ARANGO_EXCEPTION_MESSAGE(code, msg);
}
icu_64_64::RegexMatcher* QueryExpressionContext::buildRegexMatcher(
    std::string_view, bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
icu_64_64::RegexMatcher* QueryExpressionContext::buildLikeMatcher(
    std::string_view, bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
icu_64_64::RegexMatcher* QueryExpressionContext::buildSplitMatcher(
    AqlValue, velocypack::Options const*, bool&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
ValidatorBase* QueryExpressionContext::buildValidator(velocypack::Slice) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
TRI_vocbase_t& QueryExpressionContext::vocbase() const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
transaction::Methods& QueryExpressionContext::trx() const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
bool QueryExpressionContext::killed() const { return false; }
void QueryExpressionContext::setVariable(Variable const*, velocypack::Slice) {}
void QueryExpressionContext::clearVariable(Variable const*) noexcept {}

////////////////////////////////////////////////////////////////////////////////
/// ExecutionPlan stubs
////////////////////////////////////////////////////////////////////////////////

ModificationOptions ExecutionPlan::parseModificationOptions(QueryContext&,
                                                            std::string_view,
                                                            AstNode const*,
                                                            bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
bool ExecutionPlan::hasExclusiveAccessOption(AstNode const*) { return false; }

}  // namespace aql

////////////////////////////////////////////////////////////////////////////////
/// LogicalCollection stubs
////////////////////////////////////////////////////////////////////////////////

std::string LogicalCollection::distributeShardsLike() const noexcept {
  return {};
}
bool LogicalCollection::isSatellite() const noexcept { return false; }
std::shared_ptr<Index> LogicalCollection::lookupIndex(IndexId) const {
  return nullptr;
}
bool LogicalCollection::mustCreateKeyOnCoordinator() const noexcept {
  return false;
}
size_t LogicalCollection::numberOfShards() const noexcept { return 1; }
void LogicalCollection::schemaToVelocyPack(velocypack::Builder&) const {}
std::vector<std::string> const& LogicalCollection::shardKeys() const noexcept {
  static std::vector<std::string> empty;
  return empty;
}
std::string const& LogicalCollection::smartJoinAttribute() const noexcept {
  static std::string empty;
  return empty;
}
bool LogicalCollection::usesDefaultShardKeys() const noexcept { return true; }

std::vector<std::shared_ptr<Index>> PhysicalCollection::getReadyIndexes()
    const {
  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// Index / Key / ShardID
////////////////////////////////////////////////////////////////////////////////

bool Index::validateId(std::string_view) { return true; }
bool KeyGeneratorHelper::validateKey(char const*, size_t) noexcept {
  return true;
}
ShardID::ShardID(std::string_view) {}

////////////////////////////////////////////////////////////////////////////////
/// Validators
////////////////////////////////////////////////////////////////////////////////

ValidatorBase::ValidatorBase() = default;

////////////////////////////////////////////////////////////////////////////////
/// methods::Collections
////////////////////////////////////////////////////////////////////////////////

Result methods::Collections::lookup(TRI_vocbase_t const&, std::string const&,
                                    std::shared_ptr<LogicalCollection>&) {
  return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

////////////////////////////////////////////////////////////////////////////////
/// Geo stubs
////////////////////////////////////////////////////////////////////////////////

namespace geo {
S2Point ShapeContainer::centroid() const noexcept { return {}; }
double ShapeContainer::area(Ellipsoid const&) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
bool ShapeContainer::contains(ShapeContainer const&) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
double ShapeContainer::distanceFromCentroid(S2Point const&,
                                            Ellipsoid const&) const noexcept {
  return 0.0;
}
double ShapeContainer::distanceFromCentroid(S2Point const&) const noexcept {
  return 0.0;
}
bool ShapeContainer::equals(ShapeContainer const&) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
bool ShapeContainer::intersects(ShapeContainer const&) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
namespace json {
Result parseMultiPolygon(velocypack::Slice, S2Polygon&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
Result parsePolygon(velocypack::Slice, S2Polygon&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
Result parseRegion(velocypack::Slice, ShapeContainer&, bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
template<>
Result parseCoordinates<true>(velocypack::Slice, ShapeContainer&, bool,
                              coding::Options, Encoder*) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
}  // namespace json
}  // namespace geo

////////////////////////////////////////////////////////////////////////////////
/// Graph stubs
////////////////////////////////////////////////////////////////////////////////

namespace graph {
std::set<std::string> const& Graph::edgeCollections() const {
  static std::set<std::string> empty;
  return empty;
}
std::set<std::string> const& Graph::vertexCollections() const {
  static std::set<std::string> empty;
  return empty;
}
ResultT<std::unique_ptr<Graph>> GraphManager::lookupGraphByName(
    std::string const&) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
auto GraphManager::findImplicitVertexCollectionsFromEdgeCollections(
    containers::FlatHashSet<std::string> const&) const
    -> ResultT<containers::FlatHashSet<std::string>> {
  return {containers::FlatHashSet<std::string>{}};
}
}  // namespace graph

////////////////////////////////////////////////////////////////////////////////
/// async_registry stubs
////////////////////////////////////////////////////////////////////////////////

namespace async_registry {
AddToAsyncRegistry::AddToAsyncRegistry(std::source_location) {}
AddToAsyncRegistry::~AddToAsyncRegistry() = default;
void AddToAsyncRegistry::update_requester_to_current_thread() {}
auto AddToAsyncRegistry::update_state(State) -> std::optional<State> {
  return std::nullopt;
}
}  // namespace async_registry

////////////////////////////////////////////////////////////////////////////////
/// Enterprise stubs
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_ENTERPRISE
namespace SmartGraphValidationHelper {
ResultT<std::pair<std::string, bool>> SmartValidationResult::validateVertexId(
    std::string_view, TRI_vocbase_t const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
}  // namespace SmartGraphValidationHelper
#endif

////////////////////////////////////////////////////////////////////////////////
/// transaction::Context (base class)
////////////////////////////////////////////////////////////////////////////////

namespace transaction {
Context::Context(TRI_vocbase_t& vocbase, OperationOrigin origin)
    : _vocbase(vocbase),
      _resolver(std::make_unique<CollectionNameResolver>(_vocbase)),
      _operationOrigin(origin) {}
Context::~Context() = default;
CollectionNameResolver const& Context::resolver() const noexcept {
  TRI_ASSERT(_resolver != nullptr);
  return *_resolver;
}
std::shared_ptr<Context> Context::clone() const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
TransactionId Context::generateId() const { return TransactionId(1); }

// transaction::Options static members
uint64_t Options::defaultMaxTransactionSize = 128 * 1024 * 1024;
uint64_t Options::defaultIntermediateCommitSize = 512 * 1024 * 1024;
uint64_t Options::defaultIntermediateCommitCount = 1000000;
}  // namespace transaction

////////////////////////////////////////////////////////////////////////////////
/// CreateDatabaseInfo::server()
////////////////////////////////////////////////////////////////////////////////

application_features::ApplicationServer& CreateDatabaseInfo::server() const {
  return _server;
}

////////////////////////////////////////////////////////////////////////////////
/// Destructors for types held by unique_ptr in TRI_vocbase_t
////////////////////////////////////////////////////////////////////////////////

aql::QueryCacheDatabaseEntry::~QueryCacheDatabaseEntry() {}
aql::QueryPlanCache::~QueryPlanCache() {}
CursorRepository::~CursorRepository() {}
DatabaseReplicationApplier::~DatabaseReplicationApplier() {}
bool DatabaseReplicationApplier::applies() const { return false; }
void DatabaseReplicationApplier::forget() {}
void DatabaseReplicationApplier::reconfigure(
    ReplicationApplierConfiguration const&) {}
void DatabaseReplicationApplier::storeConfiguration(bool) {}
std::shared_ptr<InitialSyncer> DatabaseReplicationApplier::buildInitialSyncer()
    const {
  return nullptr;
}
std::shared_ptr<TailingSyncer> DatabaseReplicationApplier::buildTailingSyncer(
    uint64_t, bool) const {
  return nullptr;
}
std::string DatabaseReplicationApplier::getStateFilename() const { return {}; }
ReplicationApplierState::~ReplicationApplierState() {}
ReplicationClientsProgressTracker::~ReplicationClientsProgressTracker() {}
VocbaseMetrics::~VocbaseMetrics() {}

////////////////////////////////////////////////////////////////////////////////
/// ShardID conversion operator
////////////////////////////////////////////////////////////////////////////////

ShardID::operator std::string() const { return {}; }

// ValidatorBase virtual destructor (vtable anchor)
// The header says `= default`, so we need a different virtual method.
// Since validate() is pure virtual, we need to implement ValidatorJsonSchema.
ValidatorJsonSchema::ValidatorJsonSchema(velocypack::Slice) {}

// ReplicationApplier: all virtuals are pure or inline-default, so there's
// no key function.  The linker workaround handles the missing vtable.
#include "Replication/ReplicationApplier.h"
void ReplicationApplier::startTailing(TRI_voc_tick_t, bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
void ReplicationApplier::startReplication() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
void ReplicationApplier::stopAndJoin() {}
void ReplicationApplier::stop() {}
void ReplicationApplier::stop(Result const&) {}

// ValidatorBase: validate is virtual on ValidatorBase, we define on JsonSchema
Result ValidatorJsonSchema::validateOne(velocypack::Slice,
                                        velocypack::Options const*) const {
  return {};
}
void ValidatorJsonSchema::toVelocyPackDerived(velocypack::Builder&) const {}
// Also need toVelocyPack and validate from ValidatorBase:
void ValidatorBase::toVelocyPack(velocypack::Builder&) const {}
Result ValidatorBase::validate(velocypack::Slice, velocypack::Slice, bool,
                               velocypack::Options const*) const {
  return {};
}
char const* ValidatorJsonSchema::type() const { return "json"; }

QueryRegistryFeature::~QueryRegistryFeature() = default;
void QueryRegistryFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions>) {}
void QueryRegistryFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {}
void QueryRegistryFeature::prepare() {}
void QueryRegistryFeature::stop() {}
void QueryRegistryFeature::beginShutdown() {}
void QueryRegistryFeature::unprepare() {}

// ExecutorExpressionContext — need a virtual method
aql::AqlValue aql::ExecutorExpressionContext::getVariableValue(
    aql::Variable const*, bool, bool&) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

}  // namespace arangodb

////////////////////////////////////////////////////////////////////////////////
/// Collections — client-tool stub (replaces Collections.cpp)
///
/// The real Collections::add() calls getFeature<QueryRegistryFeature>() to
/// check maxCollectionsPerQuery. We skip that check here.
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Collection.h"
#include "Aql/Collections.h"

namespace arangodb::aql {

Collections::Collections(TRI_vocbase_t* vocbase) : _vocbase(vocbase) {}
Collections::~Collections() = default;

Collection* Collections::get(std::string_view name) const {
  auto it = _collections.find(name);
  return (it != _collections.end()) ? it->second.get() : nullptr;
}

Collection* Collections::add(std::string const& name,
                             AccessMode::Type accessType,
                             Collection::Hint hint) {
  auto it = _collections.find(name);
  if (it == _collections.end()) {
    auto collection =
        std::make_unique<Collection>(name, _vocbase, accessType, hint);
    it = _collections.try_emplace(name, std::move(collection)).first;
  } else {
    if (AccessMode::isReadWriteChange(accessType, it->second->accessType())) {
      it->second->isReadWrite(true);
    }
    if (AccessMode::isWriteOrExclusive(accessType) &&
        it->second->accessType() == AccessMode::Type::READ) {
      it->second->accessType(accessType);
    } else if (AccessMode::isExclusive(accessType)) {
      it->second->accessType(accessType);
    }
  }
  return it->second.get();
}

std::vector<std::string> Collections::collectionNames() const {
  std::vector<std::string> result;
  result.reserve(_collections.size());
  for (auto const& it : _collections) {
    if (!it.first.empty() && it.first[0] >= '0' && it.first[0] <= '9') {
      continue;
    }
    result.emplace_back(it.first);
  }
  return result;
}

bool Collections::empty() const { return _collections.empty(); }

void Collections::toVelocyPack(
    velocypack::Builder& builder,
    std::function<bool(std::string const&, Collection const&)> const& filter)
    const {
  builder.openArray();
  for (auto const& c : _collections) {
    if (!filter(c.first, *c.second)) {
      continue;
    }
    builder.openObject();
    builder.add("name", VPackValue(c.first));
    builder.add("type",
                VPackValue(AccessMode::typeString(c.second->accessType())));
    builder.close();
  }
  builder.close();
}

void Collections::visit(
    std::function<bool(std::string const&, Collection&)> const& visitor) const {
  for (auto const& it : _collections) {
    if (!visitor(it.first, *it.second.get())) {
      return;
    }
  }
}

}  // namespace arangodb::aql

////////////////////////////////////////////////////////////////////////////////
/// QueryContext — client-tool stub (replaces QueryContext.cpp)
///
/// The real QueryContext constructor/destructor call
/// getFeature<DatabaseFeature>() which is not available in client tools.
/// This stub skips those calls. AqlFeature::lease()/unlease() are no-ops.
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/QueryContext.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/StaticStrings.h"

namespace arangodb::aql {

QueryContext::QueryContext(TRI_vocbase_t& vocbase,
                           transaction::OperationOrigin operationOrigin,
                           QueryId id)
    : _resourceMonitor(
          std::make_shared<ResourceMonitor>(GlobalResourceMonitor::instance())),
      _queryId(id ? id : TRI_NewServerSpecificTick()),
      _collections(&vocbase),
      _vocbase(vocbase),
      _execState(QueryExecutionState::ValueType::INVALID_STATE),
      _operationOrigin(operationOrigin),
      _numRequests(0) {
  // Client-tool stub: skip DatabaseFeature / AqlFeature checks.
}

QueryContext::~QueryContext() {
  _graphs.clear();
  // Client-tool stub: skip DatabaseFeature / AqlFeature checks.
}

TRI_vocbase_t& QueryContext::vocbase() const noexcept { return _vocbase; }

transaction::OperationOrigin QueryContext::operationOrigin() const noexcept {
  return _operationOrigin;
}

Collections& QueryContext::collections() { return _collections; }

Collections const& QueryContext::collections() const { return _collections; }

std::vector<std::string> QueryContext::collectionNames() const {
  return _collections.collectionNames();
}

std::string const& QueryContext::user() const { return StaticStrings::Empty; }

QueryWarnings& QueryContext::warnings() { return _warnings; }
QueryWarnings const& QueryContext::warnings() const { return _warnings; }

ResultT<graph::Graph const*> QueryContext::lookupGraphByName(
    std::string const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void QueryContext::addDataSource(LogicalDataSource const& ds) {
  _queryDataSources.try_emplace(ds.guid(), ds.name());
}

Ast* QueryContext::ast() const { return _ast.get(); }

QueryContext::QueryApiSynchronicity QueryContext::queryApiSynchronicity()
    const noexcept {
  return _queryApiSynchronicity;
}

}  // namespace arangodb::aql

// ClusterEngine::Mocking (enterprise only, non-constexpr static)
#ifdef USE_ENTERPRISE
bool arangodb::ClusterEngine::Mocking = false;
#endif

////////////////////////////////////////////////////////////////////////////////
/// std::hash<ShardID>
////////////////////////////////////////////////////////////////////////////////

namespace std {
size_t hash<arangodb::ShardID>::operator()(
    arangodb::ShardID const&) const noexcept {
  return 0;
}
}  // namespace std

////////////////////////////////////////////////////////////////////////////////
/// IgnoreNoAccessAqlTransaction vtable stubs (enterprise)
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_ENTERPRISE
namespace arangodb::transaction {
futures::Future<OperationResult> IgnoreNoAccessAqlTransaction::any(
    std::string const&, OperationOptions const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
futures::Future<Result> IgnoreNoAccessAqlTransaction::documentFastPath(
    std::string const&, velocypack::Slice, OperationOptions const&,
    velocypack::Builder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
futures::Future<Result> IgnoreNoAccessAqlTransaction::documentFastPathLocal(
    std::string_view, std::string_view,
    IndexIterator::DocumentCallback const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
auto IgnoreNoAccessAqlTransaction::documentInternal(std::string const&,
                                                    velocypack::Slice,
                                                    OperationOptions const&,
                                                    MethodsApi)
    -> futures::Future<OperationResult> {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
futures::Future<OperationResult> IgnoreNoAccessAqlTransaction::all(
    std::string const&, uint64_t, uint64_t, OperationOptions const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
auto IgnoreNoAccessAqlTransaction::countInternal(std::string const&, CountType,
                                                 OperationOptions const&,
                                                 MethodsApi)
    -> futures::Future<OperationResult> {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
bool IgnoreNoAccessAqlTransaction::isLocked(LogicalCollection*,
                                            AccessMode::Type) const {
  return false;
}
}  // namespace arangodb::transaction
#endif

////////////////////////////////////////////////////////////////////////////////
/// date library stubs
////////////////////////////////////////////////////////////////////////////////

#include <date/tz.h>

namespace date {
tzdb const& get_tzdb() { THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED); }
tzdb_list& get_tzdb_list() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
time_zone const* current_zone() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
time_zone const* locate_zone(std::string_view) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

// time_zone virtual method stubs (needed for typeinfo)
sys_info time_zone::get_info_impl(sys_seconds) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
local_info time_zone::get_info_impl(local_seconds) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
}  // namespace date

// Dummy typeinfo / vtable symbols for classes whose destructors cannot be
// defined without massive cascades are provided by AqlStandaloneDummySymbols.c
