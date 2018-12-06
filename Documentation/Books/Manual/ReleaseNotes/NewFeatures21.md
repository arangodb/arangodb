Features and Improvements in ArangoDB 2.1
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 2.1.  ArangoDB 2.1 also contains several bugfixes that are not listed
here.

New Edges Index
---------------

The edges index (used to store connections between nodes in a graph) internally
uses a new data structure. This data structure improves the performance when
populating the edge index (i.e. when loading an edge collection). For large
graphs loading can be 20 times faster than with ArangoDB 2.0.

Additionally, the new index fixes performance problems that occurred when many
duplicate `_from` or `_to` values were contained in the index.  Furthermore, the
new index supports faster removal of edges.

Finally, when loading an existing collection and building the edges index for
the collection, less memory re-allocations will be performed.

Overall, this should considerably speed up loading edge collections.

The new index type replaces the old edges index type automatically, without any
changes being required by the end user.

The API of the new index is compatible with the API of the old index.  Still it
is possible that the new index returns edges in a different order than the old
index. This is still considered to be compatible because the old index had never
guaranteed any result order either.

AQL Improvements
----------------

AQL offers functionality to work with dates. Dates are no data types of their own
in AQL (neither they are in JSON, which is often used as a format to ship data
into and out of ArangoDB). Instead, dates in AQL are internally represented by
either numbers (timestamps) or strings. The date functions in AQL provide
mechanisms to convert from a numeric timestamp to a string representation and
vice versa.

There are two date functions in AQL to create dates for further use:

- `DATE_TIMESTAMP(date)` Creates a UTC timestamp value from `date` 

- `DATE_TIMESTAMP(year, month, day, hour, minute, second, millisecond)`:
  Same as before, but allows specifying the individual date components separately.
  All parameters after `day` are optional.

- `DATE_ISO8601(date)`: Returns an ISO8601 datetime string from `date`.
  The datetime string will always use UTC time, indicated by the `Z` at its end.

- `DATE_ISO8601(year, month, day, hour, minute, second, millisecond)`:
  same as before, but allows specifying the individual date components separately.
  All parameters after `day` are optional.

These two above date functions accept the following input values:

- numeric timestamps, indicating the number of milliseconds elapsed since the UNIX
  epoch (i.e. January 1st 1970 00:00:00 UTC).
  An example timestamp value is `1399472349522`, which translates to 
  `2014-05-07T14:19:09.522Z`.

- datetime strings in formats `YYYY-MM-DDTHH:MM:SS.MMM`, `YYYY-MM-DD HH:MM:SS.MMM`, or 
  `YYYY-MM-DD`. Milliseconds are always optional.

  A timezone difference may optionally be added at the end of the string, with the
  hours and minutes that need to be added or subtracted to the datetime value.
  For example, `2014-05-07T14:19:09+01:00` can be used to specify a one hour offset,
  and `2014-05-07T14:19:09+07:30` can be specified for seven and half hours offset. 
  Negative offsets are also possible. Alternatively to an offset, a `Z` can be used
  to indicate UTC / Zulu time. 
 
  An example value is `2014-05-07T14:19:09.522Z` meaning May 7th 2014, 14:19:09 and 
  522 milliseconds, UTC / Zulu time. Another example value without time component is 
  `2014-05-07Z`.

  Please note that if no timezone offset is specified in a datestring, ArangoDB will
  assume UTC time automatically. This is done to ensure portability of queries across
  servers with different timezone settings, and because timestamps will always be
  UTC-based. 

- individual date components as separate function arguments, in the following order:
  - year 
  - month
  - day
  - hour
  - minute
  - second
  - millisecond

  All components following `day` are optional and can be omitted. Note that no
  timezone offsets can be specified when using separate date components, and UTC /
  Zulu time will be used.
 
