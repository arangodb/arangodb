DatabaseName {#GlossaryDatabaseName}
====================================

@GE{Database Name}: A single ArangoDB instance can handle multiple databases 
in parallel. When multiple databases are used, each database must be given a 
unique name. This name is used to uniquely identify a database. The default 
database in ArangoDB is named `_system`.

The database name is a string consisting of only letters, digits 
and the `_` (underscore) and `-` (dash) characters. User-defined database 
names must always start with a letter. Database names is case-sensitive.
