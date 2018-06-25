Linking services together
=========================

Foxx dependencies can be declared in a [service's manifest](../Reference/Manifest.md) using the `provides` and `dependencies` fields:

- `provides` lists the dependencies a given service provides, i.e. which APIs it claims to be compatible with

- `dependencies` lists the dependencies a given service uses, i.e. which APIs its dependencies need to be compatible with

A dependency name should generally use the same format as a namespaced (org-scoped) NPM module, e.g. `@foxx/sessions`. The dependency name should follow either the case-insensitive format
`/^@[_$a-z][-_$a-z0-9]*(\/[_$a-z][-_$a-z0-9]*)*$/` or `/^[_$a-z][-_$a-z0-9]*$/`.

Dependency names refer to the external JavaScript API of a service rather than specific services implementing those APIs. Some dependency names defined by officially maintained services are:

- `@foxx/auth` (version `1.0.0`)
- `@foxx/api-keys` (version `1.0.0`)
- `@foxx/bugsnag` (versions `1.0.0` and `2.0.0`)
- `@foxx/mailgun` (versions `1.0.0` and `2.0.0`)
- `@foxx/postageapp` (versions `1.0.0` and `2.0.0`)
- `@foxx/postmark` (versions `1.0.0` and `2.0.0`)
- `@foxx/sendgrid` (versions `1.0.0` and `2.0.0`)
- `@foxx/oauth2` (versions `1.0.0` and `2.0.0`)
- `@foxx/segment-io` (versions `1.0.0` and `2.0.0`)
- `@foxx/sessions` (versions `1.0.0` and `2.0.0`)
- `@foxx/users` (versions `1.0.0`, `2.0.0` and `3.0.0`)

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

Dependencies can be configured from the web interface in a service's settings tab using the _Dependencies_ button.

<!-- TODO (Add link to relevant aardvark docs) -->

The local alias should be a valid identifier following the case-insensitive format `/^[_$a-z][-_$a-z0-9]*$/`.

The value for each dependency should be the database-relative mount path of the service (including the leading slash). In order to be usable as the dependency of another service both services need to be mounted in the same database. A service can be used to provide multiple dependencies for the same service (as long as the expected JavaScript APIs don't conflict).

A service that has unconfigured required dependencies can not be used until all of its dependencies have been configured.

It is possible to specify the mount path of a service that does not actually declare the dependency as provided. There is currently no validation beyond the manifest formats.

When a service uses another mounted service as a dependency the dependency's `main` entry file's `exports` object becomes available in the `module.context.dependencies` object of the other service:

**Examples**

Service A and Service B are mounted in the same database.
Service B has a dependency with the local alias `"greeter"`.
The dependency is configured to use the mount path of Service A.

```js
// Entry file of Service A
module.exports = {
  sayHi() {
    return "Hello";
  }
};

// Somewhere in Service B
const greeter = module.context.dependencies.greeter;
res.write(greeter.sayHi());
```