The following calls to `DATE_TIMESTAMP` are equivalent and will all return 
`1399472349522`:

    DATE_TIMESTAMP("2014-05-07T14:19:09.522")
    DATE_TIMESTAMP("2014-05-07T14:19:09.522Z")
    DATE_TIMESTAMP("2014-05-07 14:19:09.522")
    DATE_TIMESTAMP("2014-05-07 14:19:09.522Z")
    DATE_TIMESTAMP(2014, 5, 7, 14, 19, 9, 522)
    DATE_TIMESTAMP(1399472349522)

The same is true for calls to `DATE_ISO8601` that also accepts variable input 
formats:

    DATE_ISO8601("2014-05-07T14:19:09.522Z")
    DATE_ISO8601("2014-05-07 14:19:09.522Z")
    DATE_ISO8601(2014, 5, 7, 14, 19, 9, 522)
    DATE_ISO8601(1399472349522)

The above functions are all equivalent and will return `"2014-05-07T14:19:09.522Z"`.

The following date functions can be used with dates created by `DATE_TIMESTAMP` and
`DATE_ISO8601`:

- `DATE_DAYOFWEEK(date)`: Returns the weekday number of `date`. The return values have
  the following meanings:
  - 0: Sunday
  - 1: Monday
  - 2: Tuesday
  - 3: Wednesday
  - 4: Thursday
  - 5: Friday
  - 6: Saturday

- `DATE_YEAR(date)`: Returns the year part of `date` as a number. 

- `DATE_MONTH(date)`: Returns the month part of `date` as a number.

- `DATE_DAY(date)`: Returns the day part of `date` as a number. 

- `DATE_HOUR(date)`: Returns the hour part of `date` as a number. 

- `DATE_MINUTE(date)`: Returns the minute part of `date` as a number. 

- `DATE_SECOND(date)`: Returns the seconds part of `date` as a number. 

- `DATE_MILLISECOND(date)`: Returns the milliseconds part of `date` as a number. 

The following other date functions are also available:

- `DATE_NOW()`: Returns the current time as a timestamp.
  
  Note that this function is evaluated on every invocation and may return different 
  values when invoked multiple times in the same query.

The following other AQL functions have been added in ArangoDB 2.1:

