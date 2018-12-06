Features and Improvements in ArangoDB 2.4
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 2.4. ArangoDB 2.4 also contains several bugfixes that are not listed
here. For a list of bugfixes, please consult the [CHANGELOG](https://github.com/arangodb/arangodb/blob/devel/CHANGELOG).


V8 version upgrade
------------------

The built-in version of V8 has been upgraded from 3.16.14 to 3.29.59.
This activates several ES6 (also dubbed *Harmony* or *ES.next*) features in
ArangoDB, both in the ArangoShell and the ArangoDB server. They can be
used for scripting and in server-side actions such as Foxx routes, traversals
etc.

The following ES6 features are available in ArangoDB 2.4 by default:

* iterators
* the `of` operator
* symbols
* predefined collections types (Map, Set etc.)
* typed arrays

Many other ES6 features are disabled by default, but can be made available by
starting arangod or arangosh with the appropriate options:

* arrow functions
* proxies
* generators
* String, Array, and Number enhancements
* constants
* enhanced object and numeric literals

To activate all these ES6 features in arangod or arangosh, start it with 
the following options:

    arangosh --javascript.v8-options="--harmony --harmony_generators"

More details on the available ES6 features can be found in 
[this blog](https://jsteemann.github.io/blog/2014/12/19/using-es6-features-in-arangodb/).


FoxxGenerator
-------------

ArangoDB 2.4 is shipped with FoxxGenerator, a framework for building
standardized Hypermedia APIs easily. The generated APIs can be consumed with
client tools that understand [Siren](https://github.com/kevinswiber/siren).

Hypermedia is the simple idea that our HTTP APIs should have links between their
endpoints in the same way that our web sites have links between
them. FoxxGenerator is based on the idea that you can represent an API as a
statechart: Every endpoint is a state and the links are the transitions between
them. Using your description of states and transitions, it can then create an
API for you.

The FoxxGenerator can create APIs based on a semantic description of entities
and transitions. A blog series on the use cases and how to use the Foxx generator
is here:

* [part 1](https://www.arangodb.com/2014/11/26/building-hypermedia-api-json)
* [part 2](https://www.arangodb.com/2014/12/02/building-hypermedia-apis-design)
* [part 3](https://www.arangodb.com/2014/12/08/building-hypermedia-apis-foxxgenerator)

A cookbook recipe for getting started with FoxxGenerator is [here](https://docs.arangodb.com/2.8/Cookbook/FoxxGeneratorFirstSteps.html).

AQL improvements
----------------

### Optimizer improvements

The AQL optimizer has been enhanced to use of indexes in queries in several 
additional cases. Filters containing the `IN` operator can now make use of 
indexes, and multiple OR- or AND-combined filter conditions can now also use 
indexes if the filter conditions refer to the same indexed attribute.

Here are a few examples of queries that can now use indexes but couldn't before:

    FOR doc IN collection
      FILTER doc.indexedAttribute == 1 || doc.indexedAttribute > 99
      RETURN doc
    
    FOR doc IN collection
      FILTER doc.indexedAttribute IN [ 3, 42 ] || doc.indexedAttribute > 99
      RETURN doc

    FOR doc IN collection
      FILTER (doc.indexedAttribute > 2 && doc.indexedAttribute < 10) ||
             (doc.indexedAttribute > 23 && doc.indexedAttribute < 42)
      RETURN doc


Additionally, the optimizer rule `remove-filter-covered-by-index` has been
added. This rule removes FilterNodes and CalculationNodes from an execution 
plan if the filter condition is already covered by a previous IndexRangeNode. 
Removing the filter's CalculationNode and the FilterNode itself will speed
up query execution because the query requires less computation.

Furthermore, the new optimizer rule `remove-sort-rand` will remove a `SORT RAND()`
statement and move the random iteration into the appropriate `EnumerateCollectionNode`.
This is usually more efficient than individually enumerating and sorting.


### Data-modification queries returning documents

`INSERT`, `REMOVE`, `UPDATE` or `REPLACE` queries now can optionally return 
the documents inserted, removed, updated, or replaced. This is helpful for tracking
the auto-generated attributes (e.g. `_key`, `_rev`) created by an `INSERT` and in
a lot of other situations.

In order to return documents from a data-modification query, the statement must
immediately be immediately followed by a `LET` statement that assigns either the 
pseudo-value `NEW` or `OLD` to a variable. This `LET` statement must be followed 
by a `RETURN` statement that returns the variable introduced by `LET`:

    FOR i IN 1..100
      INSERT { value: i } IN test LET inserted = NEW RETURN inserted

    FOR u IN users
      FILTER u.status == 'deleted'
      REMOVE u IN users LET removed = OLD RETURN removed

    FOR u IN users
      FILTER u.status == 'not active'
      UPDATE u WITH { status: 'inactive' } IN users LET updated = NEW RETURN updated

`NEW` refers to the inserted or modified document revision, and `OLD` refers
to the document revision before update or removal. `INSERT` statements can 
only refer to the `NEW` pseudo-value, and `REMOVE` operations only to `OLD`. 
`UPDATE` and `REPLACE` can refer to either.

In all cases the full documents will be returned with all their attributes,
including the potentially auto-generated attributes such as `_id`, `_key`, or `_rev`
and the attributes not specified in the update expression of a partial update.


### Language improvements

#### `COUNT` clause

An optional `COUNT` clause was added to the `COLLECT` statement. The `COUNT`
clause allows for more efficient counting of values.

In previous versions of ArangoDB one had to write the following to count
documents:

    RETURN LENGTH (
      FOR doc IN collection
      FILTER ...some condition...
      RETURN doc
    )

With the `COUNT` clause, the query can be modified to
        
    FOR doc IN collection
      FILTER ...some condition...
      COLLECT WITH COUNT INTO length
      RETURN length

The latter query will be much more efficient because it will not produce any
intermediate results with need to be shipped from a subquery into the `LENGTH`
function.

The `COUNT` clause can also be used to count the number of items in each group:
    
    FOR doc IN collection
      FILTER ...some condition...
      COLLECT group = doc.group WITH COUNT INTO length
      return { group: group, length: length }


#### `COLLECT` modifications

In ArangoDB 2.4, `COLLECT` operations can be made more efficient if only a 
small fragment of the group values is needed later. For these cases, `COLLECT`
provides an optional conversion expression for the `INTO` clause. This 
expression controls the value that is inserted into the array of group values.
It can be used for projections.

The following query only copies the `dateRegistered` attribute of each document
into the groups, potentially saving a lot of memory and computation time 
compared to copying `doc` completely:
    
    FOR doc IN collection
      FILTER ...some condition...
      COLLECT group = doc.group INTO dates = doc.dateRegistered
      return { group: group, maxDate: MAX(dates) }

Compare this to the following variant of the query, which was the only way
to achieve the same result in previous versions of ArangoDB:

    FOR doc IN collection
      FILTER ...some condition...
      COLLECT group = doc.group INTO dates
      return { group: group, maxDate: MAX(dates[*].doc.dateRegistered) }

The above query will need to copy the full `doc` attribute into the `lengths`
variable, whereas the new variant will only copy the `dateRegistered`
attribute of each `doc`.


#### Subquery syntax

In previous versions of ArangoDB, subqueries required extra parentheses
around them, and this caused confusion when subqueries were used as function
parameters. For example, the following query did not work:

    LET values = LENGTH(
      FOR doc IN collection RETURN doc
    )

but had to be written as follows:    
    
    LET values = LENGTH((
      FOR doc IN collection RETURN doc
    ))

This was unintuitive and is fixed in version 2.4 so that both variants of 
the query are accepted and produce the same result.


### Web interface

The `Applications` tab for Foxx applications in the web interface has got 
a complete redesign.

It will now only show applications that are currently running in ArangoDB. 
For a selected application, a new detailed view has been created. This view 
provides a better overview of the app, e.g.: 

* author
* license
* version
* contributors
* download links
* API documentation

Installing a new Foxx application on the server is made easy using the new
`Add application` button. The `Add application` dialog provides all the 
features already available in the `foxx-manager` console application plus some more: 

* install a Foxx application from Github
* install a Foxx application from a zip file
* install a Foxx application from ArangoDB's application store
* create a new Foxx application from scratch: this feature uses a generator to
  create a Foxx application with pre-defined CRUD methods for a given list of collections. 
  The generated Foxx app can either be downloaded as a zip file or 
  be installed on the server. Starting with a new Foxx app has never been easier.


Miscellaneous improvements
--------------------------

### Default endpoint is 127.0.0.1

The default endpoint for the ArangoDB server has been changed from `0.0.0.0` to
`127.0.0.1`. This will make new ArangoDB installations unaccessible from clients other 
than localhost unless the configuration is changed. This is a security precaution
measure that has been requested as a feature a lot of times.

If you are the development option `--enable-relative`, the endpoint will still
be `0.0.0.0`.


### System collections in replication
  
By default, system collections are now included in replication and all replication API 
return values. This will lead to user accounts and credentials data being replicated 
from master to slave servers. This may overwrite slave-specific database users.

If this is undesired, the `_users` collection can be excluded from replication
easily by setting the `includeSystem` attribute to `false` in the following commands:

* replication.sync({ includeSystem: false });
* replication.applier.properties({ includeSystem: false });

This will exclude all system collections (including `_aqlfunctions`, `_graphs` etc.)
from the initial synchronization and the continuous replication.

If this is also undesired, it is also possible to specify a list of collections to
exclude from the initial synchronization and the continuous replication using the
`restrictCollections` attribute, e.g.:
      
```js
require("org/arangodb/replication").applier.properties({ 
  includeSystem: true,
  restrictType: "exclude",
  restrictCollections: [ "_users", "_graphs", "foo" ] 
});
```

