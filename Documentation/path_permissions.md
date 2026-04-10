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
to enforce it **for all routes** (with very few exceptions). We want to change
this, rather than performing this authorization check in the `CommTask`, we
want to do it on the scheduler before we run the state machine for the
`RestHandler`. The check should be implemented in a virtual method of the
`RestVocbaseBaseHandler`, which is called from `runHandler`.

The bulk of the authorization checks is then performed in the `RestHandlers`
(or, for 3.12, in the server-side JavaScript functions). The idea is that
most general permission checks are done in the `RestHandlers`, **with the
exception of collection and view access checks**.

Finally, the third place, where we do authorization checks, is this:
Since basically all collection accesses need a transaction, we enforce
collection access permissions in the transaction code, basically, when
collections/views are added to a transaction.


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
 - We move the initial read access check for the requested database to a
   virtual method of the `RestVocbaseBaseHandler`, which is called in
   `runHandler`, but already on the scheduler.
 - Every other change is an exception, which we (grudgingly) make because we
   found some issue with the current system.
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

The `ExecContext` offers the following checking methods, all of which
return a `Result`, so that a decline can return the actual reason
(which can be different for RBAC enabled and disabled):

 - `canUseAdminAction(rbac::Category::Any const action) -> Result`
 - `canUseHardenedAction(rbac::Category::Any const action) -> Result`

 - `canSeeDatabase(std::string_view db) -> Result`
 - `canCreateDatabase(std::string_view db) -> Result`
 - `canDropDatabase(std::string_view db) -> Result`
 - `canUseDatabase(std::string_view db, DatabaseAccessLevel const level) -> Result`

 - `canSeeCollection(std::string_view db, std::string_view coll) -> Result`
 - `canCreateCollection(std::string_view db, std::string_view coll) -> Result`
 - `canDropCollection(std::string_view db, std::string_view coll) -> Result`
 - `canUseCollection(std::string_view db, std::string_view coll, CollectionAccessLevel const level) -> Result`

 - `canSeeView(std::string_view db, std::string_view view) -> Result`
 - `canCreateView(std::string_view db, std::string_view view) -> Result`
 - `canDropView(std::string_view db, std::string_view view) -> Result`
 - `canUseView(std::string_view db, std::string_view view) -> Result`

 - `canSeeAnalyzer(std::string_view db, std::string_view analyzer) -> Result`
 - `canCreateAnalyzer(std::string_view db, std::string_view analyzer) -> Result`
 - `canDropAnalyzer(std::string_view db, std::string_view analyzer) -> Result`
 - `canUseAnalyzer(std::string_view db, std::string_view analyzer) -> Result`

 - `isSuperUser() -> bool`

Note that for now, `canSee*` is equivalent to `canUse*(RO)` and
`canCreate**` and `canDrop*` are equivalent to `canUse*(RW)`. For
collections `canUseCollection(RWDATA)` is needed to write data. However,
we keep the semantic checks separate in case we want to split things
further later.

There is one subtlety, though. If we ever want to separate being able
to see `canSee*` from `canUse*(RO)` later, then we want that if a
user cannot see a collection (say) and cannot read it, then the error
when trying to access it should be "NOTFOUND", to not give away the
information that the collection exists! This must be considered in the
central implementation of these methods.


### Implementation details for the abstract methods for RBAC disabled

 - `canUseAdminAction(rbac::Category::Any const action) -> Result`

   check RW access for `_system` database
  
 - `canUseHardenedAction(rbac::Category::Any const action) -> Result`

   if hardened, check RW access for `_system` database,
   if not hardened, no further check
  
 - `canSeeDatabase(std::string_view db) -> Result`

   check that database authentication level is at least RO
  
 - `canCreateDatabase(std::string_view db) -> Result`

   check RW access for `_system` database
  
 - `canDropDatabase(std::string_view db) -> Result`

   check RW access for `_system` database
  
 - `canUseDatabase(std::string_view db, DatabaseAccessLevel const level) -> Result`

   check database auth level and use this:

      - DatabaseAccessLevel::Read: needs auth::Level::RO or more
      - DatabaseAccessLevel::Write: needs auth::Level::RW

   If the user is not allowed to see the database, this must return NOT_FOUND!
  
 - `canSeeCollection(std::string_view db, std::string_view coll) -> Result`

   check RO access for database (that is, always return Ok, since this has been checked already)
  
 - `canCreateCollection(std::string_view db, std::string_view coll) -> Result`

   check RW access for database
  
 - `canDropCollection(std::string_view db, std::string_view coll) -> Result`

   check RW access for database

 - `canUseCollection(std::string_view db, std::string_view coll, CollectionAccessLevel const level) -> Result`

   check collection auth level and use this:

      - CollectionAccessLevel::Read: needs auth::Level::RO or more
      - CollectionAccessLevel::WriteData: needs auth::Level::RW
      - CollectionAccessLevel::WriteMeta: needs auth::Level::RW and auth::Level::RW on database!

   If the user is not allowed to see the collection, this must return NOT_FOUND!
  
 - `canSeeView(std::string_view db, std::string_view view) -> Result`

   check RO access for database (no-op)

 - `canCreateView(std::string_view db, std::string_view view) -> Result`

   check RW access for database
  
 - `canDropView(std::string_view db, std::string_view view) -> Result`

   check RW access for database

 - `canUseView(std::string_view db, std::string_view view) -> Result`

   Just delegate to the access level of the database (as before) and use::
  
      - AccessLevel::Read: needs auth::Level::RO or more
      - AccessLevel::WriteData: needs auth::Level::RW
      - AccessLevel::WriteMeta: needs auth::Level::RW and auth::Level::RW on database!

   Note that we leave the code as it is to additionally check if the user
   has `canUseCollection(RO)` for all linked collections.
  
   If the user is not allowed to see the view, this must return NOT_FOUND!
  
 - `canSeeAnalyzer(std::string_view db, std::string_view analyzer) -> Result`

   check RO access for database (no-op)
     
 - `canCreateAnalyzer(std::string_view db, std::string_view analyzer) -> Result`

   check RW access for database
  
 - `canDropAnalyzer(std::string_view db, std::string_view analyzer) -> Result`

   check RW access for database

 - `canUseAnalyzer(std::string_view db, std::string_view analyzer) -> Result`

   assume RO access for database is already checked. Check nothing else without RBAC.
  
   If the user is not allowed to see the analyzer, this must return NOT_FOUND!
  
