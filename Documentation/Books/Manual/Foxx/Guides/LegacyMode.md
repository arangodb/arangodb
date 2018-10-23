Working with 2.x services
=========================

ArangoDB 3 continues to support Foxx services written for ArangoDB 2.8 by
running them in a special legacy compatibility mode that provides access to
some of the modules and APIs no longer provided in 3.0 and beyond.

{% hint 'warning' %}
Legacy compatibility mode is strictly intended as a temporary stop
gap solution for supporting existing services while
[upgrading to ArangoDB 3.x](../Migrating2x/README.md)
and is not a permanent feature of ArangoDB or Foxx. It is considered
as deprecated from v3.4.0 on.
{% endhint %}

In order to mark an existing service as a legacy service,
make sure the following attribute is defined in the service manifest:

```json
"engines": {
  "arangodb": "^2.8.0"
}
```

This [semantic version range](http://semver.org) denotes that the service
is known to work with ArangoDB 2.8.0 and supports all newer versions of
ArangoDB up to but not including 3.0.0 and later.

Any similar version range the does not include 3.0.0 or greater will have
the same effect (e.g. `^2.5.0` will also trigger the legacy compatibility mode,
as will `1.2.3`, but `>=2.8.0` will not as it indicates compatibility
with *all* versions greater or equal 2.8.0,
not just those within the 2.x version range).

Features supported in legacy compatibility mode
-----------------------------------------------

Legacy compatibility mode supports the old manifest format, specifically:

* `main` is ignored
* `controllers` will be mounted as in 2.8
* `exports` will be executed as in 2.8

Additionally the `isSystem` attribute will be ignored if present but
does not result in a warning in legacy compatibility mode.

The Foxx console is available as the `console` pseudo-global variable
(shadowing the global console object).

The service context is available as the `applicationContext` pseudo-global
variable in the `controllers`, `exports`, `scripts` and `tests` as in 2.8.
The following additional properties are available on the service context
in legacy compatibility mode:

* `path()` is an alias for 3.x `fileName()` (using `path.join` to build file paths)
* `fileName()` behaves as in 2.x (using `fs.safeJoin` to build file paths)
* `foxxFileName()` is an alias for 2.x `fileName`
* `version` exposes the service manifest's `version` attribute
* `name` exposes the service manifest's `name` attribute
* `options` exposes the service's raw options

The following methods are removed on the service context in legacy compatibility mode:

* `use()` – use `@arangodb/foxx/controller` instead
* `apiDocumentation()` – use `controller.apiDocumentation()` instead
* `registerType()` – not supported in legacy compatibility mode

The following modules that have been removed or replaced in 3.0.0 are
available in legacy compatibility mode:

* `@arangodb/foxx/authentication`
* `@arangodb/foxx/console`
* `@arangodb/foxx/controller`
* `@arangodb/foxx/model`
* `@arangodb/foxx/query`
* `@arangodb/foxx/repository`
* `@arangodb/foxx/schema`
* `@arangodb/foxx/sessions`
* `@arangodb/foxx/template_middleware`

The `@arangodb/foxx` module also provides the same exports as in 2.8, namely:

* `Controller` from `@arangodb/foxx/controller`
* `createQuery` from `@arangodb/foxx/query`
* `Model` from `@arangodb/foxx/model`
* `Repository` from `@arangodb/foxx/repository`
* `toJSONSchema` from `@arangodb/foxx/schema`
* `getExports` and `requireApp` from `@arangodb/foxx/manager`
* `queues` from `@arangodb/foxx/queues`

Any feature not supported in 2.8 will also not work in legacy compatibility mode.
When migrating from an older version of ArangoDB it is a good idea to
migrate to ArangoDB 2.8 first for an easier upgrade path.

Additionally, please note the differences laid out in the chapter on
[migrating from pre-2.8](../Migrating2x/Wayback.md) in the migration guide.
