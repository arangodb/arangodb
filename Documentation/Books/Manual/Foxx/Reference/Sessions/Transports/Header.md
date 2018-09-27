Header Session Transport
========================

`const headerTransport = require('@arangodb/foxx/sessions/transports/header');`

The header transport stores session identifiers in headers on the request
and response objects.

**Examples**

```js
const sessions = sessionsMiddleware({
  storage: module.context.collection('sessions'),
  transport: headerTransport('X-FOXXSESSID')
});
module.context.use(sessions);
```

Creating a transport
--------------------

`headerTransport([options]): Transport`

Creates a [Transport](README.md) that can be used in the sessions middleware.

**Arguments**

* **options**: `Object` (optional)

  An object with the following properties:

  * **name**: `string` (Default: `X-Session-Id`)

    Name of the header that contains the session identifier (not case sensitive).

If a string is passed instead of an options object, it will be interpreted
as the *name* option.
