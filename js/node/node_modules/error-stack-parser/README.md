error-stack-parser.js - Extract meaning from JS Errors
===============
[![Build Status](https://travis-ci.org/stacktracejs/error-stack-parser.svg?branch=master)](https://travis-ci.org/stacktracejs/error-stack-parser) [![Coverage Status](https://img.shields.io/coveralls/stacktracejs/error-stack-parser.svg)](https://coveralls.io/r/stacktracejs/error-stack-parser) [![Code Climate](https://codeclimate.com/github/stacktracejs/error-stack-parser/badges/gpa.svg)](https://codeclimate.com/github/stacktracejs/error-stack-parser)

Simple, cross-browser [Error](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Error) parser.
This library parses and extracts function names, URLs, line numbers, and column numbers from the given Error's `stack` as
an Array of [StackFrame](http://git.io/stackframe)s. 

Once you have parsed out StackFrames, you can do much more interesting things. See [stacktrace-gps](http://git.io/stacktrace-gps).

Note that in IE9 and earlier, `Error` objects don't have enough information to extract much of anything. In IE 10, `Error`s
are given a `stack` once they're `throw`n.

## Browser Support
[![Sauce Test Status](https://saucelabs.com/browser-matrix/stacktracejs.svg)](https://saucelabs.com/u/stacktracejs)

## Usage
```js
ErrorStackParser.parse(new Error('boom'));

=> [
        StackFrame('funky1', [], 'path/to/file.js', 35, 79), 
        StackFrame('filter', undefined, 'https://cdn.somewherefast.com/utils.min.js', 1, 832),
        StackFrame(... and so on ...)
   ]
```

## Installation
```bash
npm install error-stack-parser
bower install error-stack-parser
https://raw.githubusercontent.com/stacktracejs/error-stack-parser/master/dist/error-stack-parser.min.js
```

## Contributing
Want to be listed as a *Contributor*? Start with the [Contributing Guide](CONTRIBUTING.md)!

## License
This project is licensed to the [Public Domain](http://unlicense.org)
