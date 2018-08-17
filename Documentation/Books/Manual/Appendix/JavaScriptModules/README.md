JavaScript Modules
==================

ArangoDB uses a Node.js compatible module system. You can use the function *require* in order to load a module or library. It returns the exported variables and functions of the module.

The global variables `global`, `process`, `console`, `Buffer`, `__filename` and `__dirname` are available throughout ArangoDB and Foxx.

Node compatibility modules
--------------------------

ArangoDB supports a number of modules for compatibility with Node.js, including:

* [assert](http://nodejs.org/api/assert.html) implements basic assertion and testing functions.

* [buffer](http://nodejs.org/api/buffer.html) implements a binary data type for JavaScript.

* [console](Console.md) is a well known logging facility to all the JavaScript developers.
  ArangoDB implements most of the [Console API](http://wiki.commonjs.org/wiki/Console),
  with the exceptions of *profile* and *count*.

* [events](http://nodejs.org/api/events.html) implements an event emitter.

* [fs](FileSystem.md) provides a file system API for the manipulation of paths, directories, files, links, and the construction of file streams.
  ArangoDB implements most [Filesystem/A](http://wiki.commonjs.org/wiki/Filesystem/A) functions.

* [module](http://nodejs.org/api/modules.html) provides direct access to the module system.

* [path](http://nodejs.org/api/path.html) implements functions dealing with filenames and paths.

* [punycode](http://nodejs.org/api/punycode.html) implements
  conversion functions for [punycode](http://en.wikipedia.org/wiki/Punycode) encoding.

* [querystring](http://nodejs.org/api/querystring.html) provides utilities for dealing with query strings.

* [stream](http://nodejs.org/api/stream.html) provides a streaming interface.

* [string_decoder](https://nodejs.org/api/string_decoder.html) implements logic for decoding buffers into strings.

* [url](http://nodejs.org/api/url.html) provides utilities for URL resolution and parsing.

* [util](http://nodejs.org/api/util.html) provides general utility functions like `format` and `inspect`.

Additionally ArangoDB provides partial implementations for the following modules:

* `net`:
  only `isIP`, `isIPv4` and `isIPv6`.

* `process`:
  only `env` and `cwd`;
  stubs for `argv`, `stdout.isTTY`, `stdout.write`, `nextTick`.

* `timers`:
  stubs for `setImmediate`, `setTimeout`, `setInterval`, `clearImmediate`, `clearTimeout`, `clearInterval` and `ref`.

* `tty`:
  only `isatty` (always returns `false`).

* `vm`:
  only `runInThisContext`.

The following Node.js modules are not available at all:
`child_process`,
`cluster`,
`constants`,
`crypto` (but see `@arangodb/crypto` below),
`dgram`,
`dns`,
`domain`,
`http` (but see `@arangodb/request` below),
`https`,
`os`,
`sys`,
`tls`,
`v8`,
`zlib`.

ArangoDB Specific Modules
-------------------------

There are a large number of ArangoDB-specific modules using the `@arangodb` namespace, mostly for internal use by ArangoDB itself. The following however are noteworthy:

* [@arangodb](ArangoDB.md) provides direct access to the database and its collections.

* [@arangodb/crypto](Crypto.md) provides various cryptography functions including hashing algorithms.

* [@arangodb/request](Request.md) provides the functionality for making synchronous HTTP/HTTPS requests.

* [@arangodb/foxx](../../Foxx/README.md) is the namespace providing the various building blocks of the Foxx microservice framework.

Bundled NPM Modules
-------------------

The following [NPM modules](https://www.npmjs.com) are preinstalled:

* [aqb](https://github.com/arangodb/aqbjs)
  is the ArangoDB Query Builder and can be used to construct AQL queries with a chaining JavaScript API.

* [chai](http://chaijs.com)
  is a full-featured assertion library for writing JavaScript tests.

* [dedent](https://github.com/dmnd/dedent)
  is a simple utility function for formatting multi-line strings.

* [error-stack-parser](http://www.stacktracejs.com)
  parses stacktraces into a more useful format.

<!-- * [expect.js] (https://github.com/Automattic/expect.js) (only for legacy tests) -->

<!-- * [extendible] (https://github.com/3rd-Eden/extendible) (only for legacy mode) -->

* [graphql-sync](https://github.com/arangodb/graphql-sync)
  is an ArangoDB-compatible GraphQL server/schema implementation.

* [highlight.js](https://highlightjs.org)
  is an HTML syntax highlighter.

* [i (inflect)](https://github.com/pksunkara/inflect)
  is a utility library for inflecting (e.g. pluralizing) words.

* [iconv-lite](https://github.com/ashtuchkin/iconv-lite)
  is a utility library for converting between character encodings

* [joi](https://github.com/hapijs/joi)
  is a validation library that is supported throughout the Foxx framework.

* [js-yaml](https://github.com/nodeca/js-yaml)
  is a JavaScript implementation of the YAML data format (a partial superset of JSON).

* [lodash](https://lodash.com)
  is a utility belt for JavaScript providing various useful helper functions.

* [minimatch](https://github.com/isaacs/minimatch)
  is a glob matcher for matching wildcards in file paths.

* [qs](https://github.com/hapijs/qs)
  provides utilities for dealing with query strings using a different format than the **querystring** module.

* [semver](https://github.com/npm/node-semver)
  is a utility library for handling semver version numbers.

* [sinon](http://sinonjs.org)
  is a mocking library for writing test stubs, mocks and spies.

* [timezone](https://github.com/bigeasy/timezone)
  is a library for converting date time values between formats and timezones.
