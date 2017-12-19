Foxx scripts and queued jobs
============================

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

Script arguments and return values
----------------------------------

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

Lifecycle Scripts
-----------------

Foxx recognizes lifecycle scripts if they are defined and will invoke them during the installation, update and removal process of the service if you want.

The following scripts are currently recognized as lifecycle scripts by their name: `"setup"` and `"teardown"`.

### Setup Script

The setup script will be executed without arguments during the installation of your Foxx service.

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
  console.log(`collection ${texts} already exists. Leaving it untouched.`);
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

Queues
------

Foxx allows defining job queues that let you perform slow or expensive actions asynchronously. These queues can be used to send e-mails, call external APIs or perform other actions that you do not want to perform directly or want to retry on failure.

@startDocuBlock foxxQueues

Please note that Foxx job queues are database-specific. Queues and jobs are always relative to the database in which they are created or accessed.

@startDocuBlock foxxQueuesPollInterval

For the low-level functionality see the chapter on the [task management module](../Appendix/JavaScriptModules/Tasks.md).

### Creating or updating a queue

`queues.create(name, [maxWorkers]): Queue`

Returns the queue for the given name. If the queue does not exist, a new queue with the given name will be created. If a queue with the given name already exists and maxWorkers is set, the queue's maximum number of workers will be updated.
The queue will be created in the current database.

**Arguments**

* **name**: `string`

  Name of the queue to create.

* **maxWorkers**: `number` (Default: `1`)

  The maximum number of workers.

**Examples**

```js
// Create a queue with the default number of workers (i.e. one)
const queue1 = queues.create("my-queue");
// Create a queue with a given number of workers
const queue2 = queues.create("another-queue", 2);
// Update the number of workers of an existing queue
const queue3 = queues.create("my-queue", 10);
// queue1 and queue3 refer to the same queue
assertEqual(queue1, queue3);
```

### Fetching an existing queue

`queues.get(name): Queue`

Returns the queue for the given name. If the queue does not exist an exception is thrown instead.

The queue will be looked up in the current database.

**Arguments**

* **name**: `string`

  Name of the queue to fetch.

**Examples**

If the queue does not yet exist an exception is thrown:

```js
queues.get("some-queue");
// Error: Queue does not exist: some-queue
//     at ...
```

Otherwise the queue will be returned:

```js
const queue1 = queues.create("some-queue");
const queue2 = queues.get("some-queue");
assertEqual(queue1, queue2);
```

### Deleting a queue

`queues.delete(name): boolean`

Returns `true` if the queue was deleted successfully. If the queue did not exist, it returns `false` instead.
The queue will be looked up and deleted in the current database.

When a queue is deleted, jobs on that queue will no longer be executed.

Deleting a queue will not delete any jobs on that queue.

**Arguments**

* **name**: `string`

  Name of the queue to delete.

**Examples**

```js
const queue = queues.create("my-queue");
queues.delete("my-queue"); // true
queues.delete("my-queue"); // false
```

### Adding a job to a queue

`queue.push(script, data, [opts]): string`

The job will be added to the specified queue in the current database.

Returns the job id.

**Arguments**

