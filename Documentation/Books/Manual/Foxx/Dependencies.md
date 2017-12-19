Dependency management
=====================

There are two things commonly called "dependencies" in Foxx:

* Module dependencies, i.e. dependencies on external JavaScript modules (e.g. from the public npm registry)

* Foxx dependencies, i.e. dependencies between Foxx services

Let's look at them in more detail:

Module dependencies
-------------------

You can use the `node_modules` folder to bundle third-party Foxx-compatible npm and Node.js modules with your Foxx service. Typically this is achieved by adding a `package.json` file to your project specifying npm dependencies using the `dependencies` attribute and installing them with the npm command-line tool.

Make sure to include the actual `node_modules` folder in your Foxx service bundle as ArangoDB will not do anything special to install these dependencies. Also keep in mind that bundling extraneous modules like development dependencies may bloat the file size of your Foxx service bundle.

### Compatibility caveats

Unlike JavaScript in browsers or Node.js, the JavaScript environment in ArangoDB is synchronous. This means any modules that depend on asynchronous behaviour like promises or `setTimeout` will not behave correctly in ArangoDB or Foxx. Additionally unlike Node.js ArangoDB does not support native extensions. All modules have to be implemented in pure JavaScript.

While ArangoDB provides a lot of compatibility code to support modules written for Node.js, some Node.js built-in modules can not be provided by ArangoDB. For a closer look at the Node.js modules ArangoDB does or does not provide check out the [appendix on JavaScript modules](../Appendix/JavaScriptModules/README.md).

Also note that these restrictions not only apply on the modules you wish to install but also the dependencies of those modules. As a rule of thumb: modules written to work in Node.js and the browser that do not rely on async behaviour should generally work; modules that rely on network or filesystem I/O or make heavy use of async behaviour most likely will not.

Foxx dependencies
-----------------

Foxx dependencies can be declared in a [service's manifest](Manifest.md) using the `provides` and `dependencies` fields:

* `provides` lists the dependencies a given service provides, i.e. which APIs it claims to be compatible with

* `dependencies` lists the dependencies a given service uses, i.e. which APIs its dependencies need to be compatible with

A dependency name should generally use the same format as a namespaced (org-scoped) NPM module, e.g. `@foxx/sessions`.

Dependency names refer to the external JavaScript API of a service rather than specific services implementing those APIs. Some dependency names defined by officially maintained services are:

* `@foxx/auth` (version `1.0.0`)
* `@foxx/api-keys` (version `1.0.0`)
* `@foxx/bugsnag` (versions `1.0.0` and `2.0.0`)
* `@foxx/mailgun` (versions `1.0.0` and `2.0.0`)
* `@foxx/postageapp` (versions `1.0.0` and `2.0.0`)
* `@foxx/postmark` (versions `1.0.0` and `2.0.0`)
* `@foxx/sendgrid` (versions `1.0.0` and `2.0.0`)
* `@foxx/oauth2` (versions `1.0.0` and `2.0.0`)
* `@foxx/segment-io` (versions `1.0.0` and `2.0.0`)
* `@foxx/sessions` (versions `1.0.0` and `2.0.0`)
* `@foxx/users` (versions `1.0.0`, `2.0.0` and `3.0.0`)

A `provides` definition maps each provided dependency's name to the provided version:

```json
"provides": {
  "@foxx/auth": "1.0.0"
}
```

A `dependencies` definition maps the local alias of a given dependency against its name and the supported version range (either as a JSON object or a shorthand string):

```json
"dependencies": {
  "mySessions": "@foxx/sessions:^2.0.0",
  "myAuth": {
    "name": "@foxx/auth",
    "version": "^1.0.0",
    "description": "This description is entirely optional.",
    "required": false
  }
}
```

Dependencies can be configured from the web interface in a service's settings tab using the *Dependencies* button.

<!-- TODO (Add link to relevant aardvark docs) -->

The value for each dependency should be the database-relative mount path of the service (including the leading slash). In order to be usable as the dependency of another service both services need to be mounted in the same database. A service can be used to provide multiple dependencies for the same service (as long as the expected JavaScript APIs don't conflict).

A service that has unconfigured required dependencies can not be used until all of its dependencies have been configured.

It is possible to specify the mount path of a service that does not actually declare the dependency as provided. There is currently no validation beyond the manifest formats.

When a service uses another mounted service as a dependency the dependency's `main` entry file's `exports` object becomes available in the `module.context.dependencies` object of the other service:

**Examples**

Service A and Service B are mounted in the same database.
Service B has a dependency with the local alias `"greeter"`.
The dependency is configured to use the mount path of Service B.

```js
// Entry file of Service A
module.exports = {
  sayHi () {
    return 'Hello';
  }
};

// Somewhere in Service B
const greeter = module.context.dependencies.greeter;
res.write(greeter.sayHi());
```
