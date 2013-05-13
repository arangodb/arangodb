New Features in ArangoDB 1.3 {#NewFeatures13}
=============================================

@NAVIGATE_NewFeatures13
@EMBEDTOC{NewFeatures13TOC}

Features and Improvements {#NewFeatures13Introduction}
======================================================

The following list shows in detail which features have been added or improved in
ArangoDB 1.3.  ArangoDB 1.3 also contains several bugfixes that are not listed
here.

Changes to the Datafile Structure{#NewFeatures13Datafile}
---------------------------------------------------------

As the datafile structure has changed, please read the 
@ref Upgrading13 "upgrade manual" carefully.

Rapid API Development with FOXX{#NewFeatures13Foxx}
---------------------------------------------------

A preview of the forthcoming Foxx is contained in 1.3. Please note that this is
not the final version, Foxx is still experimental.

Foxx is a lightweight Javascript "micro framework" which allows you to build
applications directly on top of ArangoDB and therefore skip the middleman
(Rails, Django, Symfony or whatever your favorite web framework is). Inspired by
frameworks like Sinatra Foxx is designed with simplicity and the specific use
case of modern client-side MVC frameworks in mind.

The screencast at
<a href="http://foxx.arangodb.org">http://foxx.arangodb.org</a>
explains how to use Foxx.

Transactions{#NewFeatures13Transactions}
----------------------------------------

ArangoDB provides server-side transactions that allow executing multi-document
and even multi-collection operations with ACID guarantees.

Transactions in ArangoDB are defined by providing a JavaScript object which
needs to contain the transaction code, and some declarations about the
collections involved in the transaction.

The transaction code will be executed by the server en bloc. If execution of any
statement in the transaction code fails for whatever reason, the entire
transaction will be aborted and rolled back.

Data modifications done by transactions become visible to following transactions
only when a transaction succeeds. Data modifications that are performed by a
still-ongoing transaction are not exposed to other parallel transactions. In
fact, transactions on the same collection will be executed serially.

The following example will atomically transfer money from one user account to
another:

    db._create("accounts");
    db.accounts.save({ _key: "john", amount: 423 });
    db.accounts.save({ _key: "fred", amount: 197 });

    db._executeTransaction({
      collections: {
        write: "accounts"
      },
      params: {
        user1: "fred",
        user2: "john", 
        amount: 10
      },
      action: function (params) {
        var db = require("internal").db;
        var account1 = db.accounts.document(params['user1']);
        var account2 = db.accounts.document(params['user2']);
        var amount = params['amount'];

        if (account1.amount < amount) {
          throw "account of user '" + user1 + "' does not have enough money!";
        }

        db.accounts.update(account1, { amount : account1.amount - amount });
        db.accounts.update(account2, { amount : account2.amount + amount });

        /* will commit the transaction and return the value true */
        return true; 
      }
    });


Please refer to @ref Transactions for more details and examples on transaction
usage in ArangoDB.

New Administration Interface{#NewFeatures13Admin}
-------------------------------------------------

ArangoDB 1.3 comes with a new administration front-end. The front-end is now
based on backbone and uses repl.it, which allows for instance line editing when
using the browser based ArangoDB shell.

Please note, that the "Application" tab belongs to the forthcoming @ref
NewFeatures13Foxx Foxx. The functionality below this tab is neither stable nor
complete. It has been shipped as a feature preview.

New Server Statistics{#NewFeatures13Statistics}
-----------------------------------------------

The server statistics provided by ArangoDB have been changed in 1.3.

Before version 1.3, the server provided a multi-level history of request and
connection statistics. Values for each incoming request and connection were kept
individually and mapped to the chronological period they appeared in. The server
then provided aggregated values for different periods, which was implemented
using a constant recalculation of the aggregation values.

To lower ArangoDB's CPU usage, the constant recalculation has been removed in
1.3. Instead, the server will now only keep aggregate values per figure
reported, but will not provide any chronological values.

Request and connection statistics values are 0 at server start, and will be
increased with each incoming request or connection. Clients querying the
statistics will see the accumulated values only. They can calculate the values
for a period of time by querying the statistics twice and calculating the
difference between the values themselves.

The REST APIs for the statistics in ArangoDB 1.3 can be found at:

    /_admin/statistics
    /_admin/statistics-description

The `/_admin/statistics-description` API can be used by clients to get
descriptions for the figures reported by `/_admin/statistics`. The description
will contain a textual description, the unit used for the value(s) and the
boundary of slot values used.

The previoulsy available APIs 

    /_admin/request-statistics
    /_admin/connection-statistics

are not available in ArangoDB 1.3 anymore.

AQL extensions{#NewFeatures13AQL}
---------------------------------

It is now possible to extend AQL with user-defined functions.  These functions
need to be written in Javascript, and be registered before usage in an AQL
query.

    arangosh> var aqlfunctions = require("org/arangodb/aql/functions");
    arangosh> aqlfunctions.register("myfunctions:double", function (value) { return value * 2; }, true);
    false
    arangosh> db._query("RETURN myfunctions:double(4)").toArray();
    [ 8 ]

Please refer to @ref ExtendingAql for more details on this.

There have been the following additional changes to AQL in ArangoDB 1.3:

* added AQL statistical functions `VARIANCE_POPULATION`, `VARIANCE_SAMPLE`, 
  `STDDEV_POPULATION`, `STDDEV_SAMPLE`, `AVERAGE`, `MEDIAN`. These functions
  work on lists.

* added AQL numeric function `SQRT` to calculate square-roots.

* added AQL string functions `TRIM`, `LEFT` and `RIGHT` for easier string and
  substring handling.

* the AQL functions `REVERSE` and `LENGTH` now work on string values, too.
  Previously they were allowed for lists only.

* made "limit" an optional parameter in the `NEAR` function. The "limit" parameter
  can now be either omitted completely, or set to 0. If so, an internal
  default value (currently 100) will be applied for the limit.

Please refer to @ref AqlFunctions for detailed information on the AQL functions.

Node Modules and Packages{#NewFeatures13Node}
---------------------------------------------

ArangoDB 1.3 supports some of @ref JSModulesNode "modules" and @ref JSModulesNPM
"packages" from node. The most important module is maybe the Buffer support,
which allows to handle binary data in JavaScript.

    arangosh> var Buffer = require("buffer").Buffer;
    arangosh> a = new Buffer("414243", "hex");
    ABC
    arangosh> a = new Buffer("414243", "ascii");
    414243
    arangosh> a = new Buffer([48, 49, 50]);
    012

Supplying the Buffer class makes it possible to use other interesting modules
like punycode. It enables us to support some of NPM packages available - for
instance CoffeeScript.

    arangosh> var cs = require("coffee-script");
    arangosh> cs.compile("a = 1");
    (function() {
      var a;

      a = 1;

    }).call(this);

    arangosh> cs.compile("square = x -> x * x", { bare: true });
    var square;

    square = x(function() {
      return x * x;
    });

"underscore" is also preinstalled.

    arangosh> var _ = require("underscore");
    arangosh> _.map([1,2,3], function(x) {return x*x;});
    [ 
      1, 
      4, 
      9 
    ]

The node packages can be installed using npm in the "share/npm" directory. If
you find out, that a node package is also working under ArangoDB, please share
your findings with us and other users.

Miscelleanous changes{#NewFeatures13Misc}
-----------------------------------------

* Added server startup option `--database.force-sync-properties` to force syncing of
  collection properties on collection creation, deletion and on collection properties
  change.
  
  The default value is `true` to mimic the behavior of previous versions of ArangoDB.
  If set to `false`, collection properties are still written to disk but no immediate
  system call to sync() is made.

  Setting the `--database.force-sync-properties` to `false` may speed up running
  test suites on systems where sync() is expensive, but is discouraged for regular 
  use cases.

* ArangoDB will now reject saving documents with an invalid "type".

  Previous versions of ArangoDB didn't reject documents that were just scalar values
  without any attribute names.

  Starting with version 1.3, each document saved in ArangoDB must be a JSON object 
  consisting of attribute name / attribute value pairs.

  Storing the following types of documents will be rejected by the server:

      [ "foo", "bar" ]
      1.23
      "test"

  Of course such values can be stored inside valid documents, e.g.

      { "data" : [ "foo", "bar" ] }
      { "number" : 1.23 }
      { "value" : "test" }

  User-defined document attribute names must also start with a letter or a number.
  It is disallowed to use user-defined attribute names starting with an underscore.
  This is due to name starting with an underscore being reserved for ArangoDB's
  internal use.

* Changed return value of REST API method `/_admin/log`:

  Previously, the log messages returned by the API in the `text` attribute also 
  contained the date and log level, which was redundant.

  In ArangoDB 1.3, the values in the `text` attribute contain only the mere log
  message, and no date and log level. Dates and log levels for the individual 
  messages are still available in the separate `timestamp` and `level` attributes.

* Extended output of server version and components for REST APIs `/_admin/version` 
  and `/_api/version`: 
  
  To retrieve the extended information, the REST APIs can be called with the URL
  parameter `details=true`. This will provide a list of server version details in 
  the `details` attribute of the result.

* Extended output for REST API `/_api/collection/<name>/figures`:

  The result will now contain an attribute `attributes` with a sub-attribute `count`.
  This value provides the number of different attributes that are or have been used
  in the collection.
