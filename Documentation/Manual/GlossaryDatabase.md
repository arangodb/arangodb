Database {#GlossaryDatabase}
============================

@GE{Database}: ArangoDB can handle multiple databases in the same server
instance. Databases can be used to logically group and separate data. An ArangoDB
database consists of collections and dedicated database-specific worker processes.

A database contains its own collections (which cannot be accessed from other databases),
Foxx applications, and replication loggers and appliers. Each ArangoDB database
contains its own system collections (e.g. `_users`, `_replication`, ...).

There will always be at least one database in ArangoDB. This is the default
database, named `_system`. This database cannot be dropped, and provides special
operations for creating, dropping, and enumerating databases.
Users can create additional databases and give them unique names to access them later.
Database management operations cannot be initiated from out of user-defined databases.

When ArangoDB is accessed via its HTTP REST API, the database name is read from the
first part of the request URI path (e.g. `/_db/_system/...`). If the request URI does 
not contain a database name, the database name is automatically determined by the 
algorithm described in @ref HttpDatabaseMapping.
