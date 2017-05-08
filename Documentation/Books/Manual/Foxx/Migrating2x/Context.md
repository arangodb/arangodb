The application context
=======================

The global `applicationContext` variable available in Foxx modules has been replaced with the `context` attribute of the `module` variable. For consistency it is now referred to as the *service* context throughout this documentation.

Some methods of the service context have changed in ArangoDB 3.0:

* `fileName()` now behaves like `path()` did in ArangoDB 2.x
* `path()` has been removed (use `fileName()` instead)
* `foxxFileName()` has been removed (use `fileName()` instead)

Additionally the `version` and `name` attributes have been removed and can now only be accessed via the `manifest` attribute (as `manifest.version` and `manifest.name`). Note that the corresponding manifest fields are now optional and may be omitted.

The `options` attribute has also been removed as it should be considered an implementation detail. You should instead access the `dependencies` and `configuration` attributes directly.

The internal `_prefix` attribute (which was an alias for `basePath`) and the internal `comment` and `clearComments` methods (which were used by the magical documentation comments in ArangoDB 2.x) have also been removed.

The internal `_service` attribute (which provides access to the service itself) has been renamed to `service`.
