!CHAPTER Handling Databases

This is an introduction to managing databases in ArangoDB from within 
JavaScript. 

When you have an established connection to ArangoDB, the current
database can be changed explicitly using the *db._useDatabase()*
method. This will switch to the specified database (provided it
exists and the user can connect to it). From this point on, any
following action in the same shell or connection will use the
specified database, unless otherwise specified.

*Note*: If the database is changed, client drivers need to store the 
current database name on their side, too. This is because connections
in ArangoDB do not contain any state information. All state information
is contained in the HTTP request/response data.

To connect to a specific database after arangosh has started use the command
described above. It is also possible to specify a database name when invoking
arangosh. For this purpose, use the command-line parameter *--server.database*,
e.g.

    > arangosh --server.database test 

Please note that commands, actions, scripts or AQL queries should never
access multiple databases, even if they exist. The only intended and
supported way in ArangoDB is to use one database at a time for a command,
an action, a script or a query. Operations started in one database must
not switch the database later and continue operating in another.

