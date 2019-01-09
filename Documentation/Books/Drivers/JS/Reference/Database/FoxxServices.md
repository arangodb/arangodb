<!-- don't edit here, it's from https://@github.com/arangodb/arangojs.git / docs/Drivers/ -->
# Managing Foxx services

## database.listServices

`async database.listServices([excludeSystem]): Array<Object>`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Fetches a list of all installed service.

**Arguments**

- **excludeSystem**: `boolean` (Default: `true`)

  Whether system services should be excluded.

**Examples**

```js
const services = await db.listServices();

// -- or --

const services = await db.listServices(false);
```

## database.installService

`async database.installService(mount, source, [options]): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Installs a new service.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

- **source**: `Buffer | Readable | File | string`

  The service bundle to install.

- **options**: `Object` (optional)

  An object with any of the following properties:

  - **configuration**: `Object` (optional)

    An object mapping configuration option names to values.

  - **dependencies**: `Object` (optional)

    An object mapping dependency aliases to mount points.

  - **development**: `boolean` (Default: `false`)

    Whether the service should be installed in development mode.

  - **legacy**: `boolean` (Default: `false`)

    Whether the service should be installed in legacy compatibility mode.

    This overrides the `engines` option in the service manifest (if any).

  - **setup**: `boolean` (Default: `true`)

    Whether the setup script should be executed.

**Examples**

```js
const source = fs.createReadStream("./my-foxx-service.zip");
const info = await db.installService("/hello", source);

// -- or --

const source = fs.readFileSync("./my-foxx-service.zip");
const info = await db.installService("/hello", source);

// -- or --

const element = document.getElementById("my-file-input");
const source = element.files[0];
const info = await db.installService("/hello", source);
```

## database.replaceService

`async database.replaceService(mount, source, [options]): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Replaces an existing service with a new service by completely removing the old
service and installing a new service at the same mount point.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

- **source**: `Buffer | Readable | File | string`

  The service bundle to replace the existing service with.

- **options**: `Object` (optional)

  An object with any of the following properties:

  - **configuration**: `Object` (optional)

    An object mapping configuration option names to values.

    This configuration will replace the existing configuration.

  - **dependencies**: `Object` (optional)

    An object mapping dependency aliases to mount points.

    These dependencies will replace the existing dependencies.

  - **development**: `boolean` (Default: `false`)

    Whether the new service should be installed in development mode.

  - **legacy**: `boolean` (Default: `false`)

    Whether the new service should be installed in legacy compatibility mode.

    This overrides the `engines` option in the service manifest (if any).

  - **teardown**: `boolean` (Default: `true`)

    Whether the teardown script of the old service should be executed.

  - **setup**: `boolean` (Default: `true`)

    Whether the setup script of the new service should be executed.

**Examples**

```js
const source = fs.createReadStream("./my-foxx-service.zip");
const info = await db.replaceService("/hello", source);

// -- or --

const source = fs.readFileSync("./my-foxx-service.zip");
const info = await db.replaceService("/hello", source);

// -- or --

const element = document.getElementById("my-file-input");
const source = element.files[0];
const info = await db.replaceService("/hello", source);
```

## database.upgradeService

`async database.upgradeService(mount, source, [options]): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Replaces an existing service with a new service while retaining the old
service's configuration and dependencies.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

- **source**: `Buffer | Readable | File | string`

  The service bundle to replace the existing service with.

- **options**: `Object` (optional)

  An object with any of the following properties:

  - **configuration**: `Object` (optional)

    An object mapping configuration option names to values.

    This configuration will be merged into the existing configuration.

  - **dependencies**: `Object` (optional)

    An object mapping dependency aliases to mount points.

    These dependencies will be merged into the existing dependencies.

  - **development**: `boolean` (Default: `false`)

    Whether the new service should be installed in development mode.

  - **legacy**: `boolean` (Default: `false`)

    Whether the new service should be installed in legacy compatibility mode.

    This overrides the `engines` option in the service manifest (if any).

  - **teardown**: `boolean` (Default: `false`)

    Whether the teardown script of the old service should be executed.

  - **setup**: `boolean` (Default: `true`)

    Whether the setup script of the new service should be executed.

**Examples**

```js
const source = fs.createReadStream("./my-foxx-service.zip");
const info = await db.upgradeService("/hello", source);

