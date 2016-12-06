Related modules
===============

These are some of the modules outside of Foxx you will find useful when writing Foxx services.

Additionally there are modules providing some level of compatibility with Node.js as well as a number of bundled NPM modules (like lodash and joi). For more information on these modules see [the JavaScript modules appendix](../Appendix/JavaScriptModules/README.md).

The `@arangodb` module
----------------------

This module provides access to various ArangoDB internals as well as three of the most important exports necessary to work with the database in Foxx:

### The `db` object

`require('@arangodb').db`

The `db` object represents the current database and lets you access collections and run queries. For more information see the [db object reference](../Appendix/References/DBObject.html).

**Examples**

```js
const db = require('@arangodb').db;

const thirteen = db._query('RETURN 5 + 8')[0];
```

### The `aql` template string handler

`require('@arangodb').aql`

The `aql` function is a JavaScript template string handler. It can be used to write complex AQL queries as multi-line strings without having to worry about bindVars and the distinction between collections and regular parameters.

To use it just prefix a JavaScript template string (the ones with backticks instead of quotes) with its import name (e.g. `aql`) and pass in variables like you would with a regular template string. The string will automatically be converted into an object with `query` and `bindVars` attributes which you can pass directly to `db._query` to execute. If you pass in a collection it will be automatically recognized as a collection reference and handled accordingly.

To find out more about AQL see the [AQL documentation](../../AQL/index.html).

**Examples**

```js
const aql = require('@arangodb').aql;

const filterValue = 23;
const mydata = db._collection('mydata');
const result = db._query(aql`
  FOR d IN ${mydata}
  FILTER d.num > ${filterValue}
  RETURN d
`);
```

### The `errors` object

`require('@arangodb').errors`

This object provides useful objects for each error code ArangoDB might use in `ArangoError` errors. This is helpful when trying to catch specific errors raised by ArangoDB, e.g. when trying to access a document that does not exist. Each object has a `code` property corresponding to the `errorNum` found on `ArangoError` errors.

For a complete list of the error names and codes you may encounter see the [appendix on error codes](../Appendix/ErrorCodes.md).

**Examples**

```js
const errors = require('@arangodb').errors;

try {
  mydata.document('does-not-exist');
} catch (e) {
  if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
    res.throw(404, 'Document does not exist');
  }
  res.throw(500, 'Something went wrong', e);
}
```

The `@arangodb/request` module
------------------------------

`require('@arangodb/request')`

This module provides a function for making HTTP requests to external services. Note that while this allows communicating with third-party services it may affect database performance by blocking Foxx requests as ArangoDB waits for the remote service to respond. If you routinely make requests to slow external services and are not directly interested in the response it is probably a better idea to delegate the actual request/response cycle to a gateway service running outside ArangoDB.

You can find a full description of this module [in the request module appendix](../Appendix/JavaScriptModules/Request.md).

The `@arangodb/general-graph` module
------------------------------------

`require('@arangodb/general-graph')`

This module provides access to ArangoDB graph definitions and various low-level graph operations in JavaScript. For more complex queries it is probably better to use AQL but this module can be useful in your setup and teardown scripts to create and destroy graph definitions.

For more information see the [chapter on the general graph module](../Graphs/GeneralGraphs/README.md).
