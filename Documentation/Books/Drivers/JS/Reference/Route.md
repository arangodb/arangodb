<!-- don't edit here, its from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# Route API

_Route_ instances provide access for arbitrary HTTP requests. This allows easy
access to Foxx services and other HTTP APIs not covered by the driver itself.

## route.route

`route.route([path], [headers]): Route`

Returns a new _Route_ instance for the given path (relative to the current
route) that can be used to perform arbitrary HTTP requests.

**Arguments**

* **path**: `string` (optional)

  The relative URL of the route.

* **headers**: `Object` (optional)

  Default headers that should be sent with each request to the route.

If _path_ is missing, the route will refer to the base URL of the database.

**Examples**

```js
const db = new Database();
const route = db.route("my-foxx-service");
const users = route.route("users");
// equivalent to db.route('my-foxx-service/users')
```

## route.get

`async route.get([path,] [qs]): Response`

Performs a GET request to the given URL and returns the server response.

**Arguments**

* **path**: `string` (optional)

  The route-relative URL for the request. If omitted, the request will be made
  to the base URL of the route.

* **qs**: `string` (optional)

  The query string for the request. If _qs_ is an object, it will be translated
  to a query string.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.get();
// response.body is the response body of calling
// GET _db/_system/my-foxx-service

// -- or --

const response = await route.get('users');
// response.body is the response body of calling
// GET _db/_system/my-foxx-service/users

// -- or --

const response = await route.get('users', {group: 'admin'});
// response.body is the response body of calling
// GET _db/_system/my-foxx-service/users?group=admin
```

## route.post

`async route.post([path,] [body, [qs]]): Response`

Performs a POST request to the given URL and returns the server response.

**Arguments**

* **path**: `string` (optional)

  The route-relative URL for the request. If omitted, the request will be made
  to the base URL of the route.

* **body**: `string` (optional)

  The response body. If _body_ is an object, it will be encoded as JSON.

* **qs**: `string` (optional)

  The query string for the request. If _qs_ is an object, it will be translated
  to a query string.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.post()
// response.body is the response body of calling
// POST _db/_system/my-foxx-service

// -- or --

const response = await route.post('users')
// response.body is the response body of calling
// POST _db/_system/my-foxx-service/users

// -- or --

const response = await route.post('users', {
  username: 'admin',
  password: 'hunter2'
});
// response.body is the response body of calling
// POST _db/_system/my-foxx-service/users
// with JSON request body {"username": "admin", "password": "hunter2"}

// -- or --

const response = await route.post('users', {
  username: 'admin',
  password: 'hunter2'
}, {admin: true});
// response.body is the response body of calling
// POST _db/_system/my-foxx-service/users?admin=true
// with JSON request body {"username": "admin", "password": "hunter2"}
```

## route.put

`async route.put([path,] [body, [qs]]): Response`

Performs a PUT request to the given URL and returns the server response.

**Arguments**

* **path**: `string` (optional)

  The route-relative URL for the request. If omitted, the request will be made
  to the base URL of the route.

* **body**: `string` (optional)

  The response body. If _body_ is an object, it will be encoded as JSON.

* **qs**: `string` (optional)

  The query string for the request. If _qs_ is an object, it will be translated
  to a query string.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.put();
// response.body is the response body of calling
// PUT _db/_system/my-foxx-service

// -- or --

const response = await route.put('users/admin');
// response.body is the response body of calling
// PUT _db/_system/my-foxx-service/users

// -- or --

const response = await route.put('users/admin', {
  username: 'admin',
  password: 'hunter2'
});
// response.body is the response body of calling
// PUT _db/_system/my-foxx-service/users/admin
// with JSON request body {"username": "admin", "password": "hunter2"}

// -- or --

const response = await route.put('users/admin', {
  username: 'admin',
  password: 'hunter2'
}, {admin: true});
// response.body is the response body of calling
// PUT _db/_system/my-foxx-service/users/admin?admin=true
// with JSON request body {"username": "admin", "password": "hunter2"}
```

## route.patch

`async route.patch([path,] [body, [qs]]): Response`

Performs a PATCH request to the given URL and returns the server response.

**Arguments**

* **path**: `string` (optional)

  The route-relative URL for the request. If omitted, the request will be made
  to the base URL of the route.

* **body**: `string` (optional)

  The response body. If _body_ is an object, it will be encoded as JSON.

* **qs**: `string` (optional)

  The query string for the request. If _qs_ is an object, it will be translated
  to a query string.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.patch();
// response.body is the response body of calling
// PATCH _db/_system/my-foxx-service

// -- or --

const response = await route.patch('users/admin');
// response.body is the response body of calling
// PATCH _db/_system/my-foxx-service/users

// -- or --

const response = await route.patch('users/admin', {
  password: 'hunter2'
});
// response.body is the response body of calling
// PATCH _db/_system/my-foxx-service/users/admin
// with JSON request body {"password": "hunter2"}

// -- or --

const response = await route.patch('users/admin', {
  password: 'hunter2'
}, {admin: true});
// response.body is the response body of calling
// PATCH _db/_system/my-foxx-service/users/admin?admin=true
// with JSON request body {"password": "hunter2"}
```

## route.delete

`async route.delete([path,] [qs]): Response`

Performs a DELETE request to the given URL and returns the server response.

**Arguments**

* **path**: `string` (optional)

  The route-relative URL for the request. If omitted, the request will be made
  to the base URL of the route.

* **qs**: `string` (optional)

  The query string for the request. If _qs_ is an object, it will be translated
  to a query string.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.delete()
// response.body is the response body of calling
// DELETE _db/_system/my-foxx-service

// -- or --

const response = await route.delete('users/admin')
// response.body is the response body of calling
// DELETE _db/_system/my-foxx-service/users/admin

// -- or --

const response = await route.delete('users/admin', {permanent: true})
// response.body is the response body of calling
// DELETE _db/_system/my-foxx-service/users/admin?permanent=true
```

## route.head

`async route.head([path,] [qs]): Response`

Performs a HEAD request to the given URL and returns the server response.

**Arguments**

* **path**: `string` (optional)

  The route-relative URL for the request. If omitted, the request will be made
  to the base URL of the route.

* **qs**: `string` (optional)

  The query string for the request. If _qs_ is an object, it will be translated
  to a query string.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.head();
// response is the response object for
// HEAD _db/_system/my-foxx-service
```

## route.request

`async route.request([opts]): Response`

Performs an arbitrary request to the given URL and returns the server response.

**Arguments**

* **opts**: `Object` (optional)

  An object with any of the following properties:

  * **path**: `string` (optional)

    The route-relative URL for the request. If omitted, the request will be made
    to the base URL of the route.

  * **absolutePath**: `boolean` (Default: `false`)

    Whether the _path_ is relative to the connection's base URL instead of the
    route.

  * **body**: `string` (optional)

    The response body. If _body_ is an object, it will be encoded as JSON.

  * **qs**: `string` (optional)

    The query string for the request. If _qs_ is an object, it will be
    translated to a query string.

  * **headers**: `Object` (optional)

    An object containing additional HTTP headers to be sent with the request.

  * **method**: `string` (Default: `"GET"`)

    HTTP method of this request.

**Examples**

```js
const db = new Database();
const route = db.route('my-foxx-service');
const response = await route.request({
  path: 'hello-world',
  method: 'POST',
  body: {hello: 'world'},
  qs: {admin: true}
});
// response.body is the response body of calling
// POST _db/_system/my-foxx-service/hello-world?admin=true
// with JSON request body '{"hello": "world"}'
```
