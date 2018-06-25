# Scripts

Foxx lets you define scripts that can be executed as part of the installation and removal process, invoked manually or scheduled to run at a later time using the job queue.

To register your script, just add a `scripts` section to your [service manifest](Manifest.md):

```json
{
  ...
  "scripts": {
    "setup": "scripts/setup.js",
    "send-mail": "scripts/send-mail.js"
  }
  ...
}
```

The scripts you define in your service manifest can be invoked from the web interface in the service's settings page with the *Scripts* dropdown.

<!-- TODO (Link to admin docs) -->

You can also use the scripts as queued jobs:

```js
'use strict';
const queues = require('@arangodb/foxx/queues');
queues.get('default').push(
  {mount: '/my-service-mount-point', name: 'send-mail'},
  {to: 'user@example.com', body: 'Hello'}
);
```

## Script arguments and return values

If the script was invoked with any arguments, you can access them using the `module.context.argv` array.

To return data from your script, you can assign the data to `module.exports` as usual. Please note that this data will be converted to JSON.

Any errors raised by the script will be handled depending on how the script was invoked:

* if the script was invoked from the HTTP API (e.g. using the web interface), it will return an error response using the exception's `statusCode` property if specified or 500.
* if the script was invoked from a Foxx job queue, the job's failure counter will be incremented and the job will be rescheduled or marked as failed if no attempts remain.

**Examples**

Let's say you want to define a script that takes two numeric values and returns the result of multiplying them:

```js
'use strict';
const assert = require('assert');
const argv = module.context.argv;

assert.equal(argv.length, 2, 'Expected exactly two arguments');
assert.equal(typeof argv[0], 'number', 'Expected first argument to be a number');
assert.equal(typeof argv[1], 'number', 'Expected second argument to be a number');

module.exports = argv[0] * argv[1];
```

## Lifecycle Scripts

Foxx recognizes lifecycle scripts if they are defined and will invoke them during the installation, update and removal process of the service if you want.

The following scripts are currently recognized as lifecycle scripts by their name: `"setup"` and `"teardown"`.

### Setup Script

The setup script will be executed without arguments during the installation of your Foxx service.

The setup script may be executed more than once and should therefore be treated as reentrant. Running the same setup script again should not result in any errors or duplicate data.

The setup script is typically used to create collections your service needs or insert seed data like initial administrative user accounts and so on.

**Examples**

```js
'use strict';
const db = require('@arangodb').db;
const textsCollectionName = module.context.collectionName('texts');
// `textsCollectionName` is now the prefixed name of this service's "texts" collection.
// e.g. "example_texts" if the service has been mounted at `/example`

if (db._collection(textsCollectionName) === null) {
  const collection = db._create(textsCollectionName);

  collection.save({text: 'entry 1 from collection texts'});
  collection.save({text: 'entry 2 from collection texts'});
  collection.save({text: 'entry 3 from collection texts'});
} else {
  console.debug(`collection ${texts} already exists. Leaving it untouched.`);
}
```

### Teardown Script

The teardown script will be executed without arguments during the removal of your Foxx service.

It can also optionally be executed before upgrading an service.

This script typically removes the collections and/or documents created by your service's setup script.

**Examples**

```js
'use strict';
const db = require('@arangodb').db;

const textsCollection = module.context.collection('texts');

if (textsCollection) {
  textsCollection.drop();
}
```


<!--

Bla bla `setup` is used automatically on install/upgrade/replace unless you disable via option. Use `teardown` to clean up when service is uninstalled.

Since the `setup` script can be run multiple times on an already installed service it's a good idea to make it reentrant (i.e. not result in errors if parts of it already have been run before).

The `teardown` script should remove all traces of the service for a clean uninstall. Keep in mind this results in catastrophic data loss. May want to rename to something else and invoke manually if that's too risky.

## Migrations

Running large `setup` on every upgrade can be very expensive during development. Alternative solution is to only handle basic collection creation and indexes and have one-off migration scripts and invoke them manually. Bla bla.

-->