Foxx queues
===========

`const queues = require('@arangodb/foxx/queues')`

Foxx allows defining job queues that let you perform slow or expensive actions
asynchronously. These queues can be used to send e-mails, call external APIs or
perform other actions that you do not want to perform directly or want to retry
on failure.

Foxx queue jobs can be any [script](../../Guides/Scripts.md) named in the 
[manifest](../Manifest.md) of a service in the same database.

Please note that Foxx queues are database-specific. Queues and jobs are always
relative to the database in which they are created or accessed.

For disabling the Foxx queues feature or adjusting the polling interval see the
[`foxx.queues` and `foxx.queues-poll-interval` options](../../../Programs/Arangod/Foxx.md).

For the low-level functionality see the chapter on the
[task management module](../../../Appendix/JavaScriptModules/Tasks.md).

Managing queues
---------------

### queues.create

`queues.create(name, [maxWorkers]): Queue`

Returns the queue for the given name. If the queue does not exist, a new queue
with the given name will be created. If a queue with the given name already exists
and maxWorkers is set, the queue's maximum number of workers will be updated.
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

### queues.get

`queues.get(name): Queue`

Returns the queue for the given name. If the queue does not exist an exception
is thrown instead.

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

### queues.delete

`queues.delete(name): boolean`

Returns `true` if the queue was deleted successfully.
If the queue did not exist, it returns `false` instead.
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

Queue API
---------

### queue.push

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

    Either a function that takes the number of times the job has failed before
    as input and returns the number of milliseconds to wait before trying the
    job again, or the delay to be used to calculate an
    [exponential back-off](https://en.wikipedia.org/wiki/Exponential_backoff),
    or `0` for no delay.

  * **maxFailures**: `number | Infinity` (Default: `0`):

    Number of times a single run of a job will be re-tried before it is marked
    as `"failed"`. A negative value or `Infinity` means that the job will be
    re-tried on failure indefinitely.

  * **schema**: `Schema` (optional)

    Schema to validate a job's data against before enqueuing the job.

  * **preprocess**: `Function` (optional)

    Function to pre-process a job's (validated) data before serializing it in the queue.

* **data**: `any`

  Job data of the job; must be serializable to JSON.

* **opts**: `object` (optional)

  Object with any of the following properties:

  * **success**: `Function` (optional)

    Function to be called after the job has been completed successfully.

  * **failure**: `Function` (optional)

    Function to be called after the job has failed too many times.

  * **delayUntil**: `number | Date` (Default: `Date.now()`)

    Timestamp in milliseconds (or `Date` instance) until which the execution of
    the job should be delayed.

  * **backOff**: `Function | number` (Default: `1000`)

    See *script.backOff*.

  * **maxFailures**: `number | Infinity` (Default: `0`):

    See *script.maxFailures*.

  * **repeatTimes**: `number` (Default: `0`)

    If set to a positive number, the job will be repeated this many times
    (not counting recovery when using *maxFailures*).
    If set to a negative number or `Infinity`, the job will be repeated
    indefinitely. If set to `0` the job will not be repeated.

  * **repeatUntil**: `number | Date` (optional)

    If the job is set to automatically repeat, this can be set to a timestamp
    in milliseconds (or `Date` instance) after which the job will no longer repeat.
    Setting this value to zero, a negative value or `Infinity` has no effect.

  * **repeatDelay**: `number` (Default: `0`)

    If the job is set to automatically repeat, this can be set to a non-negative
    value to set the number of milliseconds for which the job will be delayed
    before it is started again.

Note that if you pass a function for the *backOff* calculation, *success*
callback or *failure* callback options the function will be serialized to
the database as a string and therefore must not rely on any external scope
or external variables.

When the job is set to automatically repeat, the *failure* callback will only
be executed when a run of the job has failed more than *maxFailures* times.
Note that if the job fails and *maxFailures* is set, it will be rescheduled
according to the *backOff* until it has either failed too many times or
completed successfully before being scheduled according to the *repeatDelay*
again. Recovery attempts by *maxFailures* do not count towards *repeatTimes*.

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

This will *not* work, because `log` was defined outside the callback function
(the callback must be serializable to a string):

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

### queue.get

`queue.get(jobId): Job`

Creates a proxy object representing a job with the given job id.

The job will be looked up in the specified queue in the current database.

Returns the job for the given jobId. Properties of the job object will be
fetched whenever they are referenced and can not be modified.

**Arguments**

* **jobId**: `string`

  The id of the job to create a proxy object for.

**Examples**
```js
const jobId = queue.push({mount: '/logger', name: 'log'}, 'Hello World!');
const job = queue.get(jobId);
assertEqual(job.id, jobId);
```

### queue.delete

`queue.delete(jobId): boolean`

Deletes a job with the given job id.
The job will be looked up and deleted in the specified queue in the current database.

**Arguments**

* **jobId**: `string`

  The id of the job to delete.

Returns `true` if the job was deleted successfully. If the job did not exist
it returns `false` instead.

### queue.pending

`queue.pending([script]): Array<string>`

Returns an array of job ids of jobs in the given queue with the status
`"pending"`, optionally filtered by the given job type.
The jobs will be looked up in the specified queue in the current database.

**Arguments**

* **script**: `object` (optional)

  An object with the following properties:

 * **name**: `string`

  Name of the script.

 * **mount**: `string`

  Mount path of the service defining the script.

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

### queue.progress

`queue.progress([script])`

Returns an array of job ids of jobs in the given queue with the status
`"progress"`, optionally filtered by the given job type.
The jobs will be looked up in the specified queue in the current database.

**Arguments**

* **script**: `object` (optional)

  An object with the following properties:

 * **name**: `string`

  Name of the script.

 * **mount**: `string`

  Mount path of the service defining the script.

### queue.complete

`queue.complete([script]): Array<string>`

Returns an array of job ids of jobs in the given queue with the status
`"complete"`, optionally filtered by the given job type.
The jobs will be looked up in the specified queue in the current database.

**Arguments**

* **script**: `object` (optional)

  An object with the following properties:

 * **name**: `string`

  Name of the script.

 * **mount**: `string`

  Mount path of the service defining the script.

### queue.failed

`queue.failed([script]): Array<string>`

Returns an array of job ids of jobs in the given queue with the status
`"failed"`, optionally filtered by the given job type.
The jobs will be looked up in the specified queue in the current database.

**Arguments**

* **script**: `object` (optional)

  An object with the following properties:

 * **name**: `string`

  Name of the script.

 * **mount**: `string`

  Mount path of the service defining the script.

### queue.all

`queue.all([script]): Array<string>`

Returns an array of job ids of all jobs in the given queue,
optionally filtered by the given job type.
The jobs will be looked up in the specified queue in the current database.

**Arguments**

* **script**: `object` (optional)

  An object with the following properties:

 * **name**: `string`

  Name of the script.

 * **mount**: `string`

  Mount path of the service defining the script.

Job API
-------

### job.abort

`job.abort(): void`

Aborts a non-completed job.

Sets a job's status to `"failed"` if it is not already `"complete"`,
without calling the job's *onFailure* callback.
