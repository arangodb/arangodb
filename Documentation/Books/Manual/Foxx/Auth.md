Authentication
==============

`const createAuth = require('@arangodb/foxx/auth');`

Authenticators allow implementing basic password mechanism using simple built-in hashing functions.

For a full example of sessions with authentication and registration see the example in the [chapter on User Management](Users.md).

Creating an authenticator
-------------------------

`createAuth([options]): Authenticator`

Creates an authenticator.

**Arguments**

* **options**: `Object` (optional)

  An object with the following properties:

  * **method**: `string` (Default: `"sha256"`)

    The hashing algorithm to use to create password hashes. The authenticator will be able to verify passwords against hashes using any supported hashing algorithm. This only affects new hashes created by the authenticator.

    Supported values:

    * `"md5"`
    * `"sha1"`
    * `"sha224"`
    * `"sha256"`
    * `"sha384"`
    * `"sha512"`

  * **saltLength**: `number` (Default: `16`)

    Length of the salts that will be generated for password hashes.

Returns an authenticator.

Creating authentication data objects
------------------------------------

`auth.create(password): AuthData`

Creates an authentication data object for the given password with the following properties:

* **method**: `string`

  The method used to generate the hash.

* **salt**: `string`

  A random salt used to generate this hash.

* **hash**: `string`

  The hash string itself.

**Arguments**

* **password**: `string`

  A password to hash.

Returns the authentication data object.

Validating passwords against authentication data objects
--------------------------------------------------------

`auth.verify([hash, [password]]): boolean`

Verifies the given password against the given hash using a constant time string comparison.

**Arguments**

* **hash**: `AuthData` (optional)

  A authentication data object generated with the *create* method.

* **password**: `string` (optional)

  A password to verify against the hash.

Returns `true` if the hash matches the given password. Returns `false` otherwise.