- `FLATTEN`: this function can turn an array of sub-arrays into a single flat array. All
  array elements in the original array will be expanded recursively up to a configurable
  depth. The expanded values will be added to the single result array.

  Example:

      FLATTEN([ 1, 2, [ 3, 4 ], 5, [ 6, 7 ], [ 8, [ 9, 10 ] ])

  will expand the sub-arrays on the first level and produce:

      [ 1, 2, 3, 4, 5, 6, 7, 8, [ 9, 10 ] ]

  To fully flatten the array, the maximum depth can be specified (e.g. with a value of `2`):
      
      FLATTEN([ 1, 2, [ 3, 4 ], 5, [ 6, 7 ], [ 8, [ 9, 10 ] ], 2)

  This will fully expand the sub-arrays and produce:
      
      [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ]

- `CURRENT_DATABASE`: this function will return the name of the database the current
  query is executed in.

- `CURRENT_USER`: this function returns the name of the current user that is executing
  the query. If authorization is turned off or the query is executed outside of a 
  request context, no user is present and the function will return `null`.

Cluster Dump and Restore
------------------------

The dump and restore tools, _arangodump_ and _arangorestore_, can now be used to
dump and restore collections in a cluster. Additionally, a collection dump from
a standalone ArangoDB server can be imported into a cluster, and vice versa.

Web Interface Improvements
--------------------------

The web interface in version 2.1 has a more compact dashboard. It provides
charts with time-series for incoming requests, HTTP transfer volume and some
server resource usage figures.

Additionally it provides trend indicators (e.g. 15 min averages) and
distribution charts (aka histogram) for some figures.

Foxx Improvements
-----------------

To easily access a file inside the directory of a Foxx application from within
Foxx, Foxx's `applicationContext` now provides the `foxxFilename()` function. It
can be used to assemble the full filename of a file inside the application's
directory. The `applicationContext` can be accessed as global variable from any
module within a Foxx application.

The filename can be used inside Foxx actions or setup / teardown scripts,
e.g. to populate a Foxx application's collection with data.

The `require` function now also prefers local modules when used from inside a
Foxx application. This allows putting modules inside the Foxx application
directory and requiring them easily. It also allows using application-specific
versions of libraries that are bundled with ArangoDB (such as underscore.js).

Windows Installer
-----------------

The Windows installer shipped with ArangoDB now supports installation of
ArangoDB for the current user or all users, with the required privileges.  It
also supports the installation of ArangoDB as a service.

Fixes for 32 bit systems
------------------------

Several issues have been fixed that occurred only when using ArangoDB on a 32 bits
operating system, specifically:

- a crash in a third party component used to manage cluster data

- a third party library that failed to initialize on 32 bit Windows, making arangod
  and arangosh crash immediately.

- overflows of values used for nanosecond-precision timeouts: these overflows
  have led to invalid values being passed to socket operations, making them fail
  and re-try too often 

Updated drivers
---------------

Several drivers for ArangoDB have been checked for compatibility with 2.1.  The
current list of drivers with compatibility notes can be found online
[here](https://www.arangodb.org/driver).

C++11 usage
-----------

We have moved several files from C to C++, allowing more code reuse and reducing
the need for shipping data between the two. We have also decided to require
C++11 support for ArangoDB, which allows us to use some of the simplifications,
features and guarantees that this standard has in stock.

That also means a compiler with C++11 support is required to build ArangoDB from
source. For instance GNU CC of at least version 4.8.

Miscellaneous Improvements
--------------------------

- Cancelable asynchronous jobs: several potentially long-running jobs can now be
  canceled via an explicit cancel operation. This allows stopping long-running
  queries, traversals or scripts without shutting down the complete ArangoDB
  process. Job cancelation is provided for asynchronously executed jobs as is
  described in @ref HttpJobCancel.

- Server-side periodic task management: an ArangoDB server now provides
  functionality to register and unregister periodic tasks. Tasks are
  user-defined JavaScript actions that can be run periodically and
  automatically, independent of any HTTP requests.

  The following task management functions are provided:

  - require("org/arangodb/tasks").register(): registers a periodic task
  - require("org/arangodb/tasks").unregister(): unregisters and removes a periodic task
  - require("org/arangodb/tasks").get(): retrieves a specific tasks or all existing tasks

  An example task (to be executed every 15 seconds) can be registered like this:
    
      var tasks = require("org/arangodb/tasks");
      tasks.register({
        name: "this is an example task with parameters",
        period: 15,
        command: function (params) {
          var greeting = params.greeting;
          var data = JSON.stringify(params.data);
          require('console').log('%s from parameter task: %s', greeting, data);
        },
        params: { greeting: "hi", data: "how are you?" }
      });

  Please refer to the section @ref Tasks for more details.

- The `figures` method of a collection now returns data about the collection's
  index memory consumption. The returned value `indexes.size` will contain the
  total amount of memory acquired by all indexes of the collection. This figure
  can be used to assess the memory impact of indexes.

- Capitalized HTTP response headers: from version 2.1, ArangoDB will return
  capitalized HTTP headers by default, e.g. `Content-Length` instead of
  `content-length`.  Though the HTTP specification states that headers field
  name are case-insensitive, several older client tools rely on a specific case
  in HTTP response headers.  This changes make ArangoDB a bit more compatible
  with those.

- Simplified usage of `db._createStatement()`: to easily run an AQL query, the
  method `db._createStatement` now allows passing the AQL query as a string.
  Previously it required the user to pass an object with a `query` attribute
  (which then contained the query string).

  ArangoDB now supports both versions:

      db._createStatement(queryString);
      db._createStatement({ query: queryString });
