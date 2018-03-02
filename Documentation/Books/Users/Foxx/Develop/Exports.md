Working with Foxx exports
=========================

Instead of (or in addition to) defining controllers, Foxx apps can also define exports.

Foxx exports are not intended to replace regular NPM modules. They simply allow you to make your app's collections and **applicationContext** available in other Foxx apps or bundling ArangoDB-specific modules in re-usable Foxx apps.

Define an export module
-----------------------

In order to export modules in a Foxx app you need to list the files in your manifest:

```json
{
    "name": "foxx_exports_example",
    "version": "1.0.0",
    "description": "Demonstrates Foxx exports.",
    "exports": {
        "doodads": "./doodads.js",
        "anotherModule": "./someOtherFilename.js"
    },
    "controllers": {
        "/etc": "./controllers.js"
    }
}
```

The file **doodads.js** in the app's base path could look like this:

```js
var Foxx = require('org/arangodb/foxx');
var Doodad = Foxx.Model.extend({});
var doodadRepo = new Foxx.Repository(
    applicationContext.collection('doodads'),
    {model: Doodad}
);
exports.repo = doodadRepo;
exports.model = Doodad;
// or simply
module.exports = {
    repo: doodadRepo,
    model: Doodad
};
```

This module would then export the name "repo" bound to the variable **doodads** as well as the name "model" bound to the **Doodad** model.

**Note**: The **applicationContext** is available to your Foxx exports just like in your Foxx controllers.

**Note**: Node.js style exports by assigning values directly to the `module.exports` property are supported, too.

Import from another app
-----------------------

In order to import from another app, you need to know where the app is mounted.

Let's say we have mounted the example app above at **/my-doodads**. We could now access the app's exports in another app like so:

```js
var Foxx = require('org/arangodb/foxx');
var doodads = Foxx.requireApp('/my-doodads').doodads;
var Doodad = doodads.model;
var doodadRepo = doodads.repo;

// use the imported model and repository
var myDoodad = new Doodad();
doodadRepo.save(myDoodad);
```

Default exports
---------------

If you want more control over the object other apps receive when they load your app using **Foxx.requireApp**, you can specify a single filename instead of an object in your manifest file:

```json
{
    "name": "foxx_exports_example",
    "version": "1.0.0",
    "description": "Demonstrates Foxx exports.",
    "exports": "./exports.js",
    "controllers": {
        "/etc": "./controllers.js"
    }
}
```

To replicate the same behavior as in the earlier example, the file **exports.js** could look like this:

```js
exports.doodads = require('./doodads');
exports.anotherModule = require('./someOtherFilename');

// or Node.js-style

module.exports = {
    doodads: require('./doodads'),
    anotherModule: require('./someOtherFilename')
};
```

Let's assume the file **exports.js** exports something else instead:

```js
module.exports = function () {
    return "Hello!";
};
```

Assuming the app is mounted at `/hello`, we could now use it in other apps like this:

```js
var Foxx = require('org/arangodb/foxx');
var helloExport = Foxx.requireApp('/hello');
var greeting = helloExport(); // "Hello!"
```

Managing app dependencies
-------------------------

As of ArangoDB 2.6 you can define Foxx app dependencies of your Foxx app in your manifest.

Let's say you want to use the `foxx_exports_example` app from earlier in your app. Just add it to your own manifest like this:

```json
{
    "name": "foxx_dependencies_example",
    "version": "1.0.0",
    "dependencies": {
        "exportsExample": {
            "name": "foxx_exports_example",
            "version": "^1.0.0"
        }
    }
}
```

Instead of specifying the exact mount point of the app you depend on, you can now refer to it using the name `exportsExample`:

**Before:**

```js
var doodads = Foxx.requireApp(applicationContext.configuration.exportsExampleMountPath).doodads;
```

**After:**

```js
var doodads = applicationContext.dependencies.exportsExample.doodads;
```

If an app declares any dependencies,
you need to fulfill its dependencies before it becomes active.
In the meantime a fallback application will be mounted that responds to all
requests with a HTTP 500 status code indicating a server-side error.

The dependencies of a mounted app can be adjusted
from the admin frontend by clicking the *Dependencies* button in the app details
or using the **set-dependencies** command of the **foxx-manager** command-line utility.
