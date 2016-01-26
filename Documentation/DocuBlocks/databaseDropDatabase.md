

@brief drop an existing database
`db._dropDatabase(name)`

Drops the database specified by *name*. The database specified by
*name* must exist.

**Note**: Dropping databases is only possible from within the *_system*
database. The *_system* database itself cannot be dropped.

Databases are dropped asynchronously, and will be physically removed if
all clients have disconnected and references have been garbage-collected.

