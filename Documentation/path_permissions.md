# ArangoDB REST API Endpoint Permissions

## Migration philosophy
 
In the "classic" system permissions were given on a per user basis and
could only be configured for databases and collections. Essentially, there
were three levels:

  - NONE
  - RO (read-only)
  - RW (read-write)

which a particular user could have for each database and - within a
database - for each collection. Important here is that `RW` access
always **included RO** access.

The `_system` database has played an important role, since many API
accesses were authorized by asking if the user has RW access to the
`_system` database.

Often, access to meta data was goverened by RO or RW permissions on
the **container**. For example, creating an index on a collection was
allowed, if the user had RW access to the database which contained
the collection.

Finally, there is the "SUPERUSER" access, which means that a valid
JWT token without a `preferred_username` field was found. SUPERUSER
access has **no restrictions whatsoever** and is allowed to **do
everything**. This is used for cluster-internal communication.

This was all not very convenient and flexible and was for many things
very **coarse-grained**.

RBAC strives to

 - make this more fine-grained
 - create an indirection via "roles"
 - make this a lot more flexible by also adding "resource patterns"
   to be able to specify access rights to groups of resources with
   one "policy", rather than specifying everything collection-by-collection.
 - keep the SUPERUSER access

Of course, we need to maintain backwards compatibility for the case that
RBAC is **not enabled** in the core DB.

The basic way to implement this new system is to overhaul all of the
authorization across all APIs in the following way:

We create an abstraction so that we can specify which access permissions
one needs for each operation across all APIs. Then we implement this
abstraction by a number of methods on the `ExecContext`, which contains
the user and role data from authentication. For example, there will be
methods like `ExecContext::canSeeCollection(<dbname>, <collname>) -> bool`.

The `ExecContext` then has a member `AuthMode _authMode`, which implements
these abstractions. `AuthMode` itself is then just an interface and we can
have different implementations for "RBAC disabled" (implementing the old
system) and for "RBAC enabled" (implementing the new RBAC system) and
possibly others like "no authentication" and "test mode".

This means it becomes an interesting exercise to even **define** this
abstraction. It needs to be fine-grained enough to be able to be mapped
to the RBAC system. And it needs to be fine-grained enough for all nuances
of the old system (which has grown wild over time to a certain extent).

Then we have to go through **all API implementations** and change **all authorization
checks** by calls to the new abstraction API.

Then we need to implement the checks in the different `AuthMode` variants.

This makes the new system relatively well **reviewable**. We have to check
that in each API we call the right method(s) on the `ExecContext`. This is
a distributed exercise and can be parallelized, once we have a completely
clear documentation as to what the abstraction does.

Then we "only" have to review the non-RBAC implementation to see if it implements
exactly what is specified.

For RBAC enabled, we can then verify the new implementation and thus exactly know
to what new permissions the old system maps.

Finally, we must add extensive tests.


### Where to check authorization

There are essentially three places in the execution paths of APIs,
where authorization checks are done. Keep in mind that we do an
**authentication check** very early on in the still in the `CommTask`,
before we have even created a `RestHandler` object. This will essentially
verify if a proper `Authorization` header is present. This could be basic
auth or a JWT token. If there is no proper authentication, we can forbid
the request right away. If we do not know the user or the JWT signature is
not valid, we can decline the request right away as FORBIDDEN. Of course,
there are a select few URL paths, which are "open" and do not require
any authentication, and we need to handle the case of "no authentication",
too.

After authentication, we perform a first authorization check: Namely, the
identity detected (user/roles) has to have **read access** to the database
which was specified in the `/_db/<dbname>` part of the URL path. This check
is done globally already in the `CommTask` to error out early, since we want
to enforce it **for all routes** (with very few exceptions).

The bulk of the authorization checks is then performed in the `RestHandlers`
(or, for 3.12, in the server-side JavaScript functions). The idea is that
most general permission checks are done in the `RestHandlers`, **with the
exception of collection and view access checks**.

This is the third place, where we do authorization checks: Since basically
all collection accesses need a transaction, we enforce collection access
permissions in the transaction code, basically, when collections/views are
added to a transaction.


### Mapping the old permissions to the new RBAC system

Philosophy: "Keep the authorization in ´arangod` as much as possible
as it is without RBAC, with the following modifications, if RBAC is
enabled:

 - Database and collection access is controlled by RBAC instead of data
   in the _users collection.
 - It is also a bit more fine-grained (in particular for collections).
 - For databases, we have three access levels:
    1. NONE
    2. RO
    3. RW
   These are controlled by two RBAC actions:
    - `db.ReadDatabase` (with a database as resource `db:database:<name>`)
    - `db.WriteDatabase` (with a database as resource `db:database:<name>`)
   The first governs if an identity can use the database at all, at
   the same time it governs, whether or one can see a database in the
   listing of all accessible databases (`GET /_api/database/user`). The
   second governs creating and dropping, as well as changing properties
   of a database. This is very similar to the classic system, except that
   creating and dropping of database "d" used to be regulated by RW access
   to the `_system` database ("container principle"). Now, we have more
   fine grained authorization with resource patterns. The `_system` database
   is no longer so special.
   This means, one has at least level "RO", if one has `db.ReadDatabase` for
   a database, and one has level "RW", if one has both `db.ReadDatabase` and
   `db.WriteDatabase` for the database.
 - For collections, we split "Collection RW" into two separate access
   levels: "RWDATA" (which includes reading the collection meta data and
   data!) and "RW" (which additionally includes creating, dropping,
   and modifying the collection meta data), so we have these access
   levels:
    1. NONE
    2. RO
    3. RWDATA
    4. RW
 - There are three RBAC actions for collections: `db:ReadCollection`,
   `db:WriteCollectionData` and `db:WriteCollectionMeta`. To reach
   level `Read` for a collection, one only needs "allow" for
   `db:ReadCollection`. To reach level `RW data` one needs "allow" for
   `db:ReadCollection` and `db:WriteCollectionData`. To reach level `RW
   all` one needs "allow" on all three actions.
 - For views, there are three levels:
    1. NONE
    2. RO
    3. RW
   There are two RBAC actions for views: `db:ReadView` and `db:WriteView`.
   One needs `db:ReadView` to achieve at least level RO and one needs
   noth `db:ReadView` and `db:WriteView` to achieve level RW.
 - All places that previously required RW access to the `_system`
   database are assigned to exactly one of the actions with the prefix
   `db:Admin`, for which one needs "allow" to execute the operation.
 - Enabled RBAC implies `--server.harden`.
 - Every other change is an exception, which we (grudgingly) make because we
   found some issue with the current system
 - There is an additional action `db:UseApiVersion` to configure, which roles
   are allowed to use which API versions (the API version is the resource)
 
This philosophy helps in the following ways:
 
 - simple to explain and document
 - simple to implement (can keep at lot of code)
 - simple to review (in particular w.r.t. same behaviour as before with RBAC disabled!)
 - relatively simple to test (relatively few different cases)
 - maintains the "spirit" of RBAC that a "deny" should trump any potentially
   contradicting "allow" (which is why we cannot use OR conditions)
 

### Abstraction in `ExecContext` to check these permissions

