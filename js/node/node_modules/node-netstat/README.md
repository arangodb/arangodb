# node-netstat

A library utility for reading netstat data. It's been tested on Ubuntu 14.04/16.04, Windows 7 and OS X Yosemite.

## Installation

node-netstat can be installed via **npm**:

	$ npm install node-netstat

## Usage

```js
var netstat = require('node-netstat');

netstat({
	filter: {
		pid: 4123,
		protocol: 'tcp'
	},
	limit: 5
}, function (data) {
    // a single line of data read from netstat
});
```

## API

### `void netstat(object options, function handler)`

Executes a netstat query with any `options` supplied and executes `handler` for each line result read from netstat.

The `handler` signature is `void|boolean function(object parsedItem)` where `parsedItem` represents a single result from netstat. A typical `parsedItem` will look like this:

```js
var item = {
    protocol: String,   // 'tcp', 'udp', or 'tcp6'
    local: {
		port: Number,
		address: String // null if a loopback address
	},
    remote: {
		port: Number,
		address: String // null if a loopback address
	},
    state: '',
    pid: Number         // 0 if it could not be found/parsed
};
```

If the return value is `false`, processing will stop and any remaining results will not be parsed.

#### Options

- **sync** - *(boolean)* execute the operation synchronously.
	- Execution is asynchronous by default.
- **done** - *(function(error))* node-style callback, executed after the netstat command completed execution or encountered an error`.
- **platform** - *(string)* overrides the platform value returned from `os.platform()`.
- **limit** - *(number)* limits the results read and parsed from the netstat process. Nothingness means no limit.
- **filter** - *(object)* a hash of value conditions for parsed line objects. If a key/value doesn't correspond with one(s) on a parsed object, `handler` won't get called.
- **watch** - *(boolean)* repeatedly run until processing is cancelled by the line handler or by the external handler.


### `object netstat.commands`

A hash map with command pattern objects:

```js
{
	cmd: 'netstat',
	args: ['-lmnop', '--tcp']
};
```

The keys in `netstat.commands` correspond to the standard `os.platform()` return values ('linux', 'win32').

### `object netstat.parsers`

A hash map of line parse handlers with keys corresponding to `os.platform()` values.

Line parsers have the following signature:

```js
function (line, callback) {
	// parse line contents
	callback(parsedItem);
}
```

`line` is a raw line of output read from netstat. `callback` is a function and accepts a single argument: the parsed data object.

### `object netstat.parserFactories`

A hash map of the factory functions used to generate the default parsers with keys corresponding to `os.platform()` values. Some factories include options that may be customized for specific use cases.

#### Linux

##### Options

- **parseName** - *(boolean)* parse and include `processName` in results. Default: *false*

#### Common Recipes

##### Include the process name and allow filtering on it (linux only)

```js
const netstat = require('node-netstat');

netstat.parsers.linux = netstat.parserFactories.linux({
  parseName: true,
});

netstat({}, item => console.log(item));

/* output:
{ protocol: 'tcp',
  local: { port: 631, address: '127.0.0.1' },
  remote: { port: NaN, address: null },
  state: 'LISTEN',
  pid: 0,
  processName: '' }
{ protocol: 'tcp',
  local: { port: 1339, address: '127.0.0.1' },
  remote: { port: NaN, address: null },
  state: 'LISTEN',
  pid: 10474,
  processName: 'node' }
*/
```

### `object netstat.filters`

A hash map of closure factories to handle logic for certain options. See [source](https://github.com/danielkrainas/node-netstat/blob/master/lib/filters.js) for more details on implementations for specific filters.

### `object netstat.utils`

An object with several useful functions for implementing custom parsers.

### `string netstat.version`

The version of node-netstat

## Canceling `async` netstat externally

If the `watch` option is set, the line handler can never be called. To deal with this scenario, a handler is returned and can be called to cancel netstat watching externally.

```javascript
const impossibleFilter = {...};
let handler = netstat(impossibleFilter, item => console.log(item));

...

// Some time later we need to finish our script,
// we cancel netsat so
handler.cancel();
```
Any subsequent call to `handler.cancel()` takes no effect.

## Extensions

node-netstat is highly extensible via the `filters`, `parsers`, and `commands` properties.

## Bugs and Feedback

If you see a bug or have a suggestion, feel free to open an issue [here](https://github.com/danielkrainas/node-netstat/issues).

## Contributions

PR's welcome! There are no strict style guidelines, just follow best practices and try to keep with the general look & feel of the code present. All submissions must pass jslint and have a test to verify *(if applicable)*.

## License

[Unlicense](http://unlicense.org/UNLICENSE). This is a Public Domain work.

[![Public Domain](https://licensebuttons.net/p/mark/1.0/88x31.png)](http://questioncopyright.org/promise)

> ["Make art not law"](http://questioncopyright.org/make_art_not_law_interview) -Nina Paley
