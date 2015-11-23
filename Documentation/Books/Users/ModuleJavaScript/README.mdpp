!CHAPTER JavaScript Modules

!SUBSECTION Introduction to JavaScript Modules

The ArangoDB uses a [CommonJS](http://wiki.commonjs.org/wiki)
compatible module and package concept. You can use the function *require* in
order to load a module or package. It returns the exported variables and
functions of the module or package.

There are some extensions to the CommonJS concept to allow ArangoDB to load
Node.js modules as well.

!SECTION CommonJS Modules

Unfortunately, the JavaScript libraries are just in the process of being
standardized. CommonJS has defined some important modules. ArangoDB implements
the following

* "console" is a well known logging facility to all the JavaScript developers.
  ArangoDB implements most of the [Console API](http://wiki.commonjs.org/wiki/Console),
  with the exceptions of *profile* and *count*.

* "fs" provides a file system API for the manipulation of paths, directories,
  files, links, and the construction of file streams. ArangoDB implements
  most [Filesystem/A](http://wiki.commonjs.org/wiki/Filesystem/A) functions.

* Modules are implemented according to
  [Modules/1.1.1](http://wiki.commonjs.org/wiki/Modules)

* Packages are implemented according to
  [Packages/1.0](http://wiki.commonjs.org/wiki/Packages)

!SUBSECTION ArangoDB Specific Modules

A lot of the modules, however, are ArangoDB specific. These modules
are described in the following chapters.

!SUBSECTION Node Modules

ArangoDB also supports some [node](http://www.nodejs.org) modules.

* ["assert"](http://nodejs.org/api/assert.html) implements
  assertion and testing functions.

* ["buffer"](http://nodejs.org/api/buffer.html) implements
  a binary data type for JavaScript.

* ["path"](http://nodejs.org/api/path.html) implements
  functions dealing with filenames and paths.

* ["punycode"](http://nodejs.org/api/punycode.html) implements
  conversion functions for
  [punycode](http://en.wikipedia.org/wiki/Punycode) encoding.

* ["querystring"](http://nodejs.org/api/querystring.html)
  provides utilities for dealing with query strings.

* ["stream"](http://nodejs.org/api/stream.html)
  provides a streaming interface.

* ["url"](http://nodejs.org/api/url.html)
  has utilities for URL resolution and parsing.

!SUBSECTION Bundled NPM Modules

The following [NPM modules](https://npmjs.org) are preinstalled.

* ["aqb"](https://github.com/arangodb/aqbjs)
  is the ArangoDB Query Builder and can be used to construct
  AQL queries with a chaining JavaScript API.

* ["error-stack-parser"](http://www.stacktracejs.com)

* ["expect.js"](https://github.com/Automattic/expect.js)

* ["extendible"](https://github.com/3rd-Eden/extendible)

* ["foxx_generator"](https://github.com/moonglum/foxx_generator)

* ["http-errors"](https://github.com/jshttp/http-errors)

* ["i"](https://github.com/pksunkara/inflect)

* ["joi"](https://github.com/hapijs/joi)
  is a validation library that is used throughout the Foxx framework.

* ["js-yaml"](https://github.com/nodeca/js-yaml)

* ["minimatch"](https://github.com/isaacs/minimatch)

* ["qs"](https://github.com/hapijs/qs)
  provides utilities for dealing with query strings using a different format
  than the **querystring** module.

* ["ramda"](http://ramdajs.com)

* ["semver"](https://github.com/npm/node-semver)

* ["sinon"](http://sinonjs.org)

* ["underscore"](http://underscorejs.org)

!SUBSECTION Installing NPM Modules

You can install additional modules using `npm install`. Note the following limitations in ArangoDB's compatibility with node or browser modules:

* modules must be implemented in pure JavaScript (no native extensions)
* modules must be strictly synchronous (e.g. no setTimeout or promises)
* only a subset of node's built-in modules are supported (see above)
* the same limitations apply to each module's dependencies

!SUBSECTION require

`require(path)`

*require* checks if the module or package specified by *path* has already
been loaded.  If not, the content of the file is executed in a new
context. Within the context you can use the global variable *exports* in
order to export variables and functions. This variable is returned by
*require*.

Assume that your module file is *test1.js* and contains

```js
exports.func1 = function() {
  print("1");
};

exports.const1 = 1;
```

Then you can use *require* to load the file and access the exports.

```js
unix> ./arangosh
arangosh> var test1 = require("test1");

arangosh> test1.const1;
1

arangosh> test1.func1();
1
```

*require* follows the specification
[Modules/1.1.1](http://wiki.commonjs.org/wiki/Modules/1.1.1).


*require* will inject two variables into the context of the required code:

- `__filename`: contains the name of the required file/module

- `__dirname`: contains the directory name of the required file/module

The values in `__filename` and `__dirname` can be used for generic debugging and for
creating filename relative to the required file, e.g.

    @startDocuBlockInline MODJS_fsDir
    @EXAMPLE_ARANGOSH_OUTPUT{MODJS_fsDir}
    var files = require("fs");
    relativeFile = files.join(__dirname, "scripts", "test.js");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock MODJS_fsDir


*require* can also be used to load JSON data. If the name of the required module ends
with *.json*, *require* will run a `JSON.parse()` on the file/module contents and return
the result.

Note: the purpose of *require* is to load modules or packages. It cannot be used to load 
arbitrary JavaScript files.


!SECTION Modules Path versus Modules Collection

ArangoDB comes with predefined modules defined in the file-system under the path
specified by *startup.startup-directory*. In a standard installation this
point to the system share directory. Even if you are an administrator of
ArangoDB you might not have write permissions to this location. On the other
hand, in order to deploy some extension for ArangoDB, you might need to install
additional JavaScript modules. This would require you to become root and copy
the files into the share directory. In order to ease the deployment of
extensions, ArangoDB uses a second mechanism to look up JavaScript modules.

JavaScript modules can either be stored in the filesystem as regular file or in
the database collection *_modules*.

If you execute

```js
require("com/example/extension")
```
then ArangoDB will try to locate the corresponding JavaScript as file as
follows

* There is a cache for the results of previous *require* calls. First of
  all ArangoDB checks if *com/example/extension* is already in the modules
  cache. If it is, the export object for this module is returned. No further
  JavaScript is executed.

* ArangoDB will then check, if there is a file called **com/example/extension.js** in the system search path. If such a file exists, it is executed in a new module context and the value of *exports* object is returned. This value is also stored in the module cache.

* If no file can be found, ArangoDB will check if the collection *_modules*
  contains a document of the form

```js
{
  path: "/com/example/extension",
  content: "...."
}
```

**Note**: The leading */* is important - even if you call *require* without a
leading */*.  If such a document exists, then the value of the *content*
attribute must contain the JavaScript code of the module. This string is
executed in a new module context and the value of *exports* object is
returned. This value is also stored in the module cache.