// -- or --

const source = fs.readFileSync("./my-foxx-service.zip");
const info = await db.upgradeService("/hello", source);

// -- or --

const element = document.getElementById("my-file-input");
const source = element.files[0];
const info = await db.upgradeService("/hello", source);
```

## database.uninstallService

`async database.uninstallService(mount, [options]): void`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Completely removes a service from the database.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

- **options**: `Object` (optional)

  An object with any of the following properties:

  - **teardown**: `boolean` (Default: `true`)

    Whether the teardown script should be executed.

**Examples**

```js
await db.uninstallService("/my-service");
// service was uninstalled
```

## database.getService

`async database.getService(mount): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Retrieves information about a mounted service.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const info = await db.getService("/my-service");
// info contains detailed information about the service
```

## database.getServiceConfiguration

`async database.getServiceConfiguration(mount, [minimal]): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Retrieves an object with information about the service's configuration options
and their current values.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

- **minimal**: `boolean` (Default: `false`)

  Only return the current values.

**Examples**

```js
const config = await db.getServiceConfiguration("/my-service");
// config contains information about the service's configuration
```

## database.replaceServiceConfiguration

`async database.replaceServiceConfiguration(mount, configuration, [minimal]): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Replaces the configuration of the given service.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

- **configuration**: `Object`

  An object mapping configuration option names to values.

- **minimal**: `boolean` (Default: `false`)

  Only return the current values and warnings (if any).

  **Note:** when using ArangoDB 3.2.8 or older, enabling this option avoids
  triggering a second request to the database.

**Examples**

```js
const config = { currency: "USD", locale: "en-US" };
const info = await db.replaceServiceConfiguration("/my-service", config);
// info.values contains information about the service's configuration
// info.warnings contains any validation errors for the configuration
```

## database.updateServiceConfiguration

`async database.updateServiceConfiguration(mount, configuration, [minimal]): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Updates the configuration of the given service my merging the new values into
the existing ones.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

- **configuration**: `Object`

  An object mapping configuration option names to values.

- **minimal**: `boolean` (Default: `false`)

  Only return the current values and warnings (if any).

  **Note:** when using ArangoDB 3.2.8 or older, enabling this option avoids
  triggering a second request to the database.

**Examples**

```js
const config = { locale: "en-US" };
const info = await db.updateServiceConfiguration("/my-service", config);
// info.values contains information about the service's configuration
// info.warnings contains any validation errors for the configuration
```

## database.getServiceDependencies

`async database.getServiceDependencies(mount, [minimal]): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Retrieves an object with information about the service's dependencies and their
current mount points.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

- **minimal**: `boolean` (Default: `false`)

  Only return the current values and warnings (if any).

**Examples**

```js
const deps = await db.getServiceDependencies("/my-service");
// deps contains information about the service's dependencies
```

## database.replaceServiceDependencies

`async database.replaceServiceDependencies(mount, dependencies, [minimal]): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Replaces the dependencies for the given service.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

- **dependencies**: `Object`

  An object mapping dependency aliases to mount points.

- **minimal**: `boolean` (Default: `false`)

  Only return the current values and warnings (if any).

  **Note:** when using ArangoDB 3.2.8 or older, enabling this option avoids
  triggering a second request to the database.

**Examples**

```js
const deps = { mailer: "/mailer-api", auth: "/remote-auth" };
const info = await db.replaceServiceDependencies("/my-service", deps);
// info.values contains information about the service's dependencies
// info.warnings contains any validation errors for the dependencies
```

## database.updateServiceDependencies

