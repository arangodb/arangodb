ArangoDB Module
===============

`const arangodb = require('@arangodb')`

**Note**: This module should not be confused with the [`arangojs` JavaScript driver](https://github.com/arangodb/arangojs) which can be used to access ArangoDB from outside the database. Although the APIs share similarities and the functionality overlaps, the two are not compatible with each other and can not be used interchangeably.

The `db` object
---------------

`arangodb.db`

The `db` object represents the current database and lets you access collections and run queries. For more information see the [db object reference](../References/DBObject.html).

**Examples**

```js
const db = require('@arangodb').db;

const thirteen = db._query('RETURN 5 + 8').next();
```

The `aql` template string handler
---------------------------------

`arangodb.aql`

The `aql` function is a JavaScript template string handler. It can be used to write complex AQL queries as multi-line strings without having to worry about bindVars and the distinction between collections and regular parameters.

To use it just prefix a JavaScript template string (the ones with backticks instead of quotes) with its import name (e.g. `aql`) and pass in variables like you would with a regular template string. The string will automatically be converted into an object with `query` and `bindVars` attributes which you can pass directly to `db._query` to execute. If you pass in a collection it will be automatically recognized as a collection reference and handled accordingly.

To find out more about AQL see the [AQL documentation](../../../AQL/index.html).

**Examples**

```js
const aql = require('@arangodb').aql;

const filterValue = 23;
const mydata = db._collection('mydata');
const result = db._query(aql`
  FOR d IN ${mydata}
  FILTER d.num > ${filterValue}
  RETURN d
`).toArray();
```

The `errors` object
-------------------

`arangodb.errors`

This object provides useful objects for each error code ArangoDB might use in `ArangoError` errors. This is helpful when trying to catch specific errors raised by ArangoDB, e.g. when trying to access a document that does not exist. Each object has a `code` property corresponding to the `errorNum` found on `ArangoError` errors.

For a complete list of the error names and codes you may encounter see the [appendix on error codes](../ErrorCodes.md).

**Examples**

```js
const errors = require('@arangodb').errors;

try {
  someCollection.document('does-not-exist');
} catch (e) {
  if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
    throw new Error('Document does not exist');
  }
  throw new Error('Something went wrong');
}
```
