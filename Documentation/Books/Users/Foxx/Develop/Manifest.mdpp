!CHAPTER The Manifest File

In the **manifest.json** you define the components of your application.
The content is a JSON object with the following attributes (not all
attributes are required though):

* **author**: The author name
* **configuration**: Define your app's configuration parameters.
* **contributors**: An array containing objects, each represents a contributor (with **name** and optional **email**)
* **controllers**: Map routes to FoxxControllers
* **defaultDocument**: The default document when the application's root (`/`) is called (defaults to `"index.html"`)
* **dependencies**: Map names to Foxx apps
* **description**: A short description of the application (Meta information)
* **engines**: Should be an object with **arangodb** set to the ArangoDB version your Foxx app is compatible with
* **exports**: Map names to Foxx exports
* **files**: Deliver files
* **isSystem**: Mark an application as a system application
* **keywords**: An array of keywords to help people find your Foxx app
* **lib**: Base path for all required modules
* **license**: Short form of the license (MIT, GPL...)
* **name**: Name of the application (Meta information)
* **repository**: An object with information about where you can find the repository: **type** and **url**
* **scripts**: An object with script names mapped to filenames, e.g. your app's **setup** and **teardown** scripts
* **tests**: An array of names of files containing mocha tests for the Foxx app.
* **thumbnail**: Path to a thumbnail that represents the application (Meta information)
* **version**: Current version of the application (Meta information)

If you install an application using the Foxx manager or are using the
development mode, your manifest will be checked for completeness and common errors.
You should have a look at the server log files after changing a manifest file to
get informed about potential errors in the manifest.

A more complete example for a Manifest file:

```js
{
  "name": "my_website",
  "version": "1.2.1",
  "description": "My Website with a blog and a shop",
  "thumbnail": "images/website-logo.png",

  "engines": {
    "arangodb": "^2.7.0"
  },

  "configuration": {
    "currency": {
      "description": "Currency symbol to use for prices in the shop.",
      "default": "$",
      "type": "string"
    }
  },

  "controllers": {
    "/blog": "apps/blog.js",
    "/shop": "apps/shop.js"
  },

  "exports": "index.js",

  "lib": "lib",

  "files": {
    "/images": "images"
  },

  "scripts": {
    "setup": "scripts/setup.js",
    "teardown": "scripts/teardown.js",
    "some-maintenance-script": "scripts/prune-comments.js"
  },

  "tests": [
    "test/**",
    "test-*.js"
  ],

  "dependencies": {
    "sessions": "sessions@^1.0.0",
    "systemUsers": "users",
    "mailer": {
      "name": "mailer-postmark",
      "version": "*",
      "required": false
    }
  }
}
```

!SUBSECTION The setup and teardown scripts

You can provide a path to a JavaScript file that prepares ArangoDB for your
application (or respectively removes it entirely).
Use the **setup** script to create all collections your application needs
and fill them with initial data if you want to.
Use the **teardown** script to remove all collections you have created.

Note: the setup script is called on each request in the development mode.
If your application needs to set up specific collections,
you should always check in the setup script whether they are already there.

The teardown script is called when an application is uninstalled.
It is good practice to drop any collections in the teardown script
that the application used exclusively, but this is not enforced.
Maybe there are reasons to keep application data even after removing an application.
It's up to you to decide what to do.

!SUBSECTION Mocha tests

You can provide test cases for your Foxx app using the [mocha test framework](http://mochajs.org/)
and an assertion library like [expect.js](https://github.com/Automattic/expect.js)
or [chai](http://chaijs.com) (or even the built-in assert module).

The **tests** array lists the relative paths of all mocha tests of your Foxx app.
In addition to regular paths, the array can also contain any patterns supported by
the [minimatch module](https://github.com/isaacs/minimatch), for example:

* glob matching: `./tests/*.js` will match all JS files in the folder "tests"
* globstar matching: `./tests/**` will match all files and subfolders of the folder "tests"
* brace expansion: `./tests/{a,b,c}.js` will match the files "a.js", "b.js" and "c.js"

For more information on the supported patterns see the minimatch documentation.

!SUBSECTION Configuration parameters

Foxx apps can define configuration parameters to make them more re-usable.

The **configuration** object maps names to configuration parameters:

* The **key** is the name under whicht the parameter will be available
  on your **applicationContext.configuration** object

* The **value** is a parameter definition.

The parameter definition can have the following properties:

* **description**: a human readable description of the parameter.
* **type**: the type of the configuration parameter. Default: `"string"`.
* **default**: the default value of the configuration parameter.
* **required**: whether the parameter is required. Default: `true`

The **type** can be any of the following:

* **integer** or **int**: any finite integer number.
* **boolean** or **bool**: `true` or `false`.
* **number**: any finite decimal or integer number.
* **string**: any string value.
* **json**: any well-formed JSON value.
* **password**: like *string* but will be displayed as a masked input field in the web frontend.

If the configuration has parameters that do not specify a default value,
you need to configure the app before it becomes active.
In the meantime a fallback application will be mounted that responds to all
requests with a HTTP 500 status code indicating a server-side error.

The configuration parameters of a mounted app can be adjusted
from the admin frontend by clicking the *Configuration* button in the app details
or using the **configure** command of the **foxx-manager** command-line utility.

!SUBSECTION Defining dependencies

Foxx apps can depend on other Foxx apps to be installed on the same server.

The **dependencies** object maps aliases to Foxx apps:

* The **key** is the name under which the dependency's exports will be available
  on your **applicationContext.dependencies** object.

* The **value** is a dependency definition.

The dependency definition is an object with any of the following properties:

* **name** (optional): the name of the Foxx app this app depends on.
* **version** (Default: `"*"`): a [semver](http://semver.org) version or version range of the Foxx app this app depends on.
* **required** (Default: `true`): whether the dependency is required for this app to be usable or not.

Alternatively the dependency definition can be a string using any of the following formats:

* `*` will allow using any app to be used to meet the dependency.
* `sessions` or `sessions:*` will match any app with the name `sessions`
  (such as the *sessions* app in the Foxx application store).
* `sessions:1.0.0` will match the version `1.0.0` of any app with the name `sessions`.

Instead of using a specific version number, you can also use any expression supported by
the [semver](https://github.com/npm/node-semver) module.

Currently the dependency definition names and versions are not enforced in ArangoDB
but this may change in a future version.

If an app declares any required dependencies,
you need to fulfill its dependencies before it becomes active.
In the meantime a fallback application will be mounted that responds to all
requests with a HTTP 500 status code indicating a server-side error.

The dependencies of a mounted app can be adjusted
from the admin frontend by clicking the *Dependencies* button in the app details
or using the **set-dependencies** command of the **foxx-manager** command-line utility.

For more information on dependencies see the chapter on [Foxx Exports](./Exports.md).

!SUBSECTION Defining controllers

Controllers can be defined as an object mapping routes to file names:

* The **key** is the route you want to mount at

* The **value** is the path to the JavaScript file containing the
  **FoxxController** you want to mount

You can add multiple controllers in one manifest this way.

If **controllers** is set to a string instead, it will be treated as the **value**
with the **key** being implicitly set to `"/"` (i.e. the root of your app's mount point).

In other words, the following:

```json
{
  "controllers": "my-controllers.js"
}
```

is equivalent to this:

```js
{
  "controllers": {
    "/": "my-controllers.js"
  }
}
```

!SUBSECTION The files

Deliver all files in a certain folder without modifying them. You can deliver
text files as well as binaries:

```js
"files": {
  "/images": "images"
}
```
