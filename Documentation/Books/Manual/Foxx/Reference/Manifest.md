Service manifest
================

Every service comes with a `manifest.json` file providing metadata. Typically a
manifest should at least specify the version of ArangoDB the service supports and
the `main` JavaScript file which Foxx will use as the entrypoint to your service:

```json
{
  "engines": {
    "arangodb": "^3.4.0"
  },
  "main": "index.js"
}
```

Tooling integration
-------------------

If you are using an IDE or editor that supports JSON schema for code intelligence
or validation, you can use the public Foxx manifest schema
[available at the third-party JSON Schema Store](http://json.schemastore.org/foxx-manifest)
by adding a `$schema` field to your `manifest.json` file:

```json
{
  "$schema": "http://json.schemastore.org/foxx-manifest"
}
```

### Visual Studio Code

In [Visual Studio Code](https://code.visualstudio.com) you can also enable the
Foxx manifest schema for all `manifest.json` files by adding the following to your
[user or workspace settings](https://code.visualstudio.com/docs/getstarted/settings):

```json
{
  "json.schemas": [
    {
      "fileMatch": [
        "manifest.json"
      ],
      "url": "http://json.schemastore.org/foxx-manifest"
    }
  ]
}
```

Structure
---------

The following fields are allowed in manifests:

- **$schema**: `"http://json.schemastore.org/foxx-manifest"` (optional)

  The JSON schema. See above.

- **configuration**: `Object` (optional)

  An object defining the [configuration options](Configuration.md) this service requires.

  <!-- TODO: examples -->

- **defaultDocument**: `string` (optional)

  If specified, the `/` (root) route of the service will automatically redirect
  to the given relative path, e.g.:

  ```json
  "defaultDocument": "index.html"
  ```

  This would have the same effect as creating the following route in JavaScript:

  ```js
  const createRouter = require("@arangodb/foxx/router");
  const indexRouter = createRouter();
  indexRouter.all("/", function(req, res) {
    res.redirect("index.html");
  });
  module.context.use(indexRouter);
  ```

  **Note**: As of 3.0.0 this field can safely be omitted; the value no longer
  defaults to `"index.html"`.

- **dependencies**: `Object` (optional)

  An object mapping local aliases to dependency definitions.
  Each entry can be a dependency name and version range in the format
  `name:version` or an object with the following properties:

  - **name**: `string` (Default: `"*"`)

    Name of the dependency.

  - **version**: `string` (Default: `"*"`)

    Version range of the dependency.

  - **description**: `string` (optional)

    Human-readable description of the dependency or how the dependency is used.

  - **required**: `boolean` (Default: `true`)

    Whether the service requires the dependency to be assigned in order to function.
    If a required dependency is not assigned, the service will marked as
    inoperable until a service mount point has been assigned for the dependency.

  - **multiple**: `boolean` (Default: `false`)

    Whether the dependency can be specified multiple times. If a dependency is
    marked as `multiple`, the value of the local alias will be an array of all
    services assigned for the dependency.

  See [the dependencies guide](../Guides/Dependencies.md) for more information.

- **engines**: `Object` (optional)

  An object indicating the [semantic version ranges](http://semver.org) of
  ArangoDB (or compatible environments) the service will be compatible with, e.g.:

  ```json
  "engines": {
    "arangodb": "^3.0.0"
  }
  ```

  This should correctly indicate the minimum version of ArangoDB the service
  has been tested against. Foxx maintains a strict semantic versioning policy
  as of ArangoDB 3.0.0 so it is generally safe to use semver ranges
  (e.g. `^3.0.0` to match any version greater or equal to `3.0.0` and below
  `4.0.0`) for maximum compatibility.

- **files**: `Object` (optional)

  An object defining file assets served by this service.

  Each entry can represent either a single file or a directory.
  When serving entire directories, the key acts as a prefix and requests to
  that prefix will be resolved within the given directory:

  - **path**: `string`

    The relative path of the file or folder within the service.

  - **type**: `string` (optional)

    The MIME content type of the file. Defaults to an intelligent guess based
    on the filename's extension.

  - **gzip**: `boolean` (Default: `false`)

    If set to `true` the file will be served with gzip-encoding if supported
    by the client. This can be useful when serving text files like client-side
    JavaScript, CSS or HTML.

  If a string is provided instead of an object, it will be interpreted as the _path_ option.

  Example serving the `public` folder at `/static` and the `favicon.ico` at `/favicon.ico`:

  ```json
  "files": {
    "favicon.ico": {
      "path": "public/favicon.ico",
      "gzip": false
    },
    "static": "public"
  }
  ```

- **lib**: `string` (Default: `"."`)

  The relative path to the Foxx JavaScript files in the service, e.g.:

  ```json
  "lib": "lib"
  ```

  This would result in the main entry point (see below) and other JavaScript
  paths being resolved as relative to the `lib` folder inside the service folder.

- **main**: `string` (optional)

  The relative path to the main entry point of this service
  (relative to _lib_, see above), e.g.:

  ```json
  "main": "index.js"
  ```

  This would result in Foxx loading and executing the file `index.js` when
  the service is mounted or started.

  **Note**: while it is technically possible to omit this field, you will
  likely want to provide an entry point to your service as this is the only
  way to expose HTTP routes or export a JavaScript API.

- **provides**: `Object` (optional)

  An object mapping dependency names to version ranges of that dependency
  provided by this service. See [the dependencies guide](../Guides/Dependencies.md)
  for more information.

- **scripts**: `Object` (optional)

  An object defining [named scripts](../Guides/Scripts.md) provided by this
  service, which can either be used directly or as queued jobs by other services.

- **tests**: `string` or `Array<string>` (optional)

  One or more patterns to match the paths of test files, e.g.:

  ```json
  "tests": [
    "**/test_*.js",
    "**/*_test.js"
  ]
  ```

  These patterns can be either relative file paths or "globstar" patterns where

  - `*` matches zero or more characters in a filename
  - `**` matches zero or more nested directories.

Additionally manifests can provide the following metadata:

- **author**: `string` (optional)

  The full name of the author of the service (i.e. you).
  This will be shown in the web interface.

- **contributors**: `Array<string>` (optional)

  A list of names of people that have contributed to the development of the
  service in some way. This will be shown in the web interface.

- **description**: `string` (optional)

  A human-readable description of the service.
  This will be shown in the web interface.

- **keywords**: `Array<string>` (optional)

  A list of keywords that help categorize this service.
  This is used by the Foxx Store installers to organize services.

- **license**: `string` (optional)

  A string identifying the license under which the service is published, ideally
  in the form of an [SPDX license identifier](https://spdx.org/licenses).
  This will be shown in the web interface.

- **name**: `string` (optional)

  The name of the Foxx service. Allowed characters are A-Z, 0-9, the ASCII
  hyphen (`-`) and underscore (`_`) characters. The name must not start with
  a number. This will be shown in the web interface.

- **thumbnail**: `string` (optional)

  The filename of a thumbnail that will be used alongside the service in the
  web interface. This should be a JPEG or PNG image that looks good at sizes
  50x50 and 160x160.

- **version**: `string` (optional)

  The version number of the Foxx service. The version number must follow the
  [semantic versioning format](http://semver.org).
  This will be shown in the web interface.

**Examples**

```json
{
  "name": "example-foxx-service",
  "version": "3.0.0-dev",
  "license": "MIT",
  "description": "An example service with a relatively full-featured manifest.",
  "thumbnail": "foxx-icon.png",
  "keywords": ["demo", "service"],
  "author": "ArangoDB GmbH",
  "contributors": [
    "Alan Plum <alan@arangodb.example>"
  ],

  "lib": "dist",
  "main": "entry.js",
  "defaultDocument": "welcome.html",
  "engines": {
    "arangodb": "^3.0.0"
  },

  "files": {
    "welcome.html": "assets/index.html",
    "hello.jpg": "assets/hello.jpg"
    "world.jpg": {
      "path": "assets/world.jpg",
      "type": "image/jpeg",
      "gzip": false
    }
  },

  "tests": "dist/**.spec.js"
}
```
