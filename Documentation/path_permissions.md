# ArangoDB REST API Endpoint Permissions

## Migration philosophy
 
In the "classic" system permissions were given on a per-user basis and
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

Often, access to metadata was governed by RO or RW permissions on
the **container**. For example, creating an index on a collection was
allowed, if the user had RW access to the database which contained
the collection.

Finally, there is the "SUPERUSER" access, which means that a valid
JWT token without a `preferred_username` field was found. SUPERUSER
access has **no restrictions whatsoever** and is allowed to **do
everything**.

SUPERUSER is currently being used for three different reasons:
* Cluster-internal communication:

  Only Coordinators do detailed authentication or authorization.
  DBServers and Agents generally only accept API calls from a
  SUPERUSER.
* Platform-internal operations:

  Certain internal tools and services use SUPERUSER access to the
  database.
* Internally overriding permission checks:

  E.g. certain APIs need to access certain (system) collections, but
  they should work even without the user having explicit access to
  those collections. The permission checks are (or have been) ignorant
  of such decisions, so the caller did its own checks, switched to a
  SUPERUSER context, and proceeded.

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

The basic way to implement this new system is to overhaul all the
authorization across all APIs in the following way:

We create an abstraction so that we can specify which access permissions
one needs for each operation across all APIs. Then we implement this
abstraction by a number of methods on the `ExecContext`, which contains
the user and role data from authentication. For example, there will be
methods like `ExecContext::canSeeCollection(<dbname>, <collname>) -> Result`.

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

