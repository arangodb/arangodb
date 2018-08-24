Endpoints
=========

Endpoints are returned by the `use`, `all`
and HTTP verb (e.g. `get`, `post`) methods of [routers](./README.md)
as well as the `use` method of the [service context](../Context.md).
They can be used to attach metadata to mounted routes, middleware and
child routers that affects how requests and responses are processed or
provides API documentation.

Endpoints should only be used to invoke the following methods.
Endpoint methods can be chained together (each method returns the endpoint itself).

header
------

`endpoint.header(name, [schema], [description]): this`

Defines a request header recognized by the endpoint.
Any additional non-defined headers will be treated as optional string values.
The definitions will also be shown in the route details in the API documentation.

If the endpoint is a child router, all routes of that router will use this
header definition unless overridden.


**Arguments**

* **name**: `string`

  Name of the header. This should be considered case insensitive as all header
  names will be converted to lowercase.

* **schema**: `Schema` (optional)

  A schema describing the format of the header value. This can be a joi schema
  or anything that has a compatible `validate` method.

  The value of this header will be set to the `value` property of the
  validation result. A validation failure will result in an automatic 400
  (Bad Request) error response.

* **description**: `string` (optional)

  A human readable string that will be shown in the API documentation.

Returns the endpoint.

**Examples**

```js
router.get(/* ... */)
.header('arangoVersion', joi.number().min(30000).default(30000));
```

pathParam
---------

`endpoint.pathParam(name, [schema], [description]): this`

Defines a path parameter recognized by the endpoint.
Path parameters are expected to be filled as part of the endpoint's mount path.
Any additional non-defined path parameters will be treated as optional
string values. The definitions will also be shown in the route details in
the API documentation.

If the endpoint is a child router, all routes of that router will use this
parameter definition unless overridden.

**Arguments**

* **name**: `string`

  Name of the parameter.

* **schema**: `Schema` (optional)

  A schema describing the format of the parameter. This can be a joi schema
  or anything that has a compatible `validate` method.

  The value of this parameter will be set to the `value` property of the
  validation result. A validation failure will result in the route failing to
  match and being ignored (resulting in a 404 (Not Found) error response if no
  other routes match).

* **description**: `string` (optional)

  A human readable string that will be shown in the API documentation.

Returns the endpoint.

**Examples**

```js
router.get('/some/:num/here', /* ... */)
.pathParam('num', joi.number().required());
```

queryParam
----------

`endpoint.queryParam(name, [schema], [description]): this`

Defines a query parameter recognized by the endpoint.
Any additional non-defined query parameters will be treated as optional
string values. The definitions will also be shown in the route details in
the API documentation.

If the endpoint is a child router, all routes of that router will use this
parameter definition unless overridden.

**Arguments**

* **name**: `string`

  Name of the parameter.

* **schema**: `Schema` (optional)

  A schema describing the format of the parameter. This can be a joi schema or
  anything that has a compatible `validate` method.

  The value of this parameter will be set to the `value` property of the
  validation result. A validation failure will result in an automatic 400
  (Bad Request) error response.

* **description**: `string` (optional)

  A human readable string that will be shown in the API documentation.

Returns the endpoint.

**Examples**

```js
router.get(/* ... */)
.queryParam('num', joi.number().required());
```

body
----

`endpoint.body([model], [mimes], [description]): this`

Defines the request body recognized by the endpoint.
There can only be one request body definition per endpoint. The definition will
also be shown in the route details in the API documentation.

In the absence of a request body definition, the request object's *body*
property will be initialized to the unprocessed *rawBody* buffer.

If the endpoint is a child router, all routes of that router will use this body
definition unless overridden. If the endpoint is a middleware, the request body
will only be parsed once (i.e. the MIME types of the route matching the same
request will be ignored but the body will still be validated again).

**Arguments**

