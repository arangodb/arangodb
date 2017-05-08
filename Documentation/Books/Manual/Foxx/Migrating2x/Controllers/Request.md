Request objects
===============

The names of some attributes of the request object have been adjusted to more closely align with those of the corresponding methods on the endpoint objects and established conventions in other JavaScript frameworks:

* `req.urlParameters` is now called `req.pathParams`

* `req.parameters` is now called `req.queryParams`

* `req.params()` is now called `req.param()`

* `req.requestType` is now called `req.method`

* `req.compatibility` is now called `req.arangoVersion`

* `req.user` is now called `req.arangoUser`

Some attributes have been removed or changed:

* `req.cookies` has been removed entirely (use `req.cookie(name)`)

* `req.requestBody` has been removed entirely (see below)

* `req.suffix` is now a string rather than an array

Additionally the `req.server` and `req.client` attributes are no longer available. The information is now exposed in a way that can (optionally) transparently handle proxy forwarding headers:

* `req.hostname` defaults to `req.server.address`

* `req.port` defaults to `req.server.port`

* `req.remoteAddress` defaults to `client.address`

* `req.remotePort` defaults to `client.port`

Finally, the `req.cookie` method now takes the `signed` options directly.

Old:

```js
const sid = req.cookie('sid', {
  signed: {
    secret: 'keyboardcat',
    algorithm: 'sha256'
  }
});
```

New:

```js
const sid = req.cookie('sid', {
  secret: 'keyboardcat',
  algorithm: 'sha256'
});
```

Request bodies
--------------

The `req.body` is no longer a method and no longer automatically parses JSON request bodies unless a request body was defined. The `req.rawBody` now corresponds to the `req.rawBodyBuffer` of ArangoDB 2.x and is also no longer a method.

Old:

```js
ctrl.post('/', function (req, res) {
  const data = req.body();
  // ...
});
```

New:

```js
router.post('/', function (req, res) {
  const data = req.body;
  // ...
})
.body(['json']);
```

Or simply:

```js
const joi = require('joi');
router.post('/', function (req, res) {
  const data = req.body;
  // ...
})
.body(joi.object().optional());
```

Multipart requests
------------------

The `req.requestParts` method has been removed entirely. If you need to accept multipart request bodies, you can simply define the request body using a multipart MIME type like `multipart/form-data`:

Old:

```js
ctrl.post('/', function (req, res) {
  const parts = req.requestParts();
  // ...
});
```

New:

```js
router.post('/', function (req, res) {
  const parts = req.body;
  // ...
})
.body(['multipart/form-data']);
```

