<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Route API

_ArangoRoute_ instances provide access for arbitrary HTTP requests.
This allows easy access to Foxx services and other HTTP APIs not covered
by the driver itself.

## ArangoRoute.route

`ArangoRoute.route(String... path) : ArangoRoute`

Returns a new _ArangoRoute_ instance for the given path (relative to the
current route) that can be used to perform arbitrary requests.

**Arguments**

- **path**: `String...`

  The relative URL of the route

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

ArangoRoute route = db.route("my-foxx-service");
ArangoRoute users = route.route("users");
// equivalent to db.route("my-foxx-service/users")
// or db.route("my-foxx-service", "users")
```

## ArangoRoute.withHeader

`ArangoRoute.withHeader(String key, Object value) : ArangoRoute`

Header that should be sent with each request to the route.

**Arguments**

- **key**: `String`

  Header key

- **value**: `Object`

  Header value (the _toString()_ method will be called for the value}

## ArangoRoute.withQueryParam

`ArangoRoute.withQueryParam(String key, Object value) : ArangoRoute`

Query parameter that should be sent with each request to the route.

**Arguments**

- **key**: `String`

  Query parameter key

- **value**: `Object`

  Query parameter value (the _toString()_ method will be called for the value}

## ArangoRoute.withBody

`ArangoRoute.withBody(Object body) : ArangoRoute`

The response body. The body will be serialized to _VPackSlice_.

**Arguments**

- **body**: `Object`

  The request body

## ArangoRoute.delete

`ArangoRoute.delete() : Response`

Performs a DELETE request to the given URL and returns the server response.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

ArangoRoute route = db.route("my-foxx-service");
ArangoRoute route = route.delete()
// response.getBody() is the response body of calling
// DELETE _db/_system/my-foxx-service

// -- or --

ArangoRoute route = route.route("users/admin").delete()
// response.getBody() is the response body of calling
// DELETE _db/_system/my-foxx-service/users/admin

// -- or --

ArangoRoute route = route.route("users/admin").withQueryParam("permanent", true).delete()
// response.getBody() is the response body of calling
// DELETE _db/_system/my-foxx-service/users/admin?permanent=true
```

## ArangoRoute.get

`ArangoRoute.get() : Response`

Performs a GET request to the given URL and returns the server response.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

ArangoRoute route = db.route("my-foxx-service");
Response response = route.get();
// response.getBody() is the response body of calling
// GET _db/_system/my-foxx-service

// -- or --

Response response = route.route("users").get();
// response.getBody() is the response body of calling
// GET _db/_system/my-foxx-service/users

// -- or --

Response response = route.route("users").withQueryParam("group", "admin").get();
// response.getBody() is the response body of calling
// GET _db/_system/my-foxx-service/users?group=admin
```

## ArangoRoute.head

`ArangoRoute.head() : Response`

Performs a HEAD request to the given URL and returns the server response.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

ArangoRoute route = db.route("my-foxx-service");
ArangoRoute route = route.head();
// response is the response object for
// HEAD _db/_system/my-foxx-service
```

## ArangoRoute.patch

`ArangoRoute.patch() : Response`

Performs a PATCH request to the given URL and returns the server response.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

ArangoRoute route = db.route("my-foxx-service");
ArangoRoute route = route.patch();
// response.getBody() is the response body of calling
// PATCH _db/_system/my-foxx-service

// -- or --

ArangoRoute route = route.route("users/admin").patch();
// response.getBody() is the response body of calling
// PATCH _db/_system/my-foxx-service/users

// -- or --

VPackSlice body = arango.util().serialize("{ password: 'hunter2' }");
ArangoRoute route = route.route("users/admin").withBody(body).patch();
// response.getBody() is the response body of calling
// PATCH _db/_system/my-foxx-service/users/admin
// with JSON request body {"password": "hunter2"}

// -- or --

VPackSlice body = arango.util().serialize("{ password: 'hunter2' }");
ArangoRoute route = route.route("users/admin")
  .withBody(body).withQueryParam("admin", true).patch();
// response.getBody() is the response body of calling
// PATCH _db/_system/my-foxx-service/users/admin?admin=true
// with JSON request body {"password": "hunter2"}
```

## ArangoRoute.post

`ArangoRoute.post() : Response`

Performs a POST request to the given URL and returns the server response.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

ArangoRoute route = db.route("my-foxx-service");
ArangoRoute route = route.post()
// response.getBody() is the response body of calling
// POST _db/_system/my-foxx-service

// -- or --

ArangoRoute route = route.route("users").post()
// response.getBody() is the response body of calling
// POST _db/_system/my-foxx-service/users

// -- or --

VPackSlice body = arango.util().serialize("{ password: 'hunter2' }");
ArangoRoute route = route.route("users").withBody(body).post();
// response.getBody() is the response body of calling
// POST _db/_system/my-foxx-service/users
// with JSON request body {"username": "admin", "password": "hunter2"}

// -- or --

VPackSlice body = arango.util().serialize("{ password: 'hunter2' }");
ArangoRoute route = route.route("users")
  .withBody(body).withQueryParam("admin", true).post();
// response.getBody() is the response body of calling
// POST _db/_system/my-foxx-service/users?admin=true
// with JSON request body {"username": "admin", "password": "hunter2"}
```

## ArangoRoute.put

`ArangoRoute.put() : Response`

Performs a PUT request to the given URL and returns the server response.

**Examples**

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

ArangoRoute route = db.route("my-foxx-service");
ArangoRoute route = route.put();
// response.getBody() is the response body of calling
// PUT _db/_system/my-foxx-service

// -- or --

ArangoRoute route = route.route("users/admin").put();
// response.getBody() is the response body of calling
// PUT _db/_system/my-foxx-service/users

// -- or --

VPackSlice body = arango.util().serialize("{ password: 'hunter2' }");
ArangoRoute route = route.route("users/admin").withBody(body).put();
// response.getBody() is the response body of calling
// PUT _db/_system/my-foxx-service/users/admin
// with JSON request body {"username": "admin", "password": "hunter2"}

// -- or --

VPackSlice body = arango.util().serialize("{ password: 'hunter2' }");
ArangoRoute route = route.route("users/admin")
  .withBody(body).withQueryParam("admin", true).put();
// response.getBody() is the response body of calling
// PUT _db/_system/my-foxx-service/users/admin?admin=true
// with JSON request body {"username": "admin", "password": "hunter2"}
```
