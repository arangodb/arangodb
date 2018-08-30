Console Module
==============

`global.console === require('console')`

**Note**: You don't need to load this module directly. The `console` object is globally defined throughout ArangoDB and provides access to all functions in this module.

console.assert
--------------

`console.assert(expression, format, argument1, ...)`

Tests that an expression is *true*. If not, logs a message and throws
an exception.

*Examples*

```js
console.assert(value === "abc", "expected: value === abc, actual:", value);
```

console.debug
-------------

`console.debug(format, argument1, ...)`

Formats the arguments according to *format* and logs the result as
debug message. Note that debug messages will only be logged if the
server is started with log levels *debug* or *trace*.

String substitution patterns, which can be used in *format*.

* *%%s* string
* *%%d*, *%%i* integer
* *%%f* floating point number
* *%%o* object hyperlink

*Examples*

```js
console.debug("%s", "this is a test");
```

console.dir
-----------

`console.dir(object)`

Logs a listing of all properties of the object.

Example usage:
```js
console.dir(myObject);
```

console.error
-------------

`console.error(format, argument1, ...)`

Formats the arguments according to @FA{format} and logs the result as
error message.

String substitution patterns, which can be used in *format*.

* *%%s* string
* *%%d*, *%%i* integer
* *%%f* floating point number
* *%%o* object hyperlink

Example usage:
```js
console.error("error '%s': %s", type, message);
```
console.getline
---------------

`console.getline()`

Reads in a line from the console and returns it as string.

console.group
-------------

`console.group(format, argument1, ...)`

Formats the arguments according to *format* and logs the result as
log message. Opens a nested block to indent all future messages
sent. Call *groupEnd* to close the block. Representation of block
is up to the platform, it can be an interactive block or just a set of
indented sub messages.

Example usage:

```js
console.group("user attributes");
console.log("name", user.name);
console.log("id", user.id);
console.groupEnd();
```

console.groupCollapsed
----------------------

`console.groupCollapsed(format, argument1, ...)`

Same as *console.group*.

console.groupEnd
----------------

`console.groupEnd()`

Closes the most recently opened block created by a call to *group*.

console.info
------------

`console.info(format, argument1, ...)`

Formats the arguments according to *format* and logs the result as
info message.

String substitution patterns, which can be used in *format*.

* *%%s* string
* *%%d*, *%%i* integer
* *%%f* floating point number
* *%%o* object hyperlink

Example usage:
```js
console.info("The %s jumped over %d fences", animal, count);
```
console.log
-----------

`console.log(format, argument1, ...)`

Formats the arguments according to *format* and logs the result as
log message. This is an alias for *console.info*.

console.time
------------

`console.time(name)`

Creates a new timer under the given name. Call *timeEnd* with the
same name to stop the timer and log the time elapsed.

Example usage:

```js
console.time("mytimer");
...
console.timeEnd("mytimer"); // this will print the elapsed time
```

console.timeEnd
---------------

`console.timeEnd(name)`

Stops a timer created by a call to *time* and logs the time elapsed. 

console.trace
-------------

`console.trace()`

Logs a stack trace of JavaScript execution at the point where it is
called. 

console.warn
------------

`console.warn(format, argument1, ...)`

Formats the arguments according to *format* and logs the result as
warn message.

String substitution patterns, which can be used in *format*.

* *%%s* string
* *%%d*, *%%i* integer
* *%%f* floating point number
* *%%o* object hyperlink