`async database.updateServiceDependencies(mount, dependencies, [minimal]): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Updates the dependencies for the given service by merging the new values into
the existing ones.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

- **dependencies**: `Object`

  An object mapping dependency aliases to mount points.

- **minimal**: `boolean` (Default: `false`)

  Only return the current values and warnings (if any).

  **Note:** when using ArangoDB 3.2.8 or older, enabling this option avoids
  triggering a second request to the database.

**Examples**

```js
const deps = { mailer: "/mailer-api" };
const info = await db.updateServiceDependencies("/my-service", deps);
// info.values contains information about the service's dependencies
// info.warnings contains any validation errors for the dependencies
```

## database.enableServiceDevelopmentMode

`async database.enableServiceDevelopmentMode(mount): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Enables development mode for the given service.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const info = await db.enableServiceDevelopmentMode("/my-service");
// the service is now in development mode
// info contains detailed information about the service
```

## database.disableServiceDevelopmentMode

`async database.disableServiceDevelopmentMode(mount): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Disabled development mode for the given service and commits the service state to
the database.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const info = await db.disableServiceDevelopmentMode("/my-service");
// the service is now in production mode
// info contains detailed information about the service
```

## database.listServiceScripts

`async database.listServiceScripts(mount): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Retrieves a list of the service's scripts.

Returns an object mapping each name to a more readable representation.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const scripts = await db.listServiceScripts("/my-service");
// scripts is an object listing the service scripts
```

## database.runServiceScript

`async database.runServiceScript(mount, name, [scriptArg]): any`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Runs a service script and returns the result.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

- **name**: `string`

  Name of the script to execute.

- **scriptArg**: `any`

  Value that will be passed as an argument to the script.

**Examples**

```js
const result = await db.runServiceScript("/my-service", "setup");
// result contains the script's exports (if any)
```

## database.runServiceTests

`async database.runServiceTests(mount, [reporter]): any`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Runs the tests of a given service and returns a formatted report.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database

- **options**: `Object` (optional)

  An object with any of the following properties:

  - **reporter**: `string` (Default: `default`)

    The reporter to use to process the test results.

    As of ArangoDB 3.2 the following reporters are supported:

    - **stream**: an array of event objects
    - **suite**: nested suite objects with test results
    - **xunit**: JSONML representation of an XUnit report
    - **tap**: an array of TAP event strings
    - **default**: an array of test results

  - **idiomatic**: `boolean` (Default: `false`)

    Whether the results should be converted to the apropriate `string`
    representation:

    - **xunit** reports will be formatted as XML documents
    - **tap** reports will be formatted as TAP streams
    - **stream** reports will be formatted as JSON-LD streams

**Examples**

```js
const opts = { reporter: "xunit", idiomatic: true };
const result = await db.runServiceTests("/my-service", opts);
// result contains the XUnit report as a string
```

## database.downloadService

`async database.downloadService(mount): Buffer | Blob`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Retrieves a zip bundle containing the service files.

Returns a `Buffer` in Node or `Blob` in the browser version.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const bundle = await db.downloadService("/my-service");
// bundle is a Buffer/Blob of the service bundle
```

## database.getServiceReadme

`async database.getServiceReadme(mount): string?`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Retrieves the text content of the service's `README` or `README.md` file.

Returns `undefined` if no such file could be found.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const readme = await db.getServiceReadme("/my-service");
// readme is a string containing the service README's
// text content, or undefined if no README exists
```

## database.getServiceDocumentation

`async database.getServiceDocumentation(mount): Object`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Retrieves a Swagger API description object for the service installed at the
given mount point.

**Arguments**

- **mount**: `string`

  The service's mount point, relative to the database.

**Examples**

```js
const spec = await db.getServiceDocumentation("/my-service");
// spec is a Swagger API description of the service
```

## database.commitLocalServiceState

`async database.commitLocalServiceState([replace]): void`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.2 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Writes all locally available services to the database and updates any service
bundles missing in the database.

**Arguments**

- **replace**: `boolean` (Default: `false`)

  Also commit outdated services.

  This can be used to solve some consistency problems when service bundles are
  missing in the database or were deleted manually.

**Examples**

```js
await db.commitLocalServiceState();
// all services available on the coordinator have been written to the db

// -- or --

await db.commitLocalServiceState(true);
// all service conflicts have been resolved in favor of this coordinator
```