* **script**: `object`

  A job type definition, consisting of an object with the following properties:

  * **name**: `string`

    Name of the script that will be invoked.

  * **mount**: `string`

    Mount path of the service that defines the script.

  * **backOff**: `Function | number` (Default: `1000`)

    Either a function that takes the number of times the job has failed before as input and returns the number of milliseconds to wait before trying the job again, or the delay to be used to calculate an [exponential back-off](https://en.wikipedia.org/wiki/Exponential_backoff), or `0` for no delay.

  * **maxFailures**: `number | Infinity` (Default: `0`):

    Number of times a single run of a job will be re-tried before it is marked as `"failed"`. A negative value or `Infinity` means that the job will be re-tried on failure indefinitely.

  * **schema**: `Schema` (optional)

    Schema to validate a job's data against before enqueuing the job.

  * **preprocess**: `Function` (optional)

    Function to pre-process a job's (validated) data before serializing it in the queue.

  * **repeatTimes**: `Function` (Default: `0`)

    If set to a positive number, the job will be repeated this many times (not counting recovery when using *maxFailures*). If set to a negative number or `Infinity`, the job will be repeated indefinitely. If set to `0` the job will not be repeated.

  * **repeatUntil**: `number | Date` (optional)

    If the job is set to automatically repeat, this can be set to a timestamp in milliseconds (or `Date` instance) after which the job will no longer repeat. Setting this value to zero, a negative value or `Infinity` has no effect.

  * **repeatDelay**: `number` (Default: `0`)

    If the job is set to automatically repeat, this can be set to a non-negative value to set the number of milliseconds for which the job will be delayed before it is started again.

* **data**: `any`

  Job data of the job; must be serializable to JSON.

* **opts**: `object` (optional)

  Object with any of the following properties:

 * **success**: `Function` (optional)

  Function to be called after the job has been completed successfully.

 * **failure**: `Function` (optional)

  Function to be called after the job has failed too many times.

 * **delayUntil**: `number | Date` (Default: `Date.now()`)

  Timestamp in milliseconds (or `Date` instance) until which the execution of the job should be delayed.

 * **backOff**: `Function | number` (Default: `1000`)

  See *script.backOff*.

 * **maxFailures**: `number | Infinity` (Default: `0`):

  See *script.maxFailures*.

 * **repeatTimes**: `Function` (Default: `0`)

  See *script.repeatTimes*.

 * **repeatUntil**: `number | Date` (optional)

  See *script.repeatUntil*.

 * **repeatDelay**: `number` (Default: `0`)

  See *script.repeatDelay*.

Note that if you pass a function for the *backOff* calculation, *success* callback or *failure* callback options the function will be serialized to the database as a string and therefore must not rely on any external scope or external variables.

When the job is set to automatically repeat, the *failure* callback will only be executed when a run of the job has failed more than *maxFailures* times. Note that if the job fails and *maxFailures* is set, it will be rescheduled according to the *backOff* until it has either failed too many times or completed successfully before being scheduled according to the *repeatDelay* again. Recovery attempts by *maxFailures* do not count towards *repeatTimes*.

The *success* and *failure* callbacks receive the following arguments:

* **result**: `any`

  The return value of the script for the current run of the job.

* **jobData**: `any`

  The data passed to this method.

* **job**: `object`

  ArangoDB document representing the job's current state.

**Examples**

Let's say we have an service mounted at `/mailer` that provides a script called `send-mail`:

```js
'use strict';
const queues = require('@arangodb/foxx/queues');
const queue = queues.create('my-queue');
queue.push(
  {mount: '/mailer', name: 'send-mail'},
  {to: 'hello@example.com', body: 'Hello world'}
);
```

This will *not* work, because `log` was defined outside the callback function (the callback must be serializable to a string):

```js
// WARNING: THIS DOES NOT WORK!
'use strict';
const queues = require('@arangodb/foxx/queues');
const queue = queues.create('my-queue');
const log = require('console').log; // outside the callback's function scope
queue.push(
  {mount: '/mailer', name: 'send-mail'},
  {to: 'hello@example.com', body: 'Hello world'},
  {success: function () {
    log('Yay!'); // throws 'log is not defined'
  }}
);
```

Here's an example of a job that will be executed every 5 seconds until tomorrow:

```js
'use strict';
const queues = require('@arangodb/foxx').queues;
const queue = queues.create('my-queue');
queue.push(
  {mount: '/mailer', name: 'send-mail'},
  {to: 'hello@example.com', body: 'Hello world'},
  {
    repeatTimes: Infinity,
    repeatUntil: Date.now() + (24 * 60 * 60 * 1000),
    repeatDelay: 5 * 1000
  }
);
```

### Fetching a job from the queue

`queue.get(jobId): Job`

Creates a proxy object representing a job with the given job id.

The job will be looked up in the specified queue in the current database.

Returns the job for the given jobId. Properties of the job object will be fetched whenever they are referenced and can not be modified.

**Arguments**

* **jobId**: `string`

  The id of the job to create a proxy object for.

**Examples**
```js
const jobId = queue.push({mount: '/logger', name: 'log'}, 'Hello World!');
const job = queue.get(jobId);
assertEqual(job.id, jobId);
```

### Deleting a job from the queue

`queue.delete(jobId): boolean`

Deletes a job with the given job id.
The job will be looked up and deleted in the specified queue in the current database.

**Arguments**

* **jobId**: `string`

  The id of the job to delete.

Returns `true` if the job was deleted successfully. If the job did not exist it returns `false` instead.

### Fetching an array of jobs in a queue

**Examples**

```js
const logScript = {mount: '/logger', name: 'log'};
queue.push(logScript, 'Hello World!', {delayUntil: Date.now() + 50});
assertEqual(queue.pending(logScript).length, 1);
// 50 ms later...
assertEqual(queue.pending(logScript).length, 0);
assertEqual(queue.progress(logScript).length, 1);
// even later...
assertEqual(queue.progress(logScript).length, 0);
assertEqual(queue.complete(logScript).length, 1);
```

#### Fetching an array of pending jobs in a queue

`queue.pending([script]): Array<string>`

Returns an array of job ids of jobs in the given queue with the status `"pending"`, optionally filtered by the given job type.
The jobs will be looked up in the specified queue in the current database.

**Arguments**

* **script**: `object` (optional)

  An object with the following properties:

 * **name**: `string`

  Name of the script.

 * **mount**: `string`

  Mount path of the service defining the script.

#### Fetching an array of jobs that are currently in progress

`queue.progress([script])`

Returns an array of job ids of jobs in the given queue with the status `"progress"`, optionally filtered by the given job type.
The jobs will be looked up in the specified queue in the current database.

**Arguments**

* **script**: `object` (optional)

  An object with the following properties:

 * **name**: `string`

  Name of the script.

 * **mount**: `string`

  Mount path of the service defining the script.

#### Fetching an array of completed jobs in a queue

`queue.complete([script]): Array<string>`

Returns an array of job ids of jobs in the given queue with the status `"complete"`, optionally filtered by the given job type.
The jobs will be looked up in the specified queue in the current database.

**Arguments**

* **script**: `object` (optional)

  An object with the following properties:

 * **name**: `string`

  Name of the script.

 * **mount**: `string`

  Mount path of the service defining the script.

#### Fetching an array of failed jobs in a queue

`queue.failed([script]): Array<string>`

Returns an array of job ids of jobs in the given queue with the status `"failed"`, optionally filtered by the given job type.
The jobs will be looked up in the specified queue in the current database.

**Arguments**

* **script**: `object` (optional)

  An object with the following properties:

 * **name**: `string`

  Name of the script.

 * **mount**: `string`

  Mount path of the service defining the script.

#### Fetching an array of all jobs in a queue

`queue.all([script]): Array<string>`

Returns an array of job ids of all jobs in the given queue, optionally filtered by the given job type.
The jobs will be looked up in the specified queue in the current database.

**Arguments**

* **script**: `object` (optional)

  An object with the following properties:

 * **name**: `string`

  Name of the script.

 * **mount**: `string`

  Mount path of the service defining the script.

### Aborting a job

`job.abort(): void`

Aborts a non-completed job.

Sets a job's status to `"failed"` if it is not already `"complete"`, without calling the job's *onFailure* callback.
