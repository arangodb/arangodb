!CHAPTER Incompatible changes in ArangoDB 2.8

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 2.8, and adjust any client programs if necessary.


!SECTION AQL

!SUBSECTION Keywords added

The following AQL keywords were added in ArangoDB 2.8:

* `GRAPH`
* `OUTBOUND`
* `INBOUND`
* `ANY`
* `ALL`
* `NONE`
* `AGGREGATE`

Usage of these keywords for collection names, variable names or attribute names
in AQL queries will not be possible without quoting. For example, the following
AQL query will still work as it uses a quoted collection name and a quoted
attribute name:

```
FOR doc IN `OUTBOUND`
  RETURN doc.`any`
```

!SUBSECTION Changed behavior

The AQL functions `NEAR` and `WITHIN` now have stricter validations
for their input parameters `limit`, `radius` and `distance`. They may now throw
exceptions when invalid parameters are passed that may have not led
to exceptions in previous versions.


Additionally, the expansion (`[*]`) operator in AQL has changed its behavior when
handling non-array values:

In ArangoDB 2.8, calling the expansion operator on a non-array value will always
return an empty array. Previous versions of ArangoDB expanded non-array values by
calling the `TO_ARRAY()` function for the value, which for example returned an 
array with a single value for boolean, numeric and string input values, and an array
with the object's values for an object input value. This behavior was inconsistent
with how the expansion operator works for the array indexes in 2.8, so the behavior
is now unified:

- if the left-hand side operand of `[*]` is an array, the array will be returned as 
  is when calling `[*]` on it
- if the left-hand side operand of `[*]` is not an array, an empty array will be
  returned by `[*]`

AQL queries that rely on the old behavior can be changed by either calling `TO_ARRAY`
explicitly or by using the `[*]` at the correct position.

The following example query will change its result in 2.8 compared to 2.7:

    LET values = "foo" RETURN values[*]    

In 2.7 the query has returned the array `[ "foo" ]`, but in 2.8 it will return an
empty array `[ ]`. To make it return the array `[ "foo" ]` again, an explicit
`TO_ARRAY` function call is needed in 2.8 (which in this case allows the removal
of the `[*]` operator altogether). This also works in 2.7:

    LET values = "foo" RETURN TO_ARRAY(values)

Another example:

    LET values = [ { name: "foo" }, { name: "bar" } ]
    RETURN values[*].name[*]

The above returned `[ [ "foo" ], [ "bar" ] ] in 2.7. In 2.8 it will return 
`[ [ ], [ ] ]`, because the value of `name` is not an array. To change the results
to the 2.7 style, the query can be changed to

    LET values = [ { name: "foo" }, { name: "bar" } ]
    RETURN values[* RETURN TO_ARRAY(CURRENT.name)]

The above also works in 2.7. 
The following types of queries won't change:

    LET values = [ 1, 2, 3 ] RETURN values[*] 
    LET values = [ { name: "foo" }, { name: "bar" } ] RETURN values[*].name
    LET values = [ { names: [ "foo", "bar" ] }, { names: [ "baz" ] } ] RETURN values[*].names[*]
    LET values = [ { names: [ "foo", "bar" ] }, { names: [ "baz" ] } ] RETURN values[*].names[**]


!SUBSECTION Deadlock handling

Client applications should be prepared to handle error 29 (`deadlock detected`)
that ArangoDB may now throw when it detects a deadlock across multiple transactions.
When a client application receives error 29, it should retry the operation that
failed. 

The error can only occur for AQL queries or user transactions that involve 
more than a single collection.


!SUBSECTION Optimizer

The AQL execution node type `IndexRangeNode` was replaced with a new more capable
execution node type `IndexNode`. That means in execution plan explain output there
will be no more `IndexRangeNode`s but only `IndexNode`. This affects explain output
that can be retrieved via `require("org/arangodb/aql/explainer").explain(query)`,
`db._explain(query)`, and the HTTP query explain API.

The optimizer rule that makes AQL queries actually use indexes was also renamed
from `use-index-range` to `use-indexes`. Again this affects explain output
that can be retrieved via `require("org/arangodb/aql/explainer").explain(query)`,
`db._explain(query)`, and the HTTP query explain API.

The query optimizer rule `remove-collect-into` was renamed to `remove-collect-variables`.
This affects explain output that can be retrieved via `require("org/arangodb/aql/explainer").explain(query)`,
`db._explain(query)`, and the HTTP query explain API.


!SECTION HTTP API

When a server-side operation got canceled due to an explicit client cancel request 
via HTTP `DELETE /_api/job`, previous versions of ArangoDB returned an HTTP status
code of 408 (request timeout) for the response of the canceled operation.

The HTTP return code 408 has caused problems with some client applications. Some 
browsers (e.g. Chrome) handled a 408 response by resending the original request, 
which is the opposite of what is desired when a job should be canceled.

Therefore ArangoDB will return HTTP status code 410 (gone) for canceled operations
from version 2.8 on.


!SECTION Foxx

!SUBSECTION Model and Repository

Due to compatibility issues the Model and Repository types are no longer implemented as ES2015 classes.

The pre-2.7 "extend" style subclassing is supported again and will not emit any deprecation warnings.

```js
var Foxx = require('org/arangodb/foxx');
var MyModel = Foxx.Model.extend({
  // ...
  schema: {/* ... */}
});
```

!SUBSECTION Module resolution

The behavior of the JavaScript module resolution used by the `require` function has
been modified to improve compatibility with modules written for Node.js.

Specifically

* absolute paths (e.g. `/some/absolute/path`) are now always interpreted as absolute
  file system paths, relative to the file system root

* global names (e.g. `global/name`) are now first intepreted as references to modules
  residing in a relevant `node_modules` folder, a built-in module or a matching
  document in the internal `_modules` collection, and only resolved to local file paths
  if no other match is found

Previously the two formats were treated interchangeably and would be resolved to local
file paths first, leading to problems when local files used the same names as other
modules (e.g. a local file `chai.js` would cause problems when trying to load the
`chai` module installed in `node_modules`).

For more information see the [blog announcement of this change](https://www.arangodb.com/2015/11/foxx-module-resolution-will-change-in-2-8/)
and the [upgrade guide](../Administration/Upgrading/Upgrading28.md#upgrading-foxx-apps-generated-by-arangodb-27-and-earlier).

!SUBSECTION Module `org/arangodb/request`

The module now always returns response bodies, even for error responses. In versions
prior to 2.8 the module would silently drop response bodies if the response header
indicated an error.

The old behavior of not returning bodies for error responses can be restored by
explicitly setting the option `returnBodyOnError` to `false`:

```js
let response = request({
  //...
  returnBodyOnError: false
});
```

!SUBSECTION Garbage collection

The V8 garbage collection strategy was slightly adjusted so that it eventually
happens in all V8 contexts that hold V8 external objects (references to ArangoDB
documents and collections). This enables a better cleanup of these resources and
prevents other processes such as compaction being stalled while waiting for these
resources to be released.

In this context the default value for the JavaScript garbage collection frequency
(`--javascript.gc-frequency`) was also increased from 10 seconds to 15 seconds, 
as less internal operations in ArangoDB are carried out in JavaScript.

!SECTION Client tools

arangodump will now fail by default when trying to dump edges that
refer to already dropped collections. This can be circumvented by 
specifying the option `--force true` when invoking arangodump
