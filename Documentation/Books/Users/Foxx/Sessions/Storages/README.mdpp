!CHAPTER Session Storages

Session storages are used by the sessions middleware to persist sessions across requests. Session storages must implement the `fromClient` and `forClient` methods and can optionally implement the `new` method.

The built-in session storages generally provide the following attributes:

* **uid**: `string` (Default: `null`)

  A unique identifier indicating the active user.

* **created**: `number` (Default: `Date.now()`)

  The numeric timestamp of when the session was created.

* **data**: `any` (Default: `null`)

  Arbitrary data to persisted in the session.

!SECTION new

`storage.new(): Session`

Generates a new session object representing an empty session. The empty session object should not be persisted unless necessary. The return value will be exposed by the middleware as the `session` property of the request object if no session identifier was returned by the session transports and auto-creation is not explicitly disabled in the session middleware.

**Examples**

```js
new() {
  return {
    uid: null,
    created: Date.now(),
    data: null
  };
}
```

!SECTION fromClient

`storage.fromClient(sid): Session | null`

Resolves or deserializes a session identifier to a session object.

**Arguments**

* **sid**: `string`

  Session identifier to resolve or deserialize.

Returns a session object representing the session with the given session identifier that will be exposed by the middleware as the `session` property of the request object. This method will only be called if any of the session transports returned a session identifier. If the session identifier is invalid or expired, the method should return a `null` value to indicate no matching session.

**Examples**

```js
fromClient(sid) {
  return db._collection('sessions').firstExample({_key: sid});
}
```

!SECTION forClient

`storage.forClient(session): string | null`

Derives a session identifier from the given session object.

**Arguments**

* **session**: `Session`

  Session to derive a session identifier from.

Returns a session identifier for the session represented by the given session object. This method will be called with the `session` property of the request object unless that property is empty (e.g. `null`).

**Examples**

```js
forClient(session) {
  if (!session._key) {
    const meta = db._collection('sessions').save(session);
    return meta._key;
  }
  db._collection('sessions').replace(session._key, session);
  return session._key;
}
```
