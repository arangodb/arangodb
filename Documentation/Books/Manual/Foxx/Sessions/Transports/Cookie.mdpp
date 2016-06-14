!CHAPTER Cookie Session Transport

`const cookieTransport = require('@arangodb/foxx/sessions/transports/cookie');`

The cookie transport stores session identifiers in cookies on the request and response object.

**Examples**

```js
// Pass in a secure secret from the Foxx configuration
const secret = module.context.configuration.cookieSecret;
const sessions = sessionsMiddleware({
  storage: module.context.collection('sessions'),
  transport: cookieTransport({
    name: 'FOXXSESSID',
    ttl: 60 * 60 * 24 * 7, // one week in seconds
    algorithm: 'sha256',
    secret: secret
  })
});
module.context.use(sessions);
```

!SECTION Creating a transport

`cookieTransport([options]): Transport`

Creates a [Transport](README.md) that can be used in the sessions middleware.

**Arguments**

* **options**: `Object` (optional)

  An object with the following properties:

  * **name**: `string` (Default: `"sid"`)

    The name of the cookie.

  * **ttl**: `number` (optional)

    Cookie lifetime in seconds.

  * **algorithm**: `string` (optional)

    The algorithm used to sign and verify the cookie. If no algorithm is specified, the cookie will not be signed or verified. See the [cookie method on the response object](../../Router/Response.md).

  * **secret**: `string` (optional)

    Secret to use for the signed cookie. Will be ignored if no algorithm is provided.

If a string is passed instead of an options object, it will be interpreted as the *name* option.
