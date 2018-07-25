Using Webpack with Foxx
=======================

You can use [Webpack](https://webpack.js.org/) to compile your Foxx services
the same way you would compile any other JavaScript code.
However there are a few things you will need to keep in mind.

Basic configuration
-------------------

Because the ArangoDB JavaScript environment is largely compatible with Node.js,
the starting point looks fairly similar:

```js
"use strict";
module.exports = {
  mode: "production",
  target: "node",
  output: {
    libraryTarget: "commonjs2"
  },
  externals: [/^@arangodb(\/|$)/]
};
```

The service context
-------------------

Foxx extends the `module` object with a special `context` property that
reflects the current [service context](../Reference/Context.md).
As Webpack compiles multiple modules into a single file your code will
not be able to access the real `module` object provided by ArangoDB.

To work around this limitation you can use the `context` provided by the
[`@arangodb/locals` module](../Reference/Modules/README.md#the-arangodblocals-module):

```js
const { context } = require("@arangodb/locals");
```

This object is identical to `module.context` and can be used as
a drop-in replacement:

```js
const { context } = require("@arangodb/locals");
const createRouter = require("@arangodb/foxx/router");

const router = createRouter();
context.use(router);
```

Externals
---------

By default Webpack will attempt to include any dependency your code imports.
This makes it easy to use third-party modules without worrying about
[filtering `devDependencies`](BundledNodeModules.md)
but causes problems when importing modules provided by ArangoDB.

Most modules that are specific to ArangoDB or Foxx reside in the `@arangodb`
namespace. This makes it fairly straightforward to tell Webpack to ignore
them using the `externals` option:

```js
module.exports = {
  // ...
  externals: [/^@arangodb(\/|$)/]
};
```

You can also use this to exclude other modules provided by ArangoDB,
like the `joi` validation library:

```js
module.exports = {
  // ...
  externals: [/^@arangodb(\/|$)/, "joi"]
};
```

Compiling scripts
-----------------

As far as Webpack is concerned, scripts are additional entry points:

```js
const path = require("path");
module.exports = {
  // ...
  context: path.resolve(__dirname, "src"),
  entry: {
    main: "./index.js",
    setup: "./scripts/setup.js"
  }
};
```

**Note**: If your scripts are sharing a lot of code with each other or
the rest of the service this can result in some overhead as the shared code
will be included in each output file. A possible solution would be to
extract the shared code into a separe bundle.
