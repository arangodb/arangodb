Notes about Databases
=====================

Please keep in mind that each database contains its own system collections,
which need to be set up when a database is created. This will make the creation
of a database take a while. 

Replication is either configured on a
[per-database level](../../Administration/MasterSlave/DatabaseSetup.md)
or on [server level](../../Administration/MasterSlave/ServerLevelSetup.md).
In a per-database setup, any replication logging or applying for a new database
must be configured explicitly after a new database has been created, whereas all
databases are automatically replicated in case of the server-level setup using the global replication applier.

Foxx applications
are also available only in the context of the database they have been installed 
in. A new database will only provide access to the system applications shipped
with ArangoDB (that is the web interface at the moment) and no other Foxx
applications until they are explicitly installed for the particular database.
