<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Arbitrary HTTP routes

## ArangoDatabase.route

`ArangoDatabase.route(String... path) : ArangoRoute`

Returns a new _ArangoRoute_ instance for the given path
(relative to the database) that can be used to perform arbitrary requests.

**Arguments**

- **path**: `String...`

  The database-relative URL of the route

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");
ArangoRoute myFoxxService = db.route("my-foxx-service");

VPackSlice body = arango.util().serialize("{'username': 'admin', 'password': 'hunter2'");
Response response = myFoxxService.route("users").withBody(body).post();
// response.getBody() is the result of
// POST /_db/myDB/my-foxx-service/users
// with VelocyPack request body '{"username": "admin", "password": "hunter2"}'
```
