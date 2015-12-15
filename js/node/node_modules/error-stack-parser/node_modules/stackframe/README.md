stackframe 
==========
## JS Object representation of a stack frame
[![Build Status](https://travis-ci.org/stacktracejs/stackframe.svg?branch=master)](https://travis-ci.org/stacktracejs/stackframe) [![Coverage Status](https://img.shields.io/coveralls/stacktracejs/stackframe.svg)](https://coveralls.io/r/stacktracejs/stackframe?branch=master) [![Code Climate](https://codeclimate.com/github/stacktracejs/stackframe/badges/gpa.svg)](https://codeclimate.com/github/stacktracejs/stackframe)

Underlies functionality of other modules within [stacktrace.js](http://www.stacktracejs.com).

Written to closely resemble StackFrame representations in [Gecko](http://mxr.mozilla.org/mozilla-central/source/xpcom/base/nsIException.idl#14) and [V8](https://code.google.com/p/v8-wiki/wiki/JavaScriptStackTraceApi)

## Usage
```js
// Create StackFrame and set properties
var stackFrame = new StackFrame('funName', ['args'], 'http://localhost:3000/file.js', 1, 3288, 'ORIGINAL_STACK_LINE');

stackFrame.functionName      // => "funName"
stackFrame.setFunctionName('newName')
stackFrame.getFunctionName() // => "newName"

stackFrame.args              // => ["args"]
stackFrame.setArgs([])
stackFrame.getArgs()         // => []

stackFrame.fileName          // => 'http://localhost:3000/file.min.js'
stackFrame.setFileName('http://localhost:3000/file.js')  
stackFrame.getFileName()     // => 'http://localhost:3000/file.js'

stackFrame.lineNumber        // => 1
stackFrame.setLineNumber(325)
stackFrame.getLineNumber()   // => 325

stackFrame.columnNumber      // => 3288
stackFrame.setColumnNumber(20)
stackFrame.getColumnNumber() // => 20

stackFrame.source            // => 'ORIGINAL_STACK_LINE'
stackFrame.setSource('NEW_SOURCE')
stackFrame.getSource()       // => 'NEW_SOURCE'

stackFrame.toString() // => 'funName(args)@http://localhost:3000/file.js:325:20'
```

## Installation
```
npm install stackframe
bower install stackframe
https://raw.githubusercontent.com/stacktracejs/stackframe/master/dist/stackframe.min.js
```
