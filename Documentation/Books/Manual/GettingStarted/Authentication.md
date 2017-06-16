Authentication
==============

ArangoDB allows to restrict access to databases to certain users.  All
users of the system database are considered administrators. During
installation a default user *root* is created, which has access to
all databases.

You should create a database for your application together with a
user that has access rights to this database. See
[Managing Users](../Administration/ManagingUsers/README.md).

Use the *arangosh* to create a new database and user.

```
arangosh> db._createDatabase("example");
arangosh> var users = require("@arangodb/users");
arangosh> users.save("root@example", "password");
arangosh> users.grantDatabase("root@example", "example");
```

You can now connect to the new database using the user
*root@example*.

```
shell> arangosh --server.username "root@example" --server.database example
```


