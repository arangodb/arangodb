Module "console"{#JSModuleConsole}
==================================

@NAVIGATE_JSModuleConsole
@EMBEDTOC{JSModuleConsoleTOC}

Console Module{#JSModuleConsoleIntro}
=====================================

The implementation follows the CommonJS specification
<a href="http://wiki.commonjs.org/wiki/Console">Console</a>.

console.assert{#JSModuleConsoleAssert}
--------------------------------------

@FUN{console.assert(@FA{expression}, @FA{format}, @FA{argument1}, ...)}

Tests that an expression is `true`. If not, logs a message and throws
an exception.

@CLEARPAGE
console.debug{#JSModuleConsoleDebug}
------------------------------------

@FUN{console.debug(@FA{format}, @FA{argument1}, ...)}

Formats the arguments according to @FA{format} and logs the result as
debug message.

String substitution patterns, which can be used in @FA{format}.

- `%%s` string
- `%%d`, `%%i` integer
- `%%f` floating point number
- `%%o` object hyperlink

@CLEARPAGE
console.dir{#JSModuleConsoleDir}
--------------------------------

@FUN{console.dir(@FA{object})}

Logs a static / interactive listing of all properties of the object.

@CLEARPAGE
console.error{#JSModuleConsoleError}
------------------------------------

@FUN{console.error(@FA{format}, @FA{argument1}, ...)}

Formats the arguments according to @FA{format} and logs the result as
error message.

@CLEARPAGE
console.getline{#JSModuleConsoleGetline}
----------------------------------------

@FUN{console.getline()}

Reads in a line from the console and returns it as string.

@CLEARPAGE
console.group{#JSModuleConsoleGroup}
------------------------------------

@FUN{console.group(@FA{format}, @FA{argument1}, ...)}

Formats the arguments according to @FA{format} and logs the result as
log message. Opens a nested block to indent all future messages
sent. Call @FN{groupEnd} to close the block. Representation of block
is up to the platform, it can be an interactive block or just a set of
indented sub messages.

@CLEARPAGE
console.groupEnd{#JSModuleConsoleGroupEnd}
------------------------------------------

@FUN{console.groupEnd()}

Closes the most recently opened block created by a call to @FN{group}.

@CLEARPAGE
console.info{#JSModuleConsoleInfo}
----------------------------------

@FUN{console.info(@FA{format}, @FA{argument1}, ...)}

Formats the arguments according to @FA{format} and logs the result as
info message.

@CLEARPAGE
console.log{#JSModuleConsoleLog}
--------------------------------

@FUN{console.log(@FA{format}, @FA{argument1}, ...)}

Formats the arguments according to @FA{format} and logs the result as
log message.

@CLEARPAGE
console.time{#JSModuleConsoleTime}
----------------------------------

@FUN{console.time(@FA{name})}

Creates a new timer under the given name. Call @FN{timeEnd} with the
same name to stop the timer and log the time elapsed.

@CLEARPAGE
console.timeEnd{#JSModuleConsoleTimeEnd}
----------------------------------------

@FUN{console.timeEnd(@FA{name})}

Stops a timer created by a call to @FN{time} and logs the time elapsed. 

@CLEARPAGE
console.timeEnd{#JSModuleConsoleTrace}
--------------------------------------

@FUN{console.trace()}

Logs a static stack trace of JavaScript execution at the point where it is
called. 

@CLEARPAGE
console.warn{#JSModuleConsoleWarn}
----------------------------------

@FUN{console.warn(@FA{format}, @FA{argument1}, ...)}

Formats the arguments according to @FA{format} and logs the result as
warn message.
