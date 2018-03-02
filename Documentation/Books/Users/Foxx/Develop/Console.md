Foxx console
============

Foxx injects a **console** object into each Foxx app that allows writing log entries to the database in addition to the ArangoDB log file and querying them from within the app itself.

The **console** object supports the CommonJS Console API found in Node.js and modern browsers, while also providing some ArangoDB-specific additions.

ArangoDB also provides [the `console` module](../../ModuleConsole/README.md) which only supports the CommonJS Console API and only writes log entries to the ArangoDB log.

When working with transactions, keep in mind that the Foxx console will attempt to write to the `_foxxlog` system collection. This behaviour can be disabled using the `setDatabaseLogging` method if you don't want to explicitly allow writing to the log collection during transactions or for performance reasons.

Logging
-------

### Logging console messages

Write an arbitrary message to the app's log.

`console.log([format,] ...parameters)`

Applies `util.format` to the arguments and writes the resulting string to the app's log.

If the first argument is not a formatting string or any of the additional arguments are not used by the formatting string, they will be concatenated to the result string.

**Examples**

```js
console.log("%s, %s!", "Hello", "World"); // => "Hello, World!"
console.log("%s, World!", "Hello", "extra"); // => "Hello, World! extra"
console.log("Hello,", "beautiful", "world!"); // => "Hello, beautiful world!"
console.log(1, 2, 3); // => "1 2 3"
```

### Logging with different log levels

The **console** object provides additional methods to log messages with different log levels:

* `console.debug` for log level **DEBUG**
* `console.info` for log level **INFO**
* `console.warn` for log level **WARN**
* `console.error` for log level **ERROR**

By default, `console.log` uses log level **INFO**, making it functionally equivalent to `console.info`. Other than the log level, all of these methods behave identically to `console.log`.

The built-in log levels are:

* -200: **TRACE**
* -100: **DEBUG**
* 0: **INFO**
* 100: **WARN**
* 200: **ERROR**

### Logging with timers

You can start and stop timers with labels.

`console.time(label)` / `console.timeEnd(label)`

Passing a label to `console.time` starts the timer. Passing the same label to `console.timeEnd` ends the timer and logs a message with the label and the expired time in milliseconds.

Calling `console.timeEnd` with an invalid label or calling it with the same label twice (without first starting a new timer with the same label) results in an error.

By default, the timing messages will be logged with log level **INFO**.

**Examples**

```js
console.time('do something');
// ... more code ...
console.time('do more stuff');
// ... even more code ...
console.timeEnd('do more stuff'); // => "do more stuff: 23ms"
// ... a little bit more ....
console.timeEnd('do something'); // => "do something: 52ms"
```

Don't do this:

```js
console.timeEnd('foo'); // fails: label doesn't exist yet.
console.time('foo'); // works
console.timeEnd('foo'); // works
console.timeEnd('foo'); // fails: label no longer exists.
```

### Logging stack traces

You can explicitly log a message with a stack trace.

`console.trace(message)`

This creates a stack trace with the given message.

By default the stack traces will be logged with a log level of **TRACE**.

**Examples**

```js
console.trace('Hello');
/* =>
Trace: Hello
    at somefile.js:1:1
    at ...
*/
```

### Logging assertions

This creates an assertion that will log an error if it fails.

`console.assert(statement, message)`

If the given **statement** is not true (e.g. evaluates to `false` if treated as a boolean), an `AssertionError` with the given message will be created and its stack trace will be logged.

By default, the stack trace will be logged with a log level of **ERROR** and the error will be discarded after logging it (instead of being thrown).

**Examples**

```js
console.assert(2 + 2 === 5, "I'm bad at maths");
/* =>
AssertionError: I'm bad at maths
    at somefile.js:1:1
    at ...
*/
```

### Inspecting an object

This logs a more detailed string representation of a given object.

`console.dir(value)`

The regular logging functions try to format messages as nicely and readable as possible. Sometimes that may not provide you with all the information you actually want.

The **dir** method instead logs the result of `util.inspect`.

By default the message will be logged with the log level **INFO**.

**Examples**

```js
console.log(require('org/arangodb').db); // => '[ArangoDatabase "_system"]'
console.dir(require('org/arangodb').db);
/* =>
{ [ArangoDatabase "_system"]
  _version: [Function: _version],
  _id: [Function: _id],
  ...
}
*/
```

### Custom log levels

This lets you define your own log levels.

`console.custom(name, value)`

If you need more than the built-in log levels, you can easily define your own.

This method returns a function that logs messages with the given log level (e.g. an equivalent to `console.log` that uses your custom log level instead).

