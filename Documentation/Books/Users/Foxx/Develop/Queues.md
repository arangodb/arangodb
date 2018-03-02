Foxx Job Queues
===============

If you have never built a Foxx App, then you can make your own [first Foxx App](https://docs.arangodb.com/cookbook/Foxx/FirstSteps.html) using our [cookbook](https://docs.arangodb.com/cookbook).


Foxx allows defining job queues that let you perform slow or expensive actions asynchronously. These queues can be used to send e-mails, call external APIs or perform other actions that you do not want to perform directly or want to retry on failure.

@startDocuBlock foxxQueues

Please note that Foxx job queues are database-specific. Queues and jobs are always relative to the database in which they are created or accessed.

@startDocuBlock foxxQueuesPollInterval

For the low-level functionality see [the chapter on the **Task Management** module](../../ModuleTasks/README.md).

As of ArangoDB 2.6, Foxx queue job types are defined as regular Foxx scripts. To learn more about Foxx scripts, see [the chapter on Foxx scripts](./Scripts.md). Function-based job types are still supported in 2.6 but have been removed in 2.7.

For an example of re-usable job types see the various mailer apps available in the Foxx app store.

Creating or updating a queue
----------------------------

Creates a queue with the given name and maximum number of workers.

`Foxx.queues.create(name, [maxWorkers])`

Returns the **Queue** instance for the given **name**. If the queue does not exist, a new queue with the given **name** will be created. If a queue with the given **name** already exists and **maxWorkers** is set, the queue's maximum number of workers will be updated.
The queue will be created in the context of the current database.

**Parameters**

* **name**: the name of the queue to create
* **maxWorkers** (optional): the maximum number of workers. Default: **1**

**Examples**

```js
// Create a queue with the default number of workers (i.e. one)
var queue1 = Foxx.queues.create("my-queue");
// Create a queue with a given number of workers
var queue2 = Foxx.queues.create("another-queue", 2);
// Update the number of workers of an existing queue
var queue3 = Foxx.queues.create("my-queue", 10);
// queue1 and queue3 refer to the same queue
assertEqual(queue1, queue3);
```

Fetching an existing queue
--------------------------

Fetches a queue with the given name.

`Foxx.queues.get(name)`

Returns the **Queue** instance for the given **name**. If the queue does not exist an exception is thrown instead.

The queue will be looked for in the context of the current database.

**Parameters**

* **name**: the name of the queue to fetch

**Examples**

If the queue does not yet exist an exception is thrown:

```js
Foxx.queues.get("some-queue");
// Error: Queue does not exist: some-queue
//     at ...
```

Otherwise the **Queue** instance will be returned:

```js
var queue1 = Foxx.queues.create("some-queue");
var queue2 = Foxx.queues.get("some-queue");
assertEqual(queue1, queue2);
```

Deleting a queue
----------------

Deletes the queue with the given name from the database.

`Foxx.queues.delete(name)`

Returns **true** if the queue was deleted successfully. If the queue did not exist, it returns **false** instead.
The queue will be looked for and deleted in the context of the current database.

When a queue is deleted, jobs on that queue will no longer be executed.

Deleting a queue will not delete any jobs on that queue.

**Parameters**

* **name**: the name of the queue to delete

**Examples**

```js
var queue = Foxx.queues.create("my-queue");
Foxx.queues.delete("my-queue"); // true
Foxx.queues.delete("my-queue"); // false
```

Adding a job to a queue
-----------------------

Adds a job to the given queue.

The job will be added to the specified queue in the context of the current database.

`Queue::push(script, data, [opts])`

Returns the job id.

**Parameters**

* **script**: a job type definition, consisting of an object with the following properties:
  * **name**: the name of the script that will be invoked.
  * **mount**: the mount path of the app that defines the script.
  * **backOff** (optional): either a function that takes the number of times the job has failed before as input and returns the number of milliseconds to wait before trying the job again, or the delay to be used to calculate an [exponential back-off](https://en.wikipedia.org/wiki/Exponential_backoff), or `0` for no delay. Default: `1000`.
  * **maxFailures** (optional): the number of times a single run of a job will be re-tried before it is marked as `"failed"`. A negative value or `Infinity` means that the job will be re-tried on failure indefinitely. Default: `0`.
  * **schema** (optional): a [Joi](https://github.com/hapijs/joi) schema to validate a job's data against before enqueuing the job.
  * **preprocess** (optional): a function to pre-process a job's (validated) data before serializing it in the queue.
  * **repeatTimes** (optional): if set to a positive number, the job will be repeated this many times (not counting recovery when using **maxFailures**). If set to a negative number or `Infinity`, the job will be repeated indefinitely. Default: `0` (no repeat).
  * **repeatUntil** (optional): if the job is set to automatically repeat, this can be set to a timestamp in milliseconds (or `Date` instance) after which the job will no longer repeat. Setting this value to zero, a negative value or `Infinity` has no effect.
  * **repeatDelay** (optional): if the job is set to automatically repeat, this can be set to a non-negative value to set the number of milliseconds for which the job will be delayed before it is started again. Default: `0`.
* **data**: the job data of the job; must be serializable to JSON.
* **opts** (optional): an object with any of the following properties:
 * **success** (optional): a function to be called after the job has been completed successfully.
 * **failure** (optional): a function to be called after the job has failed too many times.
 * **delayUntil** (optional): a timestamp in milliseconds (or `Date` instance) until which the execution of the job should be delayed. Default: `Date.now()`.
 * **backOff** (optional): see **script.backOff**.
 * **maxFailures** (optional): see **script.maxFailures**.
 * **repeatTimes** (optional): see **script.repeatTimes**.
 * **repeatUntil** (optional): see **script.repeatUntil**.
 * **repeatDelay** (optional): see **script.repeatDelay**.

Note that if you pass a function for the **backOff** calculation, **success** callback or **failure** callback options the function will be serialized to the database as a string and therefore must not rely on any external scope or external variables.

When the job is set to automatically repeat, the **failure** callback will only be executed when a run of the job has failed more than **maxFailures** times. Note that if the job fails and **maxFailures** is set, it will be rescheduled according to the **backOff** until it has either failed too many times or completed successfully before being scheduled according to the **repeatDelay** again. Recovery attempts by **maxFailures** do not count towards **repeatTimes**.

The **success** and **failure** callbacks receive the following arguments:

* **result**: the return value of the script for the current run of the job.
* **jobData**: the **data** passed to this method.
* **job**: the ArangoDB document representing the job's current state.

**Examples**

Let's say we have an app mounted at `/mailer` that provides a script called `send-mail`:

```js
var Foxx = require("org/arangodb/foxx");
var queue = Foxx.queues.create("my-queue");
queue.push(
  {mount: "/mailer", name: "send-mail"},
  {to: "hello@example.com", body: "Hello world"}
);
```

This will **not** work, because **log** was defined outside the callback function (the callback must be serializable to a string):

```js
var Foxx = require("org/arangodb/foxx");
var queue = Foxx.queues.create("my-queue");
var log = require("console").log; // outside the callback's function scope
queue.push(
  {mount: "/mailer", name: "send-mail"},
  {to: "hello@example.com", body: "Hello world"},
  {success: function () {
    log("Yay!"); // throws "console is not defined"
  }}
);
```

Here's an example of a job that will be executed every 5 seconds until tomorrow:

```js
var Foxx = require("org/arangodb/foxx");
var queue = Foxx.queues.create("my-queue");
queue.push(
  {mount: "/mailer", name: "send-mail"},
  {to: "hello@example.com", body: "Hello world"},
  {
    repeatTimes: Infinity,
    repeatUntil: Date.now() + (24 * 60 * 60 * 1000),
    repeatDelay: 5 * 1000
  }
);
```

Fetching a job from the queue
-----------------------------

Creates a proxy object representing a job with the given job id.

The job will be looked for in the specified queue in the context of the current database.

`Queue::get(jobId)`

Returns the **Job** instance for the given **jobId**. Properties of the job object will be fetched whenever they are referenced and can not be modified.

**Parameters**

* **jobId**: the id of the job to create a proxy object for.

**Examples**
```js
var jobId = queue.push("log", "Hello World!");
var job = queue.get(jobId);
assertEqual(job.id, jobId);
```

Deleting a job from the queue
-----------------------------

Deletes a job with the given job id.
The job will be looked for and deleted in the specified queue in the context of the current database.

`Queue::delete(jobId)`

Returns **true** if the job was deleted successfully. If the job did not exist it returns **false** instead.

Fetching an array of jobs in a queue
------------------------------------

**Examples**

```js
queue.push("log", "Hello World!", {delayUntil: Date.now() + 50});
assertEqual(queue.pending("log").length, 1);
// 50 ms later...
assertEqual(queue.pending("log").length, 0);
assertEqual(queue.progress("log").length, 1);
// even later...
assertEqual(queue.progress("log").length, 0);
assertEqual(queue.complete("log").length, 1);
```
### Fetching an array of pending jobs in a queue

`Queue::pending([script])`

Returns an array of job ids of jobs in the given queue with the status `"pending"`, optionally filtered by the given job type.
The jobs will be looked for in the specified queue in the context of the current database.

**Parameters**

* **script** (optional): an object with the following properties:
 * **name**: name of the script.
 * **mount**: mount path of the app defining the script.

### Fetching an array of jobs that are currently in progress

`Queue::progress([script])`

Returns an array of job ids of jobs in the given queue with the status `"progress"`, optionally filtered by the given job type.
The jobs will be looked for in the specified queue in the context of the current database.

**Parameters**

* **script** (optional): an object with the following properties:
 * **name**: name of the script.
 * **mount**: mount path of the app defining the script.

### Fetching an array of completed jobs in a queue

`Queue::complete([script])`

Returns an array of job ids of jobs in the given queue with the status `"complete"`, optionally filtered by the given job type.
The jobs will be looked for in the specified queue in the context of the current database.

**Parameters**

* **script** (optional): an object with the following properties:
 * **name**: name of the script.
 * **mount**: mount path of the app defining the script.

### Fetching an array of failed jobs in a queue

`Queue::failed([script])`

Returns an array of job ids of jobs in the given queue with the status `"failed"`, optionally filtered by the given job type.
The jobs will be looked for in the specified queue in the context of the current database.

**Parameters**

* **script** (optional): an object with the following properties:
 * **name**: name of the script.
 * **mount**: mount path of the app defining the script.

### Fetching an array of all jobs in a queue

`Queue::all([script])`

Returns an array of job ids of all jobs in the given queue, optionally filtered by the given job type.
The jobs will be looked for in the specified queue in the context of the current database.

**Parameters**

* **script** (optional): an object with the following properties:
 * **name**: name of the script.
 * **mount**: mount path of the app defining the script.

Aborting a job
--------------

Aborts a non-completed job.

`Job::abort()`

Sets a job's status to `"failed"` if it is not already `"complete"`, without calling the job's **onFailure** callback.
