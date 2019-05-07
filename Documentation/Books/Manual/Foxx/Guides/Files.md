File access in Foxx
===================

Files within the service folder should always be considered read-only.
You should not expect to be able to write to your service folder or
modify any existing files.

ArangoDB is primarily a database. In most cases the best place to store data
is therefore inside the database, not on the file system.

Serving files
-------------

The most flexible way to serve files in your Foxx service is to simply
pass them through in your router using
the [context object's `fileName` method](../Reference/Context.md#filename) and
the [response object's `sendFile` method](../Reference/Routers/Response.md#sendfile):

```js
router.get("/some/filename.png", function(req, res) {
  const filePath = module.context.fileName("some-local-filename.png");
  res.sendFile(filePath);
});
```

While allowing for greater control of how the file should be sent to
the client and who should be able to access it,
doing this for all your static assets can get tedious.

Alternatively you can specify file assets that should be served by your
Foxx service directly in the [service manifest](../Reference/Manifest.md)
using the `files` attribute:

```json
"files": {
  "/some/filename.png": {
    "path": "some-local-filename.png",
    "type": "image/png",
    "gzip": false
  },
  "/favicon.ico": "bookmark.ico",
  "/static": "my-assets-folder"
}
```

Writing files
-------------

It is almost always an extremely bad idea to attempt to modify
the filesystem from within a service:

- The service folder itself is considered an implementation artefact and
  **may be discarded and replaced without warning**.
  ArangoDB maintains a canonical copy of each service internally to
  detect missing or damaged services and restore them automatically.

- ArangoDB uses multiple V8 contexts to allow handling multiple
  Foxx requests in parallel. Writing to the same file in a request handler
  may therefore cause race conditions and **result in corrupted data**.

- Writing to files outside the service folder introduces external state. In
  a cluster this will result in coordinators no longer being interchangeable.

- Writing to files during setup is unreliable because the setup script may
  be executed several times or not at all. In a cluster the setup script
  will only be executed on a single coordinator.

Therefore it is almost always a better option to store files using a
specialized, external file storage service
and handle file uploads outside Foxx itself.

However in some cases it may be feasible to store smaller files directly in
ArangoDB documents by using a separate collection.

{% hint 'danger' %}
Due to the way ArangoDB stores documents internally, you should not store
file contents alongside other attributes that might be updated independently.
Additionally, large file sizes will impact performance for operations
involving the document and may affect overall database performance.
{% endhint %}

{% hint 'warning' %}
In production, you should avoid storing any files in ArangoDB or handling file
uploads in Foxx. The following example will work for moderate amounts of small
files but is not recommended for large files or frequent uploads or
modifications.
{% endhint %}

To store files in a document you can simply convert the file contents
as a `Buffer` to a base64-encoded string:

```js
router.post('/avatars/:filename', (req, res) => {
  collection.save({
    filename: req.pathParams.filename,
    data: req.body.toString('base64')
  });
  res.status('no content');
});
router.get('/avatars/:filename', (req, res) => {
  const doc = collection.firstExample({
    filename: req.pathParams.filename
  });
  if (!doc) res.throw('not found');
  const data = new Buffer(doc.data, 'base64');
  res.set('content-type', 'image/png');
  res.set('content-length', data.length);
  res.write(data);
});
```