* **model**: `Model | Schema | null` (optional)

  A model or joi schema describing the request body. A validation failure will
  result in an automatic 400 (Bad Request) error response.

  If the value is a model with a `fromClient` method, that method will be
  applied to the parsed request body.

  If the value is a schema or a model with a schema, the schema will be used
  to validate the request body and the `value` property of the validation
  result of the parsed request body will be used instead of the parsed request
  body itself.

  If the value is a model or a schema and the MIME type has been omitted,
  the MIME type will default to JSON instead.

  If the value is explicitly set to `null`, no request body will be expected.

  If the value is an array containing exactly one model or schema, the request
  body will be treated as an array of items matching that model or schema.

* **mimes**: `Array<string>` (optional)

  An array of MIME types the route supports.

  Common non-mime aliases like "json" or "html" are also supported and will be
  expanded to the appropriate MIME type (e.g. "application/json" and "text/html").

  If the MIME type is recognized by Foxx the request body will be parsed into
  the appropriate structure before being validated. Currently only JSON,
  `application/x-www-form-urlencoded` and multipart formats are supported in this way.

  If the MIME type indicated in the request headers does not match any of the
  supported MIME types, the first MIME type in the list will be used instead.

  Failure to parse the request body will result in an automatic 400
  (Bad Request) error response.

* **description**: `string` (optional)

  A human readable string that will be shown in the API documentation.

Returns the endpoint.

**Examples**

```js
router.post('/expects/some/json', /* ... */)
.body(
  joi.object().required(),
  'This implies JSON.'
);

router.post('/expects/nothing', /* ... */)
.body(null); // No body allowed

router.post('/expects/some/plaintext', /* ... */)
.body(['text/plain'], 'This body will be a string.');
```

response
--------

`endpoint.response([status], [model], [mimes], [description]): this`

Defines a response body for the endpoint. When using the response object's
`send` method in the request handler of this route, the definition with the
matching status code will be used to generate the response body.
The definitions will also be shown in the route details in the API documentation.

If the endpoint is a child router, all routes of that router will use this
response definition unless overridden. If the endpoint is a middleware,
this method has no effect.

**Arguments**

