Features and Improvements in ArangoDB 2.7
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 2.7. ArangoDB 2.7 also contains several bugfixes that are not listed
here. For a list of bugfixes, please consult the
[CHANGELOG](https://github.com/arangodb/arangodb/blob/devel/CHANGELOG).

Performance improvements
------------------------

### Index buckets

The primary indexes and hash indexes of collections can now be split into multiple
index buckets. This option was available for edge indexes only in ArangoDB 2.6.

A bucket can be considered a container for a specific range of index values. For
primary, hash and edge indexes, determining the responsible bucket for an index
value is done by hashing the actual index value and applying a simple arithmetic
operation on the hash.

Because an index value will be present in at most one bucket and buckets are 
independent, using multiple buckets provides the following benefits:

* initially building the in-memory index data can be parallelized even for a 
  single index, with one thread per bucket (or with threads being responsible
  for more than one bucket at a time). This can help reducing the loading time 
  for collections.

* resizing an index when it is about to run out of reserve space is performed
  per bucket. As each bucket only contains a fraction of the entire index,
  resizing and rehashing a bucket is much faster and less intrusive than resizing 
  and rehashing the entire index. 

When creating new collections, the default number of index buckets is `8` since
ArangoDB 2.7. In previous versions, the default value was `1`. The number of
buckets can also be adjusted for existing collections so they can benefit from
the optimizations. The number of index buckets can be set for a collection at
any time by using a collection's `properties` function:
 
```js
    db.collection.properties({ indexBuckets: 16 });
```

The number of index buckets must be a power of 2.

Please note that for building the index data for multiple buckets in parallel
it is required that a collection contains a significant amount of documents because
for a low number of documents the overhead of parallelization will outweigh its
benefits. The current threshold value is 256k documents, but this value may change
in future versions of ArangoDB. Additionally, the configuration option 
`--database.index-threads` will determine how many parallel threads may be used
for building the index data.


### Faster update and remove operations in non-unique hash indexes

The unique hash indexes in ArangoDB provided an amortized O(1) lookup, insert, update 
and remove performance. Non-unique hash indexes provided amortized O(1) insert
performance, but had worse performance for update and remove operations for
non-unique values. For documents with the same index value, they maintained a 
list of collisions. When a document was updated or removed, that exact document
had to be found in the collisions list for the index value. While getting to the
start of the collisions list was O(1), scanning the list had O(n) performance in
the worst case (with n being the number of documents with the same index value).
Overall, this made update and remove operations in non-unique hash indexes
slow if the index contained many duplicate values.

This has been changed in ArangoDB 2.7 so that non-unique hash indexes now also
provide update and remove operations with an amortized complexity of O(1), even
if there are many duplicates.

Resizing non-unique hash indexes now also doesn't require looking into the
document data (which may involve a disk access) because the index maintains some
internal cache value per document. When resizing and rehashing the index (or
an index bucket), the index will first compare only the cache values before
peeking into the actual documents. This change can also lead to reduced index
resizing times.


### Throughput enhancements

The ArangoDB-internal implementations for dispatching requests, keeping statistics
and assigning V8 contexts to threads have been improved in order to use less
locks. These changes allow higher concurrency and throughput in these components,
which can also make the server handle more requests in a given period of time.

What gains can be expected depends on which operations are executed, but there
are real-world cases in which [throughput increased by between 25 % and 70 % when
compared to 2.6](https://www.arangodb.com/2015/08/throughput-enhancements/). 


### Madvise hints

The Linux variant for ArangoDB provides the OS with madvise hints about index
memory and datafile memory. These hints can speed up things when memory is tight, 
in particular at collection load time but also for random accesses later. There
is no formal guarantee that the OS actually uses the madvise hints provided by 
ArangoDB, but actual measurements have shown improvements for loading bigger
collections.


AQL improvements
----------------

### Additional date functions

ArangoDB 2.7 provides several extra AQL functions for date and time 
calculation and manipulation. These functions were contributed 
by GitHub users @CoDEmanX and @friday. A big thanks for their work! 

The following extra date functions are available from 2.7 on:

* `DATE_DAYOFYEAR(date)`: Returns the day of year number of *date*.
   The return values range from 1 to 365, or 366 in a leap year respectively.

* `DATE_ISOWEEK(date)`: Returns the ISO week date of *date*. 
   The return values range from 1 to 53. Monday is considered the first day of the week. 
   There are no fractional weeks, thus the last days in December may belong to the first 
   week of the next year, and the first days in January may be part of the previous year's
   last week.

* `DATE_LEAPYEAR(date)`: Returns whether the year of *date* is a leap year.

* `DATE_QUARTER(date)`: Returns the quarter of the given date (1-based):
   * 1: January, February, March
   * 2: April, May, June
   * 3: July, August, September
   * 4: October, November, December

- *DATE_DAYS_IN_MONTH(date)*: Returns the number of days in *date*'s month (28..31).
   
* `DATE_ADD(date, amount, unit)`: Adds *amount* given in *unit* to *date* and
  returns the calculated date.

  *unit* can be either of the following to specify the time unit to add or
  subtract (case-insensitive):
  - y, year, years
  - m, month, months
  - w, week, weeks
  - d, day, days
  - h, hour, hours
  - i, minute, minutes
  - s, second, seconds
  - f, millisecond, milliseconds

  *amount* is the number of *unit*s to add (positive value) or subtract
  (negative value).

* `DATE_SUBTRACT(date, amount, unit)`: Subtracts *amount* given in *unit* from
  *date* and returns the calculated date.
  
  It works the same as `DATE_ADD()`, except that it subtracts. It is equivalent
  to calling `DATE_ADD()` with a negative amount, except that `DATE_SUBTRACT()`
  can also subtract ISO durations. Note that negative ISO durations are not
  supported (i.e. starting with `-P`, like `-P1Y`).

* `DATE_DIFF(date1, date2, unit, asFloat)`: Calculate the difference
  between two dates in given time *unit*, optionally with decimal places.
  Returns a negative value if *date1* is greater than *date2*.

* `DATE_COMPARE(date1, date2, unitRangeStart, unitRangeEnd)`: Compare two
  partial dates and return true if they match, false otherwise. The parts to
  compare are defined by a range of time units.
  
  The full range is: years, months, days, hours, minutes, seconds, milliseconds.
  Pass the unit to start from as *unitRangeStart*, and the unit to end with as
  *unitRangeEnd*. All units in between will be compared. Leave out *unitRangeEnd*
  to only compare *unitRangeStart*.

* `DATE_FORMAT(date, format)`: Format a date according to the given format string.
  It supports the following placeholders (case-insensitive):
  - %t: timestamp, in milliseconds since midnight 1970-01-01
  - %z: ISO date (0000-00-00T00:00:00.000Z)
  - %w: day of week (0..6)
  - %y: year (0..9999)
  - %yy: year (00..99), abbreviated (last two digits)
  - %yyyy: year (0000..9999), padded to length of 4
  - %yyyyyy: year (-009999 .. +009999), with sign prefix and padded to length of 6
  - %m: month (1..12)
  - %mm: month (01..12), padded to length of 2
  - %d: day (1..31)
  - %dd: day (01..31), padded to length of 2
  - %h: hour (0..23)
  - %hh: hour (00..23), padded to length of 2
  - %i: minute (0..59)
  - %ii: minute (00..59), padded to length of 2
  - %s: second (0..59)
  - %ss: second (00..59), padded to length of 2
  - %f: millisecond (0..999)
  - %fff: millisecond (000..999), padded to length of 3
  - %x: day of year (1..366)
  - %xxx: day of year (001..366), padded to length of 3
  - %k: ISO week date (1..53)
  - %kk: ISO week date (01..53), padded to length of 2
  - %l: leap year (0 or 1)
  - %q: quarter (1..4)
  - %a: days in month (28..31)
  - %mmm: abbreviated English name of month (Jan..Dec)
  - %mmmm: English name of month (January..December)
  - %www: abbreviated English name of weekday (Sun..Sat)
  - %wwww: English name of weekday (Sunday..Saturday)
  - %&: special escape sequence for rare occasions
  - %%: literal %
  - %: ignored


### RETURN DISTINCT

To return unique values from a query, AQL now provides the `DISTINCT` keyword.
It can be used as a modifier for `RETURN` statements, as a shorter alternative to
the already existing `COLLECT` statement.

For example, the following query only returns distinct (unique) `status`
attribute values from the collection:

```
    FOR doc IN collection
      RETURN DISTINCT doc.status
```

`RETURN DISTINCT` is not allowed on the top-level of a query if there is no `FOR` 
loop in front of it. `RETURN DISTINCT` is allowed in subqueries.

`RETURN DISTINCT` ensures that the values returned are distinct (unique), but does
not guarantee any order of results. In order to have certain result order, an
additional `SORT` statement must be added to a query.


### Shorthand object notation

AQL now provides a shorthand notation for object literals in the style of ES6
object literals:

```
    LET name = "Peter"
    LET age = 42
    RETURN { name, age }
```

This is equivalent to the previously available canonical form, which is still
available and supported:

```
    LET name = "Peter"
    LET age = 42
    RETURN { name : name, age : age }
```


### Array expansion improvements

The already existing <i>[\*]</i> operator has been improved with optional
filtering and projection and limit capabilities.

For example, consider the following example query that filters values from
an array attribute:
```
    FOR u IN users
      RETURN {
        name: u.name,
        friends: (
          FOR f IN u.friends
            FILTER f.age > u.age
            RETURN f.name
        )
      }
```

With the <i>[\*]</i> operator, this query can be simplified to

```
    FOR u IN users 
      RETURN { name: u.name, friends: u.friends[* FILTER CURRENT.age > u.age].name }
```

The pseudo-variable *CURRENT* can be used to access the current array element.
The `FILTER` condition can refer to `CURRENT` or any variables valid in the
outer scope.

To return a projection of the current element, there can now be an inline `RETURN`: 

```
    FOR u IN users 
      RETURN u.friends[* RETURN CONCAT(CURRENT.name, " is a friend of ", u.name)] 
```

which is the simplified variant for:

```
    FOR u IN users 
      RETURN (
        FOR friend IN u.friends
          RETURN CONCAT(friend.name, " is a friend of ", u.name)
      ) 
```


### Array contraction

In order to collapse (or flatten) results in nested arrays, AQL now provides the <i>[\*\*]</i> 
operator. It works similar to the <i>[\*]</i> operator, but additionally collapses nested
arrays. How many levels are collapsed is determined by the amount of <i>\*</i> characters used.

For example, consider the following query that produces a nested result:

```
    FOR u IN users 
      RETURN u.friends[*].name
```

The <i>[\*\*]</i> operator can now be applied to get rid of the nested array and 
turn it into a flat array. We simply apply the <i>[\*\*]</i> on the previous query
result: 

```
    RETURN (
      FOR u IN users RETURN u.friends[*].name
    )[**]
```


### Template query strings

Assembling query strings in JavaScript has been error-prone when using simple 
string concatenation, especially because plain JavaScript strings do not have
multiline-support, and because of potential parameter injection issues. While 
multiline query strings can be assembled with ES6 template strings since ArangoDB 2.5, 
and query bind parameters are there since ArangoDB 1.0 to prevent parameter
injection, there was no JavaScript-y solution to combine these.

ArangoDB 2.7 now provides an ES6 template string generator function that can
be used to easily and safely assemble AQL queries from JavaScript. JavaScript
variables and expressions can be used easily using regular ES6 template string 
substitutions:

```js
    let name = 'test';
    let attributeName = '_key';

    let query = aqlQuery`FOR u IN users 
      FILTER u.name == ${name} 
      RETURN u.${attributeName}`;
    db._query(query);
```

This is more legible than when using a plain JavaScript string and also does
not require defining the bind parameter values separately:

```js
    let name = 'test';
    let attributeName = '_key';

    let query = "FOR u IN users " +
      "FILTER u.name == @name " + 
      "RETURN u.@attributeName";
    db._query(query, { 
      name, 
      attributeName 
    });
```

The `aqlQuery` template string generator will also handle collection objects
automatically:

```js
    db._query(aqlQuery`FOR u IN ${ db.users } RETURN u.name`);
```

Note that while template strings are available in the JavaScript functions provided
to build queries, they aren't a feature of AQL itself. AQL could always handle
multiline query strings and provided bind parameters (`@...`) for separating
the query string and the parameter values. The `aqlQuery` template string
generator function will take care of this separation, too, but will do it
*behind the scenes*.


### AQL query result cache

The AQL query result cache can optionally cache the complete results of all or 
just selected AQL queries. It can be operated in the following modes:

* `off`: the cache is disabled. No query results will be stored
* `on`: the cache will store the results of all AQL queries unless their `cache`
  attribute flag is set to `false`
* `demand`: the cache will store the results of AQL queries that have their
  `cache` attribute set to `true`, but will ignore all others

The mode can be set at server startup using the `--database.query-cache-mode` 
configuration option and later changed at runtime. The default value is `off`,
meaning that the query result cache is disabled. This is because the cache may
consume additional memory to keep query results, and also because it must be 
invalidated when changes happen in collections for which results have been
cached. 

The query result cache may therefore have positive or negative effects on query 
execution times, depending on the workload: it will not make much sense turning
on the cache in write-only or write-mostly scenarios, but the cache may be
very beneficial in case workloads are read-only or read-mostly, and query are
complex. 

If the query cache is operated in `demand` mode, it can be controlled per query
if the cache should be checked for a result.


### Miscellaneous AQL changes

### Optimizer

The AQL optimizer rule `patch-update-statements` has been added. This rule can
optimize certain AQL UPDATE queries that update documents in a collection
that they also iterate over.

For example, the following query reads documents from a collection in order
to update them:

```
    FOR doc IN collection
      UPDATE doc WITH { newValue: doc.oldValue + 1 } IN collection
```

In this case, only a single collection is affected by the query, and there is
no index lookup involved to find the to-be-updated documents. In this case, the
UPDATE query does not require taking a full, memory-intensive snapshot of the 
collection, but it can be performed in small chunks. This can lead to memory
savings when executing such queries.

### Function call arguments optimization

This optimization will lead to arguments in function calls inside AQL queries 
not being copied but being passed by reference. This may speed up calls to 
functions with bigger argument values or queries that call AQL functions a lot 
of times.


Web Admin Interface
-------------------
  
The web interface now has a new design.

The "Applications" tab in the web interfaces has been renamed to "Services".

The ArangoDB API documentation has been moved from the "Tools" menu to the "Links" menu. 
The new documentation is based on Swagger 2.0 and opens in a separate web page.


Foxx improvements
-----------------

### ES2015 Classes

All Foxx constructors have been replaced with ES2015 classes and can be extended using the class syntax. 
The `extend` method is still supported at the moment but will become deprecated in ArangoDB 2.8 and removed in ArangoDB 2.9.

**Before:**

```js
var Foxx = require('org/arangodb/foxx');
var MyModel = Foxx.Model.extend({
  // ...
  schema: {/* ... */}
});
```

**After:**

```js
var Foxx = require('org/arangodb/foxx');
class MyModel extends Foxx.Model {
  // ...
}
MyModel.prototype.schema = {/* ... */};
```

### Confidential configuration

It is now possible to specify configuration options with the type `password`. The password type is equivalent to the text type but will be masked in the web frontend to prevent accidental exposure of confidential options like API keys and passwords when configuring your Foxx application.

### Dependencies

The syntax for specifying dependencies in manifests has been extended to allow specifying optional dependencies. Unmet optional dependencies will not prevent an app from being mounted. The traditional shorthand syntax for specifying non-optional dependencies will still be supported in the upcoming versions of ArangoDB.

**Before:**

```json
{
  ...
  "dependencies": {
    "notReallyNeeded": "users:^1.0.0",
    "totallyNecessary": "sessions:^1.0.0"
  }
}
```

**After:**

```json
{
  "dependencies": {
    "notReallyNeeded": {
      "name": "users",
      "version": "^1.0.0",
      "required": false
    },
    "totallyNecessary": {
      "name": "sessions",
      "version": "^1.0.0"
    }
  }
}
```


Replication
-----------

The existing replication HTTP API has been extended with methods that replication
clients can use to determine whether a given date, identified by a tick value, is
still present on a master for replication. By calling these APIs, clients can
make an informed decision about whether the master can still provide all missing
data starting from the point up to which the client had already synchronized.
This can be helpful in case a replication client is re-started after a pause.

Master servers now also track up the point up to which they have sent changes to
clients for replication. This information can be used to determine the point of data 
that replication clients have received from the master, and if and how far approximately
they lag behind. 

Finally, restarting the replication applier on a slave server has been made more
robust in case the applier was stopped while there were pending transactions on 
the master server, and re-starting the replication applier needs to restore the
state of these transactions.


Client tools
------------

The filenames in dumps created by arangodump now contain not only the name of the 
dumped collection, but also an additional 32-digit hash value. This is done to 
prevent overwriting dump files in case-insensitive file systems when there exist 
multiple collections with the same name (but with different cases). 
  
For example, if a database had two collections *test* and *Test*, previous
versions of arangodump created the following files: 
  
* `test.structure.json` and `test.data.json` for collection *test*
* `Test.structure.json` and `Test.data.json` for collection *Test*

This did not work in case-insensitive filesystems, because the files for the
second collection would have overwritten the files of the first. arangodump in 
2.7 will create the unique files in this case, by appending the 32-digit hash
value to the collection name in all case. These filenames will be unambiguous 
even in case-insensitive filesystems.


Miscellaneous changes
---------------------

### Better control-C support in arangosh

When CTRL-C is pressed in arangosh, it will now abort the locally running command
(if any). If no command was running, pressing CTRL-C will print a `^C` first. 
Pressing CTRL-C again will then quit arangosh.

CTRL-C can also be used to reset the current prompt while entering complex nested
objects which span multiple input lines.
  
CTRL-C support has been added to the ArangoShell versions built with Readline-support 
(Linux and MacOS only). The Windows version of ArangoDB uses a different library for 
handling input, and support for CTRL-C has not been added there yet. 


### Start / stop

Linux startup scripts and systemd configuration for arangod now try to adjust the 
NOFILE (number of open files) limits for the process. The limit value is set to 
131072 (128k) when ArangoDB is started via start/stop commands.

This will prevent arangod running out of available file descriptors in case of
many parallel HTTP connections or large collections with many datafiles.

Additionally, when ArangoDB is started/stopped manually via the start/stop commands, 
the main process will wait for up to 10 seconds after it forks the supervisor
and arangod child processes. If the startup fails within that period, the
start/stop script will fail with a non-zero exit code, allowing any invoking 
scripts to handle this error. Previous versions always returned an exit code of
0, even when arangod couldn't be started.

If the startup of the supervisor or arangod is still ongoing after 10 seconds, 
the main program will still return with exit code 0 in order to not block any
scripts. The limit of 10 seconds is arbitrary because the time required for an 
arangod startup is not known in advance.


### Non-sparse logfiles

WAL logfiles and datafiles created by arangod are now non-sparse. This prevents 
SIGBUS signals being raised when a memory-mapped region backed by a sparse datafile 
was accessed and the memory region was not actually backed by disk, for example
because the disk ran out of space.

arangod now always fully allocates the disk space required for a logfile or datafile
when it creates one, so the memory region can always be backed by disk, and memory
can be accessed without SIGBUS being raised.

