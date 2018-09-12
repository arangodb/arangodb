---
layout: default
title: CommonJS Modules 1.0
---

CommonJS Modules
================

This specification addresses how modules should be written in order to be interoperable among a class of module systems that can be both client and server side, secure or insecure, implemented today or supported by future systems with syntax extensions.  These modules are offered privacy of their top scope, facility for importing singleton objects from other modules, and exporting their own API.  By implication, this specification defines the minimum features that a module system must provide in order to support interoperable modules.

Contract
--------

### Module Context ###

1. In a module, there is a free variable "require", that is a function.
    1. The "require" function accepts a module identifier.
    2. "require" returns the exported API of the foreign module.
    3. If there is a dependency cycle, the foreign module may not have finished executing at the time it is required by one of its transitive dependencies; in this case, the object returned by "require" must contain at least the exports that the foreign module has prepared before the call to require that led to the current module's execution.
    4. If the requested module cannot be returned, "require" must throw an error.
2. In a module, there is a free variable called "exports", that is an object that the module may add its API to as it executes.
3. modules must use the "exports" object as the only means of exporting.

### Module Identifiers ###

1. A module identifier is a String of "terms" delimited by forward slashes.
2. A term must be a camelCase identifier, ".", or "..".
3. Module identifiers may not have file-name extensions like ".js".
4. Module identifiers may be "relative" or "top-level".  A module identifier is "relative" if the first term is "." or "..".
5. Top-level identifiers are resolved off the conceptual module name space root.
6. Relative identifiers are resolved relative to the identifier of the module in which "require" is written and called.

### Unspecified ###

This specification leaves the following important points of interoperability unspecified:

1. Whether modules are stored with a database, file system, or factory functions, or are interchangeable with link libraries.
2. Whether a PATH is supported by the module loader for resolving module identifiers.

Sample Code
-----------

*math.js:*

    :::js
    exports.add = function() {
        var sum = arguments[0];
        for (var i=1; i < arguments.length; i++) {
            sum += arguments[i];
        }
        return sum;
    };

*increment.js:*
    
    :::js
    var add = require('math').add;
    
    exports.increment = function(val) {
        return add(val, 1);
    };

*program.js:*

    :::js
    var inc = require('increment').increment;
    var a = 1;
    inc(a); // 2

Related Documents
-----------------

* Proposal to ECMA TC39: [Module System for ES-Harmony](http://docs.google.com/Doc?id=dfgxb7gk_34gpk37z9v&hl=en)
* Presentation to ECMA TC39: [Modules](http://docs.google.com/Presentation?docid=dcd8d5dk_0cs639jg8&hl=en)
