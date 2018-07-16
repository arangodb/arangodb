


`Controller.apiDocumentation(path, [opts])`

Mounts the API documentation (Swagger) at the given `path`.

Note that the `path` can use path parameters as usual but must not use any
wildcard (`*`) or optional (`:name?`) parameters.

The optional **opts** can be an object with any of the following properties:

* **before**: a function that will be executed before a request to
  this endpoint is processed further.
* **appPath**: the mount point of the app for which documentation will be
  shown. Default: the mount point of the active app.
* **indexFile**: file path or file name of the Swagger HTML file.
  Default: `"index.html"`.
* **swaggerJson**: file path or file name of the Swagger API description JSON
  file or a function `swaggerJson(req, res, opts)` that sends a Swagger API
  description in JSON. Default: the built-in Swagger description generator.
* **swaggerRoot**: absolute path that will be used as the path path for any
  relative paths of the documentation assets, **swaggerJson** file and
  the **indexFile**. Default: the built-in Swagger distribution.

If **opts** is a function, it will be used as the value of **opts.before**.

If **opts.before** returns `false`, the request will not be processed
further.

If **opts.before** returns an object, any properties will override the
equivalent properties of **opts** for the current request.

Of course all **before**, **after** or **around** functions defined on the
controller will also be executed as usual.

**Examples**

```js
controller.apiDocumentation('/my/dox');

```

A request to `/my/dox` will be redirect to `/my/dox/index.html`,
which will show the API documentation of the active app.

```js
controller.apiDocumentation('/my/dox', function (req, res) {
  if (!req.session.get('uid')) {
    res.status(403);
    res.json({error: 'only logged in users may see the API'});
    return false;
  }
  return {appPath: req.parameters.mount};
});
```

A request to `/my/dox/index.html?mount=/_admin/aardvark` will show the
API documentation of the admin frontend (mounted at `/_admin/aardvark`).
If the user is not logged in, the error message will be shown instead.


