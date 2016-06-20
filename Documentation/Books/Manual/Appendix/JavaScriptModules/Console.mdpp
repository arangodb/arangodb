!CHAPTER Console Module

`require('console')`

The implementation follows the CommonJS specification
[Console](http://wiki.commonjs.org/wiki/Console).

!SUBSECTION console.assert

`console.assert(expression, format, argument1, ...)`

Tests that an expression is *true*. If not, logs a message and throws
an exception.

*Examples*

```js
console.assert(value === "abc", "expected: value === abc, actual:", value);
```

!SUBSECTION console.debug

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

!SUBSECTION console.dir

`console.dir(object)`

Logs a listing of all properties of the object.

Example usage:
```js
console.dir(myObject);
```

!SUBSECTION console.error

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
!SUBSECTION console.getline

`console.getline()`

Reads in a line from the console and returns it as string.

!SUBSECTION console.group

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

!SUBSECTION console.groupCollapsed

`console.groupCollapsed(format, argument1, ...)`

Same as *console.group*.

!SUBSECTION console.groupEnd

`console.groupEnd()`

Closes the most recently opened block created by a call to *group*.

!SUBSECTION console.info

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
!SUBSECTION console.log

`console.log(format, argument1, ...)`

Formats the arguments according to *format* and logs the result as
log message. This is an alias for *console.info*.

!SUBSECTION console.time

`console.time(name)`

Creates a new timer under the given name. Call *timeEnd* with the
same name to stop the timer and log the time elapsed.

Example usage:

```js
console.time("mytimer");
...
console.timeEnd("mytimer"); // this will print the elapsed time
```

!SUBSECTION console.timeEnd

`console.timeEnd(name)`

Stops a timer created by a call to *time* and logs the time elapsed. 

!SUBSECTION console.timeEnd

`console.trace()`

Logs a stack trace of JavaScript execution at the point where it is
called. 

!SUBSECTION console.warn

`console.warn(format, argument1, ...)`

Formats the arguments according to *format* and logs the result as
warn message.

String substitution patterns, which can be used in *format*.

* *%%s* string
* *%%d*, *%%i* integer
* *%%f* floating point number
* *%%o* object hyperlink