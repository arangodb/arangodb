Queries Module
==============

`const queries = require('@arangodb/aql/queries')`

The query module provides the infrastructure for working with currently running AQL queries via arangosh.

Properties
----------

`queries.properties()` Returns the servers current query tracking configuration; we change the slow query threshold to get better results:

    @startDocuBlockInline QUERY_01_properyOfQueries
    @EXAMPLE_ARANGOSH_OUTPUT{QUERY_01_properyOfQueries}
    var queries = require("@arangodb/aql/queries");
    queries.properties();
    queries.properties({slowQueryThreshold: 1});
    queries.properties({slowStreamingQueryThreshold: 1});
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock QUERY_01_properyOfQueries

Currently running queries
-------------------------

We [create a task](Tasks.md) that spawns queries, so we have nice output. Since this task
uses resources, you may want to increase `period` (and not forget to remove it... afterwards):

    @startDocuBlockInline QUERY_02_listQueries
    @EXAMPLE_ARANGOSH_OUTPUT{QUERY_02_listQueries}
    ~var queries = require("@arangodb/aql/queries");
    var theQuery = 'FOR sleepLoooong IN 1..5 LET sleepLoooonger = SLEEP(1000) RETURN sleepLoooong';
    var tasks = require("@arangodb/tasks");
    |tasks.register({
    |  id: "mytask-1",
    |  name: "this is a sample task to spawn a slow aql query",
    |  command: "require('@arangodb').db._query('" + theQuery + "');"
    });
    |~ while (true) {
    |~   require("internal").wait(1);
    |~   if (queries.current().filter(function(query) {
    |~                           return query.query === theQuery;
    |~                         }).length > 0) {
    |~     break;
    |~   }
    ~}
    queries.current();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock QUERY_02_listQueries
The function returns the currently running AQL queries as an array.

Slow queries
------------

The function returns the last AQL queries that exceeded the slow query threshold as an array:

    @startDocuBlockInline QUERY_03_listSlowQueries
    @EXAMPLE_ARANGOSH_OUTPUT{QUERY_03_listSlowQueries}
    ~var queries = require("@arangodb/aql/queries");
    queries.slow();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock QUERY_03_listSlowQueries

Clear slow queries
------------------

Clear the list of slow AQL queries:

    @startDocuBlockInline QUERY_04_clearSlowQueries
    @EXAMPLE_ARANGOSH_OUTPUT{QUERY_04_clearSlowQueries}
    ~var queries = require("@arangodb/aql/queries");
    queries.clearSlow();
    queries.slow();
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock QUERY_04_clearSlowQueries

Kill
----

Kill a running AQL query:

    @startDocuBlockInline QUERY_05_killQueries
    @EXAMPLE_ARANGOSH_OUTPUT{QUERY_05_killQueries}
    ~var queries = require("@arangodb/aql/queries");
    ~var tasks = require("@arangodb/tasks");
    ~var theQuery = 'FOR sleepLoooong IN 1..5 LET sleepLoooonger = SLEEP(1000) RETURN sleepLoooong';
    |var runningQueries = queries.current().filter(function(query) {
    |   return query.query === theQuery;
    });
    queries.kill(runningQueries[0].id);
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock QUERY_05_killQueries
