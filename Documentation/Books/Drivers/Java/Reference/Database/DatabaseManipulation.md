<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Manipulation databases

These functions implement the
[HTTP API for manipulating databases](../../../..//HTTP/Database/index.html).

## ArangoDB.createDatabase

`ArangoDB.createDatabase(String name) : Boolean`

Creates a new database with the given name.

**Arguments**

- **name**: `String`

  Name of the database to create

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
arango.createDatabase("myDB");
```

## ArangoDatabase.create()

`ArangoDatabase.create() : Boolean`

Creates the database.

Alternative for [ArangoDB.createDatabase](#arangodbcreatedatabase).

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
db.create();
```

## ArangoDatabase.exists()

`ArangoDatabase.exists() : boolean`

Checks whether the database exists

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
boolean exists = db.exists();
```

## ArangoDatabase.getInfo

`ArangoDatabase.getInfo() : DatabaseEntity`

Retrieves information about the current database

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
DatabaseEntity info = db.getInfo();
```

## ArangoDB.getDatabases

`ArangoDB.getDatabases() : Collection<String>`

Retrieves a list of all existing databases

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
Collection<String> names = arango.getDatabases();
```

## ArangoDatabase.drop

`ArangoDatabase.drop() : Boolean`

Deletes the database from the server.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
db.drop();
```
