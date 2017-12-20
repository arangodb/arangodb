Manifest files
==============

Every service comes with a `manifest.json` file providing metadata. The following fields are allowed in manifests:

* **configuration**: `Object` (optional)

  An object defining the [configuration options](Configuration.md) this service requires.
  <!-- TODO: examples -->

* **defaultDocument**: `string` (optional)

  If specified, the `/` (root) route of the service will automatically redirect to the given relative path, e.g.:

  ```json
  "defaultDocument": "index.html"
  ```

  This would have the same effect as creating the following route in JavaScript:

  ```js
  const createRouter = require('@arangodb/foxx/router');
  const indexRouter = createRouter();
  indexRouter.all('/', function (req, res) {
    res.redirect('index.html');
  });
  module.context.use(indexRouter);
  ```

  **Note**: As of 3.0.0 this field can safely be omitted; the value no longer defaults to `"index.html"`.

* **dependencies**: `Object` (optional) and **provides**: `Object` (optional)

  Objects specifying other services this service has as [dependencies](Dependencies.md) and what dependencies it can provide to other services.
  <!-- TODO: examples -->

* **engines**: `Object` (optional)

  An object indicating the [semantic version ranges](http://semver.org) of ArangoDB (or compatible environments) the service will be compatible with, e.g.:

  ```json
  "engines": {
    "arangodb": "^3.0.0"
  }
  ```

  This should correctly indicate the minimum version of ArangoDB the service has been tested against. Foxx maintains a strict semantic versioning policy as of ArangoDB 3.0.0 so it is generally safe to use semver ranges (e.g. `^3.0.0` to match any version greater or equal to `3.0.0` and below `4.0.0`) for maximum compatibility.

* **files**: `Object` (optional)

  An object defining [file assets](Assets.md) served by this service.

* **lib**: `string` (Default: `"."`)

  The relative path to the Foxx JavaScript files in the service, e.g.:

  ```json
  "lib": "lib"
  ```

  This would result in the main entry point (see below) and other JavaScript paths being resolved as relative to the `lib` folder inside the service folder.

* **main**: `string` (optional)

  The relative path to the main entry point of this service (relative to *lib*, see above), e.g.:

  ```json
  "main": "index.js"
  ```

  This would result in Foxx loading and executing the file `index.js` when the service is mounted or started.

  **Note**: while it is technically possible to omit this field, you will likely want to provide an entry point to your service as this is the only way to expose HTTP routes or export a JavaScript API.

* **scripts**: `Object` (optional)

  An object defining [named scripts](Scripts.md) provided by this service, which can either be used directly or as queued jobs by other services.

* **tests**: `string` or `Array<string>` (optional)

  A path or list of paths of JavaScript tests provided for this service. <!-- TODO link to Testing.md -->

Additionally manifests can provide the following metadata:

* **author**: `string` (optional)

  The full name of the author of the service (i.e. you). This will be shown in the web interface.

* **contributors**: `Array<string>` (optional)

  A list of names of people that have contributed to the development of the service in some way. This will be shown in the web interface.

* **description**: `string` (optional)

  A human-readable description of the service. This will be shown in the web interface.

* **keywords**: `Array<string>` (optional)

  A list of keywords that help categorize this service. This is used by the Foxx Store installers to organize services.

* **license**: `string` (optional)

  A string identifying the license under which the service is published, ideally in the form of an [SPDX license identifier](https://spdx.org/licenses). This will be shown in the web interface.

* **name**: `string` (optional)

  The name of the Foxx service. Allowed characters are A-Z, 0-9, the ASCII hyphen (`-`) and underscore (`_`) characters. The name must not start with a number. This will be shown in the web interface.

* **thumbnail**: `string` (optional)

  The filename of a thumbnail that will be used alongside the service in the web interface. This should be a JPEG or PNG image that looks good at sizes 50x50 and 160x160.

* **version**: `string` (optional)

  The version number of the Foxx service. The version number must follow the [semantic versioning format](http://semver.org). This will be shown in the web interface.

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
