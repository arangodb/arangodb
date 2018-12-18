Features and Improvements in ArangoDB 3.0
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 3.0. ArangoDB 3.0 also contains several bugfixes that are not listed
here.

Internal data format changes
----------------------------

ArangoDB now uses [VelocyPack](https://github.com/arangodb/velocypack) for
storing documents, query results and temporarily computed values. Using a single
data format removed the need for some data conversions in the core that slowed
operations down previously.

The VelocyPack format is also quite compact, and reduces storage space
requirements for "small" values such as boolean, integers, short strings. This
can speed up several operations inside AQL queries.

VelocyPack document entries stored on disk are also self-contained, in the sense
that each stored document will contain all of its data type and attribute name
descriptions. While this may require a bit more space for storing the documents,
it removes the overhead of fetching attribute names and document layout from
shared structures as in previous versions of ArangoDB. It also simplifies the
code paths for storing and reading documents.

AQL improvements
----------------

### Syntax improvements

#### `LIKE` string-comparison operator

AQL now provides a `LIKE` operator and can be used to compare strings like this,
for example inside filter conditions:

```
value LIKE search
```

This change makes `LIKE` an AQL keyword. Using `LIKE` as an attribute or collection 
name in AQL thus requires quoting the name from now on.

The `LIKE` operator is currently implemented by calling the already existing AQL
function `LIKE`, which also remains operational in 3.0. Use the `LIKE` function
in case you want to search case-insensitive (optional parameter), as the `LIKE`
operator always compares case-sensitive.

#### AQL array comparison operators

All AQL comparison operators now also exist in an array variant. In the
array variant, the operator is preceded with one of the keywords *ALL*, *ANY*
or *NONE*. Using one of these keywords changes the operator behavior to
execute the comparison operation for all, any, or none of its left hand
argument values. It is therefore expected that the left hand argument
of an array operator is an array.

Examples:

```
[ 1, 2, 3 ] ALL IN [ 2, 3, 4 ]   // false
[ 1, 2, 3 ] ALL IN [ 1, 2, 3 ]   // true
[ 1, 2, 3 ] NONE IN [ 3 ]        // false
[ 1, 2, 3 ] NONE IN [ 23, 42 ]   // true
[ 1, 2, 3 ] ANY IN [ 4, 5, 6 ]   // false
[ 1, 2, 3 ] ANY IN [ 1, 42 ]     // true
[ 1, 2, 3 ] ANY == 2             // true
[ 1, 2, 3 ] ANY == 4             // false
[ 1, 2, 3 ] ANY > 0              // true
[ 1, 2, 3 ] ANY <= 1             // true
[ 1, 2, 3 ] NONE < 99            // false
[ 1, 2, 3 ] NONE > 10            // true
[ 1, 2, 3 ] ALL > 2              // false
[ 1, 2, 3 ] ALL > 0              // true
[ 1, 2, 3 ] ALL >= 3             // false
["foo", "bar"] ALL != "moo"      // true
["foo", "bar"] NONE == "bar"     // false
["foo", "bar"] ANY == "foo"      // true
```

#### Regular expression string-comparison operators

AQL now supports the operators *=~* and *!~* for testing strings against regular
expressions. *=~* tests if a string value matches a regular expression, and *!~* tests 
if a string value does not match a regular expression.

The two operators expect their left-hand operands to be strings, and their right-hand 
operands to be strings containing valid regular expressions as specified below.

The regular expressions may consist of literal characters and the following 
characters and sequences:

- `.` – the dot matches any single character except line terminators.
  To include line terminators, use `[\s\S]` instead to simulate `.` with *DOTALL* flag.
- `\d` – matches a single digit, equivalent to `[0-9]`
- `\s` – matches a single whitespace character
- `\S` – matches a single non-whitespace character
- `\t` – matches a tab character
- `\r` – matches a carriage return
- `\n` – matches a line-feed character
- `[xyz]` – set of characters. matches any of the enclosed characters (i.e.
  *x*, *y* or *z* in this case
- `[^xyz]` – negated set of characters. matches any other character than the
  enclosed ones (i.e. anything but *x*, *y* or *z* in this case)
- `[x-z]` – range of characters. Matches any of the characters in the 
  specified range, e.g. `[0-9A-F]` to match any character in
  *0123456789ABCDEF*
- `[^x-z]` – negated range of characters. Matches any other character than the
  ones specified in the range
- `(xyz)` – defines and matches a pattern group
- `(x|y)` – matches either *x* or *y*
- `^` – matches the beginning of the string (e.g. `^xyz`)
- `$` – matches the end of the string (e.g. `xyz$`)

Note that the characters `.`, `*`, `?`, `[`, `]`, `(`, `)`, `{`, `}`, `^`, 
and `$` have a special meaning in regular expressions and may need to be 
escaped using a backslash (`\\`). A literal backslash should also be escaped
using another backslash, i.e. `\\\\`.

Characters and sequences may optionally be repeated using the following
quantifiers:

- `x*` – matches zero or more occurrences of *x*
- `x+` – matches one or more occurrences of *x*
- `x?` – matches one or zero occurrences of *x*
- `x{y}` – matches exactly *y* occurrences of *x*
- `x{y,z}` – matches between *y* and *z* occurrences of *x*
- `x{y,}` – matches at least *y* occurences of *x*

#### Enclosing identifiers in forward ticks

AQL identifiers can now optionally be enclosed in forward ticks in addition to using
backward ticks. This allows convenient writing of AQL queries in JavaScript template 
strings (which are delimited with backticks themselves), e.g.

```js
var q = `FOR doc IN ´collection´ RETURN doc.´name´`;
```

### Functions added

The following AQL functions have been added in 3.0:

- *REGEX_TEST(value, regex)*: tests whether the string *value* matches the regular expression
  specified in *regex*. Returns *true* if it matches, and *false* otherwise.

  The syntax for regular expressions is the same as for the regular expression operators
  *=~* and *!~*.

- *HASH(value)*: Calculates a hash value for *value*. *value* is not required to be a 
  string, but can have any data type. The calculated hash value will take the data type
  of *value* into account, so for example the number *1* and the string *"1"* will have 
  different hash values. For arrays the hash values will be creared if the arrays contain
  exactly the same values (including value types) in the same order. For objects the same 
  hash values will be created if the objects have exactly the same attribute names and 
  values (including value types). The order in which attributes appear inside objects
  is not important for hashing.
  The hash value returned by this function is a number. The hash algorithm is not guaranteed
  to remain the same in future versions of ArangoDB. The hash values should therefore be
  used only for temporary calculations, e.g. to compare if two documents are the same, or
  for grouping values in queries.

- *TYPENAME(value)*: Returns the data type name of *value*. The data type name can
  be either *null*, *bool*, *number*, *string*, *array* or *object*.

- *LOG(value)*: Returns the natural logarithm of *value*. The base is Euler's constant 
  (2.71828...).

- *LOG2(value)*: Returns the base 2 logarithm of *value*.

- *LOG10(value)*: Returns the base 10 logarithm of *value*.

- *EXP(value)*: Returns Euler's constant (2.71828...) raised to the power of *value*.

- *EXP2(value)*: Returns 2 raised to the power of *value*.

- *SIN(value)*: Returns the sine of *value*.

- *COS(value)*: Returns the cosine of *value*.

- *TAN(value)*: Returns the tangent of *value*.

- *ASIN(value)*: Returns the arcsine of *value*.

- *ACOS(value)*: Returns the arccosine of *value*.

- *ATAN(value)*: Returns the arctangent of *value*.

- *ATAN2(y, x)*: Returns the arctangent of the quotient of *y* and *x*.

- *RADIANS(value)*: Returns the angle converted from degrees to radians.

- *DEGREES(value)*: Returns the angle converted from radians to degrees.

### Optimizer improvements

#### "inline-subqueries" rule

The AQL optimizer rule "inline-subqueries" has been added. This rule can pull
out certain subqueries that are used as an operand to a `FOR` loop one level
higher, eliminating the subquery completely. This reduces complexity of the
query's execution plan and will likely enable further optimizations. For
example, the query

```
FOR i IN (
    FOR j IN [1,2,3]
      RETURN j
  )
  RETURN i
```

will be transformed by the rule to:

```
FOR i IN [1,2,3]
  RETURN i
```

The query

```
FOR name IN (
  FOR doc IN _users
    FILTER doc.status == 1
    RETURN doc.name
  )
  LIMIT 2
  RETURN name
```

will be transformed into

```
FOR tmp IN _users
  FILTER tmp.status == 1
  LIMIT 2
  RETURN tmp.name
```

The rule will only fire when the subquery is used as an operand to a `FOR` loop,
and if the subquery does not contain a `COLLECT` with an `INTO` variable.

#### "remove-unnecessary-calculations" rule

The AQL optimizer rule "remove-unnecessary-calculations" now fires in more cases
than in previous versions. This rule removes calculations from execution plans,
and by having less calculations done, a query may execute faster or requires
less memory.

The rule will now remove calculations that are used exactly once in other
expressions (e.g. `LET a = doc RETURN a.value`) and calculations, or calculations 
that are just references to other variables (e.g. `LET a = b`).

#### "optimize-traversals" rule

The AQL optimizer rule "merge-traversal-filter" was renamed to "optimize-traversals".
The rule will remove unused edge and path result variables from the traversal in case 
they are specified in the `FOR` section of the traversal, but not referenced later in 
the query. This saves constructing edges and paths results that are not used later.

AQL now uses VelocyPack internally for storing intermediate values. For many value types
it can now get away without extra memory allocations and less internal conversions.
Values can be passed into internal AQL functions without copying them. This can lead to
reduced query execution times for queries that use C++-based AQL functions.

#### "replace-or-with-in" and "use-index-for-sort" rules

These rules now fire in some additional cases, which allows simplifying index lookup
conditions and removing SortNodes from execution plans.

Cluster state management
------------------------

The cluster's internal state information is now also managed by ArangoDB instances.
Earlier versions relied on third party software being installed for the storing the
cluster state.
The state is managed by dedicated ArangoDB instances, which can be started in a special
*agency* mode. These instances can operate in a distributed fashion. They will
automatically elect one of them to become their leader, being responsibile for storing
the state changes sent from servers in the cluster. The other instances will automatically
follow the leader and will transparently stand in should it become unavailable.
The agency instances are also self-organizing: they will continuously probe each
other and re-elect leaders. The communication between the agency instances use the
consensus-based RAFT protocol.

The operations for storing and retrieving cluster state information are now much less
expensive from an ArangoDB cluster node perspective, which in turn allows for faster
cluster operations that need to fetch or update the overall cluster state.

`_from` and `_to` attributes of edges are updatable and usable in indexes
-------------------------------------------------------------------------

In ArangoDB prior to 3.0 the attributes `_from` and `_to` of edges were treated
specially when loading or storing edges. That special handling led to these attributes
being not as flexible as regular document attributes. For example, the `_from` and
`_to` attribute values of an existing edge could not be updated once the edge was
created. Now this is possible via the single-document APIs and via AQL.

Additionally, the `_from` and `_to` attributes could not be indexed in
user-defined indexes, e.g. to make each combination of `_from` and `_to` unique.
Finally, as `_from` and `_to` referenced the linked collections by collection id
and not by collection name, their meaning became unclear once a referenced collection
was dropped. The collection id stored in edges then became unusable, and when
accessing such edge the collection name part of it was always translated to `_undefined`.

In ArangoDB 3.0, the `_from` and `_to` values of edges are saved as regular strings.
This allows using `_from` and `_to` in user-defined indexes. Additionally, this allows
to update the `_from` and `_to` values of existing edges. Furthermore, collections
referenced by `_from` and `_to` values may be dropped and re-created later. Any
`_from` and `_to` values of edges pointing to such dropped collection are unaffected
by the drop operation now.

Unified APIs for CRUD operations
--------------------------------

The CRUD APIs for documents and edge have been unified. Edges can now be inserted
and modified via the same APIs as documents. `_from` and `_to` attribute values can
be passed as regular document attributes now:

```js
db.myedges.insert({ _from: "myvertices/some", _to: "myvertices/other", ... });
```

Passing `_from` and `_to` separately as it was required in earlier versions is not
necessary anymore but will still work:

```js
db.myedges.insert("myvertices/some", "myvertices/other", { ... });
```

The CRUD operations now also support batch variants that works on arrays of
documents/edges, e.g.

```js
db.myedges.insert([
  { _from: "myvertices/some", _to: "myvertices/other", ... },
  { _from: "myvertices/who", _to: "myvertices/friend", ... },
  { _from: "myvertices/one", _to: "myvertices/two", ... },
]);
```

The batch variants are also available in ArangoDB's HTTP API. They can be used to
more efficiently carry out operations with multiple documents than their single-document
equivalents, which required one HTTP request per operation. With the batch operations,
the HTTP request/response overhead can be amortized across multiple operations.

Persistent indexes
------------------

ArangoDB 3.0 provides an experimental persistent index feature. Persistent indexes store
the index values on disk instead of in-memory only. This means the indexes do not need
to be rebuilt in-memory when a collection is loaded or reloaded, which should improve
collection loading times.

The persistent indexes in ArangoDB are based on the RocksDB engine.
To create a persistent index for a collection, create an index of type "rocksdb" as
follows:

```js
db.mycollection.ensureIndex({ type: "rocksdb", fields: [ "fieldname" ]});
```

The persistent indexes are sorted, so they allow equality lookups and range queries.
Note that the feature is still highly experimental and has some known deficiencies. It 
will be finalized until the release of the 3.0 stable version.

Upgraded V8 version
-------------------

The V8 engine that is used inside ArangoDB to execute JavaScript code has been upgraded from
version 4.3.61 to 5.0.71.39. The new version makes several more ES6 features available by
default, including

- arrow functions
- computed property names
- rest parameters
- array destructuring
- numeric and object literals

Web Admin Interface
-------------------

The ArangoDB 3.0 web interface is significantly improved. It now comes with a more
responsive design, making it easier to use on different devices. Navigation and menus
have been simplified, and related items have been regrouped to stay closer together
and allow tighter workflows.

The AQL query editor is now much easier to use. Multiple queries can be started and tracked 
in parallel, while results of earlier queries are still preserved. Queries still running 
can be canceled directly from the editor. The AQL query editor now allows the usage of bind 
parameters too, and provides a helper for finding collection names, AQL function names and 
keywords quickly.

The web interface now keeps track of whether the server is offline and of which server-side 
operations have been started and are still running. It now remains usable while such
longer-running operations are ongoing. It also keeps more state about user's choices (e.g. 
windows sizes, whether the tree or the code view was last used in the document editor).

Cluster statistics are now integrated into the web interface as well. Additionally, a
menu item "Help us" has been added to easily provide the ArangoDB team feedback about
the product.

The frontend may now be mounted behind a reverse proxy on a different path. For this to work
the proxy should send a X-Script-Name header containing the path.

A backend configuration for haproxy might look like this:

```
reqadd X-Script-Name:\ /arangodb
```

The frontend will recognize the subpath and produce appropriate links. ArangoDB will only
accept paths from trusted frontend proxies. Trusted proxies may be added on startup:

```
--frontend.proxy-request-check true --frontend.trusted-proxy 192.168.1.117
```

--frontend.trusted-proxy may be any address or netmask.

To disable the check and blindly accept any x-script-name set --frontend.proxy-request-check
to false.

Foxx improvements
-----------------

The Foxx framework has been completely rewritten for 3.0 with a new, simpler and 
more familiar API. The most notable changes are:

* Legacy mode for 2.8 services

  Stuck with old code? You can continue using your 2.8-compatible Foxx services with 
  3.0 by adding `"engines": {"arangodb": "^2.8.0"}` (or similar version ranges that 
  exclude 3.0 and up) to the service manifest.

* No more global variables and magical comments

  The `applicationContext` is now `module.context`. Instead of magical comments just 
  use the `summary` and `description` methods to document your routes.

* Repository and Model have been removed

  Instead of repositories just use ArangoDB collections directly. For validation simply 
  use the joi schemas (but wrapped in `joi.object()`) that previously lived inside the 
  model. Collections and queries return plain JavaScript objects.

* Controllers have been replaced with nestable routers

  Create routers with `require('@arangodb/foxx/router')()`, attach them to your service 
  with `module.context.use(router)`. Because routers are no longer mounted automagically, 
  you can export and import them like any other object. Use `router.use('/path', subRouter)` 
  to nest routers as deeply as you want.

* Routes can be named and reversed

  No more memorizing URLs: add a name to your route like
  `router.get('/hello/:name', function () {...}, 'hello')` and redirect to the full URL 
  with `res.redirect(req.resolve('hello', {name: 'world'}))`.

* Simpler express-like middleware

  If you already know express, this should be familiar. Here's a request logger in 
  three lines of code:

  ```js
  router.use(function (req, res, next) {
    var start = Date.now();
    try {next();}
    finally {console.log(`${req.method} ${req.url} ${res.statusCode} ${Date.now() - start}ms`);}
  });
  ```

* Sessions and auth without dependencies

  To make it easier to get started, the functionality previously provided by the 
  `simple-auth`, `oauth2`, `sessions-local` and `sessions-jwt` services have been moved 
  into Foxx as the `@arangodb/foxx/auth`, `@arangodb/foxx/oauth2` and `@arangodb/foxx/sessions` 
  modules.

Logging
-------

ArangoDB's logging is now grouped into topics. The log verbosity and output files can
be adjusted per log topic. For example

```
--log.level startup=trace --log.level queries=trace --log.level info
```

will log messages concerning startup at trace level, AQL queries at trace level and
everything else at info level. `--log.level` can be specified multiple times at startup,
for as many topics as needed.

Some relevant log topics available in 3.0 are:

- *collector*: information about the WAL collector's state
- *compactor*: information about the collection datafile compactor
- *datafiles*: datafile-related operations
- *mmap*: information about memory-mapping operations (including msync)
- *queries*: executed AQL queries, slow queries
- *replication*: replication-related info
- *requests*: HTTP requests
- *startup*: information about server startup and shutdown
- *threads*: information about threads

This also allows directing log output to different files based on topics. For
example, to log all AQL queries to a file "queries.log" one can use the options:

```
--log.level queries=trace --log.output queries=file:///path/to/queries.log
```

To additionally log HTTP request to a file named "requests.log" add the options:

```
--log.level requests=info --log.output requests=file:///path/to/requests.log
```

Build system
------------

ArangoDB now uses the cross-platform build system CMake for all its builds.
Previous versions used two different build systems, making development and
contributions harder than necessary. Now the build system is unified, and
all targets (Linux, Windows, MacOS) are built from the same set of build
instructions.

Documentation
-------------

The documentation has been enhanced and re-organized to be more intuitive.

A new introduction for beginners should bring you up to speed with ArangoDB
in less than an hour. Additional topics have been introduced and will be
extended with upcoming releases.

The topics AQL and HTTP API are now separated from the manual for better
searchability and less confusion. A version switcher makes it easier to
jump to the version of the docs you are interested in.