In the meantime we have changed this to perform header parsing but **not
any further checks** in the `CommTask`. Then, we have some virtual methods
on the `RestHandler` class which are called early during the execution
(but already on the Scheduler thread) of the `RestHandler`. These perform
then authentication checks - depending on the particular needs of the
URL path (some do checks, some don't).

After authentication, we perform a first authorization check: Namely,
the identity detected (user/roles) has to have **read access** to the
database which was specified in the `/_db/<dbname>` part of the URL
path. This check is done globally already in the above mentioned virtual
methods of the `RestHandler` to error out early, since we want to
enforce it **for all routes** (with very few exceptions).

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
   levels (which are independent of each other): "RWDATA" (which includes
   reading the collection meta data and
   data!) and "RWMETA" (which includes and modifying the collection meta data,
   for example creating and dropping indexes), so we have these access
   levels:
    1. NONE
    2. RO
    3. RWDATA
    4. RWMETA
   where RWDATA and RWMETA include RO, but RWMETA does not include RWDATA,
   since it is entirely possible that we want to allow somebody to modify
   indexes of a collection but not data.
 - Permission to create and drop collections are separate from this hierarchy.
 - There are five RBAC actions for collections: `db:ReadCollection`,
   `db:WriteCollectionData` and `db:WriteCollectionMeta`. To reach
   level `Read` for a collection, one only needs "allow" for
   `db:ReadCollection`. To reach level `RWDATA` one needs "allow" for
   `db:ReadCollection` and `db:WriteCollectionData`. To reach level `RWMETA`
   one needs "allow" on `db:ReadCollection` and `db:WriteCollectionData`.
   To create a collection, one needs `db:CreateCollection`, to drop a collection,
   one needs `db:DropCollection`.
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

 - `canCreateIndex(std::string_view db, std::string_view coll) -> Result`
 - `canDropIndex(std::stgring_view db, std::string_view coll) -> Result`

 - `canSeeView(std::string_view db, std::string_view view) -> Result`
 - `canCreateView(std::string_view db, std::string_view view) -> Result`
 - `canDropView(std::string_view db, std::string_view view) -> Result`
 - `canUseView(std::string_view db, std::string_view view) -> Result`

 - `canSeeAnalyzer(std::string_view db, std::string_view analyzer) -> Result`
 - `canCreateAnalyzer(std::string_view db, std::string_view analyzer) -> Result`
 - `canDropAnalyzer(std::string_view db, std::string_view analyzer) -> Result`
 - `canUseAnalyzer(std::string_view db, std::string_view analyzer) -> Result`

 - `canSeeGraph(std::string_view db, std::string_view graph) -> Result`
 - `canCreateGraph(std::string_view db, std::string_view graph, std::span<std::string_view> collectionNamesToCreate, std::span<std::string_view> collectionNamesToRead) -> Result`
 - `canDropGraph(std::string_view db, std::string_view graph, std::span<std::string_view> collectionNames) -> Result`
 - `canUseGraph(std::string_view db, std::string_view graph, GraphAccessLevel const level) -> Result`
 
 - `canReadUser(std::string_view user) -> Result`
 - `canWriteUser(std::string_view user) -> Result`
 - `canReadUsers(std::span<std::string_view const> -> std::vector<bool>`

 - `isSuperuser() -> bool`

Note that for now, `canSee*` is equivalent to `canUse*(RO)`. For
collections `canUseCollection(RWDATA)` is needed to write data. Testing
existence of a collection only needs `canSeeCollection`, whereas reading
the metadata of a collection needs `canUseCollection(RO)` and similarly
for databases, views, analyzers and graphs.

However, we keep the semantic checks separate in case we want to split
things further later.

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
  
 - `canCreateIndex(std::string_view db, std::string_view coll) -> Result`

   The user needs to have CollectionAccessLevel::WriteMeta for the collection
   and DatabaseAccessLevel::Write for the database.
  
 - `canDropIndex(std::stgring_view db, std::string_view coll) -> Result`

   The user needs to have CollectionAccessLevel::WriteMeta for the collection
   and DatabaseAccessLevel::Write for the database.
  
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
  
 - `canSeeGraph(std::string_view db, std::string_view graph) -> Result`

   This is just checking if we have read access to the database, since
   this automatically implies read access to the `_graphs` collection.
  
 - `canCreateGraph(std::string_view db, std::string_view graph, std::span<std::string_view> collectionNamesToCreate, std::span<std::string_view> collectionNamesToRead) -> Result`

   This is checking if we have write access to the database (since we need write access
   to the `_graphs` collection). Furthermore, it checks if we are able to
   create the collections in the list `collectionNamesToCreate` and to read the collections
   in the list `collectionNamesToRead`.
  
 - `canDropGraph(std::string_view db, std::string_view graph, std::span<std::string_view> collectionNames) -> Result`

   This is checking if we have write access to the database (since we need write access
   to the `_graphs` collection). Furthermore, it checks if we are able to
   drop the collections in the list `collectionNames`.
  
 - `canUseGraph(std::string_view db, std::string_view graph, GraphAccessLevel const level) -> Result`
 
   Currently, this is just checking if we have read access to the database, since
   this automatically implies read access to the `_graphs` collection. For
   CollectionAccessLevel::WriteMeta (needed to change the graph), we need write
   access to the database.
  
 - `canReadUser(std::string_view user) -> Result`

   check RO access in system database
  
 - `canWriteUser(std::string_view user) -> Result`

   check RW access in system database
  
 - `canReadUsers(std::span<std::string_view const> -> std::vector<bool>`

   check RO access in system database
  
- `isSuperuser() -> bool`

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

   check `db:CreateCollection`
  
 - `canDropCollection(std::string_view db, std::string_view coll) -> Result`

   check `db:DropCollection`
  
 - `canUseCollection(std::string_view db, std::string_view coll, CollectionAccessLevel const level) -> Result`

   check collection access level, i.e., check RBAC actions
   `db:ReadCollection` and `db:WriteCollectionData` and
   `db:WriteCollectionMeta` to find NONE, or RO, or RWDATA, or RW,
   as described above.

   If the user is not allowed to see the collection, this must return NOT_FOUND!
  
 - `canCreateIndex(std::string_view db, std::string_view coll) -> Result`

   check `db:WriteCollectionMeta` for the collection.
  
 - `canDropIndex(std::stgring_view db, std::string_view coll) -> Result`

   check `db:WriteCollectionMeta` for the collection.
  
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
  
 - `canSeeGraph(std::string_view db, std::string_view graph) -> Result`

   This checks if we have `db:ReadGraph` for the resource
   `db:graph:<dbname>:<graphname>`. The access to the `_graphs` collection
   is then implicit.
  
 - `canCreateGraph(std::string_view db, std::string_view graph, std::span<std::string_view> collectionNamesToCreate, std::span<std::string_view> collectionNamesToRead) -> Result`

   This is checking if we have create access to the graph,
   that is, we have `db:CreateGraph` With the resource
   `db:graph:<dbname>:<graphname>`. Furthermore, it checks if we are
   able to create the collections in the list `collectionNamesToCreate`
   and read the collections in the list `collectionNamesToRead`..
  
 - `canDropGraph(std::string_view db, std::string_view graph, std::span<std::string_view> collectionNames) -> Result`

   This is checking if we have drop access to the graph,
   that is, we have `db:DropGraph` with the resource
   `db:graph:<dbname>:<graphname>`. Furthermore, it checks if we are
   able to drop the collections in the list `collectionNames`.
  
 - `canUseGraph(std::string_view db, std::string_view graph, GraphAccessLevel const level) -> Result`
 
   This is checking if we have `db:ReadGraph` and `db:WriteGraph`
   respectively on the resource `db:graph:<dbname>:<graphname>`. For
   `CollectionAccessLevel::Read` we only need `db:ReadGraph`, for
   `CollectionAccessLevel:WriteMeta` we need both.

 - `canReadUser(std::string_view user) -> Result`

   check RBAC action `db:ReadUser`
  
 - `canWriteUser(std::string_view user) -> Result`

   check RBAC action `db:WriteUser`
  
 - `canReadUsers(std::span<std::string_view const> -> std::vector<bool>`

   check RBAC action `db:ReadUser`
  
- `isSuperuser() -> bool`

  must return `true` if and only if the authenticated user is the superuser (JWT
  token with empty `preferred_username`.
  
### Implementation details when authentication is switched off

All these functions should return Ok. The `isSuperuser` method should return `true`.


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
canUseHard(X) - stands for canUseHardenedAction(AdminX)
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
| X  |    | X  | POST   | `/_open/auth`                                                | RestAuthHandler            | -                                        | OPEN                                | needs special exception in AUTHEN check! |                        |
| X  |    | X  | POST   | `/_open/auth/renew`                                          | RestAuthHandler            | -                                        | OPEN                                | needs special exception in AUTHEN check! |                        |
| X  |    | X  | GET    | `/_admin/actions`                                            | MaintenanceRestHandler     | -                                        | AUTHEN                              | Only really relevant on DBServers        |                        |
| X  |    | X  | POST   | `/_admin/actions`                                            | MaintenanceRestHandler     | -                                        | AUTHEN                              | Only really relevant on DBServers        |                        |
| X  |    | X  | PUT    | `/_admin/actions`                                            | MaintenanceRestHandler     | -                                        | AUTHEN                              | Only really relevant on DBServers        |                        |
| X  |    | X  | DELETE | `/_admin/actions/{id}`                                       | MaintenanceRestHandler     | -                                        | AUTHEN                              | Only really relevant on DBServers        |                        |
| X  |    | X  | GET    | `/_admin/activities`                                         | activities::RestHandler    | isSuperuser / canUseAdmin(MonInternal)   | S/A AdminMonitoringInternal         |                                          |                        |
| X  |    | X  | GET    | `/_admin/async-registry`                                     | async_registry::RestHandler| canUseAdmin(MonInternal)                 | AdminMonitoringInternal             |                                          |                        |
| X  |    | X  | POST   | `/_admin/auth/reload`                                        | RestAdminAuthReloadHandler | canuseadmin(AuthReload)                  | AdminAuthReload                     |                                          |                        |
| X  |    | X  | POST   | `/_admin/backup/create`                                      | RestHotBackupHandler       | isSuperuser / canUseAdmin(Backup)        | S/A AdminBackup                     |                                          |                        |
| X  |    | X  | POST   | `/_admin/backup/delete`                                      | RestHotBackupHandler       | isSuperuser / canUseAdmin(Backup)        | S/A AdminBackup                     |                                          |                        |
| X  |    |    | POST   | `/_admin/backup/download`                                    | RestHotBackupHandler       | isSuperuser / canUseAdmin(Backup)        | S/A AdminBackup                     |                                          |                        |
| X  |    | X  | POST   | `/_admin/backup/list`                                        | RestHotBackupHandler       | isSuperuser / canUseAdmin(Backup)        | S/A AdminBackup                     |                                          |                        |
| X  |    |    | POST   | `/_admin/backup/upload`                                      | RestHotBackupHandler       | isSuperuser / canUseAdmin(Backup)        | S/A AdminBackup                     |                                          |                        |
| X  |    |    | POST   | `/_admin/backup/restore`                                     | RestHotBackupHandler       | isSuperuser / canUseAdmin(Backup)        | S/A AdminBackup                     |                                          |                        |
| X  |    | X  | GET    | `/_admin/cluster/collectionShardDistribution`                | RestAdminClusterHandler    | canUseAdmin(ClusterInfo)                 | AdminClusterInfo                    | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | POST   | `/_admin/cluster/cancelAgencyJob`                            | RestAdminClusterHandler    | canUseAdmin(MoveShards)                  | AdminMoveShards                     | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | POST   | `/_admin/cluster/cleanOutServer`                             | RestAdminClusterHandler    | canUseAdmin(MoveShards)                  | AdminMoveShards                     | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | GET    | `/_admin/cluster/health`                                     | RestAdminClusterHandler    | -                                        | AUTHEN                              | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | GET    | `/_admin/cluster/maintenance`                                | RestAdminClusterHandler    | canUseAdmin(Maintenance)                 | AdminMaintenance                    | SA/SW/LEG, only coordinator+single       |                        |
| X  |    | X  | PUT    | `/_admin/cluster/maintenance`                                | RestAdminClusterHandler    | canUseAdmin(Maintenance)                 | AdminMaintenance                    | SA/SW/LEG, only coordinator+single       |                        |
| X  |    | X  | GET    | `/_admin/cluster/maintenance/{serverId}`                     | RestAdminClusterHandler    | canUseAdmin(Maintenance)                 | AdminMaintenance                    | SA/SW/LEG, only coordinator+single       |                        |
| X  |    | X  | PUT    | `/_admin/cluster/maintenance/{serverId}`                     | RestAdminClusterHandler    | canUseAdmin(Maintenance)                 | AdminMaintenance                    | SA/SW/LEG, only coordinator+single       |                        |
| X  |    | X  | POST   | `/_admin/cluster/moveShard`                                  | RestAdminClusterHandler    | canUseAdmin(MoveShard) | canUseColl(RW)  | AdminMoveShards or COLL RW          | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | GET    | `/_admin/cluster/nodeEngine`                                 | RestAdminClusterHandler    | -                                        | AUTHEN                              | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | GET    | `/_admin/cluster/nodeStatistics`                             | RestAdminClusterHandler    | -                                        | AUTHEN                              | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | GET    | `/_admin/cluster/nodeVersion`                                | RestAdminClusterHandler    | -                                        | AUTHEN                              | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | GET    | `/_admin/cluster/numberOfServers`                            | RestAdminClusterHandler    | -                                        | AUTHEN                              | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | PUT    | `/_admin/cluster/numberOfServers`                            | RestAdminClusterHandler    | canUseHard(Maintenance)                  | AdminMaintenance, HARD              | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | GET    | `/_admin/cluster/queryAgencyJob`                             | RestAdminClusterHandler    | canUseAdmin(MoveShards)                  | AdminMoveShards                     | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | GET    | `/_admin/cluster/rebalance`                                  | RestAdminClusterHandler    | canUseAdmin(Rebalance)                   | AdminRebalance                      | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | PUT    | `/_admin/cluster/rebalance`                                  | RestAdminClusterHandler    | canUseAdmin(Rebalance)                   | AdminRebalance                      | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | PUT    | `/_admin/cluster/rebalanceShards`                            | RestAdminClusterHandler    | canUseAdmin(Rebalance)                   | AdminRebalance                      | SA/SW/LEG, only coordinator              | Was: AUTHEN + DB RW    |
| X  |    | X  | POST   | `/_admin/cluster/removeServer`                               | RestAdminClusterHandler    | canUseAdmin(RemoveServer)                | AdminRemoveServer                   | SA/SW/LEG                                |                        |
| X  |    | X  | POST   | `/_admin/cluster/resignLeadership`                           | RestAdminClusterHandler    | canUseAdmin(MoveShards)                  | AdminMoveShards                     | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | GET    | `/_admin/cluster/shardDistribution`                          | RestAdminClusterHandler    | canUseAdmin(ClusterInfo)                 | AdminClusterInfo                    | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | GET    | `/_admin/cluster/shardStatistics`                            | RestAdminClusterHandler    | canUseadmin(ClusterInfo)                 | AdminClusterInfo                    | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | GET    | `/_admin/cluster/statistics`                                 | RestAdminClusterHandler    | -                                        | AUTHEN                              | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | PUT    | `/_admin/cluster/uniqId`                                     | RestAdminClusterHandler    | canUseAdmin(Maintenance)                 | AdminMaintenance                    | SA/SW/LEG, only coordinator              |                        |
| X  |    | X  | PUT    | `/_admin/cluster/vpackSortMigration/{serverId}`              | RestAdminClusterHandler    | isSuperuser                              | SUPER                               | SA/SW/LEG                                |                        |
| X  |    | X  | PUT    | `/_admin/compact`                                            | RestCompactHandler         | isSuperuser                              | SUPER                               |                                          |                        |
| X  |    | X  | GET    | `/_admin/crashes`                                            | RestCrashHandler           | canUseAdmin(CrashHandler)                | AdminCrashHandler                   |                                          |                        |
| X  |    | X  | GET    | `/_admin/crashes/{id}`                                       | RestCrashHandler           | canUseAdmin(CrashHandler)                | AdminCrashHandler                   |                                          |                        |
| X  |    | X  | DELETE | `/_admin/crashes/{id}`                                       | RestCrashHandler           | canUseAdmin(CrashHandler)                | AdminCrashHandler                   |                                          |                        |
| X  |    | X  | GET    | `/_admin/database/target-version`                            | RestAdminDatabaseHandler   | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | GET    | `/_admin/debug/failat`                                       | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    | X  | GET    | `/_admin/debug/failat/all`                                   | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    | X  | PUT    | `/_admin/debug/failat/{name}`                                | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    | X  | DELETE | `/_admin/debug/failat`                                       | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    | X  | DELETE | `/_admin/debug/failat/{name}`                                | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    | X  | DELETE | `/_admin/debug/raceControl`                                  | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    | X  | PUT    | `/_admin/debug/crash`                                        | RestDebugHandler           | -                                        | AUTHEN                              | (maintainer mode only)                   |                        |
| X  |    | X  | GET    | `/_admin/deployment/id`                                      | RestAdminDeploymentHandler | -                                        | AUTHEN                              | only coordinators and single             |                        |
| X  |    | X  | POST   | `/_admin/execute`                                            | RestAdminExecuteHandler    | -                                        | AUTHEN                              | only --javascript.allow-admin-execute    |                        |
| X  |    | X  | GET    | `/_admin/job/{id}`                                           | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | GET    | `/_admin/job/{type}`                                         | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | PUT    | `/_admin/job/{id}`                                           | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | PUT    | `/_admin/job/{id}/cancel`                                    | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | DELETE | `/_admin/job/all`                                            | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | DELETE | `/_admin/job/expired`                                        | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | DELETE | `/_admin/job/{id}`                                           | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | GET    | `/_api/job/{id}`                                             | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | GET    | `/_api/job/{type}`                                           | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | PUT    | `/_api/job/{id}`                                             | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | PUT    | `/_api/job/{id}/cancel`                                      | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | DELETE | `/_api/job/all`                                              | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | DELETE | `/_api/job/expired`                                          | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | DELETE | `/_api/job/{id}`                                             | RestJobHandler             | -                                        | AUTHEN                              | We check in the JobManager same use      |                        |
| X  |    | X  | GET    | `/_admin/license`                                            | RestLicenseHandler(EE)     | canUseHard(License)                      | AdminLicense, HARD                  |                                          |                        |
| X  |    | X  | PUT    | `/_admin/license`                                            | RestLicenseHandler(EE)     | canUseHard(License)                      | AdminLicense, HARD                  |                                          |                        |
| X  |    | X  | GET    | `/_admin/log`                                                | RestAdminLogHandler        | isSuperuser / canUseAdmin(ReadLogs)      | AdminReadLogs                       | ?/S/A                                    |                        |
| X  |    | X  | GET    | `/_admin/log/entries`                                        | RestAdminLogHandler        | isSuperuser / canUseAdmin(ReadLogs)      | AdminReadLogs                       | ?/S/A                                    |                        |
| X  |    | X  | GET    | `/_admin/log/level`                                          | RestAdminLogHandler        | isSuperuser / canUseAdmin(ReadLogs)      | AdminReadLogs                       | ?/S/A                                    |                        |
| X  |    | X  | GET    | `/_admin/log/structured`                                     | RestAdminLogHandler        | isSuperuser / canUseAdmin(ReadLogs)      | AdminReadLogs                       | ?/S/A                                    |                        |
| X  |    | X  | PUT    | `/_admin/log/level`                                          | RestAdminLogHandler        | isSuperuser / canUseAdmin(SetLogLevel)   | AdminSetLogLevel                    | ?/S/A                                    |                        |
| X  |    | X  | PUT    | `/_admin/log/structured`                                     | RestAdminLogHandler        | isSuperuser / canUseAdmin(SetLogLevel)   | AdminSetLogLevel                    | ?/S/A                                    |                        |
| X  |    | X  | DELETE | `/_admin/log`                                                | RestAdminLogHandler        | isSuperuser / canUseAdmin(SetLogLevel)   | AdminSetLogLevel                    | ?/S/A                                    |                        |
| X  |    | X  | DELETE | `/_admin/log/entries`                                        | RestAdminLogHandler        | isSuperuser / canUseAdmin(SetLogLevel)   | AdminSetLogLevel                    | ?/S/A                                    |                        |
| X  |    | X  | DELETE | `/_admin/log/level`                                          | RestAdminLogHandler        | isSuperuser / canUseAdmin(SetLogLevel)   | AdminSetLogLevel                    | ?/S/A                                    |                        |
| X  |    | X  | GET    | `/_admin/metrics`                                            | RestMetricsHandler         | canUseHard(Monitoring)                   | AdminMonitoring, HARD               |                                          |                        |
| X  |    | X  | GET    | `/_admin/options`                                            | RestOptionsHandler         | isSuperuser / canUseAdmin(Options) / -   | AdminOptions                        | S/A/AU                                   |                        |
| X  |    | X  | GET    | `/_admin/options-description`                                | RestOptionsDescriptionHandler | isSuperuser / canUseAdmin(Options) / -| AdminOptions                        | S/A/AU                                   |                        |
| X  |    | X  | GET    | `/_admin/options-public`                                     | RestPublicOptionsHandler   | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | POST   | `/_admin/routing/reload`                                     | RestAdminRoutingHandler    | -                                        | AUTHEN                              | (V8 required)                            |                        |
| X  |    | X  | GET    | `/_admin/server/api-calls`                                   | RestAdminServerHandler     | isSuperuser / canUseAdmin(ApiCalls)      | AdminApiCalls                       | ?/S/A                                    |                        |
| X  |    | X  | GET    | `/_admin/server/aql-queries`                                 | RestAdminServerHandler     | isSuperuser / canUseAdmin(AqlQueries)    | AdminAqlQueries                     | ?/S/A                                    |                        |
| X  |    | X  | GET    | `/_admin/server/availability`                                | RestAdminServerHandler     | -                                        | OPEN                                |                                          |                        |
| X  |    | X  | GET    | `/_admin/server/databaseDefaults`                            | RestAdminServerHandler     | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | GET    | `/_admin/server/id`                                          | RestAdminServerHandler     | -                                        | AUTHEN                              | (cluster only)                           |                        |
| X  |    | X  | GET    | `/_admin/server/mode`                                        | RestAdminServerHandler     | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | PUT    | `/_admin/server/mode`                                        | RestAdminServerHandler     | canUseAdmin(Maintenance)                 | AdminMaintenance                    |                                          |                        |
| X  |    | X  | GET    | `/_admin/server/role`                                        | RestAdminServerHandler     | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | GET    | `/_admin/server/tls`                                         | RestAdminServerHandler     | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | POST   | `/_admin/server/tls`                                         | RestAdminServerHandler     | isSuperuser                              | SUPER                               |                                          |                        |
| X  |    | X  | GET    | `/_admin/server/jwt`                                         | RestAdminServerHandler     | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | POST   | `/_admin/server/jwt`                                         | RestAdminServerHandler     | isSuperuser                              | SUPER                               |                                          |                        |
| X  |    | X  | GET    | `/_admin/server/encryption`                                  | RestAdminServerHandler     | -                                        | AUTHEN                              | (not on coordinators)                    |                        |
| X  |    | X  | POST   | `/_admin/server/encryption`                                  | RestAdminServerHandler     | isSuperuser                              | SUPER                               | (not on coordinators)                    |                        |
| X  |    | X  | GET    | `/_admin/shutdown`                                           | RestShutdownHandler        | -                                        | AUTHEN                              | (only coordinator for soft shutdown)     |                        |
| X  |    |    | DELETE | `/_admin/shutdown`                                           | RestShutdownHandler        | canUseAdmin(Shutdown)                    | AdminShutdown                       |                                          |                        |
| X  |    | X  | GET    | `/_admin/statistics`                                         | RestAdminStatisticsHandler | canUseHard(Monitoring)                   | AdminMonitoring, HARD               |                                          |                        |
| X  |    | X  | GET    | `/_admin/statistics-description`                             | RestAdminStatisticsHandler | canUseHard(Monitoring)                   | AdminMonitoring, HARD               |                                          |                        |
| X  |    | X  | GET    | `/_admin/status`                                             | RestAdminStatusHandler     | canUseHard(Monitoring)                   | AdminMonitoring HARD                |                                          |                        |
| X  |    | X  | GET    | `/_admin/supervisionState`                                   | RestSupervisionStateHandler| canUseAdmin(SupervisionState)            | AdminSupervisionState               | (coordinator only)                       |                        |
| X  |    | X  | GET    | `/_admin/support-info`                                       | RestSupportInfoHandler     | isSuperuser / canUseAdmin(Monitoring) / -| AdminMonitoring                     | ?/S/A/AU                                 |                        |
| X  |    | X  | GET    | `/_admin/system-report`                                      | RestSystemReportHandler    | canUseHard(MonitoringInternal)           | AdminMonitoringInternali, HARD      |                                          |                        |
| X  |    | X  | GET    | `/_admin/telemetrics`                                        | RestTelemetricsHandler     | isSuperuser / canUseAdmin(MonitoringInternal) / -  | AdminMonitoringInternal   | ?/S/A/AU                                 |                        |
| X  |    | X  | DELETE | `/_admin/telemetrics`                                        | RestTelemetricsHandler     | isSuperuser / canUseAdmin(MonitoringInternal) / -  | AdminMonitoringInternal   | ?/S/A/AU                                 |                        |
| X  |    | X  | GET    | `/_admin/time`                                               | RestTimeHandler            | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | GET    | `/_admin/usage-metrics`                                      | RestUsageMetricsHandler    | canUseHard(MonitoringInternal)           | AdminMonitoringInternal HARD        |                                          |                        |
| X  |    | X  | GET    | `/_admin/version`                                            | RestVersionhandler         | -/canUseHard(MonitoringInternal)         | AUTHEN, details (2)                 |                                          |                        |
| X  |    | X  | GET    | `/_admin/wal/properties`                                     | RestWalAccessHandler       | -                                        | SUPER                               | (RocksDB engine) only DBServer           |                        |
| X  |    | X  | PUT    | `/_admin/wal/properties`                                     | RestWalAccessHandler       | -                                        | SUPER                               | (RocksDB engine) only DBServer           |                        |
| X  |    | X  | GET    | `/_admin/wal/transactions`                                   | RestWalAccessHandler       | -                                        | SUPER                               | (RocksDB engine) only DBServer           |                        |
| X  |    | X  | PUT    | `/_admin/wal/flush`                                          | RestWalAccessHandler       | -                                        | SUPER                               | (RocksDB engine) only DBServer           |                        |
| X  |    | X  | PUT    | `/_admin/wal/wait_for_estimator_sync`                        | RestWalAccessHandler       | -                                        | SUPER                               | (RocksDB engine) only DBServer           |                        |
| X  |    | X  | GET    | `/_admin/wal/properties`                                     | ClusterRestWalHandler      | -                                        | AUTHEN                              | (Cluster engine) NOT_IMPL                |                        |
| X  |    | X  | PUT    | `/_admin/wal/properties`                                     | ClusterRestWalHandler      | -                                        | AUTHEN                              | (Cluster engine) NOT_IMPL                |                        |
| X  |    | X  | GET    | `/_admin/wal/transactions`                                   | ClusterRestWalHandler      | -                                        | AUTHEN                              | (Cluster engine) NOT_IMPL                |                        |
| X  |    | X  | PUT    | `/_admin/wal/flush`                                          | ClusterRestWalHandler      | -                                        | AUTHEN                              | (Cluster engine) DELEGATED to DBServers  |                        |
| X  |    | X  | PUT    | `/_admin/wal/wait_for_estimator_sync`                        | ClusterRestWalHandler      | canUseAdmin(WalAccess) / isSuperuser     | AdminWalAccess (PROD)/SUPER (MAINT) | (Cluster engine)                         |                        |
| X  |    | X  | GET    | `/_api/aql-builtin`                                          | RestAqlFunctionsHandler    | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | GET    | `/_api/aqlfunction`                                          | RestAqlUserFunctionsHandler| -, then run AQL with _aqlfunctions coll  | AUTHEN + COLL RO _aqlfunctions      | (V8 required) Note: system-collection!   |                        |
| X  |    | X  | GET    | `/_api/aqlfunction/{namespace}`                              | RestAqlUserFunctionsHandler| -, then run AQL with _aqlfunctions coll  | AUTHEN + COLL RO _aqlfunctions      | (V8 required) Note: system-collection!   |                        |
| X  |    | X  | POST   | `/_api/aqlfunction`                                          | RestAqlUserFunctionsHandler| -, then run AQL with _aqlfunctions coll  | AUTHEN + COLL RW _aqlfunctions      | (V8 required) Note: system-collection!   |                        |
| X  |    | X  | DELETE | `/_api/aqlfunction/{name}`                                   | RestAqlUserFunctionsHandler| -, then run AQL with _aqlfunctions coll  | AUTHEN + COLL RW _aqlfunctions      | (V8 required) Note: system-collection!   |                        |
| X  |    | X  | GET    | `/_api/analyzer`                                             | RestAnalyzerHandler        | -, then run AQL with _analyzers coll     | AUTHEN + COLL RO _analyzers         | Note: system-collection!                 |                        |
| X  |    | X  | GET    | `/_api/analyzer/{name}`                                      | RestAnalyzerHandler        | -, then run AQL with _analyzers coll     | AUTHEN + COLL RO _analyzers         | Note: system-collection!                 |                        |
| X  |    | X  | POST   | `/_api/analyzer`                                             | RestAnalyzerHandler        | -, then run AQL with _analyzers coll     | AUTHEN + COLL RW _analyzers         | Note: system-collection!                 |                        |
| X  |    | X  | DELETE | `/_api/analyzer/{name}`                                      | RestAnalyzerHandler        | -, then run AQL with _analyzers coll     | AUTHEN + COLL RW _analyzers         | Note: system-collection!                 |                        |
| X  |    | X  | GET    | `/_api/cluster/agency-cache`                                 | RestClusterHandler         | canUseAdmin(ReadAgency)                  | AdminReadAgency                     | (coordinator only)                       |                        |
| X  |    | X  | GET    | `/_api/cluster/agency-dump`                                  | RestClusterHandler         | canUseAdmin(ReadAgency)                  | AdminReadAgency                     | (coordinator only)                       |                        |
| X  |    | X  | GET    | `/_api/cluster/cluster-info`                                 | RestClusterHandler         | canUseAdmin(ClusterInfo)                 | AdminClusterInfo                    | (cluster only)                           |                        |
| X  |    | X  | PUT    | `/.../flush`                                                 | RestClusterHandler         | isSuperuser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    | X  | GET    | `/.../get_collection_info/{db}/{coll}`                       | RestClusterHandler         | isSuperuser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    | X  | GET    | `/.../get_collection_info_current/{db}/{coll}/{shard}`       | RestClusterHandler         | isSuperuser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    | X  | POST   | `/.../get_responsible_servers`                               | RestClusterHandler         | isSuperuser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    | X  | POST   | `/.../get_responsible_shard/{db}/{coll}`                     | RestClusterHandler         | isSuperuser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    | X  | GET    | `/.../get_analyzers_revision/{db}`                           | RestClusterHandler         | isSuperuser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    | X  | GET    | `/.../wait_for_plan_version/{version}`                       | RestClusterHandler         | isSuperuser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    | X  | GET    | `/.../get_max_number_of_shards`                              | RestClusterHandler         | isSuperuser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    | X  | GET    | `/.../get_max_replication_factor`                            | RestClusterHandler         | isSuperuser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    | X  | GET    | `/.../get_min_replication_factor`                            | RestClusterHandler         | isSuperuser                              | SUPER (no check in MAINTAINERMODE)  | (cluster only)                           |                        |
| X  |    | X  | GET    | `/_api/cluster/endpoints`                                    | RestClusterHandler         | -                                        | AUTHEN                              | (coordinator only)                       |                        |
| X  |    | X  | POST   | `/_api/collection`                                           | RestCollectionHandler      | canCreateCollection                      | COLL RW                             |                                          |                        |
| X  |    | X  | GET    | `/_api/collection`                                           | RestCollectionHandler      | canSeeCollection, only see readable      | AUTHEN, details (3)                 |                                          |                        |
| X  |    | X  | GET    | `/_api/collection/{name}`                                    | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    | X  | GET    | `/_api/collection/{name}/checksum`                           | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    | X  | GET    | `/_api/collection/{name}/count`                              | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    | X  | GET    | `/_api/collection/{name}/figures`                            | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    | X  | GET    | `/_api/collection/{name}/properties`                         | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    | X  | GET    | `/_api/collection/{name}/revision`                           | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    | X  | GET    | `/_api/collection/{name}/shards`                             | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    | X  | PUT    | `/_api/collection/{name}/compact`                            | RestCollectionHandler      | canUseCollection(WriteMeta)              | COLL RW                             |                                          |                        |
| X  |    | X  | PUT    | `/_api/collection/{name}/load`                               | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    | X  | PUT    | `/_api/collection/{name}/loadIndexesIntoMemory`              | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    | X  | PUT    | `/_api/collection/{name}/properties`                         | RestCollectionHandler      | canUseCollection(WriteMeta)              | COLL RW                             |                                          |                        |
| X  |    |    | PUT    | `/_api/collection/{name}/rename`                             | RestCollectionHandler      | canUseCollection(WriteMeta)              | COLL RW                             |                                          |                        |
| X  |    | X  | PUT    | `/_api/collection/{name}/responsibleShard`                   | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    | X  | PUT    | `/_api/collection/{name}/truncate`                           | RestCollectionHandler      | canUseCollection(WriteData)              | COLL RWDATA                         |                                          |                        |
| X  |    | X  | PUT    | `/_api/collection/{name}/unload`                             | RestCollectionHandler      | canUseCollection(Read)                   | COLL RO                             |                                          |                        |
| X  |    | X  | DELETE | `/_api/collection/{name}`                                    | RestCollectionHandler      | canDropCollection                        | COLL RW                             |                                          |                        |
| X  |    | X  | POST   | `/_api/cursor`                                               | RestCursorHandler          | then run AQL and rely on trx             | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| X  |    |    | POST   | `/_api/cursor/json`                                          | RestCursorHandler          | then run AQL and rely on trx             | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| X  |    | X  | POST   | `/_api/cursor/{id}`                                          | RestCursorHandler          | then run AQL and rely on trx             | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| X  |    |    | POST   | `/_api/cursor/{id}/{batch-id}`                               | RestCursorHandler          | then run AQL and rely on trx             | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| X  |    | X  | PUT    | `/_api/cursor/{id}`                                          | RestCursorHandler          | then run AQL and rely on trx             | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| X  |    | X  | DELETE | `/_api/cursor/{id}`                                          | RestCursorHandler          | then run AQL and rely on trx             | AUTHEN + COLL ACCESS via trx        |                                          |                        |
| X  |    | X  | GET    | `/_api/database`                                             | RestDatabaseHandler        | check to be in _system database          | AUTHEN, _system, list all           |                                          | FIXME?                 |
| X  |    | X  | GET    | `/_api/database/user`                                        | RestDatabaseHandler        | _system, canSeeDatabase                  | AUTHEN, _system, detail (4)         |                                          |                        |
| X  |    | X  | GET    | `/_api/database/current`                                     | RestDatabaseHandler        | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | GET    | `/_api/database/shardStatistics`                             | RestDatabaseHandler        | -                                        | AUTHEN                              | (coordinator only)                       |                        |
| X  |    | X  | POST   | `/_api/database`                                             | RestDatabaseHandler        | _system, canCreateDb                     | AUTHEN, _system, canCreateDB        |                                          |                        |
| X  |    | X  | DELETE | `/_api/database/{name}`                                      | RestDatabaseHandler        | _system, canDropDb                       | AUTHEN, _system, canDropDB          | should be canCreateOrDropDatabase        | FIXME                  |
| X  |    | X  | GET    | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Read), (via trx)        | COLL RO                             |                                          |                        |
| X  |    | X  | HEAD   | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Read), (via trx)        | COLL RO                             |                                          |                        |
| X  |    | X  | POST   | `/_api/document/{collection}`                                | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    | X  | PUT    | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    | X  | PUT    | `/_api/document/{collection}`                                | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    | X  | PATCH  | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    | X  | PATCH  | `/_api/document/{collection}`                                | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    | X  | DELETE | `/_api/document/{collection}/{key}`                          | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    | X  | DELETE | `/_api/document/{collection}`                                | RestDocumentHandler        | canUseCollection(Write), (via trx)       | COLL RWDATA                         |                                          |                        |
| X  |    |    | GET    | `/_api/document-state`                                       | RestDocumentStateHandler   | canUseAdmin(ReadReplicatedLog)           | AdminReadReplicatedLog              |                                          |                        |
| X  |    |    | POST   | `/_api/document-state`                                       | RestDocumentStateHandler   | canUseAdmin(WriteReplicatedLog)          | AdminWriteReplicatedLog             |                                          |                        |
| X  |    |    | DELETE | `/_api/document-state`                                       | RestDocumentStateHandler   | canUseAdmin(WriteReplicatedLog)          | AdminWriteReplicatedLog             |                                          |                        |
| X  |    |    | POST   | `/_api/dump/next/{id}`                                       | RestDumpHandler            | SAME USER                                | SAME USER                           | (dbserver and single only)               |                        |
| X  |    |    | POST   | `/_api/dump/start`                                           | RestDumpHandler            | canUseCollection(Read)                   | AUTHEN + COLL RO                    | (dbserver and single only)               | AdminDump + SINGLE => escalate to SUPER FIXME? |                 |
| X  |    |    | DELETE | `/_api/dump/{id}`                                            | RestDumpHandler            | SAME USER                                | SAME USER                           | (dbserver and single only)               |                        |
| X  |    |    | GET    | `/_api/edges/{collection}`                                   | RestEdgesHandler           | canUseCollection(Read) (via trx)         | COLL RO                             |                                          |                        |
| X  |    |    | POST   | `/_api/edges/{collection}`                                   | RestEdgesHandler           | canUseCollection(Read) (via trx)         | COLL RO                             |                                          |                        |
| X  |    | X  | GET    | `/_api/endpoint`                                             | RestEndpointHandler        | _system                                  | AUTHEN, _system                     |                                          |                        |
| X  |    | X  | GET    | `/_api/engine`                                               | RestEngineHandler          | canUseHard(MonitoringInternal)           | AdminMonitoringInternal, HARD       |                                          |                        |
| X  |    | X  | GET    | `/_api/engine/stats`                                         | RestEngineHandler          | canUseHard(MonitoringInternal)           | AdminMonitoringInternal, HARD       |                                          |                        |
| X  |    | X  | POST   | `/_api/explain`                                              | RestExplainHandler         | canUseCollection(Read) (via trx)         | AUTHEN, COLL RO via trx             |                                          |                        |
| X  |    |    | GET    | `/_api/gharial`                                              | RestGraphHandler           | canSeeGraph: only list those             | canSeeGraph see (6)                 |                                          |                        |
| X  |    |    | POST   | `/_api/gharial`                                              | RestGraphHandler           | canCreateGraph                           | canCreateGraph + coll checks        |                                          |                        |
| X  |    |    | GET    | `/_api/gharial/{graph}`                                      | RestGraphHandler           | canUseGraph(RO)                          | canUseGraph(RO)                     |                                          |                        |
| X  |    |    | DELETE | `/_api/gharial/{graph}`                                      | RestGraphHandler           | canDropGraph                             | canDropGraph + canDropColl(...)     |                                          |                        |
| X  |    |    | GET    | `/_api/gharial/{graph}/edge`                                 | RestGraphHandler           | canUseGraph(RO)                          | canUseGraph(RO)                     |                                          |                        |
| X  |    |    | POST   | `/_api/gharial/{graph}/edge`                                 | RestGraphHandler           | canUseGraph(RW)                          | canUseGraph(RW)                     |                                          |                        |
| X  |    |    | GET    | `/_api/gharial/{graph}/edge/{definition}/{key}`              | RestGraphHandler           | canUseGraph(RO)                          | canUseGraph(RO)                     |                                          |                        |
| X  |    |    | POST   | `/_api/gharial/{graph}/edge/{definition}`                    | RestGraphHandler           | canUseGraph(RO) + canUseColl(RWDATA)     | canUseGraph(RO) + COLL RWDATA       |                                          |                        |
| X  |    |    | PUT    | `/_api/gharial/{graph}/edge/{definition}`                    | RestGraphHandler           | canUseGraph(RW)                          | canUseGraph(RW)                     |                                          |                        |
| X  |    |    | DELETE | `/_api/gharial/{graph}/edge/{definition}`                    | RestGraphHandler           | canUseGraph(RW)                          | canUseGraph(RW)                     |                                          |                        |
| X  |    |    | PUT    | `/_api/gharial/{graph}/edge/{definition}/{key}`              | RestGraphHandler           | canUseGraph(RO) + canUseColl(RWDATA)     | canUseGraph(RO) + COLL RWDATA       |                                          |                        |
| X  |    |    | PATCH  | `/_api/gharial/{graph}/edge/{definition}/{key}`              | RestGraphHandler           | canUseGraph(RO) + canUseColl(RWDATA)     | canUseGraph(RO) + COLL RWDATA       |                                          |                        |
| X  |    |    | DELETE | `/_api/gharial/{graph}/edge/{definition}/{key}`              | RestGraphHandler           | canUseGraph(RO) + canUseColl(RWDATA)     | canUseGraph(RO) + COLL RWDATA       |                                          |                        |
| X  |    |    | GET    | `/_api/gharial/{graph}/vertex`                               | RestGraphHandler           | canUseGraph(RO)                          | canUseGraph(RO)                     |                                          |                        |
| X  |    |    | POST   | `/_api/gharial/{graph}/vertex`                               | RestGraphHandler           | canUseGraph(RW)                          | canUseGraph(RW)                     |                                          |                        |
| X  |    |    | GET    | `/_api/gharial/{graph}/vertex/{collection}/{key}`            | RestGraphHandler           | canUseGraph(RO)                          | canUseGraph(RO)                     |                                          |                        |
| X  |    |    | POST   | `/_api/gharial/{graph}/vertex/{collection}`                  | RestGraphHandler           | canUseGraph(RO) + canUseColl(RWDATA)     | canUseGraph(RO) + COLL RWDATA       |                                          |                        |
| X  |    |    | DELETE | `/_api/gharial/{graph}/vertex/{collection}`                  | RestGraphHandler           | canUseGraph(RW)                          | canUseGraph(RW)                     |                                          |                        |
| X  |    |    | PUT    | `/_api/gharial/{graph}/vertex/{collection}/{key}`            | RestGraphHandler           | canUseGraph(RO) + canUseColl(RWDATA)     | canUseGraph(RO) + COLL RWDATA       |                                          |                        |
| X  |    |    | PATCH  | `/_api/gharial/{graph}/vertex/{collection}/{key}`            | RestGraphHandler           | canUseGraph(RO) + canUseColl(RWDATA)     | canUseGraph(RO) + COLL RWDATA       |                                          |                        |
| X  |    |    | DELETE | `/_api/gharial/{graph}/vertex/{collection}/{key}`            | RestGraphHandler           | canUseGraph(RO) + canUseColl(RWDATA)     | canUseGraph(RO) + COLL RWDATA       |                                          |                        |
| X  |    |    | GET    | `/_api/index`                                                | RestIndexHandler           | canUseColl(Read)                         | COLL RO                             |                                          |                        |
| X  |    |    | GET    | `/_api/index/selectivity`                                    | RestIndexHandler           | canUseColl(Read) (via trx)               | COLL RO                             |                                          |                        |
| X  |    |    | POST   | `/_api/index`                                                | RestIndexHandler           | canCreateIndex(coll)                     | COLL RWMETA                         |                                          |                        |
| X  |    |    | POST   | `/_api/index/sync-caches`                                    | RestIndexHandler           | AUTHEN                                   | AUTHEN                              |                                          |                        |
| X  |    |    | DELETE | `/_api/index/{collection}/{id}`                              | RestIndexHandler           | canDropIndex(coll)                       | COLL RWMETA                         |                                          |                        |
| X  |    | X  | GET    | `/_api/key-generators`                                       | RestKeyGeneratorsHandler   | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | GET    | `/_api/log`                                                  | RestLogHandler             | canUseAdmin(AdminReadReplicatedLog       | AdminReadReplicatedLog              | (replication2 + cluster only)            |                        |
| X  |    | X  | POST   | `/_api/log`                                                  | RestLogHandler             | canUseAdmin(AdminWriteReplicatedLog      | AdminWriteReplicatedLog             | (replication2 + cluster only)            |                        |
| X  |    | X  | DELETE | `/_api/log`                                                  | RestLogHandler             | canUseAdmin(AdminWriteReplicatedLog      | AdminWriteReplicatedLog             | (replication2 + cluster only)            |                        |
| X  |    | X  | GET    | `/_api/log-internal`                                         | RestLogInternalHandler     | isSuperuser                              | SUPER                               | (replication2 + cluster only)            |                        |
| X  |    | X  | GET    | `/_api/query/slow`                                           | RestQueryHandler           | _system + isSuperuser (if for all DBs)   | AUTHEN, for all DBs _system + SUPER |                                          |                        |
| X  |    | X  | GET    | `/_api/query/current`                                        | RestQueryHandler           | _system + isSuperuser (if for all DBs)   | AUTHEN, for all DBs _system + SUPER |                                          |                        |
| X  |    | X  | GET    | `/_api/query/properties`                                     | RestQueryHandler           | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | GET    | `/_api/query/registry`                                       | RestQueryHandler           | isSuperuser                              | SUPER                               |                                          |                        |
| X  |    | X  | GET    | `/_api/query/rules`                                          | RestQueryHandler           | -                                        | AUTHEN                              |                                          |                        |
| X  |    |    | POST   | `/_api/query`                                                | RestQueryHandler           | -                                        | AUTHEN                              |                                          |                        |
| X  |    |    | DELETE | `/_api/query/{id}`                                           | RestQueryHandler           | _system + isSuperuser (if for all DBs)   | AUTHEN, for all DBs _system + SUPER |                                          |                        |
| X  |    |    | DELETE | `/_api/query/slow`                                           | RestQueryHandler           | _system + isSuperuser (if for all DBs)   | AUTHEN, for all DBs _system + SUPER |                                          |                        |
| X  |    | X  | GET    | `/_api/query-cache/entries`                                  | RestQueryCacheHandler      | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | GET    | `/_api/query-cache/properties`                               | RestQueryCacheHandler      | -                                        | AUTHEN                              |                                          |                        |
| X  |    | X  | PUT    | `/_api/query-cache/properties`                               | RestQueryCacheHandler      | _system + canUseAdmin(AdminQueryCache)   | _system + AdminQueryCache           |                                          |                        |
| X  |    | X  | DELETE | `/_api/query-cache`                                          | RestQueryCacheHandler      | _system + canUseAdmin(AdminQueryCache)   | _system + AdminQueryCache           |                                          |                        |
| X  |    | X  | GET    | `/_api/query-plan-cache`                                     | RestQueryPlanCacheHandler  | canUseColl(...)                          | AUTHEN, details (5)                 |                                          |                        |
| X  |    | X  | DELETE | `/_api/query-plan-cache`                                     | RestQueryPlanCacheHandler  | canUseDb(Write)                          | AUTHEN, DB RW                       | needs an RBAC solution?                  | FIXME                  |
| X  |    |    | GET    | `/_api/replication/applier-state`                            | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     | Coord restriction new  |
| X  |    |    | DELETE | `/_api/replication/applier-state`                            | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     | Coord restriction new  |
| X  |    |    | GET    | `/_api/replication/applier-state-all`                        | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     | Coord restriction new  |
| X  |    |    | GET    | `/_api/replication/applier-config`                           | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     | Coord restriction new  |
| X  |    |    | PUT    | `/_api/replication/applier-config`                           | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     | Coord restriction new  |
| X  |    |    | PUT    | `/_api/replication/applier-start`                            | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | PUT    | `/_api/replication/applier-stop`                             | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | POST   | `/_api/replication/batch`                                    | RestReplicationHandler     | -                                        | SUPER                               | only actually DBServer (Coordinator forw)|                        |
| X  |    |    | PUT    | `/_api/replication/batch`                                    | RestReplicationHandler     | -                                        | SUPER                               | only actually DBServer (Coordinator forw)|                        |
| X  |    |    | DELETE | `/_api/replication/batch`                                    | RestReplicationHandler     | -                                        | SUPER                               | only actually DBServer (Coordinator forw)|                        |
| X  |    |    | GET    | `/_api/replication/clusterInventory`                         | RestReplicationHandler     | AdminClusterInfo or canUseColl(Read)     | AdminClusterInfo or COLL RO         | only Coordinator, lists only those       |                        |
| X  |    |    | GET    | `/_api/replication/dump`                                     | RestReplicationHandler     | canUseAdmin(Dump) or canUseColl(Read)    | AdminDump or COLL RO                | only actually DBServer (Coordinator forw)|                        |
| X  |    |    | POST   | `/_api/replication/holdReadLockCollection`                   | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator forbidden)    |                        |
| X  |    |    | DELETE | `/_api/replication/holdReadLockCollection`                   | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator forbidden)    |                        |
| X  |    |    | GET    | `/_api/replication/inventory`                                | RestReplicationHandler     | -                                        | SUPER                               | only actually DBServer (Coordinator forw)|                        |
| X  |    |    | GET    | `/_api/replication/keys/{id}`                                | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | POST   | `/_api/replication/keys`                                     | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | PUT    | `/_api/replication/keys/{id}`                                | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | DELETE | `/_api/replication/keys`                                     | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | DELETE | `/_api/replication/keys/{id}`                                | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | GET    | `/_api/replication/logger-first-tick`                        | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | GET    | `/_api/replication/logger-follow`                            | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | PUT    | `/_api/replication/logger-follow`                            | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | GET    | `/_api/replication/logger-state`                             | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (ClusterEngine not impl)   |                        |
| X  |    |    | GET    | `/_api/replication/logger-tick-ranges`                       | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | PUT    | `/_api/replication/make-follower`                            | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | PUT    | `/_api/replication/addFollower`                              | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordiantor forbidden)    |                        |
| X  |    |    | PUT    | `/_api/replication/removeFollower`                           | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordiantor forbidden)    |                        |
| X  |    |    | PUT    | `/_api/replication/restore-collection`                       | RestReplicationHandler     | AdminRestore or COLL RW (see (1))        | AdminRestore or COLL RW (1)         |                                          |                        |
| X  |    |    | PUT    | `/_api/replication/restore-data`                             | RestReplicationHandler     | Esc. to SUPER if AdminRestore,COLL RWDATA| AdminRestore or COLL RWDATA         |                                          |                        |
| X  |    |    | PUT    | `/_api/replication/restore-indexes`                          | RestReplicationHandler     | AdminRestore or canCreateIndex           | AdminRestore or INDEX CREATE        |                                          |                        |
| X  |    |    | PUT    | `/_api/replication/restore-view`                             | RestReplicationHandler     | AdminRestore or (canDropView&&canCreateV)| AdminRestore or VIEW RECREATE       |                                          |                        |
| X  |    |    | GET    | `/_api/replication/revisions/tree`                           | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | POST   | `/_api/replication/revisions/tree`                           | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | PUT    | `/_api/replication/revisions/tree`                           | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not i.) MAINT |                        |
| X  |    |    | GET    | `/_api/replication/revisions/treepending`                    | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not i.) MAINT |                        |
| X  |    |    | PUT    | `/_api/replication/revisions/documents`                      | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | PUT    | `/_api/replication/revisions/ranges`                         | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | GET    | `/_api/replication/server-id`                                | RestReplicationHandler     | -                                        | AUTHEN                              |                                          |                        |U
| X  |    |    | PUT    | `/_api/replication/set-the-leader`                           | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator forbidden)    |                        |
| X  |    |    | PUT    | `/_api/replication/sync`                                     | RestReplicationHandler     | -                                        | SUPER                               | only DBServer (Coordinator not impl)     |                        |
| X  |    |    | PUT    | `/_api/simple/all`                                           | RestSimpleQueryHandler     | canUseColl(Read) (via AQL/trx)           | AUTHEN, COLL RO                     |                                          |                        |
| X  |    |    | PUT    | `/_api/simple/all-keys`                                      | RestSimpleQueryHandler     | canUseColl(Read) (via AQL/trx)           | AUTHEN, COLL RO                     |                                          |                        |
| X  |    |    | PUT    | `/_api/simple/by-example`                                    | RestSimpleQueryHandler     | canUseColl(Read) (via AQL/trx)           | AUTHEN, COLL RO                     |                                          |                        |
| X  |    |    | PUT    | `/_api/simple/lookup-by-keys`                                | RestSimpleHandler          | canUseColl(Read) (via AQL/trx)           | AUTHEN, COLL RO                     |                                          |                        |
| X  |    |    | PUT    | `/_api/simple/remove-by-keys`                                | RestSimpleHandler          | canUseColl(WriteData) (via AQL/trx)      | AUTHEN, COLL RWDATA                 |                                          |                        |
| X  |    |    | GET    | `/_api/tasks`                                                | RestTasksHandler           | isSuperuser, check SELF                  | AUTHEN, list only SUPER or SELF     | (V8 required)                            |                        |
| X  |    |    | GET    | `/_api/tasks/{id}`                                           | RestTasksHandler           | isSuperuser, check SELF                  | AUTHEN, SUPER or SELF               | (V8 required)                            |                        |
| X  |    |    | POST   | `/_api/tasks`                                                | RestTasksHandler           | canUseDb(Write)                          | DB RW                               | (V8 required)                            | NO FIX, tasks gone soon|
| X  |    |    | PUT    | `/_api/tasks/{id}`                                           | RestTasksHandler           | canUseDb(Write)                          | DB RW                               | (V8 required)                            | NO FIX, tasks gone soon|
| X  |    |    | DELETE | `/_api/tasks/{id}`                                           | RestTasksHandler           | canUseDb(Write)                          | DB RW                               | (V8 required)                            | NO FIX, tasks gone soon|
| X  |    |    | GET    | `/_api/token/{user}`                                         | RestAccessTokenHandler     | canReadUser                              | canReadUser                         |                                          |                        |
| X  |    |    | POST   | `/_api/token/{user}`                                         | RestAccessTokenHandler     | canWriteUser                             | canWriteUser                        |                                          |                        |
| X  |    |    | DELETE | `/_api/token/{user}/{id}`                                    | RestAccessTokenHandler     | canWriteUser                             | canWriteUser                        |                                          |                        |
| X  |    | X  | GET    | `/_api/ttl/properties`                                       | RestTtlHandler             | _system                                  | AUTHEN, _system                     |                                          |                        |
| X  |    | X  | GET    | `/_api/ttl/statistics`                                       | RestTtlHandler             | _system                                  | AUTHEN, _system                     |                                          |                        |
| X  |    | X  | PUT    | `/_api/ttl/properties`                                       | RestTtlHandler             | _system                                  | AUTHEN, _system                     |                                          |                        |
| X  |    | X  | POST   | `/_api/upload`                                               | RestUploadHandler          | -                                        | AUTHEN                              | Gone in 4.0                              | NO FIX                 |
| X  |    |    | GET    | `/_api/user`                                                 | RestUsersHandler           | canReadUsers(list)                       | AUTHEN, see only canReadUser(u)     |                                          |                        |
| X  |    |    | POST   | `/_api/user`                                                 | RestUsersHandler           | canWriteUser(u)                          | canWriteUser(u)                     |                                          |                        |
| X  |    |    | POST   | `/_api/user/{user}`                                          | RestUsersHandler           | -                                        | AUTHEN, just check credentials      |                                          |                        |
| X  |    |    | GET    | `/_api/user/{user}`                                          | RestUsersHandler           | canReadUser(u)                           | canReadUser(u)                      |                                          |                        |
| X  |    |    | GET    | `/_api/user/{user}/config`                                   | RestUsersHandler           | canReadUser(u)                           | canReadUser(u)                      |                                          |                        |
| X  |    |    | GET    | `/_api/user/{user}/database`                                 | RestUsersHandler           | canReadUser(u)                           | canReadUser(u)                      |                                          |                        |
| X  |    |    | GET    | `/_api/user/{user}/database/{db}`                            | RestUsersHandler           | canReadUser(u)                           | canReadUser(u)                      |                                          |                        |
| X  |    |    | GET    | `/_api/user/{user}/database/{db}/{coll}`                     | RestUsersHandler           | canReadUser(u)                           | canReadUser(u)                      |                                          |                        |
| X  |    |    | PUT    | `/_api/user/{user}`                                          | RestUsersHandler           | canWriteUser(u)                          | canWriteUser(u)                     |                                          |                        |
| X  |    |    | PUT    | `/_api/user/{user}/database/{db}`                            | RestUsersHandler           | canWriteUser(u)                          | canWriteUser(u)                     |                                          |                        |
| X  |    |    | PUT    | `/_api/user/{user}/database/{db}/{coll}`                     | RestUsersHandler           | canWriteUser(u)                          | canWriteUser(u)                     |                                          |                        |
| X  |    |    | PUT    | `/_api/user/{user}/config/{key}`                             | RestUsersHandler           | canWriteUser(u)                          | canWriteUser(u)                     |                                          |                        |
| X  |    |    | PATCH  | `/_api/user/{user}`                                          | RestUsersHandler           | canWriteUser(u)                          | canWriteUser(u)                     |                                          |                        |
| X  |    |    | DELETE | `/_api/user/{user}`                                          | RestUsersHandler           | canWriteUser(u)                          | canWriteUser(u)                     |                                          |                        |
| X  |    |    | DELETE | `/_api/user/{user}/config/{key}`                             | RestUsersHandler           | canWriteUser(u)                          | canWriteUser(u)                     |                                          |                        |
| X  |    |    | DELETE | `/_api/user/{user}/database/{db}`                            | RestUsersHandler           | canWriteUser(u)                          | canWriteUser(u)                     |                                          |                        |
| X  |    |    | DELETE | `/_api/user/{user}/database/{db}/{coll}`                     | RestUsersHandler           | canWriteUser(u)                          | canWriteUser(u)                     |                                          |                        |
| X  |    | X  | GET    | `/_api/version`                                              | RestVersionHandler         | canUseHard(MonitoringInt) for details    | AUTHEN, details (2)                 |                                          |                        |
| X  |    |    | GET    | `/_api/view`                                                 | RestViewHandler            | only see those with canSeeView           | canSeeView                          |                                          |                        |
| X  |    |    | POST   | `/_api/view`                                                 | RestViewHandler            | canCreateView                            | canCreateView                       |                                          |                        |
| X  |    |    | DELETE | `/_api/view/{name}`                                          | RestViewHandler            | canDropView                              | canDropView                         |                                          |                        |
| X  |    |    | GET    | `/_api/view/{name}`                                          | RestViewHandler            | canUseView(RO)                           | canUseView(RO)                      |                                          |                        |
| X  |    |    | GET    | `/_api/view/{name}/properties`                               | RestViewHandler            | canUseView(RO)                           | canUseView(RO)                      |                                          |                        |
| X  |    |    | PATCH  | `/_api/view/{name}/properties`                               | RestViewHandler            | canUseView(modify)                       | canUseView(modify)                  |                                          |                        |
| X  |    |    | PATCH  | `/_api/view/{name}/rename`                                   | RestViewHandler            | canRenameView                            | canRenameView                       |                                          |                        |
| X  |    |    | PUT    | `/_api/view/{name}/properties`                               | RestViewHandler            | canUseView(modify)                       | canUseView(modify)                  |                                          |                        |
| X  |    |    | PUT    | `/_api/view/{name}/rename`                                   | RestViewHandler            | canRenameView                            | canRenameView                       |                                          |                        |
| X  |    | X  | GET    | `/_api/wal/lastTick`                                         | RestWalAccessHandler       | -                                        | SUPER                               | only DBServer/Single, not on coord       |                        |
| X  |    | X  | GET    | `/_api/wal/open-transactions`                                | RestWalAccessHandler       | -                                        | SUPER                               | only DBServer/Single, not on coord       |                        |
| X  |    | X  | GET    | `/_api/wal/range`                                            | RestWalAccessHandler       | -                                        | SUPER                               | only DBServer/Single, not on coord       |                        |
| X  |    | X  | GET    | `/_api/wal/tail`                                             | RestWalAccessHandler       | -                                        | SUPER                               | only DBServer/Single, not on coord       |                        |
| X  |    | X  | PUT    | `/_api/wal/tail`                                             | RestWalAccessHandler       | -                                        | SUPER                               | only DBServer/Single, not on coord       |                        |
| X  |    | X  | DELETE | `/_api/wal/tail`                                             | RestWalAccessHandler       | -                                        | SUPER                               | only DBServer/Single, not on coord       |                        |
| X  |    |    | GET    | `/_api/transaction`                                          | RestTransactionHandler     | (same user and same db) or isSuperuser   | AUTHEN, only see same user and db   | Coord/Single for users, DBServer internal|                        |
| X  |    |    | GET    | `/_api/transaction/{id}`                                     | RestTransactionHandler     | -                                        | AUTHEN                              | Coord/Single for users, DBServer internal|                        |
| X  |    |    | GET    | `/_api/transaction/history`                                  | RestTransactionHandler     | isSuperuser                              | SUPER                               | only maintainer                          |                        |
| X  |    |    | POST   | `/_api/transaction`                                          | RestTransactionHandler     | -                                        | AUTHEN                              | Coord/Single for users, DBServer internal|                        |
| X  |    |    | POST   | `/_api/transaction/begin`                                    | RestTransactionHandler     | -                                        | AUTHEN                              | Coord/Single for users, DBServer internal|                        |
| X  |    |    | PUT    | `/_api/transaction/{id}`                                     | RestTransactionHandler     | same user or isSuperuser                 | AUTHEN, same user or SUPER          | Coord/Single for users, DBServer internal|                        |
| X  |    |    | DELETE | `/_api/transaction/{id}`                                     | RestTransactionHandler     | same user or isSuperuser                 | AUTHEN                              | Coord/Single for users, DBServer internal|                        |
| X  |    |    | DELETE | `/_api/transaction/write`                                    | RestTransactionHandler     | only same user or isSuperuser            | AUTHEN, only same user or SUPER     | Coord/Single for users, DBServer internal|                        |
| X  |    |    | DELETE | `/_api/transaction/history`                                  | RestTransactionHandler     | isSuperuser                              | SUPER                               | only maintainer                          |                        |
| X  |    |    | PUT    | `/_internal/traverser/{option}/{engine-id}`                  | InternalRestTraverserHand. | -                                        | SUPER                               | onle DBServer                            |                        |
| X  |    |    | DELETE | `/_internal/traverser/{engine-id}`                           | InternalRestTraverserHand. | -                                        | SUPER                               | onle DBServer                            |                        |
| X  |    | X  | GET    | `/openapi.json`                                              | RestOpenApiHandler         | -                                        | OPEN                                |                                          |                        |
|----|----|----|--------|--------------------------------------------------------------|----------------------------|------------------------------------------|-------------------------------------|------------------------------------------|------------------------|
| X  |    |    | JS     | `JS_CreateQueue`                                             | v8-dispatcher.cpp          | canUseDatabase(RW) (_system RW for runAs)| DB RW (_system RW for runAs)        |                                          |                        |
| X  |    |    | JS     | `TRI_RequestCppToV8`                                         | v8-actions.cpp             | isSuperuser to set isAdminUser flag      | SUPER                               |                                          |                        |
| X  |    |    | JS     | `JS_GetReplicatedLog`                                        | v8-replicated-logs.cpp     | canUseAdmin(ReadReplicatedLog)           | AdminReadReplicatedLog              |                                          |                        |
| X  |    |    | JS     | `JS_CreateReplicatedLog`                                     | v8-replicated-logs.cpp     | canUseAdmin(WriteReplicatedLog)          | AdminWriteReplicatedLog             |                                          |                        |
| X  |    |    | JS     | `JS_Id`                                                      | v8-replicated-logs.cpp     | canUseAdmin(ReadReplicatedLog)           | AdminReadReplicatedLog              |                                          |                        |
| X  |    |    | JS     | `JS_Drop`                                                    | v8-replicated-logs.cpp     | canUseAdmin(WriteReplicatedLog)          | AdminWriteReplicatedLog             |                                          |                        |
| X  |    |    | JS     | `JS_Insert`                                                  | v8-replicated-logs.cpp     | canUseAdmin(WriteReplicatedLog)          | AdminWriteReplicatedLog             |                                          |                        |
| X  |    |    | JS     | `JS_Ping`                                                    | v8-replicated-logs.cpp     | canUseAdmin(WriteReplicatedLog)          | AdminWriteReplicatedLog             |                                          |                        |
| X  |    |    | JS     | `JS_MultiInsert`                                             | v8-replicated-logs.cpp     | canUseAdmin(WriteReplicatedLog)          | AdminWriteReplicatedLog             |                                          |                        |
| X  |    |    | JS     | `JS_Status`                                                  | v8-replicated-logs.cpp     | canUseAdmin(ReadReplicatedLog)           | AdminReadReplicatedLog              |                                          |                        |
| X  |    |    | JS     | `JS_GlobalStatus`                                            | v8-replicated-logs.cpp     | canUseAdmin(ReadReplicatedLog)           | AdminReadReplicatedLog              |                                          |                        |
| X  |    |    | JS     | `JS_Head`                                                    | v8-replicated-logs.cpp     | canUseAdmin(ReadReplicatedLog)           | AdminReadReplicatedLog              |                                          |                        |
| X  |    |    | JS     | `JS_Tail`                                                    | v8-replicated-logs.cpp     | canUseAdmin(ReadReplicatedLog)           | AdminReadReplicatedLog              |                                          |                        |
| X  |    |    | JS     | `JS_Slice`                                                   | v8-replicated-logs.cpp     | canUseAdmin(ReadReplicatedLog)           | AdminReadReplicatedLog              |                                          |                        |
| X  |    |    | JS     | `JS_Poll`                                                    | v8-replicated-logs.cpp     | canUseAdmin(ReadReplicatedLog)           | AdminReadReplicatedLog              |                                          |                        |
| X  |    |    | JS     | `JS_At`                                                      | v8-replicated-logs.cpp     | canUseAdmin(ReadReplicatedLog)           | AdminReadReplicatedLog              |                                          |                        |
| X  |    |    | JS     | `JS_Release`                                                 | v8-replicated-logs.cpp     | canUseAdmin(WriteReplicatedLog)          | AdminWriteReplicatedLog             |                                          |                        |
| X  |    |    | JS     | `JS_Compact`                                                 | v8-replicated-logs.cpp     | canUseAdmin(WriteReplicatedLog)          | AdminWriteReplicatedLog             |                                          |                        |
| X  |    |    | JS     | `JS_RemoveUser`                                              | v8-users.cpp               | canWriteUser()                           | canWriteUser                        |                                          |                        |
| X  |    |    | JS     | `JS_ReloadAuthData`                                          | v8-users.cpp               | canUseAdmin(AuthReload)                  | AdminAuthReload                     |                                          |                        |
| X  |    |    | JS     | `JS_GrantDatabase`                                           | v8-users.cpp               | canWriteUser()                           | canWriteUser                        |                                          |                        |
| X  |    |    | JS     | `JS_RevokeDatabase`                                          | v8-users.cpp               | canWriteUser()                           | canWriteUser                        |                                          |                        |
| X  |    |    | JS     | `JS_GrantCollection`                                         | v8-users.cpp               | canWriteUser()                           | canWriteUser                        |                                          |                        |
| X  |    |    | JS     | `JS_RevokeCollection`                                        | v8-users.cpp               | canWriteUser()                           | canWriteUser                        |                                          |                        |
| X  |    |    | JS     | `StoreUser`                                                  | v8-users.cpp               | canWriteUser()                           | canWriteUser                        |                                          |                        |
| X  |    |    | JS     | `JS_UpdateUser`                                              | v8-users.cpp               | canWriteUser()                           | canWriteUser                        |                                          |                        |
| X  |    |    | JS     | `JS_GetUser`                                                 | v8-users.cpp               | canReadUser()                            | canReadUser                         |                                          |                        |
| X  |    |    | JS     | `JS_UpdateConfigData`                                        | v8-users.cpp               | canWriteUser()                           | canWriteUser                        |                                          |                        |
| X  |    |    | JS     | `JS_GetConfigData`                                           | v8-users.cpp               | canReaduser()                            | canReadUser                         |                                          |                        |
| X  |    |    | CPP    | `Databases::grantCurrentUser` (creation of database)         | Databases.Cpp              | canWriteUser()                           | canWriteUser                        |                                          |                        |
| X  |    |    | JS     | `JS_GetGraphKeys`                                            | v8-general-graph.cpp       | canSeeGraph                              | canSeeGraph, list only visible      |                                          |                        |


