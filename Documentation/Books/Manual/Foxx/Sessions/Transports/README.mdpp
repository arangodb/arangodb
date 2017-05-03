Session Transports
==================

Session transports are used by the sessions middleware to store and retrieve session identifiers in requests and responses. Session transports must implement the `get` and/or `set` methods and can optionally implement the `clear` method.

get
---

`transport.get(request): string | null`

Retrieves a session identifier from a request object.

If present this method will automatically be invoked for each transport until a transport returns a session identifier.

**Arguments**

* **request**: `Request`

  [Request object](../../Router/Request.md) to extract a session identifier from.

Returns the session identifier or `null` if the transport can not find a session identifier in the request.

**Examples**

```js
get(req) {
  return req.get('x-session-id') || null;
}
```

set
---

`transport.set(response, sid): void`

Attaches a session identifier to a response object.

If present this method will automatically be invoked at the end of a request regardless of whether the session was modified or not.

**Arguments**

* **response**: `Response`

  [Response object](../../Router/Response.md) to attach a session identifier to.

* **sid**: `string`

  Session identifier to attach to the response.

Returns nothing.

**Examples**

```js
set(res) {
  res.set('x-session-id', value);
}
```

clear
-----

`transport.clear(response): void`

Attaches a payload indicating that the session has been cleared to the response object.
This can be used to clear a session cookie when the session has been destroyed (e.g. during logout).

If present this method will automatically be invoked instead of `set` when the `req.session` attribute was removed by the route handler.

**Arguments**

* **response**: `Response`

  Response object to remove the session identifier from.

Returns nothing.
