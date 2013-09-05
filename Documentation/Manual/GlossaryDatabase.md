Database {#GlossaryDatabase}
============================

@GE{Database}: ArangoDB can handle multiple databases in the same server
instance. Databases can be used to logically separate data. An ArangoDB
database consists of collections and dedicated database-specific worker processes.
There will always be at least one database in ArangoDB. This is the default
database, named `_system`. Users can create additional databases and give them
unique names to access them later.
Any databases but the default database can be dropped.
