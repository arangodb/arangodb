Static file assets
==================

The most flexible way to serve files in your Foxx service is to simply pass them through in your router using the [context object's `fileName` method](../Foxx/Context.md#filename) and the [response object's `sendFile` method](../Foxx/Router/Response.md#sendfile):

```js
router.get('/some/filename.png', function (req, res) {
  const filePath = module.context.fileName('some-local-filename.png');
  res.sendFile(filePath);
});
```

While allowing for greater control of how the file should be sent to the client and who should be able to access it, doing this for all your static assets can get tedious.

Alternatively you can specify file assets that should be served by your Foxx service directly in the [service manifest](../Foxx/Manifest.md) using the `files` attribute:

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

Each entry in the `files` attribute can represent either a single file or a directory. When serving entire directories, the key acts as a prefix and requests to that prefix will be resolved within the given directory.

**Options**

* **path**: `string`

  The relative path of the file or folder within the service.

* **type**: `string` (optional)

  The MIME content type of the file. Defaults to an intelligent guess based on the filename's extension.

* **gzip**: `boolean` (Default: `false`)

  If set to `true` the file will be served with gzip-encoding if supported by the client. This can be useful when serving text files like client-side JavaScript, CSS or HTML.

If a string is provided instead of an object, it will be interpreted as the *path* option.