**Parameter**

* **name**: name of the log level as it appears in the database, usually all-uppercase
* **value** (optional): value of the log level. Default: `50`

The **value** is used when determining whether a log entry meets the minimum log level that can be defined in various places. For a list of the built-in log levels and their values see the section on logging with different log levels above.

### Preventing entries from being logged

You can define a minimum log level entries must match in order to be logged.

`console.setLogLevel(level)`

The **level** can be a numeric value or the case-sensitive name of an existing log level.

Any entries with a log level lower than the given value will be discarded silently.

This can be helpful if you want to toggle logging diagnostic messages in development mode that should not be logged in production.

The default log level is set to `-999`. For a list of built-in log levels and their values see the section on logging with different log levels.

### Enabling extra stack traces

You can toggle the logging of stack trace objects for every log entry.

`console.setTracing(trace)`

If **trace** is set to `true`, all log entries will be logged with a parsed stack trace as an additional `stack` property that can be useful for identifying where the entry originated and how the code triggering it was called.

Because this results in every logging call creating a stack trace (which may have a significant performance impact), this option is disabled by default.

### Disabling logging to the ArangoDB console

You can toggle whether logs should be written to the ArangoDB console.

`console.setNativeLogging(nativeLogging)`

If **nativeLogging** is set to `false`, log entries will not be logged to the ArangoDB console (which usually writes to the file system).

### Disabling logging to the database

You can toggle whether logs should be written to the database.

`console.setDatabaseLogging(databaseLogging)`

If **databaseLogging** is set to `false`, log entries will not be logged to the internal `_foxxlog` collection.

This is only useful if logging to the ArangoDB console is not also disabled.

### Enabling assertion errors

You can toggle whether console assertions should throw if they fail.

`console.setAssertThrows(assertThrows)`

If **assertThrows** is set to `true`, any failed assertions in `console.assert` will result in the generated error being thrown instead of being discarded after it is logged.

By default, this setting is disabled.

### Changing the default log levels

Most of the logging methods have an implied log level that is set to a reasonable default. If you would like to have them use different log levels, you can easily change them.

* `console.log.level` defaults to **INFO**
* `console.dir.level` defaults to **INFO**
* `console.time.level` defaults to **INFO**
* `console.trace.level` defaults to **TRACE**
* `console.assert.level` defaults to **ERROR**

To use different log levels, just set these properties to the case-sensitive name of the desired log level.

**Examples**

```js
console.log('this uses "INFO"');
console.log.level = 'WARN';
console.log('now it uses "WARN"');
```

Known custom levels work, too:

```js
console.custom('BANANA', 23);
console.log.level = 'BANANA';
console.log('now it uses "BANANA"');
```

Unknown levels result in an error:

```js
console.log.level = 'POTATO';
console.log('this throws'); // => Error: Unknown log level: POTATO
```

Querying a Foxx app's log entries
---------------------------------

As the log entries are logged to a collection in the database, you can easily query them in your own application.

### The logs object

The logs object can be found on the console itself:

`console.logs`

It provides three easy methods for querying the log entries:

* `logs.list([opts])`
* `logs.searchByMessage(message, [opts])`
* `logs.searchByFileName(fileName, [opts])`

Each method takes an optional `opts` argument, which can be an object with any of the following properties:

* **startTime**: the oldest time to include (in milliseconds). Default: 2 hours ago.
* **endTime**: the most recent time to include (in milliseconds).
* **level**: only return entries with this log level (name or value).
* **minLevel**: only return entries with this log level or above (name or value).
* **sort**: sorting direction of the result (**ASC** or **DESC** by time). Default: **ASC**.
* **limit**: maximum number of entries to return.
* **offset**: if **limit** is set, skip this many entries.

The default value for **startTime** can be changed by overriding `logs.defaultMaxAge` with a different time offset in milliseconds.

#### Search by message

This lists all log entries with messages that contain the given token.

`logs.searchByMessage(message, [opts])`

This works like `logs.list` except it only returns log entries containing the given **message** part in their message.

#### Search by file name

This lists all log entries with stack traces that contain the given token.

`logs.searchByFileName(fileName, [opts])`

This works like `logs.list` except it only returns log entries containing the given **fileName** part in one of the file names of their stack trace.

This method can only be used if the console has **tracing** enabled. See the section on enabling extra stack traces.

Note that entries that were logged while tracing was not enabled can not be found with this method because they don't have any parsed stack traces associated with them. This method does not search the log entries messages for the file name, so entries generated by `console.assert` or `console.trace` are not treated differently.
