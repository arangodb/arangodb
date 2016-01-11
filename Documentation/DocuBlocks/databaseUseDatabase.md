

@brief change the current database
`db._useDatabase(name)`

Changes the current database to the database specified by *name*. Note
that the database specified by *name* must already exist.

Changing the database might be disallowed in some contexts, for example
server-side actions (including Foxx).

When performing this command from arangosh, the current credentials (username
and password) will be re-used. These credentials might not be valid to
connect to the database specified by *name*. Additionally, the database
only be accessed from certain endpoints only. In this case, switching the
database might not work, and the connection / session should be closed and
restarted with different username and password credentials and/or
endpoint data.

