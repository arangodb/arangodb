Request objects
===============

The request object specifies the following properties:

* **arangoUser**: `string | null`

  The authenticated ArangoDB username used to make the request.
  This value is only set if authentication is enabled in ArangoDB and the
  request set an `authorization` header ArangoDB was able to verify.
  You are strongly encouraged to implement
  [your own authentication logic](../../Guides/Auth.md) for your own services
  but this property can be useful if you need to integrate with ArangoDB's
  own authentication mechanisms.

* **arangoVersion**: `number`

  The numeric value of the `x-arango-version` header or the numeric version
  of the ArangoDB server (e.g. `30102` for version 3.1.2) if no valid header
  was provided.

* **auth**: `object | null`

  The credentials supplied in the `authorization` header if any.

  If the request uses basic authentication, the value is an object like
  `{basic: {username: string}}` or
  `{basic: {username: string, password: string}}` or
  `{basic: {}}` (if the credentials were malformed or empty).

  If the request uses bearer authentication, the value is an object like
  `{bearer: string}`.

* **baseUrl**: `string`

  Root-relative base URL of the service, i.e. the prefix `"/_db/"` followed
  by the value of *database*.

* **body**: `any`

  The processed and validated request body for the current route.
  If no body has been defined for the current route, the value will be
  identical to *rawBody*.

  For details on how request bodies can be processed and validated by Foxx
  see the [body method of the endpoint object](Endpoints.md#body).

* **context**: `Context`

  The [service context](../Context.md) in which the router was mounted
  (rather than the context in which the route was defined).

* **database**: `string`

  The name of the database in which the request is being handled, e.g. `"_system"`.

* **headers**: `object`

  The raw headers object.

  For details on how request headers can be validated by Foxx see the
  [header method of the endpoint object](Endpoints.md#header).

* **hostname**: `string`

  The hostname (domain name) indicated in the request headers.

  Defaults to the hostname portion (i.e. excluding the port) of the `Host`
  header and falls back to the listening address of the server.

* **method**: `string`

  The HTTP verb used to make the request, e.g. `"GET"`.

* **originalUrl**: `string`

  Root-relative URL of the request, i.e. *path* followed by the raw query
  parameters, if any.

* **path**: `string`

  Database-relative path of the request URL (not including the query parameters).

* **pathParams**: `object`

  An object mapping the names of path parameters of the current route to
  their validated values.

  For details on how path parameters can be validated by Foxx see the
  [pathParam method of the endpoint object](Endpoints.md#pathparam).

* **port**: `number`

  The port indicated in the request headers.

  Defaults to the port portion (i.e. excluding the hostname) of the `Host`
  header and falls back to the listening port or the appropriate default
  port (`443` for HTTPS or `80` for HTTP, depending on *secure*) if the
  header only indicates a hostname.

  If the request was made using a trusted proxy (see *trustProxy*),
  this is set to the port portion of the `X-Forwarded-Host` header
  (or appropriate default port) if present.

* **protocol**: `string`

  The protocol used for the request.

  Defaults to `"https"` or `"http"` depending on whether ArangoDB is
  configured to use SSL or not.

  If the request was made using a trusted proxy (see *trustProxy*),
  this is set to the value of the `X-Forwarded-Proto` header if present.

* **queryParams**: `object`

  An object mapping the names of query parameters of the current route to
  their validated values.

  For details on how query parameters can be validated by Foxx see the
  [queryParam method of the endpoint object](Endpoints.md#queryparam).

* **rawBody**: `Buffer`

  The raw, unparsed, unvalidated request body as a buffer.

* **remoteAddress**: `string`

  The IP of the client that made the request.

  If the request was made using a trusted proxy (see *trustProxy*),
  this is set to the first IP listed in the `X-Forwarded-For` header if present.

* **remoteAddresses**: `Array<string>`

  A list containing the IP addresses used to make the request.

  Defaults to the value of *remoteAddress* wrapped in an array.

  If the request was made using a trusted proxy (see *trustProxy*),
  this is set to the list of IPs specified in the `X-Forwarded-For` header if present.

* **remotePort**: `number`

  The listening port of the client that made the request.

  If the request was made using a trusted proxy (see *trustProxy*),
  this is set to the port specified in the `X-Forwarded-Port` header if present.

* **secure**: `boolean`

  Whether the request was made over a secure connection (i.e. HTTPS).

  This is set to `false` when *protocol* is `"http"` and `true` when
  *protocol* is `"https"`.

* **suffix**: `string`

  The trailing path relative to the current route if the current route ends
  in a wildcard (e.g. `/something/*`).

  **Note**: Starting with ArangoDB 3.2 is passed into the service as-is, i.e.
  percentage escape sequences like `%2F` will no longer be unescaped.
  Also note that the suffix may contain path segments like `..` which may have
  special meaning if the suffix is used to build filesystem paths.

* **trustProxy**: `boolean`

  Indicates whether the request was made using a trusted proxy.
  If the origin server's address was specified in the ArangoDB configuration
  using `--frontend.trusted-proxy` or the service's `trustProxy` setting is
  enabled, this will be `true`, otherwise it will be `false`.

* **url**: `string`

  The URL of the request.<!-- TODO expand on this -->

* **xhr**: `boolean`

  Whether the request indicates it was made within a browser using AJAX.

  This is set to `true` if the `X-Requested-With` header is present and is
  a case-insensitive match for the value `"xmlhttprequest"`.

  Note that this value does not guarantee whether the request was made from
  inside a browser or whether AJAX was used and is merely a convention
  established by JavaScript frameworks like jQuery.

accepts
-------

`req.accepts(types): string | false`

`req.accepts(...types): string | false`

`req.acceptsCharsets(charsets): string | false`

`req.acceptsCharsets(...charsets): string | false`

`req.acceptsEncodings(encodings): string | false`

`req.acceptsEncodings(...encodings): string | false`

`req.acceptsLanguages(languages): string | false`

`req.acceptsLanguages(...languages): string | false`

These methods wrap the corresponding content negotiation methods of the
[accepts module](https://github.com/jshttp/accepts) for the current request.

**Examples**

```js
if (req.accepts(['json', 'html']) === 'html') {
  // Client explicitly prefers HTML over JSON
  res.write('<h1>Client prefers HTML</h1>');
} else {
  // Otherwise just send JSON
  res.json({success: true});
}
```

cookie
------

`req.cookie(name, options): string | null`

Gets the value of a cookie by name.

**Arguments**

* **name**: `string`

  Name of the cookie.

* **options**: `object` (optional)

  An object with any of the following properties:

  * **secret**: `string` (optional)

    Secret that was used to sign the cookie.

    If a secret is specified, the cookie's signature is expected to be present
    in a second cookie with the same name and the suffix `.sig`.
    Otherwise the signature (if present) will be ignored.

  * **algorithm**: `string` (Default: `"sha256"`)

    Algorithm that was used to sign the cookie.

If a string is passed instead of an options object it will be interpreted as
the *secret* option.

Returns the value of the cookie or `null` if the cookie is not set or its
signature is invalid.

get / header
------------

`req.get(name): string`

`req.header(name): string`

Gets the value of a header by name. You can validate request headers using the
[header method of the endpoint](Endpoints.md#header).

**Arguments**

* **name**: `string`

  Name of the header.

Returns the header value.

is
--

`req.is(types): string`

`req.is(...types): string`

This method wraps the (request body) content type detection method of the
[type-is module](https://github.com/jshttp/type-is) for the current request.

**Examples**

```js
const type = req.is('html', 'application/xml', 'application/*+xml');
if (type === false) { // no match
  handleDefault(req.rawBody);
} else if (type === 'html') {
  handleHtml(req.rawBody);
} else { // is XML
  handleXml(req.rawBody);
}
```

json
----

`req.json(): any`

Attempts to parse the raw request body as JSON and returns the result.

It is generally more useful to define a
[request body on the endpoint](Endpoints.md#body) and use the `req.body`
property instead.

Returns `undefined` if the request body is empty. May throw a `SyntaxError`
if the body could not be parsed.

makeAbsolute
------------

`req.makeAbsolute(path, [query]): string`

Resolves the given path relative to the `req.context.service`'s mount path
to a full URL.

**Arguments**

* **path**: `string`

  The path to resovle.

* **query**: `string | object`

  A string or object with query parameters to add to the URL.

Returns the formatted absolute URL.

params
------

`req.param(name): any`

**Arguments**

Looks up a parameter by name, preferring `pathParams` over `queryParams`.

It's probably better style to use the `req.pathParams` or `req.queryParams`
objects directly.

* **name**: `string`

  Name of the parameter.

Returns the (validated) value of the parameter.

range
-----

`req.range([size]): Ranges | number`

This method wraps the range header parsing method of the
[range-parser module](https://github.com/jshttp/range-parser) for the current request.

**Arguments**

* **size**: `number` (Default: `Infinity`)

  Length of the satisfiable range (e.g. number of bytes in the full response).
  If present, ranges exceeding the size will be considered unsatisfiable.

Returns `undefined` if the `Range` header is absent, `-2` if the header is
present but malformed, `-1` if the range is invalid (e.g. start offset is
larger than end offset) or unsatisfiable for the given size.

Otherwise returns an array of objects with the properties *start* and *end*
values for each range. The array has an additional property *type* indicating
the request range type.

**Examples**

```js
console.log(req.headers.range); // "bytes=40-80"
const ranges = req.range(100);
console.log(ranges); // [{start: 40, end: 80}]
console.log(ranges.type); // "bytes"
```

reverse
-------

`req.reverse(name, [params]): string`

Looks up the URL of a named route for the given parameters.

**Arguments**

* **name**: `string`

  Name of the route to look up.

* **params**: `object` (optional)

  An object containing values for the (path or query) parameters of the route.

Returns the URL of the route for the given parameters.

**Examples**

```js
router.get('/items/:id', function (req, res) {
  /* ... */
}, 'getItemById');

router.post('/items', function (req, res) {
  // ...
  const url = req.reverse('getItemById', {id: createdItem._key});
  res.set('location', req.makeAbsolute(url));
});
```