The `ExecContext` offers the following checking methods:

 - `canUseAdminAction(rbac::Category::Any const action) -> bool`
 - `canUseHardenedAction(rbac::Category::Any const action) -> bool`

 - `canSeeDatabase(std::string_view db) -> bool`
 - `canCreateDatabase(std::string_view db) -> bool`
 - `canDropDatabase(std::string_view db) -> bool`
 - `canUseDatabase(std::string_view db, AccessLevel const level) -> bool`

 - `canSeeCollection(std::string_view db, std::string_view coll) -> bool`
 - `canCreateCollection(std::string_view db, std::string_view coll) -> bool`
 - `canDropCollection(std::string_view db, std::string_view coll) -> bool`
 - `canUseCollection(std::string_view db, std::string_view view, AccessLevel const level) -> bool`

 - `canSeeView(std::string_view db, std::string_view view) -> bool`
 - `canCreateView(std::string_view db, std::string_view view) -> bool`
 - `canDropView(std::string_view db, std::string_view view) -> bool`
 - `canUseView(std::string_view db, std::string_view view) -> bool`

 - `canSeeAnalyzer(std::string_view db, std::string_view analyzer) -> bool`
 - `canCreateAnalyzer(std::string_view db, std::string_view analyzer) -> bool`
 - `canDropAnalyzer(std::string_view db, std::string_view analyzer) -> bool`
 - `canUseAnalyzer(std::string_view db, std::string_view analyzer) -> bool`

Note that for now, `canSee*` is equivalent to `canUse*(RO)` and
`canCreate*` and `canDrop*` are equivalent to `canUse*(RW)`. For
collections `canUseCollection(RWDATA)` is needed to write data. However,
we keep the semantic checks separate in case we want to split things
further later.

There is one subtlety, though. If we separate `canSee*` from
`canUse*(RO)` later, then we want that if a user cannot see a collection
(say) and cannot read it, then the error when trying to access it should
be "NOTFOUND", to not give away the information that the collection
exists!

Therefore, a common pattern in the code will be:

```
if (!canSee*(db, coll)) {
  return NOTFOUND;
}
if (!canUse*(db, coll, RO)) {
  return FORBIDDEN;
}
```


## Complete Endpoint–Action Reference Table

(still being edited)

Ideas:

 - `NONE` stays no authentication required
 - `ANY` stays any authenticated user (no further checks in handler)
 - `DB` access stays as it is
 - `COLL` access distinguishes meta data and document data
 - views and analyzers get their own actions like collection meta data
 - `ADMIN` is split into many actions with prefix `db:Admin`
 - `HARDENED` is considered to be always on for RBAC, so becomes the same as `ADMIN`
 - `SUPERUSER` stays superuser only

Note that some APIs are marked "ANY", but this is **not** dangerous because
  - the API is only available in MAINTAINER_MODE
  - the API is only available on DBServers or agents, which do not have users, so they
    only accept SUPERUSER anyway
  - the API is only compiled in when FAILURE_TESTS are activated

Others are marked `ADMIN` but run only on DBServers or agents, so that they actually only
accept SUPERUSER anyway.
  
Furthermore, there are some command line switches, which change authentication behaviour,
in a lot of cases these have 3 possible values: `SUPERUSER`, `ADMIN`, `ANY`, sometimes
it is possible to switch off the API entirely. These switches remain and take precedence.
RBAC will only be considered if the switch is on `ADMIN`.


## Table of paths and authentification
 
Meanings of abbreviations:

OPEN         - always open
AUTHEN       - some existing user (or SUPERUSER) has to be authenticated, no further authorization check
               must have read access to the used database from /_db/<dbname>
Admin*       - with RBAC, one needs that action, without RBAC, one needs RW on _system
HARD         - without RBAC, one needs RW on _system (with RBAC, --server.hardened is always on)
               (if Admin* and HARD are written, then AUTHEN holds when --server.hardened is off without RBAC)
DB RW        - Read/write auth level for the database
DB RO        - At least read-only auth level for the database
COLL RO      - At least Read auth level for the collection
`_system` RW - Read/write auth level for _system database
?/S/A        - API is switchable between off, superuser and admin access, additionally, an Admin* is specified
S/A          - API is switchable between superuser and admin access, additionally, an Admin* is specified