(1) For `arangorestore`, if `--overwrite=true` or the collection needs to be created, then we need canCreateColl,
    if `--overwrite=false` and the collection is already there, we only need COLL RWDATA

(2) For `/_api/version`, details can only be queried with `AdminMonitoringInternal`, if `--server.harden=true`

(3) For `/_api/collection`, RO for database is needed, then all collections with canSeeCollection are listed

(4) For `/_api/database`, all databases with canSeeDatabase() are listed

(5) For `GET /_api/query-plan-cache` only those entries are returned, for which the user has read access to all occurring collections

(6) For graphs, the regulate authorization as follows:
     - to create, we check `canCreateGraph`, which gets the list of collections
       which need to be created and a list of collections which we need to be
       able to read. Without RBAC, this needs write access to the db
       (to modify _graphs) and this implies being able to create the collections.
       With RBAC enabled, this needs `db:CreateGraph` and `db:CreateCollection`
       for those collections needed and `db:ReadCollection` for those which we
       need to be able to read.
     - to drop, we check `canDropGraph`, which gets the list of collections
       which need to be dropped. Without RBAC, this needs write access to the db
       (to modify _graphs) and this implies being able to drop the collections.
       With RBAC enabled, this needs `db:DropGraph` and `db:DropCollection`
       for those collections needed.
     - to list the graph, we check `canSeeGraph`, which checks `db:ReadGraph`
       with RBAC and read access to the db without RBAC (to read `_graphs`).
     - to see properties and use a graph, we check `canUseGraph(RO)`, which checks
       `db:ReadGraph` with RBAC and read access to the db without RBAC
       (to read `_graphs`).
     - modify a graph definition, we check `canUseGraph(RWMeta)`, which checks
       `db:WriteGraph` with RBAC and write access to the db without RBAC
       (to modify `_graphs`).
            
Rules:
 - internal use of system collections allowed without check
 - read access to system collections can be regulated by RBAC if switched on
 - write access (with normal APIs) to system collections is superuser only
