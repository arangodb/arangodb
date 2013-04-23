JavaScript Modules{#JSModules}
==============================

@NAVIGATE_JSModules
@EMBEDTOC{JSModulesTOC}

Introduction to Javascript Modules{#JSModulesIntro}
===================================================

The ArangoDB uses a <a href="http://wiki.commonjs.org/wiki">CommonJS</a>
compatible module and package concept. You can use the function @FN{require} in
order to load a module or package. It returns the exported variables and
functions of the module or package.

CommonJS Modules{#JSModulesCommonJS}
------------------------------------

Unfortunately, the JavaScript libraries are just in the process of being
standardized. CommonJS has defined some important modules. ArangoDB implements
the following

- "console" is a well known logging facility to all the JavaScript developers.
  ArangoDB implements all of the functions described
  <a href="http://wiki.commonjs.org/wiki/Console">here</a>, with the exceptions
  of `profile` and `count`.

- "fs" provides a file system API for the manipulation of paths, directories, 
  files, links, and the construction of file streams. ArangoDB implements
  most of Filesystem/A functions described
  <a href="http://wiki.commonjs.org/wiki/Filesystem/A">here</a>.

- Modules are implemented according to 
  <a href="http://wiki.commonjs.org/wiki/Modules">Modules/1.1.1</a>

- Packages are implemented according to
  <a href="http://wiki.commonjs.org/wiki/Packages">Packages/1.0</a>

ArangoDB Specific Modules{#JSModulesArangoDB}
---------------------------------------------

A lot of the modules, however, are ArangoDB specific. These are described in the
following chapters. You can use the command-line option
@CO{startup.modules-path} to specify the location of the JavaScript files.

Node Modules{#JSModulesNode}
----------------------------

ArangoDB also support some <a href="http://www.nodejs.org/">node</a> modules.

- <a href="http://www.nodejs.org/api/assert.html">"assert"</a> implements 
  assertion and testing functions.

- <a href="http://www.nodejs.org/api/buffer.html">"buffer"</a> implements
  a binary data type for JavaScript.

- <a href="http://www.nodejs.org/api/path.html">"path"</a> implements
  functions dealing with filenames and paths.

- <a href="http://www.nodejs.org/api/punycode.html">"punycode"</a> implements
  conversion functions for
  <a href="http://en.wikipedia.org/wiki/Punycode">punycode</a> encoding.

- <a href="http://www.nodejs.org/api/querystring.html">"querystring"</a>
  provides utilities for dealing with query strings.

- <a href="http://www.nodejs.org/api/url.html">"url"</a>
  has utilities for URL resolution and parsing.

Node Packages{#JSModulesNPM}
----------------------------

The following <a href="https://npmjs.org/">node packages</a> are preinstalled.

- <a href="http://coffeescript.org/">"coffee-script"</a> implements a
  coffee-script to JavaScript compiler. ArangoDB supports the `compile` 
  function of the package, but not the `eval` functions.

You can use the command-line option @CO{startup.package-path} to specify the
location of the node packages.

require{#JSModulesRequire}
--------------------------

@FUN{require(@FA{path})}

@FN{require} checks if the module or package specified by @FA{path} has already
been loaded.  If not, the content of the file is executed in a new
context. Within the context you can use the global variable @LIT{exports} in
order to export variables and functions. This variable is returned by
@FN{require}.

Assume that your module file is @LIT{test1.js} and contains

    exports.func1 = function() {
      print("1");
    };

    exports.const1 = 1;

Then you can use @FN{require} to load the file and access the exports.

    unix> ./arangod --console /tmp/vocbase
    arangod> var test1 = require("test1");

    arangod> test1.const1;
    1

    arangod> test1.func1();
    1

@FN{require} follows the specification
<a href="http://wiki.commonjs.org/wiki/Modules/1.1.1">Modules/1.1.1</a>.

Modules Path versus Modules Collection{#JSModulesPath}
======================================================

ArangoDB comes with predefined modules defined in the file-system under the path
specified by @CO{startup.modules-path}. In a standard installation this point to
the system share directory. Even if you are an administrator of ArangoDB you
might not have write permissions to this location. On the other hand, in order
to deploy some extension for ArangoDB, you might need to install additional
JavaScript modules. This would require you to become root and copy the files
into the share directory. In order to easy the deployment of extensions,
ArangoDB uses a second mechanism to look up JavaScript modules.

JavaScript modules can either be stored in the filesystem as regular file or in
the database collection `_modules`.

Given the directory paths

    /usr/share/arangodb/A;/usr/share/arangodb/B

for @CO{startup.modules-path}. If you execute

    require("com/example/extension")

then ArangoDB will try to locate the corresponding JavaScript as file as
follows

- There is a cache for the results of previous `require` calls. First of
  all ArangoDB checks if `com/example/extension` is already in the modules
  cache. If it is, the export object for this module is returned. No further
  JavaScript is executed.

- ArangoDB will then check, if there is a file called 

    /usr/share/arangodb/A/com/example/extension.js

  If such a file exists, it is executed in a new module context and the value of
  `exports` object is returned. This value is also stored in the module cache.

- Next the file
  
    /usr/share/arangodb/B/com/example/extension.js

  is checked.

- If no file can be found, ArangoDB will check if the collection `modules`
  contains a document of the form
  
      {
        path: "com/example/extension",
        content: "...."
      }
  
  If such a document exists, then the value of the `content` attribute must
  contain the JavaScript code of the module. This string is executed in a new
  module context and the value of `exports` object is returned. This value is
  also stored in the module cache.

Path Normalization{#JSModulesNormalization}
-------------------------------------------

If you `require` a module, the path is normalized according the CommonJS rules
and an absolute path is used to look up the module. However, the leading `/` is
omitted.  Therefore, you should always use absolute paths without a leading `/`
when storing modules in the `_modules` collection.

For example, if you execute

    require("./lib/../extra/world");

within a module `com/example` the path used for look-up will be

    com/example/extra/world

Modules Cache{#JSModulesCache}
------------------------------

As `require` uses a module cache to store the exports objects of the required
modules, changing the design documents for the modules in the `_modules` collection
might have no effect at all.

You need to clear the cache, when manually changing documents in the `_modules`
collection.

    arangosh> require("internal").flushServerModules()

This initiate a flush of the modules in the ArangoDB *server* process.

Please note, that the ArangoDB JavaScript shell uses the same mechanism as the
server to locate JavaScript modules. But the do not share the same module cache.
If you flush the server cache, this will not flush the shell cache - and vice
versa.

In order to flush the modules cache of the JavaScript shell, you should use

    arangosh> require("internal").flushModuleCache()
