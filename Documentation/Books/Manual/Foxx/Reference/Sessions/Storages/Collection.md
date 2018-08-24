Collection Session Storage
==========================

`const collectionStorage = require('@arangodb/foxx/sessions/storages/collection');`

The collection session storage persists sessions to a collection in the database.

Creating a storage
------------------

`collectionStorage(options): Storage`

Creates a [Storage](README.md) that can be used in the sessions middleware.

**Arguments**

* **options**: `Object`

  An object with the following properties:

  * **collection**: `ArangoCollection`

    The collection that should be used to persist the sessions.
    If a string is passed instead of a collection it is assumed to be the fully
    qualified name of a collection in the current database.

  * **ttl**: `number` (Default: `60 * 60`)

    The time in seconds since the last update until a session will be
    considered expired.

  * **pruneExpired**: `boolean` (Default: `false`)

    Whether expired sessions should be removed from the collection when they
    are accessed instead of simply being ignored.

  * **autoUpdate**: `boolean` (Default: `true`)

    Whether sessions should be updated in the collection every time they
    are accessed to keep them from expiring. Disabling this option
    **will improve performance** but means you will have to take care of
    keeping your sessions alive yourself.

If a string or collection is passed instead of an options object, it will
be interpreted as the *collection* option.

prune
-----

`storage.prune(): Array<string>`

Removes all expired sessions from the collection. This method should be called
even if the *pruneExpired* option is enabled to clean up abandoned sessions.

Returns an array of the keys of all sessions that were removed.

save
----

`storage.save(session): Session`

Saves (replaces) the given session object in the collection. This method needs
to be invoked explicitly after making changes to the session or the changes
will not be persisted. Assigns a new `_key` to the session if it previously
did not have one.

**Arguments**

* **session**: `Session`

 A session object.

Returns the modified session.

clear
-----

`storage.clear(session): boolean`

Removes the session from the collection. Has no effect if the session was
already removed or has not yet been saved to the collection (i.e. has no `_key`).

**Arguments**

* **session**: `Session`

 A session object.

Returns `true` if the session was removed or `false` if it had no effect.