* **status**: `number | string` (Default: `200` or `204`)

  HTTP status code the response applies to. If a string is provided instead of
  a numeric status code it will be used to look up a numeric status code using
  the [statuses](https://github.com/jshttp/statuses) module.

* **model**: `Model | Schema | null` (optional)

  A model or joi schema describing the response body.

  If the value is a model with a `forClient` method, that method will be
  applied to the data passed to `response.send` within the route if the
  response status code matches (but also if no status code has been set).

  If the value is a schema or a model with a schema, the actual schema will
  not be used to validate the response body and only serves to document the
  response in more detail in the API documentation.

  If the value is a model or a schema and the MIME type has been omitted,
  the MIME type will default to JSON instead.

  If the value is explicitly set to `null` and the status code has been omitted,
  the status code will default to `204` ("no content") instead of `200`.

  If the value is an array containing exactly one model or schema, the response
  body will be an array of items matching that model or schema.

* **mimes**: `Array<string>` (optional)

  An array of MIME types the route might respond with for this status code.

  Common non-mime aliases like "json" or "html" are also supported and will be
  expanded to the appropriate MIME type (e.g. "application/json" and "text/html").

  When using the `response.send` method the response body will be converted to
  the appropriate MIME type if possible.

* **description**: `string` (optional)

  A human-readable string that briefly describes the response and will be shown
  in the endpoint's detailed documentation.

Returns the endpoint.

**Examples**

```js
// This example only provides documentation
// and implies a generic JSON response body.
router.get(/* ... */)
.response(
  joi.array().items(joi.string()),
  'A list of doodad identifiers.'
);

// No response body will be expected here.
router.delete(/* ... */)
.response(null, 'The doodad no longer exists.');

// An endpoint can define multiple response types
// for different status codes -- but never more than
// one for each status code.
router.post(/* ... */)
.response('found', 'The doodad is located elsewhere.')
.response(201, ['text/plain'], 'The doodad was created so here is a haiku.');

// Here the response body will be set to
// the querystring-encoded result of
// FormModel.forClient({some: 'data'})
// because the status code defaults to 200.
router.patch(function (req, res) {
  // ...
  res.send({some: 'data'});
})
.response(FormModel, ['application/x-www-form-urlencoded'], 'OMG.');

// In this case the response body will be set to
// SomeModel.forClient({some: 'data'}) because
// the status code has been set to 201 before.
router.put(function (req, res) {
  // ...
  res.status(201);
  res.send({some: 'data'});
})
.response(201, SomeModel, 'Something amazing happened.');
```

error
-----

`endpoint.error(status, [description]): this`

Documents an error status for the endpoint.

If the endpoint is a child router, all routes of that router will use this
error description unless overridden. If the endpoint is a middleware,
this method has no effect.

This method only affects the generated API documentation and has not other
effect within the service itself.

**Arguments**

* **status**: `number | string`

  HTTP status code for the error (e.g. `400` for "bad request"). If a string is
  provided instead of a numeric status code it will be used to look up a numeric
  status code using the [statuses](https://github.com/jshttp/statuses) module.

* **description**: `string` (optional)

  A human-readable string that briefly describes the error condition and will
  be shown in the endpoint's detailed documentation.

Returns the endpoint.

**Examples**

```js
router.get(function (req, res) {
  // ...
  res.throw(403, 'Validation error at x.y.z');
})
.error(403, 'Indicates that a validation has failed.');
```

summary
-------

`endpoint.summary(summary): this`

Adds a short description to the endpoint's API documentation.

If the endpoint is a child router, all routes of that router will use this
summary unless overridden. If the endpoint is a middleware, this method has no effect.

This method only affects the generated API documentation and has not other
effect within the service itself.

**Arguments**

* **summary**: `string`

  A human-readable string that briefly describes the endpoint and will appear
  next to the endpoint's path in the documentation.

Returns the endpoint.

**Examples**

```js
router.get(/* ... */)
.summary('List all discombobulated doodads')
```

description
-----------

`endpoint.description(description): this`

Adds a long description to the endpoint's API documentation.

If the endpoint is a child router, all routes of that router will use
this description unless overridden. If the endpoint is a middleware,
this method has no effect.

This method only affects the generated API documentation and has not
other effect within the service itself.

**Arguments**

* **description**: `string`

  A human-readable string that describes the endpoint in detail and
  will be shown in the endpoint's detailed documentation.

Returns the endpoint.

**Examples**

```js
// The "dedent" library helps formatting
// multi-line strings by adjusting indentation
// and removing leading and trailing blank lines
const dd = require('dedent');
router.post(/* ... */)
.description(dd`
  This route discombobulates the doodads by
  frobnicating the moxie of the request body.
`)
```

deprecated
----------

`endpoint.deprecated([deprecated]): this`

Marks the endpoint as deprecated.

If the endpoint is a child router, all routes of that router will also be
marked as deprecated. If the endpoint is a middleware, this method has no effect.

This method only affects the generated API documentation and has not other
effect within the service itself.

**Arguments**

* **deprecated**: `boolean` (Default: `true`)

  Whether the endpoint should be marked as deprecated. If set to `false` the
  endpoint will be explicitly marked as *not* deprecated.

Returns the endpoint.

**Examples**

```js
router.get(/* ... */)
.deprecated();
```


tag
---

`endpoint.tag(...tags): this`

Marks the endpoint with the given tags that will be used to group related
routes in the generated API documentation.

If the endpoint is a child router, all routes of that router will also be
marked with the tags. If the endpoint is a middleware, this method has no effect.

This method only affects the generated API documentation and has not other
effect within the service itself.

**Arguments**

* **tags**: `string`

  One or more strings that will be used to group the endpoint's routes.

Returns the endpoint.

**Examples**

```js
router.get(/* ... */)
.tag('auth', 'restricted');
```
