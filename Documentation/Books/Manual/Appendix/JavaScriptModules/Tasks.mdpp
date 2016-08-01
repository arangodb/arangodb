!CHAPTER Task Management

!SUBSECTION Introduction to Task Management in ArangoDB

ArangoDB can execute user-defined JavaScript functions as one-shot
or periodic tasks. This functionality can be used to implement timed
or recurring jobs in the database.

Tasks in ArangoDB consist of a JavaScript snippet or function that is
executed when the task is scheduled. A task can be a one-shot task
(meaning it is run once and not repeated) or a periodic task (meaning
that it is re-scheduled after each execution). Tasks can have optional
parameters, which are defined at task setup time. The parameters
specified at task setup time will be passed as arguments to the
task whenever it gets executed. Periodic Tasks have an execution 
frequency that needs to be specified when the task is set up. One-shot
tasks have a configurable delay after which they'll get executed.

Tasks will be executed on the server they have been set up on. 
Tasks will not be shipped around in a cluster. A task will be 
executed in the context of the database it was created in. However,
when dropping a database, any tasks that were created in the context
of this database will remain active. It is therefore sensible to
first unregister all active tasks for a database before dropping the
database.

Tasks registered in ArangoDB will be executed until the server
gets shut down or restarted. After a restart of the server, any 
user-defined one-shot or periodic tasks will be lost. 

!SUBSECTION Commands for Working with Tasks

ArangoDB provides the following commands for working with tasks.
All commands can be accessed via the *tasks* module, which can be
loaded like this:

`require("@arangodb/tasks")`

Please note that the *tasks* module is available inside the ArangoDB
server only. It cannot be used from the ArangoShell or ArangoDB's web
interface.

!SUBSECTION Register a task

To register a task, the JavaScript snippet or function needs to be
specified in addition to the execution frequency. Optionally, a task
can have an id and a name. If no id is specified, it will be auto-assigned
for a new task. The task id is also the means to access or unregister a
task later. Task names are informational only. They can be used to make
a task distinguishable from other tasks also running on the server.

The following server-side commands register a task. The command to be
executed is a JavaScript string snippet which prints a message to the 
server's logfile:


```js
const tasks = require("@arangodb/tasks");

tasks.register({
  id: "mytask-1",
  name: "this is a snippet task",
  period: 15,
  command: "require('console').log('hello from snippet task');"
});
```


The above has register a task with id *mytask-1*, which will be executed
every 15 seconds on the server. The task will write a log message whenever
it is invoked.

Tasks can also be set up using a JavaScript callback function like this:

```js
const tasks = require("@arangodb/tasks");

tasks.register({
  id: "mytask-2",
  name: "this is a function task",
  period: 15,
  command: function () {
    require('console').log('hello from function task');
  }
});
```

It is important to note that the callback function is late bound and 
will be executed in a different context than in the creation context. 
The callback function must therefore not access any variables defined 
outside of its own scope. The callback function can still define and
use its own variables.

To pass parameters to a task, the *params* attribute can be set when
registering a task. Note that the parameters are limited to data types
usable in JSON (meaning no callback functions can be passed as parameters
into a task):

```js
const tasks = require("@arangodb/tasks");

tasks.register({
  id: "mytask-3",
  name: "this is a parameter task",
  period: 15,
  command: function (params) {
    var greeting = params.greeting;
    var data = JSON.stringify(params.data);
    require('console').log('%s from parameter task: %s', greeting, data);
  },
  params: { greeting: "hi", data: "how are you?" }
});
```

Registering a one-shot task works the same way, except that the 
*period* attribute must be omitted. If *period* is omitted, then the
task will be executed just once. The task invocation delay can optionally
be specified with the *offset* attribute:

```js
const tasks = require("@arangodb/tasks");

tasks.register({
  id: "mytask-once",
  name: "this is a one-shot task",
  offset: 10,
  command: function (params) {
    require('console').log('you will see me just once!');
  }
});
```

**Note**: When specifying an *offset* value of 0, ArangoDB will internally add 
a very small value to the offset so will be slightly greater than zero.

**Note**: Normally `offset` cannot be greater than `interval`. 
The only way to set `offset` greater than `interval` is to register 
a non-periodic timer with the specified offset and when this fires this can 
install a periodic timer with an offset of 0.

!SUBSECTION Unregister a task

After a task has been registered, it can be unregistered using its id:

```js
const tasks = require("@arangodb/tasks");
tasks.unregister("mytask-1");
```

Note that unregistering a non-existing task will throw an exception.


!SUBSECTION List all tasks

To get an overview of which tasks are registered, there is the *get* 
method. If the *get* method is called without any arguments, it will
return an array of all tasks:

```js
const tasks = require("@arangodb/tasks");
tasks.get();
```

If *get* is called with a task id argument, it will return information 
about this particular task:

```js
const tasks = require("@arangodb/tasks");
tasks.get("mytask-3");
```

The *created* attribute of a task reveals when a task was created. It is
returned as a Unix timestamp. 
