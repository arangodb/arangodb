

@brief return the database type
`db._isSystem()`

Returns whether the currently used database is the *_system* database.
The system database has some special privileges and properties, for example,
database management operations such as create or drop can only be executed
from within this database. Additionally, the *_system* database itself
cannot be dropped.

