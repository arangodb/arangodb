<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Accessing views

These functions implement the
[HTTP API for accessing view](../../../..//HTTP/Views/Getting.html).

## ArangoDatabase.view

`ArangoDatabase.view(String name) : ArangoView`

Returns a _ArangoView_ instance for the given view name.

**Arguments**

- **name**: `String`

  Name of the view

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoView view = db.view("myView");
```

## ArangoDatabase.arangoSearch

`ArangoDatabase.arangoSearch(String name) : ArangoSearch`

Returns a _ArangoSearch_ instance for the given ArangoSearch view name.

**Arguments**

- **name**: `String`

  Name of the view

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoSearch view = db.arangoSearch("myArangoSearchView");
```

## ArangoDatabase.getViews

`ArangoDatabase.getViews() : Collection<ViewEntity>`

Fetches all views from the database and returns an list of collection descriptions.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
Collection<ViewEntity> infos = db.getViews();
```
