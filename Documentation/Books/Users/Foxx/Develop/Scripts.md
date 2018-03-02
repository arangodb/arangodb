Scripts and the Job Queue
=========================

Foxx allows you to define scripts that can be executed as part of the installation and removal process, invoked manually or scheduled to run at a later time using the job queue.

To register your script, just add a **scripts** section to your application manifest:

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

The scripts you define in your application manifest can be invoked from the web admin frontend or with the **foxx-manager** CLI:

```sh
unix>foxx-manager run /my-app-mount-point send-mail '{"to": "user@example.com", "body": "Hello"}'
```

You can also use the script with the Foxx job queue:

```js
var Foxx = require('org/arangodb/foxx');
var queue = Foxx.queues.get('default');
queue.push(
  {mount: '/my-app-mount-point', name: 'send-mail'},
  {to: 'user@example.com', body: 'Hello'}
);
```

For more information the Foxx job queue, see [the chapter about queues](./Queues.md).

Script arguments and return values
----------------------------------

If the script was invoked with any arguments, you can access them using the **applicationContext.argv** array.

To return data from your script, you can assign the data to **module.exports** as usual. Please note that this data will be converted to JSON.

Any errors raised by the script will be handled depending on how the script was invoked:

* if the script was invoked with the **foxx-manager** CLI, it will exit with a non-zero exit status and print the error message.
* if the script was invoked from the HTTP API (e.g. using the web admin frontend), it will return an error response using the exception's `statusCode` property if specified or 500.
* if the script was invoked from the Foxx job queue, the job's failure counter will be incremented and the job will be rescheduled or marked as failed if no attempts remain.

**Examples**

Let's say you want to define a script that takes two numeric values and returns the result of multiplying them:

```js
var assert = require('assert');
var argv = applicationContext.argv;

assert.equal(argv.length, 2, 'Expected exactly two arguments');
assert.equal(typeof argv[0], 'number', 'Expected first argument to be a number');
assert.equal(typeof argv[1], 'number', 'Expected second argument to be a number');

module.exports = argv[0] * argv[1];
```

Life-Cycle Scripts
------------------

Foxx recognizes life-cycle scripts if they are defined and will invoke them during the installation, update and removal process of the app if you want.

The following scripts are currently recognized as life-cycle scripts:

### Setup Script

The **setup** script will be executed without arguments during the installation of your Foxx app:

```sh
unix>foxx-manager install hello-foxx /example
```

Note that the app may not yet have been configured when the setup script is executed, so the **applicationContext.configuration** and **applicationContext.dependencies** may be unavailable.

The setup script is typically used to create collections your app needs or insert seed data like initial administrative user accounts and so on.

**Examples**

```js
var db = require("org/arangodb").db;
var textsCollectionName = applicationContext.collectionName("texts");
// `textsCollectionName` is now the prefixed name of this app's "texts" collection.
// e.g. "example_texts" if the app has been mounted at `/example`

if (db._collection(textsCollectionName) === null) {
  var collection = db._create(textsCollectionName);

  collection.save({ text: "entry 1 from collection texts" });
  collection.save({ text: "entry 2 from collection texts" });
  collection.save({ text: "entry 3 from collection texts" });
} else {
  console.log("collection '%s' already exists. Leaving it untouched.", texts);
}
```

### Teardown Script

The **teardown** script will be executed without arguments during the removal of your Foxx app:

```sh
unix>foxx-manager uninstall /example
````

It can also optionally be executed before upgrading an app.

This script typically removes the collections and/or documents created by your app's **setup** script.

**Examples**

```js
var db = require("org/arangodb").db;

var textsCollection = applicationContext.collection("texts");

if (textsCollection) {
  textsCollection.drop();
}
```