

@brief throw collection not loaded error
`--database.throw-collection-not-loaded-error flag`

Accessing a not-yet loaded collection will automatically load a collection
on first access. This flag controls what happens in case an operation
would need to wait for another thread to finalize loading a collection. If
set to *true*, then the first operation that accesses an unloaded
collection
will load it. Further threads that try to access the same collection while
it is still loading will get an error (1238, *collection not loaded*).
When
the initial operation has completed loading the collection, all operations
on the collection can be carried out normally, and error 1238 will not be
thrown.

If set to *false*, the first thread that accesses a not-yet loaded
collection
will still load it. Other threads that try to access the collection while
loading will not fail with error 1238 but instead block until the
collection
is fully loaded. This configuration might lead to all server threads being
blocked because they are all waiting for the same collection to complete
loading. Setting the option to *true* will prevent this from happening,
but
requires clients to catch error 1238 and react on it (maybe by scheduling
a retry for later).

The default value is *false*.

