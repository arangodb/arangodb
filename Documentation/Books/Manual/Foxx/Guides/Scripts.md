Scripts and scheduling
======================

In addition to the main entry point which defines your service's [routes](Routing.md) and [exports](Dependencies.md) you can define scripts that need to be invoked directly and can be used to implement one-off tasks or scheduled and recurring jobs using queues.

These scripts can be declared in the `scripts` section of the [service manifest](Manifest.md):

```json
"scripts": {
  "setup": "scripts/setup.js",
  "send-mail": "scripts/send-mail.js"
}
```

Invoking scripts
----------------

Scripts can be invoked manually using the [web interface](), the [Foxx CLI]() or the [Foxx HTTP API]().

Additionally the special `setup` and `teardown` lifecycle scripts can be invoked automatically by Foxx as part of a service's lifecycle (see below).

Script arguments and return values
----------------------------------

When invoking a script any arguments will be exposed to the script as the `argv` array property of the [service context](../Reference/Context.md).

Any value exported by the script using `module.exports` will be the script's return value. Please note that this data will be converted to JSON.

Any errors raised by the script will be handled depending on how the script was invoked:

* if the script was invoked manually (e.g. using the Foxx CLI), it will return an error response using the exception's `statusCode` property or `500`.

* if the script was invoked from a Foxx job queue, the job's failure counter will be incremented and the job will be rescheduled or marked as failed if no attempts remain.

**Examples**

The following script will use its argument to generate a personal greeting:

```js
'use strict';
const { argv } = module.context;

module.exports = `Hello ${argv[0]}!`;
```

Lifecycle Scripts
-----------------

Scripts named `setup` or `teardown` are considered lifecycle scripts and will (by default) be invoked automatically by Foxx:

* when a service is installed, upgraded or replaced, the new service's `setup` script will be executed before it is mounted

* when a service is removed or replaced, the old service's `teardown` script will be executed before it is unmounted

* when a service is upgraded, the old service's `teardown` script *can* optionally be executed before it is unmounted

However it's possible to override this behavior as needed.

Note that in these cases the scripts will always be invoked without arguments and their exports will be ignored.

### Setup Script

The setup script is typically used to create collections a service needs, to define indexes or to initialize collections with necessary data like administrative accounts.

As the setup script may be executed more than once it should be treated as reentrant: running the setup script again should not result in any errors or duplicate data:

```js
const { db } = require("@arangodb");
const users = module.context.collectionName("users");

if (!db._collection(users)) {
  // This won't be run if the collection exists
  const collection = db._createDocumentCollection(users);
  collection.ensureIndex({
    type: "hash",
    unique: true,
    fields: ["username"]
  });
  collection.save({
    username: "admin",
    password: auth.create("hunter2")
  });
}
```

### Teardown Script

The teardown script typically removes the collections and/or documents created by the service's setup script.

In practice teardown scripts are rarely used due to the risk of catastrophic data loss when accidentally running the script while managing the service.

Migrations
----------

Depending on the amount of data managed by the service and the amount of work that needs to be done to prepare collections for the service, running a `setup` script on every upgrade can be very expensive.

An alternative approach is to perform incremental steps in separate migration scripts and run them manually after the service is installed.

A `setup` script should always create all the collections a service uses but any additional steps like creating indexes, importing data fixtures or migrating existing data can safely be performed in separate scripts.

Queues
------

Services can schedule scripts of any service mounted in the same database using [Foxx queues](../Reference/Modules/Queues.md):

```js
"use strict";
const queues = require("@arangodb/foxx/queues");
const queue = queues.get("default");

// later
router.post("/signup", (req, res) => {
  const user = performSignup(req.body);
  // schedule sending welcome e-mail using a script
  queue.push(
    {
      mount: module.context.mount, // i.e. this current service
      name: "send-mail" // script name in the service manifest
    },
    { to: user.email, body: welcomeEmailText } // arguments
  );
});
```