- `isSuperUser() -> bool`

  must return `true` if and only if the authenticated user is the superuser (JWT
  token with empty `preferred_username`.
  

### Implementation details for the abstract methods for RBAC enabled

 - `canUseAdminAction(rbac::Category::Any const action) -> Result`

   check the given RBAC action via the authorization service
  
 - `canUseHardenedAction(rbac::Category::Any const action) -> Result`

   default to hardened, check the given RBAC action
  
 - `canSeeDatabase(std::string_view db) -> Result`

   check RBAC action `db:ReadDatabase`, i.e. access level for database is
   at least "Read".
  
 - `canCreateDatabase(std::string_view db) -> Result`

   check RBAC actions `db:ReadDatabase` and `db:WriteDatabase`, i.e. access level i
   for database is at least RW
  
 - `canDropDatabase(std::string_view db) -> Result`

   check RBAC actions `db:ReadDatabase` and `db:WriteDatabase`, i.e. access level i
   for database is at least RW
  
 - `canUseDatabase(std::string_view db, DatabaseAccessLevel const level) -> Result`

   check database access level, i.e. check RBAC actions `db:ReadDatabase` and
   `db:WriteDatabase`

   If the user is not allowed to see the database, this must return NOT_FOUND!
  
 - `canSeeCollection(std::string_view db, std::string_view coll) -> Result`

   check collection access level to be at least RO, i.e., check RBAC action
   `db:ReadCollection`
  
 - `canCreateCollection(std::string_view db, std::string_view coll) -> Result`

   check collection access level to be RW, i.e., check RBAC actions
   `db:ReadCollection` and `db:WriteCollectionData` and `db:WriteCollectionMeta`.
  
 - `canDropCollection(std::string_view db, std::string_view coll) -> Result`

   check collection access level to be RW, i.e., check RBAC actions
   `db:ReadCollection` and `db:WriteCollectionData` and `db:WriteCollectionMeta`.
  
 - `canUseCollection(std::string_view db, std::string_view coll, CollectionAccessLevel const level) -> Result`

   check collection access level, i.e., check RBAC actions
   `db:ReadCollection` and `db:WriteCollectionData` and
   `db:WriteCollectionMeta` to find NONE, or RO, or RWDATA, or RW

   If the user is not allowed to see the collection, this must return NOT_FOUND!
  
 - `canSeeView(std::string_view db, std::string_view view) -> Result`

   check view access level to be at least RO, i.e., check RBAC action
   `db:ReadView`
  
 - `canCreateView(std::string_view db, std::string_view view) -> Result`

   check view access level to be RW, i.e., check RBAC actions
   `db:ReadView` and `db:WriteView`
  
 - `canDropView(std::string_view db, std::string_view view) -> Result`

   check view access level to be RW, i.e., check RBAC actions
   `db:ReadView` and `db:WriteView`
  
 - `canUseView(std::string_view db, std::string_view view) -> Result`

   check view access level, i.e., check RBAC actions `db:ReadView` and
   `db:WriteView` to find NONE, or RO, or RW

   Note that we leave the code as it is to additionally check if the user
   has `canUseCollection(RO)` for all linked collections.
  
   If the user is not allowed to see the view, this must return NOT_FOUND!
  
 - `canSeeAnalyzer(std::string_view db, std::string_view analyzer) -> Result`

   check analyzer access level to be at least RO, i.e., check RBAC action
   `db:ReadAnalyzer`
     
 - `canCreateAnalyzer(std::string_view db, std::string_view analyzer) -> Result`

   check analyzer access level to be RW, i.e., check RBAC actions
   `db:ReadAnalyzer` and `db:WriteAnalyzer`
  
 - `canDropAnalyzer(std::string_view db, std::string_view analyzer) -> Result`

   check analyzer access level to be RW, i.e., check RBAC actions
   `db:ReadAnalyzer` and `db:WriteAnalyzer`
  
 - `canUseAnalyzer(std::string_view db, std::string_view analyzer) -> Result`

   check analyzer access level, i.e., check RBAC actions `db:ReadAnalyzer` and
   `db:WriteAnalyzer` to find NONE, or RO, or RW

   Note that this is a different behaviour from before, but it is more sensible.
   To call the API, one has to have at least RO access to the database anyway.
   But writing an analyzer is now done via RBAC and reading, too.

   If the user is not allowed to see the analyzer, this must return NOT_FOUND!
  
- `isSuperUser() -> bool`

  must return `true` if and only if the authenticated user is the superuser (JWT
  token with empty `preferred_username`.
  
### Implementation details when authentication is switched off

All these functions should return Ok. The `isSuperUser` method should return `true`.


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
               must have read access to the used database from /_db/<dbname
canUseAdmin(X) - stands for canUseAdminAction(AdminX)
canAccessHard(X) - stands for canAccessHardenedApi(AdminX)
isSuperuser  - check for superuser
canUseColl(l) - canUseCollection(AccessLevel::l)
canUseDb(l)  - canUseDatabase(DatabaseAccessLevel::l)
Admin*       - with RBAC, one needs that action, without RBAC, one needs RW on _system
HARD         - without RBAC, one needs RW on _system (with RBAC, --server.hardened is always on)
               (if Admin* and HARD are written, then AUTHEN holds when --server.hardened is off without RBAC)
DB RW        - Read/write auth level for the database
DB RO        - At least read-only auth level for the database
COLL RO      - At least Read auth level for the collection
`_system` RW - Read/write auth level for _system database
?/S/A        - API is switchable between off, superuser and admin access, additionally, an Admin* is specified
S/A          - API is switchable between superuser and admin access, additionally, an Admin* is specified
S/A/AU       - API is switchable between superuser only and admin only and AUTHEN
SA/SW/LEG    - API is switchable between SA (superuser needed for everything), SW (superuser needed for write
               operations), LEG (legacy mode, superuser not needed, further authorization applies
?/S/A/O      - API is switchable between off, superuser only, admin only and public (which is AUTHEN)


|DONE|REVI|TEST| Method | Path                                                         | RestHandler                | Abstract auth call                       | Authorization                       | Comments                                 | Changes to before RBAC |
|----|----|----|--------|--------------------------------------------------------------|----------------------------|------------------------------------------|-------------------------------------|------------------------------------------|------------------------|
| X  |    |    | POST   | `/_open/auth`                                                | RestAuthHandler            | -                                        | OPEN                                | needs special exception in AUTEN check!  |                        |
| X  |    |    | POST   | `/_open/auth/renew`                                          | RestAuthHandler            | -                                        | OPEN                                | needs special exception in AUTEN check!  |                        |
| X  |    |    | GET    | `/_admin/actions`                                            | MaintenanceRestHandler     | -                                        | AUTHEN                              | Only really relevant on DBServers        |                        |
| X  |    |    | POST   | `/_admin/actions`                                            | MaintenanceRestHandler     | -                                        | AUTHEN                              | Only really relevant on DBServers        |                        |
| X  |    |    | PUT    | `/_admin/actions`                                            | MaintenanceRestHandler     | -                                        | AUTHEN                              | Only really relevant on DBServers        |                        |
| X  |    |    | DELETE | `/_admin/actions/{id}`                                       | MaintenanceRestHandler     | -                                        | AUTHEN                              | Only really relevant on DBServers        |                        |
| X  |    |    | GET    | `/_admin/activities`                                         | activities::RestHandler    | isSuperuser / canUseAdmin(MonInternal)   | S/A AdminMonitoringInternal         |                                          |                        |
| X  |    |    | GET    | `/_admin/async-registry`                                     | async_registry::RestHandler| canUseAdmin(MonInternal)                 | AdminMonitoringInternal             |                                          |                        |
| X  |    |    | POST   | `/_admin/auth/reload`                                        | RestAdminAuthReloadHandler | canuseadmin(AuthReload)                  | AdminAuthReload                     |                                          |                        |
| X  |    |    | POST   | `/_admin/backup/create`                                      | RestHotBackupHandler       | isSuperuser / canUseAdmin(Backup)        | S/A AdminBackup                     |                                          |                        |
| X  |    |    | POST   | `/_admin/backup/delete`                                      | RestHotBackupHandler       | isSuperuser / canUseAdmin(Backup)        | S/A AdminBackup                     |                                          |                        |
| X  |    |    | POST   | `/_admin/backup/download`                                    | RestHotBackupHandler       | isSuperuser / canUseAdmin(Backup)        | S/A AdminBackup                     |                                          |                        |
| X  |    |    | POST   | `/_admin/backup/list`                                        | RestHotBackupHandler       | isSuperuser / canUseAdmin(Backup)        | S/A AdminBackup                     |                                          |                        |
| X  |    |    | POST   | `/_admin/backup/upload`                                      | RestHotBackupHandler       | isSuperuser / canUseAdmin(Backup)        | S/A AdminBackup                     |                                          |                        |
| X  |    |    | GET    | `/_admin/cluster/collectionShardDistribution`                | RestAdminClusterHandler    | canUseAdmin(ClusterInfo)                 | AdminClusterInfo                    | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | PUT    | `/_admin/cluster/cancelAgencyJob`                            | RestAdminClusterHandler    | canUseAdmin(MoveShards)                  | AdminMoveShards                     | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | PUT    | `/_admin/cluster/cleanOutServer`                             | RestAdminClusterHandler    | canUseAdmin(MoveShards)                  | AdminMoveShards                     | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | GET    | `/_admin/cluster/health`                                     | RestAdminClusterHandler    | -                                        | AUTHEN                              | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | GET    | `/_admin/cluster/maintenance`                                | RestAdminClusterHandler    | canUseAdmin(Maintenance)                 | AdminMaintenance                    | SA/SW/LEG, only coordinator+single       |                        |
| X  |    |    | PUT    | `/_admin/cluster/maintenance`                                | RestAdminClusterHandler    | canUseAdmin(Maintenance)                 | AdminMaintenance                    | SA/SW/LEG, only coordinator+single       |                        |
| X  |    |    | GET    | `/_admin/cluster/maintenance/{serverId}`                     | RestAdminClusterHandler    | canUseAdmin(Maintenance)                 | AdminMaintenance                    | SA/SW/LEG, only coordinator+single       |                        |
| X  |    |    | PUT    | `/_admin/cluster/maintenance/{serverId}`                     | RestAdminClusterHandler    | canUseAdmin(Maintenance)                 | AdminMaintenance                    | SA/SW/LEG, only coordinator+single       |                        |
| X  |    |    | POST   | `/_admin/cluster/moveShard`                                  | RestAdminClusterHandler    | canUseAdmin(MoveShard) | canUseColl(RW)  | AdminMoveShards or COLL RW          | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | GET    | `/_admin/cluster/nodeEngine`                                 | RestAdminClusterHandler    | -                                        | AUTHEN                              | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | GET    | `/_admin/cluster/nodeStatistics`                             | RestAdminClusterHandler    | -                                        | AUTHEN                              | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | GET    | `/_admin/cluster/nodeVersion`                                | RestAdminClusterHandler    | -                                        | AUTHEN                              | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | GET    | `/_admin/cluster/numberOfServers`                            | RestAdminClusterHandler    | -                                        | AUTHEN                              | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | PUT    | `/_admin/cluster/numberOfServers`                            | RestAdminClusterHandler    | canAccessHard(Maintenance)               | AdminMaintenance, HARD              | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | GET    | `/_admin/cluster/queryAgencyJob`                             | RestAdminClusterHandler    | canUseAdmin(MoveShards)                  | AdminMoveShards                     | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | GET    | `/_admin/cluster/rebalance`                                  | RestAdminClusterHandler    | canUseAdmin(Rebalance)                   | AdminRebalance                      | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | PUT    | `/_admin/cluster/rebalance`                                  | RestAdminClusterHandler    | canUseAdmin(Rebalance)                   | AdminRebalance                      | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | PUT    | `/_admin/cluster/rebalanceShards`                            | RestAdminClusterHandler    | canUseAdmin(Rebalance)                   | AdminRebalance                      | SA/SW/LEG, only coordinator              | Was: AUTHEN + DB RW    |
| X  |    |    | POST   | `/_admin/cluster/removeServer`                               | RestAdminClusterHandler    | canUseAdmin(RemoveServer)                | AdminRemoveServer                   | SA/SW/LEG                                |                        |
| X  |    |    | PUT    | `/_admin/cluster/resignLeadership`                           | RestAdminClusterHandler    | canUseAdmin(MoveShards)                  | AdminMoveShards                     | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | GET    | `/_admin/cluster/shardDistribution`                          | RestAdminClusterHandler    | canUseAdmin(ClusterInfo)                 | AdminClusterInfo                    | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | GET    | `/_admin/cluster/shardStatistics`                            | RestAdminClusterHandler    | canUseadmin(ClusterInfo)                 | AdminClusterInfo                    | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | GET    | `/_admin/cluster/statistics`                                 | RestAdminClusterHandler    | -                                        | AUTHEN                              | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | PUT    | `/_admin/cluster/uniqId`                                     | RestAdminClusterHandler    | canUseAdmin(Maintenance)                 | AdminMaintenance                    | SA/SW/LEG, only coordinator              |                        |
| X  |    |    | PUT    | `/_admin/cluster/vpackSortMigration/{serverId}`              | RestAdminClusterHandler    | isSuperuser                              | SUPER                               | SA/SW/LEG                                |                        |
| X  |    |    | PUT    | `/_admin/compact`                                            | RestCompactHandler         | isSuperuser                              | SUPER                               |                                          |                        |
| X  |    |    | GET    | `/_admin/crashes`                                            | RestCrashHandler           | canUseAdmin(CrashHandler)                | AdminCrashHandler                   |                                          |                        |
| X  |    |    | GET    | `/_admin/crashes/{id}`                                       | RestCrashHandler           | canUseAdmin(CrashHandler)                | AdminCrashHandler                   |                                          |                        |
| X  |    |    | DELETE | `/_admin/crashes/{id}`                                       | RestCrashHandler           | canUseAdmin(CrashHandler)                | AdminCrashHandler                   |                                          |                        |
| X  |    |    | GET    | `/_admin/database/target-version`                            | RestAdminDatabaseHandler   | -                                        | AUTHEN                              |                                          |                        |
| X  |    |    | GET    | `/_admin/debug/failat`                                       | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    |    | GET    | `/_admin/debug/failat/all`                                   | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    |    | PUT    | `/_admin/debug/failat/{name}`                                | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    |    | DELETE | `/_admin/debug/failat`                                       | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    |    | DELETE | `/_admin/debug/failat/{name}`                                | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    |    | DELETE | `/_admin/debug/raceControl`                                  | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    |    | PUT    | `/_admin/debug/crash`                                        | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    |    | GET    | `/_admin/deployment/id`                                      | RestAdminDeploymentHandler | -                                        | AUTHEN                              | only coordinators and single             |                        |
| X  |    |    | POST   | `/_admin/execute`                                            | RestAdminExecuteHandler    | -                                        | AUTHEN                              | only --javascript.allow-admin-execute    |                        |
| X  |    |    | GET    | `/_admin/job/{id}`                                           | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | GET    | `/_admin/job/{type}`                                         | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | PUT    | `/_admin/job/{id}`                                           | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | PUT    | `/_admin/job/{id}/cancel`                                    | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | DELETE | `/_admin/job/all`                                            | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | DELETE | `/_admin/job/expired`                                        | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | DELETE | `/_admin/job/{id}`                                           | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | GET    | `/_api/job/{id}`                                             | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | GET    | `/_api/job/{type}`                                           | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | PUT    | `/_api/job/{id}`                                             | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | PUT    | `/_api/job/{id}/cancel`                                      | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | DELETE | `/_api/job/all`                                              | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | DELETE | `/_api/job/expired`                                          | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | DELETE | `/_api/job/{id}`                                             | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    |    | GET    | `/_admin/license`                                            | RestLicenseHandler(EE)     | canAccessHard(License)                   | AdminLicense, HARD                  |                                          |                        |
| X  |    |    | PUT    | `/_admin/license`                                            | RestLicenseHandler(EE)     | canAccessHard(License)                   | AdminLicense, HARD                  |                                          |                        |
| X  |    |    | GET    | `/_admin/log`                                                | RestAdminLogHandler        | isSuperuser / canUseAdmin(ReadLogs)      | AdminReadLogs                       | ?/S/A                                    |                        |
| X  |    |    | GET    | `/_admin/log/entries`                                        | RestAdminLogHandler        | isSuperuser / canUseAdmin(ReadLogs)      | AdminReadLogs                       | ?/S/A                                    |                        |
| X  |    |    | GET    | `/_admin/log/level`                                          | RestAdminLogHandler        | isSuperuser / canUseAdmin(ReadLogs)      | AdminReadLogs                       | ?/S/A                                    |                        |
| X  |    |    | GET    | `/_admin/log/structured`                                     | RestAdminLogHandler        | isSuperuser / canUseAdmin(ReadLogs)      | AdminReadLogs                       | ?/S/A                                    |                        |
| X  |    |    | PUT    | `/_admin/log/level`                                          | RestAdminLogHandler        | isSuperuser / canUseAdmin(SetLogLevel)   | AdminSetLogLevel                    | ?/S/A                                    |                        |
| X  |    |    | PUT    | `/_admin/log/structured`                                     | RestAdminLogHandler        | isSuperuser / canUseAdmin(SetLogLevel)   | AdminSetLogLevel                    | ?/S/A                                    |                        |
| X  |    |    | DELETE | `/_admin/log`                                                | RestAdminLogHandler        | isSuperuser / canUseAdmin(SetLogLevel)   | AdminSetLogLevel                    | ?/S/A                                    |                        |
| X  |    |    | DELETE | `/_admin/log/entries`                                        | RestAdminLogHandler        | isSuperuser / canUseAdmin(SetLogLevel)   | AdminSetLogLevel                    | ?/S/A                                    |                        |
| X  |    |    | DELETE | `/_admin/log/level`                                          | RestAdminLogHandler        | isSuperuser / canUseAdmin(SetLogLevel)   | AdminSetLogLevel                    | ?/S/A                                    |                        |
| X  |    |    | GET    | `/_admin/metrics`                                            | RestMetricsHandler         | canAccessHard(Monitoring)                | AdminMonitoring, HARD               |                                          |                        |
| X  |    |    | GET    | `/_admin/options`                                            | RestOptionsHandler         | isSuperuser / canUseAdmin(Options) / -   | AdminOptions                        | S/A/AU                                   |                        |
| X  |    |    | GET    | `/_admin/options-description`                                | RestOptionsDescriptionHandler | isSuperuser / canUseAdmin(Options) / -| AdminOptions                        | S/A/AU                                   |                        |
| X  |    |    | GET    | `/_admin/options-public`                                     | RestPublicOptionsHandler   | -                                        | AUTHEN                              |                                          |                        |
| X  |    |    | POST   | `/_admin/routing/reload`                                     | RestAdminRoutingHandler    | -                                        | AUTHEN                              | (V8 required)                            |                        |
| X  |    |    | GET    | `/_admin/server/api-calls`                                   | RestAdminServerHandler     | isSuperuser / canUseAdmin(ApiCalls)      | AdminApiCalls                       | ?/S/A                                    |                        |
| X  |    |    | GET    | `/_admin/server/aql-queries`                                 | RestAdminServerHandler     | isSuperuser / canUseAdmin(AqlQueries)    | AdminAqlQueries                     | ?/S/A                                    |                        |
| X  |    |    | GET    | `/_admin/server/availability`                                | RestAdminServerHandler     | -                                        | OPEN                                |                                          |                        |
| X  |    |    | GET    | `/_admin/server/databaseDefaults`                            | RestAdminServerHandler     | -                                        | AUTHEN                              |                                          |                        |
| X  |    |    | GET    | `/_admin/server/id`                                          | RestAdminServerHandler     | -                                        | AUTHEN                              | (cluster only)                           |                        |
| X  |    |    | GET    | `/_admin/server/mode`                                        | RestAdminServerHandler     | -                                        | AUTHEN                              |                                          |                        |
| X  |    |    | PUT    | `/_admin/server/mode`                                        | RestAdminServerHandler     | canUseAdmin(Maintenance)                 | AdminMaintenance                    |                                          |                        |
| X  |    |    | GET    | `/_admin/server/role`                                        | RestAdminServerHandler     | -                                        | AUTHEN                              |                                          |                        |
| X  |    |    | GET    | `/_admin/server/tls`                                         | RestAdminServerHandler     | -                                        | AUTHEN                              |                                          |                        |
| X  |    |    | POST   | `/_admin/server/tls`                                         | RestAdminServerHandler     | isSuperUser                              | SUPER                               |                                          |                        |
| X  |    |    | GET    | `/_admin/server/jwt`                                         | RestAdminServerHandler     | -                                        | AUTHEN                              |                                          |                        |
| X  |    |    | POST   | `/_admin/server/jwt`                                         | RestAdminServerHandler     | isSuperUser                              | SUPER                               |                                          |                        |
| X  |    |    | GET    | `/_admin/server/encryption`                                  | RestAdminServerHandler     | -                                        | AUTHEN                              | (not on coordinators)                    |                        |
| X  |    |    | POST   | `/_admin/server/encryption`                                  | RestAdminServerHandler     | isSuperUser                              | SUPER                               | (not on coordinators)                    |                        |
| X  |    |    | GET    | `/_admin/shutdown`                                           | RestShutdownHandler        | -                                        | AUTHEN                              | (only coordinator for soft shutdown)     |                        |
| X  |    |    | DELETE | `/_admin/shutdown`                                           | RestShutdownHandler        | canUseAdmin(Shutdown)                    | AdminShutdown                       |                                          |                        |
| X  |    |    | GET    | `/_admin/statistics`                                         | RestAdminStatisticsHandler | canAccessHard(Monitoring)                | AdminMonitoring, HARD               |                                          |                        |
| X  |    |    | GET    | `/_admin/statistics-description`                             | RestAdminStatisticsHandler | canAccessHard(Monitoring)                | AdminMonitoring, HARD               |                                          |                        |
| X  |    |    | GET    | `/_admin/status`                                             | RestAdminStatusHandler     | canAccessHard(Monitoring)                | AdminMonitoring HARD                |                                          |                        |
| X  |    |    | GET    | `/_admin/supervisionState`                                   | RestSupervisionStateHandler| canUseAdmin(SupervisionState)            | AdminSupervisionState               | (coordinator only)                       |                        |
| X  |    |    | GET    | `/_admin/support-info`                                       | RestSupportInfoHandler     | isSuperUser / canUseAdmin(Monitoring) / -| AdminMonitoring                     | ?/S/A/AU                                 |                        |
| X  |    |    | GET    | `/_admin/system-report`                                      | RestSystemReportHandler    | canAccessHard(MonitoringInternal)        | AdminMonitoringInternali, HARD      |                                          |                        |
| X  |    |    | GET    | `/_admin/telemetrics`                                        | RestTelemetricsHandler     | isSuperUser / canUseAdmin(MonitoringInternal) / -  | AdminMonitoringInternal   | ?/S/A/AU                                 |                        |
| X  |    |    | DELETE | `/_admin/telemetrics`                                        | RestTelemetricsHandler     | isSuperUser / canUseAdmin(MonitoringInternal) / -  | AdminMonitoringInternal   | ?/S/A/AU                                 |                        |
| X  |    |    | GET    | `/_admin/time`                                               | RestTimeHandler            | -                                        | AUTHEN                              |                                          |                        |
| X  |    |    | GET    | `/_admin/usage-metrics`                                      | RestUsageMetricsHandler    | canAccessHard(MonitoringInternal)        | AdminMonitoringInternal HARD        |                                          |                        |
| X  |    |    | GET    | `/_admin/version`                                            | RestVersionhandler         | -/canAccessHard(MonitoringInternal)      | AUTHEN, details (2)                 |                                          |                        |
| X  |    |    | GET    | `/_admin/wal/properties`                                     | RestWalAccessHandler       | -                                        | SUPER                               | (RocksDB engine) only DBServer           |                        |
| X  |    |    | PUT    | `/_admin/wal/properties`                                     | RestWalAccessHandler       | -                                        | SUPER                               | (RocksDB engine) only DBServer           |                        |
| X  |    |    | GET    | `/_admin/wal/transactions`                                   | RestWalAccessHandler       | -                                        | SUPER                               | (RocksDB engine) only DBServer           |                        |
| X  |    |    | PUT    | `/_admin/wal/flush`                                          | RestWalAccessHandler       | -                                        | SUPER                               | (RocksDB engine) only DBServer           |                        |
| X  |    |    | PUT    | `/_admin/wal/wait_for_estimator_sync`                        | RestWalAccessHandler       | -                                        | SUPER                               | (RocksDB engine) only DBServer           |                        |
| X  |    |    | GET    | `/_admin/wal/properties`                                     | ClusterRestWalHandler      | -                                        | AUTHEN                              | (Cluster engine) NOT_IMPL                |                        |
| X  |    |    | PUT    | `/_admin/wal/properties`                                     | ClusterRestWalHandler      | -                                        | AUTHEN                              | (Cluster engine) NOT_IMPL                |                        |
| X  |    |    | GET    | `/_admin/wal/transactions`                                   | ClusterRestWalHandler      | -                                        | AUTHEN                              | (Cluster engine) NOT_IMPL                |                        |
| X  |    |    | PUT    | `/_admin/wal/flush`                                          | ClusterRestWalHandler      | -                                        | AUTHEN                              | (Cluster engine) DELEGATED to DBServers  |                        |
| X  |    |    | PUT    | `/_admin/wal/wait_for_estimator_sync`                        | ClusterRestWalHandler      | canUseAdmin(WalAccess) / isSuperuser     | AdminWalAccess (PROD)/SUPER (MAINT) | (Cluster engine)                         |                        |
| X  |    |    | GET    | `/_api/aql-builtin`                                          | RestAqlFunctionsHandler    | -                                        | AUTHEN                              |                                          |                        |
| X  |    |    | GET    | `/_api/aqlfunction`                                          | RestAqlUserFunctionsHandler| -, then run AQL with _aqlfunctions coll  | AUTHEN + COLL RO _aqlfunctions      | (V8 required) Note: system-collection!   |                        |
| X  |    |    | GET    | `/_api/aqlfunction/{namespace}`                              | RestAqlUserFunctionsHandler| -, then run AQL with _aqlfunctions coll  | AUTHEN + COLL RO _aqlfunctions      | (V8 required) Note: system-collection!   |                        |
| X  |    |    | POST   | `/_api/aqlfunction`                                          | RestAqlUserFunctionsHandler| -, then run AQL with _aqlfunctions coll  | AUTHEN + COLL RW _aqlfunctions      | (V8 required) Note: system-collection!   |                        |
| X  |    |    | DELETE | `/_api/aqlfunction/{name}`                                   | RestAqlUserFunctionsHandler| -, then run AQL with _aqlfunctions coll  | AUTHEN + COLL RW _aqlfunctions      | (V8 required) Note: system-collection!   |                        |
| X  |    |    | GET    | `/_api/analyzer`                                             | RestAnalyzerHandler        | -, then run AQL with _anaylizers coll    | AUTHEN + COLL RO _analyzers         | Note: system-collection!                 |                        |
| X  |    |    | GET    | `/_api/analyzer/{name}`                                      | RestAnalyzerHandler        | -, then run AQL with _anaylizers coll    | AUTHEN + COLL RO _analyzers         | Note: system-collection!                 |                        |
| X  |    |    | POST   | `/_api/analyzer`                                             | RestAnalyzerHandler        | -, then run AQL with _anaylizers coll    | AUTHEN + COLL RW _analyzers         | Note: system-collection!                 |                        |
| X  |    |    | DELETE | `/_api/analyzer/{name}`                                      | RestAnalyzerHandler        | -, then run AQL with _anaylizers coll    | AUTHEN + COLL RW _analyzers         | Note: system-collection!                 |                        |
| X  |    |    | GET    | `/_api/cluster/agency-cache`                                 | RestClusterHandler         | canUseAdmin(ReadAgency)                  | AdminReadAgency                     | (coordinator only)                       |                        |
| X  |    |    | GET    | `/_api/cluster/agency-dump`                                  | RestClusterHandler         | canUseAdmin(ReadAgency)                  | AdminReadAgency                     | (coordinator only)                       |                        |
| X  |    |    | GET    | `/_api/cluster/cluster-info`                                 | RestClusterHandler         | canUseAdmin(ClusterInfo)                 | AdminClusterInfo                    | (cluster only)                           |                        |
| X  |    |    | PUT    | `/.../flush`                                                 | RestClusterHandler         | isSuperUser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    |    | GET    | `/.../get_collection_info/{db}/{coll}`                       | RestClusterHandler         | isSuperUser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    |    | GET    | `/.../get_collection_info_current/{db}/{coll}/{shard}`       | RestClusterHandler         | isSuperUser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    |    | POST   | `/.../get_responsible_servers`                               | RestClusterHandler         | isSuperUser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    |    | POST   | `/.../get_responsible_shard/{db}/{coll}`                     | RestClusterHandler         | isSuperUser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    |    | GET    | `/.../get_analyzers_revision/{db}`                           | RestClusterHandler         | isSuperUser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    |    | GET    | `/.../wait_for_plan_version/{version}`                       | RestClusterHandler         | isSuperUser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    |    | GET    | `/.../get_max_number_of_shards`                              | RestClusterHandler         | isSuperUser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    |    | GET    | `/.../get_max_replication_factor`                            | RestClusterHandler         | isSuperUser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    |    | GET    | `/.../get_min_replication_factor`                            | RestClusterHandler         | isSuperUser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    |    | GET    | `/_api/cluster/endpoints`                                    | RestClusterHandler         | -                                        | AUTHEN                              | (coordinator only)                       |                        |
| X  |    |    | POST   | `/_api/collection`                                           | RestCollectionHandler      | canCreateCollection                      | COLL RW                             |                                          |                        |
| X  |    |    | GET    | `/_api/collection`                                           | RestCollectionHandler      | canSeeCollection, only see readable      | AUTHEN, details (3)                 |                                          |                        |
| X  |    |    | GET    | `/_api/collection/{name}`                                    | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    |    | GET    | `/_api/collection/{name}/checksum`                           | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    |    | GET    | `/_api/collection/{name}/count`                              | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    |    | GET    | `/_api/collection/{name}/figures`                            | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    |    | GET    | `/_api/collection/{name}/properties`                         | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    |    | GET    | `/_api/collection/{name}/revision`                           | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    |    | GET    | `/_api/collection/{name}/shards`                             | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    |    | PUT    | `/_api/collection/{name}/compact`                            | RestCollectionHandler      | canUseCollection(WriteMeta)              | COLL RW                             |                                          |                        |
| X  |    |    | PUT    | `/_api/collection/{name}/load`                               | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    |    | PUT    | `/_api/collection/{name}/loadIndexesIntoMemory`              | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    |    | PUT    | `/_api/collection/{name}/properties`                         | RestCollectionHandler      | canUseCollection(WriteMeta)              | COLL RW                             |                                          |                        |
| X  |    |    | PUT    | `/_api/collection/{name}/rename`                             | RestCollectionHandler      | canUseCollection(WriteMeta)              | COLL RW                             |                                          |                        |
| X  |    |    | PUT    | `/_api/collection/{name}/responsibleShard`                   | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    |    | PUT    | `/_api/collection/{name}/truncate`                           | RestCollectionHandler      | canUseCollection(WriteData)              | COLL RWDATA                         |                                          |                        |
| X  |    |    | PUT    | `/_api/collection/{name}/unload`                             | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    |    | DELETE | `/_api/collection/{name}`                                    | RestCollectionHandler      | canDropCollection                        | COLL RW                             |                                          |                        |
| X  |    |    | POST   | `/_api/cursor`                                               | RestCursorHandler          | then run AQL and rely on trx             | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| X  |    |    | POST   | `/_api/cursor/json`                                          | RestCursorHandler          | then run AQL and rely on trx             | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| X  |    |    | POST   | `/_api/cursor/{id}`                                          | RestCursorHandler          | then run AQL and rely on trx             | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| X  |    |    | POST   | `/_api/cursor/{id}/{batch-id}`                               | RestCursorHandler          | then run AQL and rely on trx             | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| X  |    |    | PUT    | `/_api/cursor/{id}`                                          | RestCursorHandler          | then run AQL and rely on trx             | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| X  |    |    | DELETE | `/_api/cursor/{id}`                                          | RestCursorHandler          | then run AQL and rely on trx             | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| X  |    |    | GET    | `/_api/database`                                             | RestDatabaseHandler        | check to be in _system database          | AUTHEN, _system, list all           |                                          | FIXME?                 |
| X  |    |    | GET    | `/_api/database/user`                                        | RestDatabaseHandler        | _system, canSeeDatabase                  | AUTHEN, _system, detail (4)         |                                          |                        |
| X  |    |    | GET    | `/_api/database/current`                                     | RestDatabaseHandler        | -                                        | AUTHEN                              |                                          |                        |
| X  |    |    | GET    | `/_api/database/shardStatistics`                             | RestDatabaseHandler        | -                                        | AUTHEN                              | (coordinator only)                       |                        |
| X  |    |    | POST   | `/_api/database`                                             | RestDatabaseHandler        | _system, canCreateDb                     | AUTHEN, _system, canCreateDB        |                                          |                        |
| X  |    |    | DELETE | `/_api/database/{name}`                                      | RestDatabaseHandler        | _system, canDropDb                       | AUTHEN, _system, canDropDB          | should be canCreateOrDropDatabase        | FIXME                  |
| X  |    |    | GET    | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Read), (via trx)        | COLL RO                             |                                          |                        |
| X  |    |    | HEAD   | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Read), (via trx)        | COLL RO                             |                                          |                        |
| X  |    |    | POST   | `/_api/document/{collection}`                                | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    |    | PUT    | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    |    | PUT    | `/_api/document/{collection}`                                | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    |    | PATCH  | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    |    | PATCH  | `/_api/document/{collection}`                                | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    |    | DELETE | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    |    | DELETE | `/_api/document/{collection}`                                | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    |    | GET    | `/_api/document-state`                                       | RestDocumentStateHandler   | canUseAdmin(ReadReplicatedLog)           | AdminReadReplicatedLog              |                                          |                        |
| X  |    |    | POST   | `/_api/document-state`                                       | RestDocumentStateHandler   | canUseAdmin(WriteReplicatedLog)          | AdminWriteReplicatedLog             |                                          |                        |
| X  |    |    | DELETE | `/_api/document-state`                                       | RestDocumentStateHandler   | canUseAdmin(WriteReplicatedLog)          | AdminWriteReplicatedLog             |                                          |                        |
| X  |    |    | POST   | `/_api/dump/next/{id}`                                       | RestDumpHandler            | SAME USER                                | SAME USER                           | (dbserver and single only)               |                        |
| X  |    |    | POST   | `/_api/dump/start`                                           | RestDumpHandler            | canUseCollection(Read)                   | AUTHEN + COLL RO                    | (dbserver and single only)               | AdminDump + SINGLE => escalate to SUPER FIXME? |                 |
| X  |    |    | DELETE | `/_api/dump/{id}`                                            | RestDumpHandler            | SAME USER                                | SAME USER                           | (dbserver and single only)               |                        |
| X  |    |    | GET    | `/_api/edges/{collection}`                                   | RestEdgesHandler           | canUseCollection(Read) (via trx)         | COLL RO                             |                                          |                        |
| X  |    |    | POST   | `/_api/edges/{collection}`                                   | RestEdgesHandler           | canUseCollection(Read) (via trx)         | COLL RO                             |                                          |                        |
| X  |    |    | GET    | `/_api/endpoint`                                             | RestEndpointHandler        | _system                                  | AUTHEN, _system                     |                                          |                        |
| X  |    |    | GET    | `/_api/engine`                                               | RestEngineHandler          | canAccessHard(MonitoringInternal)        | AdminMonitoringInternal, HARD       |                                          |                        |
| X  |    |    | GET    | `/_api/engine/stats`                                         | RestEngineHandler          | canAccessHard(MonitoringInternal)        | AdminMonitoringInternal, HARD       |                                          |                        |
| X  |    |    | POST   | `/_api/explain`                                              | RestExplainHandler         | canUseCollection(Read) (via trx)         | AUTHEN, COLL RO via trx             |                                          |                        |
|    |    |    | GET    | `/_api/gharial`                                              | RestGraphHandler           |                                          | canUseCollection(_graphs, RO)       |                                          |                        |
|    |    |    | POST   | `/_api/gharial`                                              | RestGraphHandler           |                                          | DB RW, all cools RO                 | should be canUseCollection(_graphs, RW)? | FIXME?                 |
|    |    |    | GET    | `/_api/gharial/{graph}`                                      | RestGraphHandler           |                                          | _graphs(Read)                       |                                          |                        |
|    |    |    | DELETE | `/_api/gharial/{graph}`                                      | RestGraphHandler           |                                          | _graphs(Write), DB RW for drop coll | should be canUseCollection(...)          | FIXME                  |
|    |    |    | GET    | `/_api/gharial/{graph}/edge`                                 | RestGraphHandler           |                                          | _graphs(Read)                       |                                          |                        |
|    |    |    | POST   | `/_api/gharial/{graph}/edge`                                 | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | GET    | `/_api/gharial/{graph}/edge/{definition}/{key}`              | RestGraphHandler           |                                          | _graphs(Read)                       |                                          |                        |
|    |    |    | POST   | `/_api/gharial/{graph}/edge/{definition}`                    | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | PUT    | `/_api/gharial/{graph}/edge/{definition}`                    | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | PUT    | `/_api/gharial/{graph}/edge/{definition}/{key}`              | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | PATCH  | `/_api/gharial/{graph}/edge/{definition}/{key}`              | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | DELETE | `/_api/gharial/{graph}/edge/{definition}`                    | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | DELETE | `/_api/gharial/{graph}/edge/{definition}/{key}`              | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | GET    | `/_api/gharial/{graph}/vertex`                               | RestGraphHandler           |                                          | _graphs(Read)                       |                                          |                        |
|    |    |    | POST   | `/_api/gharial/{graph}/vertex`                               | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | GET    | `/_api/gharial/{graph}/vertex/{collection}/{key}`            | RestGraphHandler           |                                          | _graphs(Read)                       |                                          |                        |
|    |    |    | POST   | `/_api/gharial/{graph}/vertex/{collection}`                  | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | PUT    | `/_api/gharial/{graph}/vertex/{collection}/{key}`            | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | PATCH  | `/_api/gharial/{graph}/vertex/{collection}/{key}`            | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | DELETE | `/_api/gharial/{graph}/vertex/{collection}`                  | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | DELETE | `/_api/gharial/{graph}/vertex/{collection}/{key}`            | RestGraphHandler           |                                          | _graphs(Write), DB RW for colls     | should be canCreateDropCollection(...)   | FIXME                  |
|    |    |    | GET    | `/_api/index`                                                | RestIndexHandler           |                                          | DB Read, canUseCollection(Read)     | needs API in ExecContext                 | FIXME                  |
|    |    |    | GET    | `/_api/index/selectivity`                                    | RestIndexHandler           |                                          | DB Read, canUseCollection(Read)     | needs API in ExecContext                 | FIXME                  |
|    |    |    | POST   | `/_api/index`                                                | RestIndexHandler           |                                          | DB Read, canUseCollection(WriteMeta)| needs API in ExecContext                 | FIXME                  |
|    |    |    | POST   | `/_api/index/sync-caches`                                    | RestIndexHandler           |                                          | DB Read, canUseCollection(WriteMeta)| needs API in ExecContext                 | FIXME                  |
|    |    |    | DELETE | `/_api/index/{collection}/{id}`                              | RestIndexHandler           |                                          | DB Read, canUseCollection(WriteMEta)| needs API in ExecContext                 | FIXME                  |
|    |    |    | GET    | `/_api/key-generators`                                       | RestKeyGeneratorsHandler   |                                          | AUTHEN                              |                                          |                        |
|    |    |    | GET    | `/_api/log`                                                  | RestLogHandler             |                                          | AdminReadReplicatedLog              | (replication2 + cluster only)            |                        |
|    |    |    | POST   | `/_api/log`                                                  | RestLogHandler             |                                          | AdminWriteReplicatedLog             | (replication2 + cluster only)            |                        |
|    |    |    | DELETE | `/_api/log`                                                  | RestLogHandler             |                                          | AdminWriteReplicatedLog             | (replication2 + cluster only)            |                        |
|    |    |    | GET    | `/_api/log-internal`                                         | RestLogInternalHandler     |                                          | SUPER                               | (replication2 + cluster only)            |                        |
|    |    |    | GET    | `/_api/query/slow`                                           | RestQueryHandler           |                                          | AUTHEN, for all DBs _system + SUPER |                                          |                        |
|    |    |    | GET    | `/_api/query/current`                                        | RestQueryHandler           |                                          | AUTHEN, for all DBs _system + SUPER |                                          |                        |
|    |    |    | GET    | `/_api/query/properties`                                     | RestQueryHandler           |                                          | AUTHEN                              |                                          |                        |
|    |    |    | GET    | `/_api/query/registry`                                       | RestQueryHandler           |                                          | SUPER                               |                                          |                        |
|    |    |    | GET    | `/_api/query/rules`                                          | RestQueryHandler           |                                          | AUTHEN                              |                                          |                        |
|    |    |    | POST   | `/_api/query`                                                | RestQueryHandler           |                                          | AUTHEN                              |                                          |                        |
|    |    |    | PUT    | `/_api/query`                                                | RestQueryHandler           |                                          | AUTHEN                              |                                          | FIXME                  |
|    |    |    | DELETE | `/_api/query/{id}`                                           | RestQueryHandler           |                                          | AUTHEN, for all DBs _system + SUPER |                                          |                        |
|    |    |    | DELETE | `/_api/query/slow`                                           | RestQueryHandler           |                                          | AUTHEN, for all DBs _system + SUPER |                                          |                        |
|    |    |    | GET    | `/_api/query-cache/entries`                                  | RestQueryCacheHandler      |                                          | AUTHEN                              |                                          |                        |
|    |    |    | GET    | `/_api/query-cache/properties`                               | RestQueryCacheHandler      |                                          | AUTHEN                              |                                          |                        |
|    |    |    | PUT    | `/_api/query-cache/properties`                               | RestQueryCacheHandler      |                                          | AUTHEN                              |                                          |                        |
|    |    |    | DELETE | `/_api/query-cache`                                          | RestQueryCacheHandler      |                                          | AUTHEN                              |                                          |                        |
|    |    |    | GET    | `/_api/query-plan-cache`                                     | RestQueryPlanCacheHandler  |                                          | AUTHEN, details (5)                 | redundant check for DB RO                | FIXME                  |
|    |    |    | DELETE | `/_api/query-plan-cache`                                     | RestQueryPlanCacheHandler  |                                          | AUTHEN, DB RW                       | needs an RBAC solution                   | FIXME                  |
|    |    |    | DELETE | `/_api/replication/applier-state`                            | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/replication/applier-config`                           | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/replication/applier-state`                            | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/replication/applier-state-all`                        | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/applier-config`                           | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/applier-start`                            | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/applier-stop`                             | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | POST   | `/_api/replication/batch`                                    | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/batch`                                    | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/replication/clusterInventory`                         | RestReplicationHandler     |                                          | AdminClusterInfo or COLL RO         |                                          |                        |
|    |    |    | GET    | `/_api/replication/dump`                                     | RestReplicationHandler     |                                          | AdminDump or COLL RO                |                                          |                        |
|    |    |    | DELETE | `/_api/replication/holdReadLockCollection`                   | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | POST   | `/_api/replication/holdReadLockCollection`                   | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/replication/inventory`                                | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | DELETE | `/_api/replication/keys`                                     | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | DELETE | `/_api/replication/keys/{id}`                                | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/replication/keys/{id}`                                | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | POST   | `/_api/replication/keys`                                     | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/keys/{id}`                                | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/replication/logger-first-tick`                        | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/replication/logger-follow`                            | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/logger-follow`                            | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/replication/logger-state`                             | RestReplicationHandler     |                                          | AUTHEN                              |                                          |                        |
|    |    |    | GET    | `/_api/replication/logger-tick-ranges`                       | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/make-follower`                            | RestReplicationHandler     |                                          | AdminReplication                    |                                          |                        |
|    |    |    | PUT    | `/_api/replication/addFollower`                              | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/removeFollower`                           | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/restore-collection`                       | RestReplicationHandler     |                                          | AdminRestore or COLL RW (1)         |                                          |                        |
|    |    |    | PUT    | `/_api/replication/restore-data`                             | RestReplicationHandler     |                                          | AdminRestore or COLL RWDATA         |                                          |                        |
|    |    |    | PUT    | `/_api/replication/restore-indexes`                          | RestReplicationHandler     |                                          | AdminRestore or COLL RW (1)         |                                          |                        |
|    |    |    | PUT    | `/_api/replication/restore-view`                             | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/replication/revisions/tree`                           | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | POST   | `/_api/replication/revisions/tree`                           | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/revisions/documents`                      | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/revisions/ranges`                         | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/replication/server-id`                                | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/set-the-leader`                           | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/replication/sync`                                     | RestReplicationHandler     |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/simple/all`                                           | RestSimpleQueryHandler     |                                          | AUTHEN, COLL RO                     |                                          |                        |
|    |    |    | PUT    | `/_api/simple/all-keys`                                      | RestSimpleQueryHandler     |                                          | AUTHEN, COLL RO                     |                                          |                        |
|    |    |    | PUT    | `/_api/simple/by-example`                                    | RestSimpleQueryHandler     |                                          | AUTHEN, COLL RO                     |                                          |                        |
|    |    |    | PUT    | `/_api/simple/lookup-by-keys`                                | RestSimpleHandler          |                                          | AUTHEN, COLL RO                     |                                          |                        |
|    |    |    | PUT    | `/_api/simple/remove-by-keys`                                | RestSimpleHandler          |                                          | AUTHEN, COLL RO                     |                                          |                        |
|    |    |    | GET    | `/_api/tasks`                                                | RestTasksHandler           |                                          | AUTHEN, list only SUPER or SELF     | (V8 required)                            |                        |
|    |    |    | GET    | `/_api/tasks/{id}`                                           | RestTasksHandler           |                                          | AUTHEN, SUPER or SELF               | (V8 required)                            |                        |
|    |    |    | POST   | `/_api/tasks`                                                | RestTasksHandler           |                                          | DB RW                               | (V8 required), adapt to RBAC             | FIXME                  |
|    |    |    | PUT    | `/_api/tasks/{id}`                                           | RestTasksHandler           |                                          | DB RW                               | (V8 required), adapt to RBAC             | FIXME                  |
|    |    |    | DELETE | `/_api/tasks/{id}`                                           | RestTasksHandler           |                                          | DB RW                               | (V8 required), adapt to RBAC             | FIXME                  |
|    |    |    | GET    | `/_api/token/{user}`                                         | RestAccessTokenHandler     |                                          | canReadUser                         |                                          |                        |
|    |    |    | POST   | `/_api/token/{user}`                                         | RestAccessTokenHandler     |                                          | canWriteUser                        |                                          |                        |
|    |    |    | DELETE | `/_api/token/{user}/{id}`                                    | RestAccessTokenHandler     |                                          | canWriteUser                        |                                          |                        |
|    |    |    | GET    | `/_api/ttl/properties`                                       | RestTtlHandler             |                                          | AUTHEN, _system                     |                                          |                        |
|    |    |    | GET    | `/_api/ttl/statistics`                                       | RestTtlHandler             |                                          | AUTHEN, _system                     |                                          |                        |
|    |    |    | PUT    | `/_api/ttl/properties`                                       | RestTtlHandler             |                                          | AUTHEN, _system                     |                                          |                        |
|    |    |    | POST   | `/_api/upload`                                               | RestUploadHandler          |                                          | AUTHEN                              | should this not be restricted further??? | FIXME                  |
|    |    |    | GET    | `/_api/user`                                                 | RestUsersHandler           |                                          | AUTHEN, see only canReadUser(u)     |                                          |                        |
|    |    |    | POST   | `/_api/user`                                                 | RestUsersHandler           |                                          | canWriteUser(u)                     |                                          |                        |
|    |    |    | POST   | `/_api/user/{user}`                                          | RestUsersHandler           |                                          | AUTHEN, just check credentials      |                                          |                        |
|    |    |    | GET    | `/_api/user/{user}`                                          | RestUsersHandler           |                                          | canReadUser(u)                      |                                          |                        |
|    |    |    | GET    | `/_api/user/{user}/config`                                   | RestUsersHandler           |                                          | canReadUser(u)                      |                                          |                        |
|    |    |    | GET    | `/_api/user/{user}/database`                                 | RestUsersHandler           |                                          | canReadUser(u)                      |                                          |                        |
|    |    |    | GET    | `/_api/user/{user}/database/{db}`                            | RestUsersHandler           |                                          | canReadUser(u)                      |                                          |                        |
|    |    |    | GET    | `/_api/user/{user}/database/{db}/{coll}`                     | RestUsersHandler           |                                          | canReadUser(u)                      |                                          |                        |
|    |    |    | PUT    | `/_api/user/{user}`                                          | RestUsersHandler           |                                          | canWriteUser(u)                     |                                          |                        |
|    |    |    | PUT    | `/_api/user/{user}/database/{db}`                            | RestUsersHandler           |                                          | canWriteUser(u)                     |                                          |                        |
|    |    |    | PUT    | `/_api/user/{user}/database/{db}/{coll}`                     | RestUsersHandler           |                                          | canWriteUser(u)                     |                                          |                        |
|    |    |    | PUT    | `/_api/user/{user}/config/{key}`                             | RestUsersHandler           |                                          | canWriteUser(u)                     |                                          |                        |
|    |    |    | PATCH  | `/_api/user/{user}`                                          | RestUsersHandler           |                                          | canWriteUser(u)                     |                                          |                        |
|    |    |    | DELETE | `/_api/user/{user}`                                          | RestUsersHandler           |                                          | canWriteUser(u)                     |                                          |                        |
|    |    |    | DELETE | `/_api/user/{user}/config/{key}`                             | RestUsersHandler           |                                          | canWriteUser(u)                     |                                          |                        |
|    |    |    | DELETE | `/_api/user/{user}/database/{db}`                            | RestUsersHandler           |                                          | canWriteUser(u)                     |                                          |                        |
|    |    |    | DELETE | `/_api/user/{user}/database/{db}/{coll}`                     | RestUsersHandler           |                                          | canWriteUser(u)                     |                                          |                        |
|    |    |    | GET    | `/_api/version`                                              | RestVersionHandler         |                                          | AUTHEN, details (2)                 |                                          |                        |
|    |    |    | GET    | `/_api/view`                                                 |                            |                                          |                                     |                                          |                        |
|    |    |    | POST   | `/_api/view`                                                 |                            |                                          |                                     |                                          |                        |
|    |    |    | DELETE | `/_api/view/{name}`                                          |                            |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/view/{name}`                                          |                            |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/view/{name}/properties`                               |                            |                                          |                                     |                                          |                        |
|    |    |    | PATCH  | `/_api/view/{name}/properties`                               |                            |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/view/{name}/properties`                               |                            |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/view/{name}/rename`                                   |                            |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/wal/lastTick`                                         |                            |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/wal/open-transactions`                                |                            |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/wal/range`                                            |                            |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/wal/tail`                                             |                            |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/wal/tail`                                             |                            |                                          |                                     |                                          |                        |
|    |    |    | DELETE | `/_api/wal/tail`                                             |                            |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/transaction`                                          |                            |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/_api/transaction/{id}`                                     |                            |                                          |                                     |                                          |                        |
|    |    |    | POST   | `/_api/transaction`                                          |                            |                                          |                                     |                                          |                        |
|    |    |    | POST   | `/_api/transaction/begin`                                    |                            |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_api/transaction/{id}`                                     |                            |                                          |                                     |                                          |                        |
|    |    |    | DELETE | `/_api/transaction/{id}`                                     |                            |                                          |                                     |                                          |                        |
|    |    |    | DELETE | `/_api/transaction/write`                                    |                            |                                          |                                     |                                          |                        |
|    |    |    | PUT    | `/_internal/traverser/{option}/{engine-id}`                  |                            |                                          |                                     |                                          |                        |
|    |    |    | DELETE | `/_internal/traverser/{engine-id}`                           |                            |                                          |                                     |                                          |                        |
|    |    |    | GET    | `/openapi.json`                                              |                            |                                          |                                     |                                          |                        |
|----|----|----|--------|--------------------------------------------------------------|----------------------------|------------------------------------------|-------------------------------------|------------------------------------------|------------------------|
|    |    |    | JS     | `JS_CreateQueue`                                             | v8-dispatcher.cpp          |                                          | AdminTasks                          |                                          |                        |
|    |    |    | JS     | `TRI_RequestCppToV8`                                         | v8-dispatcher.cpp          |                                          | AdminFoxx                           |                                          |                        |
|    |    |    | JS     | `JS_GetReplicatedLog`                                        | v8-dispatcher.cpp          |                                          | AdminReadReplicatedLog              |                                          |                        |
|    |    |    | JS     | `JS_CreateReplicatedLog`                                     | v8-dispatcher.cpp          |                                          | AdminWriteReplicatedLog             |                                          |                        |
|    |    |    | JS     | `JS_Id`                                                      | v8-dispatcher.cpp          |                                          | AdminReadReplicatedLog              |                                          |                        |
|    |    |    | JS     | `JS_Drop`                                                    | v8-dispatcher.cpp          |                                          | AdminWriteReplicatedLog             |                                          |                        |
|    |    |    | JS     | `JS_Insert`                                                  | v8-dispatcher.cpp          |                                          | AdminWriteReplicatedLog             |                                          |                        |
|    |    |    | JS     | `JS_Ping`                                                    | v8-dispatcher.cpp          |                                          | AdminWriteReplicatedLog             |                                          |                        |
|    |    |    | JS     | `JS_MultiInsert`                                             | v8-dispatcher.cpp          |                                          | AdminWriteReplicatedLog             |                                          |                        |
|    |    |    | JS     | `JS_Status`                                                  | v8-dispatcher.cpp          |                                          | AdminReadReplicatedLog              |                                          |                        |
|    |    |    | JS     | `JS_GlobalStatus`                                            | v8-dispatcher.cpp          |                                          | AdminReadReplicatedLog              |                                          |                        |
|    |    |    | JS     | `JS_Head`                                                    | v8-dispatcher.cpp          |                                          | AdminReadReplicatedLog              |                                          |                        |
|    |    |    | JS     | `JS_Tail`                                                    | v8-dispatcher.cpp          |                                          | AdminReadReplicatedLog              |                                          |                        |
|    |    |    | JS     | `JS_Slice`                                                   | v8-dispatcher.cpp          |                                          | AdminReadReplicatedLog              |                                          |                        |
|    |    |    | JS     | `JS_Poll`                                                    | v8-dispatcher.cpp          |                                          | AdminReadReplicatedLog              |                                          |                        |
|    |    |    | JS     | `JS_At`                                                      | v8-dispatcher.cpp          |                                          | AdminReadReplicatedLog              |                                          |                        |
|    |    |    | JS     | `JS_Release`                                                 | v8-dispatcher.cpp          |                                          | AdminWriteReplicatedLog             |                                          |                        |
|    |    |    | JS     | `JS_Compact`                                                 | v8-dispatcher.cpp          |                                          | AdminWriteReplicatedLog             |                                          |                        |
|    |    |    | JS     | `JS_RemoveUser`                                              | v8-users.cpp               |                                          | canWriteUser                        |                                          |                        |
|    |    |    | JS     | `JS_ReloadAuthData`                                          | v8-users.cpp               |                                          | AdminAuthReload                     |                                          |                        |
|    |    |    | JS     | `JS_GrantDatabase`                                           | v8-users.cpp               |                                          | canWriteUser                        |                                          |                        |
|    |    |    | JS     | `JS_RevokeDatabase`                                          | v8-users.cpp               |                                          | canWriteUser                        |                                          |                        |
|    |    |    | JS     | `JS_GrantCollection`                                         | v8-users.cpp               |                                          | canWriteUser                        |                                          |                        |
|    |    |    | JS     | `JS_RevokeCollection`                                        | v8-users.cpp               |                                          | canWriteUser                        |                                          |                        |
|    |    |    | JS     | `StoreUser`                                                  | v8-users.cpp               |                                          | canWriteUser                        |                                          |                        |
|    |    |    | JS     | `JS_UpdateUser`                                              | v8-users.cpp               |                                          | canWriteUser                        |                                          |                        |
|    |    |    | JS     | `JS_GetUser`                                                 | v8-users.cpp               |                                          | canReadUser                         |                                          |                        |
|    |    |    | JS     | `JS_UpdateConfigData`                                        | v8-users.cpp               |                                          | canWriteUser                        |                                          |                        |
|    |    |    | JS     | `JS_GetConfigData`                                           | v8-users.cpp               |                                          | canReadUser                         |                                          |                        |
|    |    |    | CPP    | `Databases::grantCurrentUser` (creation of database)         | Databases.Cpp              |                                          | canWriteUser                        |                                          |                        |


(1) For `arangorestore`, if `--overwrite=true`, then we need COLL RW, if `--overwrite=false`, we only need COLL RWDATA

(2) For `/_api/version`, details can only be queried with `AdminMonitoringInternal`, if `--server.harden=true`

(3) For `/_api/collection`, RO for database is needed, then all collections with canSeeCollection are listed

(4) For `/_api/database`, all databases with canSeeDatabase() are listed

(5) For `GET /_api/query-plan-cache` only those entries are returned, for which the user has read access to all occurring collections

Rules:
 - internal use of system collections allowed without check
 - read access to system collections can be regulated by RBAC if switched on
 - write access (with normal APIs) to system collections is superuser only
