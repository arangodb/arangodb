DatabaseName {#GlossaryDatabaseName}
====================================

@GE{Database Name}: ArangoDB can handle multiple databases in parallel
in the same server instance. When multiple databases are used, each
database must be given a unique name. This name is used to uniquely
identify a database. The default database in ArangoDB is named `_system`.
The `_system` database will always be there, and cannot be dropped.
When ArangoDB is accessed and no database name is explicitly specified,
ArangoDB will automatically assume the default database `_system` is
accessed. This is done for downwards-compatibility reasons.

The database name is a string consisting of only lower-case letters, digits 
and the `_` (underscore) and `-` (dash) characters. User-defined database 
names must always start with a letter.
Please refer to @ref NamingConventions for more information on valid 
database names.