| Method | Path                                                         | RestHandler                | Authorization                       | Comments                                 | Changes to before RBAC |
|--------|--------------------------------------------------------------|----------------------------|-------------------------------------|------------------------------------------|------------------------|
| POST   | `/_open/auth`                                                | RestAuthHandler            | OPEN                                |                                          |                        |
| POST   | `/_open/auth/renew`                                          | RestAuthHandler            | OPEN                                |                                          |                        |
| GET    | `/_admin/actions`                                            | MaintenanceRestHandler     | AUTHEN                              | Only really relevant on DBServers        |                        |
| POST   | `/_admin/actions`                                            | MaintenanceRestHandler     | AUTHEN                              | Only really relevant on DBServers        |                        |
| PUT    | `/_admin/actions`                                            | MaintenanceRestHandler     | AUTHEN                              | Only really relevant on DBServers        |                        |
| DELETE | `/_admin/actions/{id}`                                       | MaintenanceRestHandler     | AUTHEN                              | Only really relevant on DBServers        |                        |
| GET    | `/_admin/activities`                                         | activities::RestHandler    | S/A AdminMonitoringInternal         |                                          |                        |
| GET    | `/_admin/async-registry`                                     | async_registry::RestHandler| S/A AdminMonitoringInternal         |                                          |                        |
| POST   | `/_admin/auth/reload`                                        | RestAdminAuthReloadHandler | AdminAuthReload                     |                                          |                        |
| POST   | `/_admin/backup/create`                                      | RestHotBackupHandler       | S/A AdminBackup                     | (EE only)                                |                        |
| POST   | `/_admin/backup/delete`                                      | RestHotBackupHandler       | S/A AdminBackup                     | (EE only)                                |                        |
| POST   | `/_admin/backup/download`                                    | RestHotBackupHandler       | S/A AdminBackup                     | (EE only)                                |                        |
| POST   | `/_admin/backup/list`                                        | RestHotBackupHandler       | S/A AdminBackup                     | (EE only)                                |                        |
| POST   | `/_admin/backup/upload`                                      | RestHotBackupHandler       | S/A AdminBackup                     | (EE only)                                |                        |
| GET    | `/_admin/cluster/collectionShardDistribution`                | RestAdminClusterHandler    | AdminMoveShards                     | (cluster only)                           |                        |
| PUT    | `/_admin/cluster/cancelAgencyJob`                            | RestAdminClusterHandler    | AdminMoveShards                     | (cluster only)                           |                        |
| PUT    | `/_admin/cluster/cleanOutServer`                             | RestAdminClusterHandler    | AdminMoveShards                     | (cluster only)                           |                        |
| GET    | `/_admin/cluster/health`                                     | RestAdminClusterHandler    | AUTHEN                              | (cluster only)                           |                        |
| GET    | `/_admin/cluster/maintenance`                                | RestAdminClusterHandler    | AdminMaintenance                    | (cluster only)                           |                        |
| PUT    | `/_admin/cluster/maintenance`                                | RestAdminClusterHandler    | AdminMaintenance                    | (cluster only)                           |                        |
| GET    | `/_admin/cluster/maintenance/{serverId}`                     | RestAdminClusterHandler    | AdminMaintenance                    | (cluster only)                           |                        |
| PUT    | `/_admin/cluster/maintenance/{serverId}`                     | RestAdminClusterHandler    | AdminMaintenance                    | (cluster only)                           |                        |
| POST   | `/_admin/cluster/moveShard`                                  | RestAdminClusterHandler    | AdminMoveShards or coll RW          | (cluster only)                           |                        |
| PUT    | `/_admin/cluster/moveShard`                                  | RestAdminClusterHandler    | AdminMoveShards or coll RW          | (cluster only)                           |                        |
| GET    | `/_admin/cluster/nodeEngine`                                 | RestAdminClusterHandler    | AUTHEN                              | (cluster only)                           |                        |
| GET    | `/_admin/cluster/nodeStatistics`                             | RestAdminClusterHandler    | AUTHEN                              | (cluster only)                           |                        |
| GET    | `/_admin/cluster/nodeVersion`                                | RestAdminClusterHandler    | AUTHEN                              | (cluster only)                           |                        |
| GET    | `/_admin/cluster/numberOfServers`                            | RestAdminClusterHandler    | AUTHEN                              | (cluster only)                           |                        |
| PUT    | `/_admin/cluster/numberOfServers`                            | RestAdminClusterHandler    | AdminMaintenance, HARD              | (cluster only)                           |                        |
| PUT    | `/_admin/cluster/queryAgencyJob`                             | RestAdminClusterHandler    | AdminMoveShards                     | (cluster only)                           |                        |
| GET    | `/_admin/cluster/rebalance`                                  | RestAdminClusterHandler    | AdminMoveShards                     | (cluster only)                           |                        |
| PUT    | `/_admin/cluster/rebalance`                                  | RestAdminClusterHandler    | AdminMoveShards                     | (cluster only)                           |                        |
| PUT    | `/_admin/cluster/rebalanceShards`                            | RestAdminClusterHandler    | AUTHEN + DB RW                      | (cluster only)                           |                        |
| POST   | `/_admin/cluster/removeServer`                               | RestAdminClusterHandler    | AdminRemoveServer                   | (cluster only)                           |                        |
| PUT    | `/_admin/cluster/resignLeadership`                           | RestAdminClusterHandler    | AdminMoveShards                     | (cluster only)                           |                        |
| GET    | `/_admin/cluster/shardDistribution`                          | RestAdminClusterHandler    | AdminClusterInfo                    | (cluster only)                           |                        |
| GET    | `/_admin/cluster/shardStatistics`                            | RestAdminClusterHandler    | AdminClusterInfo                    | (cluster only)                           |                        |
| GET    | `/_admin/cluster/statistics`                                 | RestAdminClusterHandler    | AUTHEN                              | (cluster only)                           |                        |
| PUT    | `/_admin/cluster/uniqId`                                     | RestAdminClusterHandler    | AdminMaintenance                    | (cluster only)                           |                        |
| PUT    | `/_admin/cluster/vpackSortMigration/{serverId}`              | RestAdminClusterHandler    | SUPER                               | (cluster only)                           |                        |
| PUT    | `/_admin/compact`                                            | RestCompactHandler         | SUPER                               |                                          |                        |
| GET    | `/_admin/crashes`                                            | RestCrashHandler           | AdminCrashHandler                   |                                          |                        |
| GET    | `/_admin/crashes/{id}`                                       | RestCrashHandler           | AdminCrashHandler                   |                                          |                        |
| DELETE | `/_admin/crashes/{id}`                                       | RestCrashHandler           | AdminCrashHandler                   |                                          |                        |
| GET    | `/_admin/database/target-version`                            | RestAdminDatabaseHandler   | AUTHEN                              |                                          |                        |
| GET    | `/_admin/debug/failat`                                       | RestDebugHandler           | AUTHEN                              | (maintainer mode only)                   |                        |
| GET    | `/_admin/debug/failat/all`                                   | RestDebugHandler           | AUTHEN                              | (maintainer mode only)                   |                        |
| PUT    | `/_admin/debug/failat/{name}`                                | RestDebugHandler           | AUTHEN                              | (maintainer mode only)                   |                        |
| DELETE | `/_admin/debug/failat`                                       | RestDebugHandler           | AUTHEN                              | (maintainer mode only)                   |                        |
| DELETE | `/_admin/debug/failat/{name}`                                | RestDebugHandler           | AUTHEN                              | (maintainer mode only)                   |                        |
| DELETE | `/_admin/debug/raceControl`                                  | RestDebugHandler           | AUTHEN                              | (maintainer mode only)                   |                        |
| PUT    | `/_admin/debug/crash`                                        | RestDebugHandler           | AUTHEN                              | (maintainer mode only)                   |                        |
| GET    | `/_admin/deployment/id`                                      | RestAdminDeploymentHandler | AUTHEN                              | only coordinators and single             |                        |
| POST   | `/_admin/execute`                                            | RestAdminExecuteHandler    | AUTHEN                              | only --javascript.allow-admin-execute    |                        |
| GET    | `/_admin/job/{id}`                                           | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| GET    | `/_admin/job/{type}`                                         | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| PUT    | `/_admin/job/{id}`                                           | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| PUT    | `/_admin/job/{id}/cancel`                                    | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| DELETE | `/_admin/job/all`                                            | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| DELETE | `/_admin/job/expired`                                        | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| DELETE | `/_admin/job/{id}`                                           | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| GET    | `/_api/job/{id}`                                             | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| GET    | `/_api/job/{type}`                                           | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| PUT    | `/_api/job/{id}`                                             | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| PUT    | `/_api/job/{id}/cancel`                                      | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| DELETE | `/_api/job/all`                                              | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| DELETE | `/_api/job/expired`                                          | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| DELETE | `/_api/job/{id}`                                             | RestJobHandler             | AUTHEN                              | THIS DOES NOT SOUND RIGHT!               | ??? FIXME              |
| GET    | `/_admin/license`                                            | RestLicenseHandler(EE)     | AdminLicense HARD                   |                                          |                        |
| PUT    | `/_admin/license`                                            | RestLicenseHandler(EE)     | AdminLicense HARD                   |                                          |                        |
| GET    | `/_admin/log`                                                | RestAdminLogHandler        | ?/S/A AdminReadLogs                 |                                          |                        |
| GET    | `/_admin/log/entries`                                        | RestAdminLogHandler        | ?/S/A AdminReadLogs                 |                                          |                        |
| GET    | `/_admin/log/level`                                          | RestAdminLogHandler        | ?/S/A AdminReadLogs                 |                                          |                        |
| GET    | `/_admin/log/structured`                                     | RestAdminLogHandler        | ?/S/A AdminReadLogs                 |                                          |                        |
| PUT    | `/_admin/log/level`                                          | RestAdminLogHandler        | ?/S/A AdminSetLogLevel              |                                          |                        |
| PUT    | `/_admin/log/structured`                                     | RestAdminLogHandler        | ?/S/A AdminSetLogLevel              |                                          |                        |
| DELETE | `/_admin/log`                                                | RestAdminLogHandler        | ?/S/A AdminSetLogLevel              |                                          |                        |
| DELETE | `/_admin/log/entries`                                        | RestAdminLogHandler        | ?/S/A AdminSetLogLevel              |                                          |                        |
| DELETE | `/_admin/log/level`                                          | RestAdminLogHandler        | ?/S/A AdminSetLogLevel              |                                          |                        |
| GET    | `/_admin/metrics`                                            | RestMetricsHandler         | AdminMonitoring HARD                |                                          |                        |
| GET    | `/_admin/options`                                            | RestOptionsHandler         | S/A AdminOptions                    |                                          |                        |
| GET    | `/_admin/options-description`                                | RestOptionsHandler         | S/A AdminOptions                    |                                          |                        |
| GET    | `/_admin/options-public`                                     | RestOptionsHandler         | AUTHEN + DB RO                      | NON_STANDARD                             |                        |
| POST   | `/_admin/routing/reload`                                     | RestAdminRoutingHandler    | AUTHEN                              | (V8 required)                            |                        |
| GET    | `/_admin/server/api-calls`                                   | RestAdminServerHandler     | ?/S/A AdminApiCalls                 |                                          |                        |
| GET    | `/_admin/server/aql-queries`                                 | RestAdminServerHandler     | ?/S/A AdminAqlQueries               |                                          |                        |
| GET    | `/_admin/server/availability`                                | RestAdminServerHandler     | OPEN                                |                                          |                        |
| GET    | `/_admin/server/databaseDefaults`                            | RestAdminServerHandler     | AUTHEN                              |                                          |                        |
| GET    | `/_admin/server/id`                                          | RestAdminServerHandler     | AUTHEN                              |                                          |                        |
| GET    | `/_admin/server/mode`                                        | RestAdminServerHandler     | AUTHEN                              |                                          |                        |
| PUT    | `/_admin/server/mode`                                        | RestAdminServerHandler     | AdminMaintenance                    |                                          |                        |
| GET    | `/_admin/server/role`                                        | RestAdminServerHandler     | AUTHEN                              |                                          |                        |
| GET    | `/_admin/server/tls`                                         | RestAdminServerHandler     | AUTHEN                              |                                          |                        |
| POST   | `/_admin/server/tls`                                         | RestAdminServerHandler     | SUPER                               |                                          |                        |
| GET    | `/_admin/server/jwt`                                         | RestAdminServerHandler     | AUTHEN                              |                                          |                        |
| POST   | `/_admin/server/jwt`                                         | RestAdminServerHandler     | SUPER                               |                                          |                        |
| GET    | `/_admin/server/encryption`                                  | RestAdminServerHandler     | AUTHEN                              |                                          |                        |
| POST   | `/_admin/server/encryption`                                  | RestAdminServerHandler     | SUPER                               |                                          |                        |
| GET    | `/_admin/shutdown`                                           | RestShutdownHandler        |                                     |                                          |                        |
| DELETE | `/_admin/shutdown`                                           | RestShutdownHandler        |                                     |                                          |                        |
| GET    | `/_admin/statistics`                                         | RestAdminStatisticsHandler | AdminMonitoring HARD                |                                          |                        |
| GET    | `/_admin/statistics-description`                             | RestAdminStatisticsHandler | AdminMonitoring HARD                |                                          |                        |
| GET    | `/_admin/status`                                             | RestAdminStatusHandler     | AdminMonitoring HARD                |                                          |                        |
| GET    | `/_admin/supervisionState`                                   | RestSupervisionStateHandler| AdminSupervisionState               | (cluster only)                           |                        |
| GET    | `/_admin/support-info`                                       | RestSupportInfoHandler     | AdminMonitoring                     |                                          |                        |
| GET    | `/_admin/system-report`                                      | RestSystemReportHandler    | AdminMonitoringInternal HARD        |                                          |                        |
| GET    | `/_admin/telemetrics`                                        | RestTelemtricsHandler      | AdminMonitoringInternal             |                                          |                        |
| DELETE | `/_admin/telemetrics`                                        | RestTelemetricsHandler     | AdminMonitoringInternal             |                                          |                        |
| GET    | `/_admin/time`                                               | RestTimeHandler            | AUTHEN                              |                                          |                        |
| GET    | `/_admin/usage-metrics`                                      | RestUsageMetricsHandler    | AdminMonitoringInternal HARD        |                                          |                        |
| GET    | `/_admin/version`                                            | RestVersionhandler         | AUTHEN, details (2)                 |                                          |                        |
| GET    | `/_admin/wal/properties`                                     | RestWalAccessHandler       | SUPER                               | (RocksDB engine)                         |                        |
| PUT    | `/_admin/wal/properties`                                     | RestWalAccessHandler       | SUPER                               | (RocksDB engine)                         |                        |
| GET    | `/_admin/wal/transactions`                                   | RestWalAccessHandler       | SUPER                               | (RocksDB engine)                         |                        |
| PUT    | `/_admin/wal/flush`                                          | RestWalAccessHandler       | SUPER                               | (RocksDB engine)                         |                        |
| PUT    | `/_admin/wal/wait_for_estimator_sync`                        | RestWalAccessHandler       | SUPER                               | (RocksDB engine)                         |                        |
| GET    | `/_admin/wal/properties`                                     | ClusterRestWalHandler      | AUTHEN                              | (Cluster engine) NOT_IMPL                |                        |
| PUT    | `/_admin/wal/properties`                                     | ClusterRestWalHandler      | AUTHEN                              | (Cluster engine) NOT_IMPL                |                        |
| GET    | `/_admin/wal/transactions`                                   | ClusterRestWalHandler      | AUTHEN                              | (Cluster engine) NOT_IMPL                |                        |
| PUT    | `/_admin/wal/flush`                                          | ClusterRestWalHandler      | AUTHEN                              | (Cluster engine) DELEGATED               |                        |
| PUT    | `/_admin/wal/wait_for_estimator_sync`                        | ClusterRestWalHandler      | AdminWalAccess (PROD)/SUPER (MAINT) | (Cluster engine)                         |                        |
| GET    | `/_api/aql-builtin`                                          | RestAqlFunctionsHandler    | AUTHEN                              |                                          |                        |
| GET    | `/_api/aqlfunction`                                          | RestAqlUserFunctionsHandler| AUTHEN + COLL RO _aqlfunctions      | (V8 required)                            |                        |
| GET    | `/_api/aqlfunction/{namespace}`                              | RestAqlUserFunctionsHandler| AUTHEN + COLL RO _aqlfunctions      | (V8 required)                            |                        |
| POST   | `/_api/aqlfunction`                                          | RestAqlUserFunctionsHandler| AUTHEN + COLL RW _aqlfunctions      | (V8 required)                            |                        |
| DELETE | `/_api/aqlfunction/{name}`                                   | RestAqlUserFunctionsHandler| AUTHEN + COLL RW _aqlfunctions      | (V8 required)                            |                        |
| GET    | `/_api/analyzer`                                             | RestAnalyzerHandler        | AUTHEN + COLL RO _analyzers         | IS THIS OK???                            |                        |
| GET    | `/_api/analyzer/{name}`                                      | RestAnalyzerHandler        | AUTHEN + COLL RO _analyzers         | IS THIS OK???                            |                        |
| POST   | `/_api/analyzer`                                             | RestAnalyzerHandler        | AUTHEN + COLL RW _analyzers         | IS THIS OK???                            |                        |
| DELETE | `/_api/analyzer/{name}`                                      | RestAnalyzerHandler        | AUTHEN + COLL RW _analyzers         | IS THIS OK???                            |                        |
| GET    | `/_api/cluster/agency-cache`                                 | RestClusterHandler         | AdminReadAgency                     | (cluster only)                           | was RW/_system         |
| GET    | `/_api/cluster/agency-dump`                                  | RestClusterHandler         | AdminReadAgency                     | (cluster only)                           |                        |
| GET    | `/_api/cluster/cluster-info`                                 | RestClusterHandler         | AdminClusterInfo                    | (cluster only)                           |                        |
| PUT    | `/.../flush`                                                 | RestClusterHandler         | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| GET    | `/.../get_collection_info/{db}/{coll}`                       | RestClusterHandler         | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| GET    | `/.../get_collection_info_current/{db}/{coll}/{shard}`       | RestClusterHandler         | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| POST   | `/.../get_responsible_servers`                               | RestClusterHandler         | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| POST   | `/.../get_responsible_shard/{db}/{coll}`                     | RestClusterHandler         | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| GET    | `/.../get_analyzers_revision/{db}`                           | RestClusterHandler         | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| GET    | `/.../wait_for_plan_version/{version}`                       | RestClusterHandler         | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| GET    | `/.../get_max_number_of_shards`                              | RestClusterHandler         | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| GET    | `/.../get_max_replication_factor`                            | RestClusterHandler         | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| GET    | `/.../get_min_replication_factor`                            | RestClusterHandler         | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| GET    | `/_api/cluster/endpoints`                                    | RestClusterHandler         | AUTHEN                              | (cluster only)                           |                        |
| GET    | `/_api/collection`                                           | RestCollectionHandler      | AUTHEN, details (3)                 |                                          |                        |
| POST   | `/_api/collection`                                           | RestCollectionHandler      | DB RW                               | should be canCreateDropCollection        | FIXME                  |
| GET    | `/_api/collection/{name}`                                    | RestCollectionHandler      | canUseCollection(Read)              |                                          |                        |
| GET    | `/_api/collection/{name}/checksum`                           | RestCollectionHandler      | canUseCollection(Read)              |                                          |                        |
| GET    | `/_api/collection/{name}/count`                              | RestCollectionHandler      | canUseCollection(Read)              |                                          |                        |
| GET    | `/_api/collection/{name}/figures`                            | RestCollectionHandler      | canUseCollection(Read)              |                                          |                        |
| GET    | `/_api/collection/{name}/properties`                         | RestCollectionHandler      | canUseCollection(Read)              |                                          |                        |
| GET    | `/_api/collection/{name}/revision`                           | RestCollectionHandler      | canUseCollection(Read)              |                                          |                        |
| GET    | `/_api/collection/{name}/shards`                             | RestCollectionHandler      | canUseCollection(Read)              |                                          |                        |
| PUT    | `/_api/collection/{name}/compact`                            | RestCollectionHandler      | canUseCollection(WriteMeta)         |                                          |                        |
| PUT    | `/_api/collection/{name}/load`                               | RestCollectionHandler      | canUseCollection(Read)              |                                          |                        |
| PUT    | `/_api/collection/{name}/loadIndexesIntoMemory`              | RestCollectionHandler      | canUseCollection(Read)              |                                          |                        |
| PUT    | `/_api/collection/{name}/properties`                         | RestCollectionHandler      | canUseCollection(WriteMeta)         |                                          |                        |
| PUT    | `/_api/collection/{name}/rename`                             | RestCollectionHandler      | DB RW + canUseCollection(WriteMeta) |                                          |                        |
| PUT    | `/_api/collection/{name}/responsibleShard`                   | RestCollectionHandler      | canUseCollection(Read)              |                                          |                        |
| PUT    | `/_api/collection/{name}/truncate`                           | RestCollectionHandler      | canUseCollection(WriteData)         |                                          |                        |
| PUT    | `/_api/collection/{name}/unload`                             | RestCollectionHandler      | canUseCollection(Read)              |                                          |                        |
| DELETE | `/_api/collection/{name}`                                    | RestCollectionHandler      | DB RW + canUseCollection(WriteMeta) | should be canCreateDropCollection        | FIXME                  |
| POST   | `/_api/cursor`                                               | RestCursorHandler          | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| POST   | `/_api/cursor/json`                                          | RestCursorHandler          | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| POST   | `/_api/cursor/{id}`                                          | RestCursorHandler          | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| POST   | `/_api/cursor/{id}/{batch-id}`                               | RestCursorHandler          | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| PUT    | `/_api/cursor/{id}`                                          | RestCursorHandler          | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| DELETE | `/_api/cursor/{id}`                                          | RestCursorHandler          | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| GET    | `/_api/database`                                             | RestDatabaseHandler        | AUTHEN, _system, list all           |                                          | FIXME?                 |
| GET    | `/_api/database/current`                                     | RestDatabaseHandler        | AUTHEN                              |                                          |                        |
| GET    | `/_api/database/shardStatistics`                             | RestDatabaseHandler        | AUTHEN                              |                                          |                        |
| GET    | `/_api/database/user`                                        | RestDatabaseHandler        | AUTHEN, _system, detail (4)         |                                          |                        |
| POST   | `/_api/database`                                             | RestDatabaseHandler        | AUTHEN, _system, canCreateDropDB    |                                          |                        |
| DELETE | `/_api/database/{name}`                                      | RestDatabaseHandler        | AUTHEN, _system, DB RW              | should be canCreateOrDropDatabase        | FIXME                  |
| GET    | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Read)              |                                          |                        |
| HEAD   | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Read)              |                                          |                        |
| POST   | `/_api/document/{collection}`                                | RestDocumentHandler        | canUseCollection(Write)             |                                          |                        |
| PUT    | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Write)             |                                          |                        |
| PUT    | `/_api/document/{collection}`                                | RestDocumentHandler        | canUseCollection(Write)             |                                          |                        |
| PATCH  | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Write)             |                                          |                        |
| PATCH  | `/_api/document/{collection}`                                | RestDocumentHandler        | canUseCollection(Write)             |                                          |                        |
| DELETE | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Write)             |                                          |                        |
| DELETE | `/_api/document/{collection}`                                | RestDocumentHandler        | canUseCollection(Write)             |                                          |                        |
| GET    | `/_api/document-state`                                       | RestDocumentStateHandler   | AdminReadReplicatedLog              |                                          |                        |
| POST   | `/_api/document-state`                                       | RestDocumentStateHandler   | AdminWriteReplicatedLog             |                                          |                        |
| DELETE | `/_api/document-state`                                       | RestDocumentStateHandler   | AdminWriteReplicatedLog             |                                          |                        |
| POST   | `/_api/dump/next/{id}`                                       | RestDumpHandler            | SAME USER                           |                                          |                        |
| POST   | `/_api/dump/start`                                           | RestDumpHandler            | AUTHEN + COLL RO                    | AdminDump + SINGLE => escalate to SUPER  | FIXME?                 |
| DELETE | `/_api/dump/{id}`                                            | RestDumpHandler            | SAME USER                           |                                          |                        |
| GET    | `/_api/edges/{collection}`                                   | RestEdgesHandler           | canUseCollection(Read)              |                                          |                        |
| POST   | `/_api/edges/{collection}`                                   | RestEdgesHandler           | canUseCollection(Read)              |                                          |                        |
| GET    | `/_api/endpoint`                                             | RestEndpointHandler        | AUTHEN, _system                     |                                          |                        |
| GET    | `/_api/engine`                                               | RestEngineHandler          | AdminMonitoringInternal HARD        |                                          |                        |
| GET    | `/_api/engine/stats`                                         | RestEngineHandler          | AdminMonitoringInternal HARD        |                                          |                        |
| POST   | `/_api/explain`                                              | RestExplainHandler         | AUTHEN, colls via trx               |                                          |                        |
| GET    | `/_api/gharial`                                              | RestGraphHandler           | canUseCollection(_graphs, RO)       |                                          |                        |
| POST   | `/_api/gharial`                                              | RestGraphHandler           | DB RW, all cools RO                 | should be canUseCollection(_graphs, RW)? | FIXME?                 |
| GET    | `/_api/gharial/{graph}`                                      | RestGraphHandler           | _graphs(Read)                       |                                          |                        |
| DELETE | `/_api/gharial/{graph}`                                      | RestGraphHandler           | _graphs(Write), DB RW for drop coll | should be canUseCollection(...)          | FIXME                  |
| GET    | `/_api/gharial/{graph}/edge`                                 | RestGraphHandler           | _graphs(Read)                       |                                          |                        |
| POST   | `/_api/gharial/{graph}/edge`                                 | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| GET    | `/_api/gharial/{graph}/edge/{definition}/{key}`              | RestGraphHandler           | _graphs(Read)                       |                                          |                        |
| POST   | `/_api/gharial/{graph}/edge/{definition}`                    | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| PUT    | `/_api/gharial/{graph}/edge/{definition}`                    | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| PUT    | `/_api/gharial/{graph}/edge/{definition}/{key}`              | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| PATCH  | `/_api/gharial/{graph}/edge/{definition}/{key}`              | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| DELETE | `/_api/gharial/{graph}/edge/{definition}`                    | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| DELETE | `/_api/gharial/{graph}/edge/{definition}/{key}`              | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| GET    | `/_api/gharial/{graph}/vertex`                               | RestGraphHandler           | _graphs(Read)                       |                                          |                        |
| POST   | `/_api/gharial/{graph}/vertex`                               | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| GET    | `/_api/gharial/{graph}/vertex/{collection}/{key}`            | RestGraphHandler           | _graphs(Read)                       |                                          |                        |
| POST   | `/_api/gharial/{graph}/vertex/{collection}`                  | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| PUT    | `/_api/gharial/{graph}/vertex/{collection}/{key}`            | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| PATCH  | `/_api/gharial/{graph}/vertex/{collection}/{key}`            | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| DELETE | `/_api/gharial/{graph}/vertex/{collection}`                  | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| DELETE | `/_api/gharial/{graph}/vertex/{collection}/{key}`            | RestGraphHandler           | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
| GET    | `/_api/index`                                                | RestIndexHandler           | DB Read, canUseCollection(Read)     | needs API in ExecContext                 | FIXME                  |
| GET    | `/_api/index/selectivity`                                    | RestIndexHandler           | DB Read, canUseCollection(Read)     | needs API in ExecContext                 | FIXME                  |
| POST   | `/_api/index`                                                | RestIndexHandler           | DB Read, canUseCollection(WriteMeta)| needs API in ExecContext                 | FIXME                  |
| POST   | `/_api/index/sync-caches`                                    | RestIndexHandler           | DB Read, canUseCollection(WriteMeta)| needs API in ExecContext                 | FIXME                  |
| DELETE | `/_api/index/{collection}/{id}`                              | RestIndexHandler           | DB Read, canUseCollection(WriteMEta)| needs API in ExecContext                 | FIXME                  |
| GET    | `/_api/key-generators`                                       | RestKeyGeneratorsHandler   | AUTHEN                              |                                          |                        |
| GET    | `/_api/log`                                                  | RestLogHandler             | AdminReadReplicatedLog              | (replication2 + cluster only)            |                        |
| POST   | `/_api/log`                                                  | RestLogHandler             | AdminWriteReplicatedLog             | (replication2 + cluster only)            |                        |
| DELETE | `/_api/log`                                                  | RestLogHandler             | AdminWriteReplicatedLog             | (replication2 + cluster only)            |                        |
| GET    | `/_api/log-internal`                                         | RestLogInternalHandler     | SUPER                               | (replication2 + cluster only)            |                        |
| GET    | `/_api/query/slow`                                           | RestQueryHandler           | AUTHEN, for all DBs _system + SUPER |                                          |                        |
| GET    | `/_api/query/current`                                        | RestQueryHandler           | AUTHEN, for all DBs _system + SUPER |                                          |                        |
| GET    | `/_api/query/properties`                                     | RestQueryHandler           | AUTHEN                              |                                          |                        |
| GET    | `/_api/query/registry`                                       | RestQueryHandler           | SUPER                               |                                          |                        |
| GET    | `/_api/query/rules`                                          | RestQueryHandler           | AUTHEN                              |                                          |                        |
| POST   | `/_api/query`                                                | RestQueryHandler           | AUTHEN                              |                                          |                        |
| PUT    | `/_api/query`                                                | RestQueryHandler           | AUTHEN                              |                                          | FIXME                  |
| DELETE | `/_api/query/{id}`                                           | RestQueryHandler           | AUTHEN, for all DBs _system + SUPER |                                          |                        |
| DELETE | `/_api/query/slow`                                           | RestQueryHandler           | AUTHEN, for all DBs _system + SUPER |                                          |                        |
| GET    | `/_api/query-cache/entries`                                  | RestQueryCacheHandler      | AUTHEN                              |                                          |                        |
| GET    | `/_api/query-cache/properties`                               | RestQueryCacheHandler      | AUTHEN                              |                                          |                        |
| PUT    | `/_api/query-cache/properties`                               | RestQueryCacheHandler      | AUTHEN                              |                                          |                        |
| DELETE | `/_api/query-cache`                                          | RestQueryCacheHandler      | AUTHEN                              |                                          |                        |
| GET    | `/_api/query-plan-cache`                                     | RestQueryPlanCacheHandler  | AUTHEN, details (5)                 | redundant check for DB RO                | FIXME                  |
| DELETE | `/_api/query-plan-cache`                                     | RestQueryPlanCacheHandler  | AUTHEN, DB RW                       | needs an RBAC solution                   | FIXME                  |
| DELETE | `/_api/replication/applier-state`                            | RestReplicationHandler     |                                     |                                          |                        |
| GET    | `/_api/replication/applier-config`                           | RestReplicationHandler     |                                     |                                          |                        |
| GET    | `/_api/replication/applier-state`                            | RestReplicationHandler     |                                     |                                          |                        |
| GET    | `/_api/replication/applier-state-all`                        | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/applier-config`                           | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/applier-start`                            | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/applier-stop`                             | RestReplicationHandler     |                                     |                                          |                        |
| POST   | `/_api/replication/batch`                                    | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/batch`                                    | RestReplicationHandler     |                                     |                                          |                        |
| GET    | `/_api/replication/clusterInventory`                         | RestReplicationHandler     | AdminClusterInfo or COLL RO         |                                          |                        |
| GET    | `/_api/replication/dump`                                     | RestReplicationHandler     | AdminDump or COLL RO                |                                          |                        |
| DELETE | `/_api/replication/holdReadLockCollection`                   | RestReplicationHandler     |                                     |                                          |                        |
| POST   | `/_api/replication/holdReadLockCollection`                   | RestReplicationHandler     |                                     |                                          |                        |
| GET    | `/_api/replication/inventory`                                | RestReplicationHandler     |                                     |                                          |                        |
| DELETE | `/_api/replication/keys`                                     | RestReplicationHandler     |                                     |                                          |                        |
| DELETE | `/_api/replication/keys/{id}`                                | RestReplicationHandler     |                                     |                                          |                        |
| GET    | `/_api/replication/keys/{id}`                                | RestReplicationHandler     |                                     |                                          |                        |
| POST   | `/_api/replication/keys`                                     | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/keys/{id}`                                | RestReplicationHandler     |                                     |                                          |                        |
| GET    | `/_api/replication/logger-first-tick`                        | RestReplicationHandler     |                                     |                                          |                        |
| GET    | `/_api/replication/logger-follow`                            | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/logger-follow`                            | RestReplicationHandler     |                                     |                                          |                        |
| GET    | `/_api/replication/logger-state`                             | RestReplicationHandler     | AUTHEN                              |                                          |                        |
| GET    | `/_api/replication/logger-tick-ranges`                       | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/make-follower`                            | RestReplicationHandler     | AdminReplication                    |                                          |                        |
| PUT    | `/_api/replication/addFollower`                              | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/removeFollower`                           | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/restore-collection`                       | RestReplicationHandler     | AdminRestore or COLL RW (1)         |                                          |                        |
| PUT    | `/_api/replication/restore-data`                             | RestReplicationHandler     | AdminRestore or COLL RWDATA         |                                          |                        |
| PUT    | `/_api/replication/restore-indexes`                          | RestReplicationHandler     | AdminRestore or COLL RW (1)         |                                          |                        |
| PUT    | `/_api/replication/restore-view`                             | RestReplicationHandler     |                                     |                                          |                        |
| GET    | `/_api/replication/revisions/tree`                           | RestReplicationHandler     |                                     |                                          |                        |
| POST   | `/_api/replication/revisions/tree`                           | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/revisions/documents`                      | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/revisions/ranges`                         | RestReplicationHandler     |                                     |                                          |                        |
| GET    | `/_api/replication/server-id`                                | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/set-the-leader`                           | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/replication/sync`                                     | RestReplicationHandler     |                                     |                                          |                        |
| PUT    | `/_api/simple/all`                                           | RestSimpleQueryHandler     | AUTHEN, COLL RO                     |                                          |                        |
| PUT    | `/_api/simple/all-keys`                                      | RestSimpleQueryHandler     | AUTHEN, COLL RO                     |                                          |                        |
| PUT    | `/_api/simple/by-example`                                    | RestSimpleQueryHandler     | AUTHEN, COLL RO                     |                                          |                        |
| PUT    | `/_api/simple/lookup-by-keys`                                | RestSimpleHandler          | AUTHEN, COLL RO                     |                                          |                        |
| PUT    | `/_api/simple/remove-by-keys`                                | RestSimpleHandler          | AUTHEN, COLL RO                     |                                          |                        |
| GET    | `/_api/tasks`                                                | RestTasksHandler           | AUTHEN, list only SUPER or SELF     | (V8 required)                            |                        |
| GET    | `/_api/tasks/{id}`                                           | RestTasksHandler           | AUTHEN, SUPER or SELF               | (V8 required)                            |                        |
| POST   | `/_api/tasks`                                                | RestTasksHandler           | DB RW                               | (V8 required), adapt to RBAC             | FIXME                  |
| PUT    | `/_api/tasks/{id}`                                           | RestTasksHandler           | DB RW                               | (V8 required), adapt to RBAC             | FIXME                  |
| DELETE | `/_api/tasks/{id}`                                           | RestTasksHandler           | DB RW                               | (V8 required), adapt to RBAC             | FIXME                  |
| GET    | `/_api/token/{user}`                                         | RestAccessTokenHandler     | canReadUser                         |                                          |                        |
| POST   | `/_api/token/{user}`                                         | RestAccessTokenHandler     | canWriteUser                        |                                          |                        |
| DELETE | `/_api/token/{user}/{id}`                                    | RestAccessTokenHandler     | canWriteUser                        |                                          |                        |
| GET    | `/_api/ttl/properties`                                       | RestTtlHandler             | AUTHEN, _system                     |                                          |                        |
| GET    | `/_api/ttl/statistics`                                       | RestTtlHandler             | AUTHEN, _system                     |                                          |                        |
| PUT    | `/_api/ttl/properties`                                       | RestTtlHandler             | AUTHEN, _system                     |                                          |                        |
| POST   | `/_api/upload`                                               | RestUploadHandler          | AUTHEN                              | should this not be restricted further??? | FIXME                  |
| GET    | `/_api/user`                                                 | RestUsersHandler           | AUTHEN, see only canReadUser(u)     |                                          |                        |
| POST   | `/_api/user`                                                 | RestUsersHandler           | canWriteUser(u)                     |                                          |                        |
| POST   | `/_api/user/{user}`                                          | RestUsersHandler           | AUTHEN, just check credentials      |                                          |                        |
| GET    | `/_api/user/{user}`                                          | RestUsersHandler           | canReadUser(u)                      |                                          |                        |
| GET    | `/_api/user/{user}/config`                                   | RestUsersHandler           | canReadUser(u)                      |                                          |                        |
| GET    | `/_api/user/{user}/database`                                 | RestUsersHandler           | canReadUser(u)                      |                                          |                        |
| GET    | `/_api/user/{user}/database/{db}`                            | RestUsersHandler           | canReadUser(u)                      |                                          |                        |
| GET    | `/_api/user/{user}/database/{db}/{coll}`                     | RestUsersHandler           | canReadUser(u)                      |                                          |                        |
| PUT    | `/_api/user/{user}`                                          | RestUsersHandler           | canWriteUser(u)                     |                                          |                        |
| PUT    | `/_api/user/{user}/database/{db}`                            | RestUsersHandler           | canWriteUser(u)                     |                                          |                        |
| PUT    | `/_api/user/{user}/database/{db}/{coll}`                     | RestUsersHandler           | canWriteUser(u)                     |                                          |                        |
| PUT    | `/_api/user/{user}/config/{key}`                             | RestUsersHandler           | canWriteUser(u)                     |                                          |                        |
| PATCH  | `/_api/user/{user}`                                          | RestUsersHandler           | canWriteUser(u)                     |                                          |                        |
| DELETE | `/_api/user/{user}`                                          | RestUsersHandler           | canWriteUser(u)                     |                                          |                        |
| DELETE | `/_api/user/{user}/config/{key}`                             | RestUsersHandler           | canWriteUser(u)                     |                                          |                        |
| DELETE | `/_api/user/{user}/database/{db}`                            | RestUsersHandler           | canWriteUser(u)                     |                                          |                        |
| DELETE | `/_api/user/{user}/database/{db}/{coll}`                     | RestUsersHandler           | canWriteUser(u)                     |                                          |                        |
| GET    | `/_api/version`                                              | RestVersionHandler         | AUTHEN, details (2)                 |                                          |                        |
| GET    | `/_api/view`                                                 |                            |                                     |                                          |                        |
| POST   | `/_api/view`                                                 |                            |                                     |                                          |                        |
| DELETE | `/_api/view/{name}`                                          |                            |                                     |                                          |                        |
| GET    | `/_api/view/{name}`                                          |                            |                                     |                                          |                        |
| GET    | `/_api/view/{name}/properties`                               |                            |                                     |                                          |                        |
| PATCH  | `/_api/view/{name}/properties`                               |                            |                                     |                                          |                        |
| PUT    | `/_api/view/{name}/properties`                               |                            |                                     |                                          |                        |
| PUT    | `/_api/view/{name}/rename`                                   |                            |                                     |                                          |                        |
| GET    | `/_api/wal/lastTick`                                         |                            |                                     |                                          |                        |
| GET    | `/_api/wal/open-transactions`                                |                            |                                     |                                          |                        |
| GET    | `/_api/wal/range`                                            |                            |                                     |                                          |                        |
| GET    | `/_api/wal/tail`                                             |                            |                                     |                                          |                        |
| PUT    | `/_api/wal/tail`                                             |                            |                                     |                                          |                        |
| DELETE | `/_api/wal/tail`                                             |                            |                                     |                                          |                        |
| GET    | `/_api/transaction`                                          |                            |                                     |                                          |                        |
| GET    | `/_api/transaction/{id}`                                     |                            |                                     |                                          |                        |
| POST   | `/_api/transaction`                                          |                            |                                     |                                          |                        |
| POST   | `/_api/transaction/begin`                                    |                            |                                     |                                          |                        |
| PUT    | `/_api/transaction/{id}`                                     |                            |                                     |                                          |                        |
| DELETE | `/_api/transaction/{id}`                                     |                            |                                     |                                          |                        |
| DELETE | `/_api/transaction/write`                                    |                            |                                     |                                          |                        |
| PUT    | `/_internal/traverser/{option}/{engine-id}`                  |                            |                                     |                                          |                        |
| DELETE | `/_internal/traverser/{engine-id}`                           |                            |                                     |                                          |                        |
| GET    | `/openapi.json`                                              |                            |                                     |                                          |                        |
|--------|--------------------------------------------------------------|----------------------------|-------------------------------------|------------------------------------------|------------------------|
| JS     | `JS_CreateQueue`                                             | v8-dispatcher.cpp          | AdminTasks                          |                                          |                        |
| JS     | `TRI_RequestCppToV8`                                         | v8-dispatcher.cpp          | AdminFoxx                           |                                          |                        |
| JS     | `JS_GetReplicatedLog`                                        | v8-dispatcher.cpp          | AdminReadReplicatedLog              |                                          |                        |
| JS     | `JS_CreateReplicatedLog`                                     | v8-dispatcher.cpp          | AdminWriteReplicatedLog             |                                          |                        |
| JS     | `JS_Id`                                                      | v8-dispatcher.cpp          | AdminReadReplicatedLog              |                                          |                        |
| JS     | `JS_Drop`                                                    | v8-dispatcher.cpp          | AdminWriteReplicatedLog             |                                          |                        |
| JS     | `JS_Insert`                                                  | v8-dispatcher.cpp          | AdminWriteReplicatedLog             |                                          |                        |
| JS     | `JS_Ping`                                                    | v8-dispatcher.cpp          | AdminWriteReplicatedLog             |                                          |                        |
| JS     | `JS_MultiInsert`                                             | v8-dispatcher.cpp          | AdminWriteReplicatedLog             |                                          |                        |
| JS     | `JS_Status`                                                  | v8-dispatcher.cpp          | AdminReadReplicatedLog              |                                          |                        |
| JS     | `JS_GlobalStatus`                                            | v8-dispatcher.cpp          | AdminReadReplicatedLog              |                                          |                        |
| JS     | `JS_Head`                                                    | v8-dispatcher.cpp          | AdminReadReplicatedLog              |                                          |                        |
| JS     | `JS_Tail`                                                    | v8-dispatcher.cpp          | AdminReadReplicatedLog              |                                          |                        |
| JS     | `JS_Slice`                                                   | v8-dispatcher.cpp          | AdminReadReplicatedLog              |                                          |                        |
| JS     | `JS_Poll`                                                    | v8-dispatcher.cpp          | AdminReadReplicatedLog              |                                          |                        |
| JS     | `JS_At`                                                      | v8-dispatcher.cpp          | AdminReadReplicatedLog              |                                          |                        |
| JS     | `JS_Release`                                                 | v8-dispatcher.cpp          | AdminWriteReplicatedLog             |                                          |                        |
| JS     | `JS_Compact`                                                 | v8-dispatcher.cpp          | AdminWriteReplicatedLog             |                                          |                        |
| JS     | `JS_RemoveUser`                                              | v8-users.cpp               | canWriteUser                        |                                          |                        |
| JS     | `JS_ReloadAuthData`                                          | v8-users.cpp               | AdminAuthReload                     |                                          |                        |
| JS     | `JS_GrantDatabase`                                           | v8-users.cpp               | canWriteUser                        |                                          |                        |
| JS     | `JS_RevokeDatabase`                                          | v8-users.cpp               | canWriteUser                        |                                          |                        |
| JS     | `JS_GrantCollection`                                         | v8-users.cpp               | canWriteUser                        |                                          |                        |
| JS     | `JS_RevokeCollection`                                        | v8-users.cpp               | canWriteUser                        |                                          |                        |
| JS     | `StoreUser`                                                  | v8-users.cpp               | canWriteUser                        |                                          |                        |
| JS     | `JS_UpdateUser`                                              | v8-users.cpp               | canWriteUser                        |                                          |                        |
| JS     | `JS_GetUser`                                                 | v8-users.cpp               | canReadUser                         |                                          |                        |
| JS     | `JS_UpdateConfigData`                                        | v8-users.cpp               | canWriteUser                        |                                          |                        |
| JS     | `JS_GetConfigData`                                           | v8-users.cpp               | canReadUser                         |                                          |                        |
| CPP    | `Databases::grantCurrentUser` (creation of database)         | Databases.Cpp              | canWriteUser                        |                                          |                        |


(1) For `arangorestore`, if `--overwrite=true`, then we need COLL RW, if `--overwrite=false`, we only need COLL RWDATA
(2) For `/_api/version`, details can only be queried with `AdminMonitoringInternal`, if `--server.harden=true`
(3) For `/_api/collection`, RO for database is needed, then all collections with COLL RO are listed
(4) For `/_api/database`, all databases with DB RO are listed
(5) For `GET /_api/query-plan-cache` only those entries are returned, for which the user has read access to all occurring collections

Rules:
 - internal use of system collections allowed without check
 - read access to system collections can be regulated by RBAC if switched on
 - write access (with normal APIs) to system collections is superuser only
