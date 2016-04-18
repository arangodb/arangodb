!CHAPTER Foxx Queries

This chapter describes helpers for performing AQL queries in Foxx. For a full overview of AQL syntax and semantics see the chapter on the ArangoDB Query Language (AQL).

!SECTION Raw AQL Queries

The most straightforward way to perform AQL queries is using the `db._query` API. You can learn more about this API in the [chapter on invoking AQL queries](../../Aql/Invoke.md).

**Examples**
```js
var db = require('org/arangodb').db;
var console = require('console');

var result = db._query('RETURN 42').toArray();
console.log(result);
```

!SECTION AQL Template Strings

ArangoDB supports ES2015-style template strings for queries using the `aqlQuery` template helper; First lets
have a look at how its working, then use it for a query:

**Examples**

```js
var db = require('org/arangodb').db;
var console = require('console');

var isAdmin = true;
var userCollection = applicationContext.collection('users');

var key = 'testKey';
aqlQuery`FOR c IN mycollection FILTER c._key == ${key} RETURN c._key`;
var results_in = {
  "query" : "FOR c IN mycollection FILTER c._key == @value0 RETURN c._key", 
  "bindVars" : { 
    "value0" : "testKey" 
  } 
}

var usernames = db._query(aqlQuery`
    FOR user IN ${userCollection}
    FILTER user.isAdmin == ${isAdmin}
    RETURN user
`).toArray();

console.log('usernames:', usernames);
```

!SECTION ArangoDB Query Builder

The [ArangoDB Query Builder](https://www.npmjs.org/package/aqb) NPM module comes bundled with Foxx and provides a fluid API for generating complex AQL queries while avoiding raw string concatenation.

Query Builder objects can be used in any function that would normally expect an AQL string.

For a full overview of the query builder API [see the project documentation](https://github.com/arangodb/aqbjs).

**Examples**

```js
var db = require('org/arangodb').db;
var qb = require('aqb');
var console = require('console');

var isAdmin = true;
var userCollection = applicationContext.collection('users');

var usernames = db._query(qb
    .for('user')
    .in(userCollection)
    .filter(qb(isAdmin).eq('user.isAdmin'))
    .return('user')
).toArray();

console.log('usernames:', usernames);
```

!SECTION Foxx.createQuery

`Foxx.createQuery(cfg)`

Creates a query function that performs the given query and returns the result.

The returned query function optionally takes an object as its argument. If an object is provided, its properties will be used as the query's bind parameters. Any additional arguments will be passed to the transform function (or dropped if no transform function is defined).

**Parameter**

* **cfg**: an object with the following properties:
    * **query**: an AQL query string or an ArangoDB Query Builder query object.
    * **params** (optional): an array of parameter names.
    * **context** (optional): an **applicationContext**.
    * **model** (optional): a **Foxx.Model** that will be applied to the query results.
    * **defaults** (optional): default values for the query's bind parameters. These can be overridden by passing a value for the same name to the query function.
    * **transform** (optional): a function that will be applied to the return value.

If **cfg** is a string, it will be used as the value of **cfg.query** instead.

If a **context** is specified, the values of all collection bind parameters will be passed through the context's **collectionName** method.

Note that collection bind parameters in AQL need to be referenced with two at-signs instead of one, e.g. `@@myCollectionVar` and their parameter name needs to be prefixed with an at-sign as well, e.g. `{'@myCollectionVar': 'collection_name'}`.

If **params** is provided, the query function will accept positional arguments instead of an object. If **params** is a string, it will be treated as an array containing that string.

If both *model*** and **transform** are provided, the **transform** function will be applied to the result array _after_ the results have been converted into model instances. The **transform** function is always passed the entire result array and its return value will be returned by the query function.

**Note**: `Foxx.createQuery` provides a high-level abstraction around the underlying `db._query` API and provides an easy way to generate Foxx models from query results at the expense of hiding internals like query metadata (e.g. number of documents affected by a modification query). If you just want to perform AQL queries and don't need the abstractions provided by `Foxx.createQuery` you can just use the lower-level `db._query` API directly.

**Examples**

Basic usage example:

```js
var query = Foxx.createQuery('FOR u IN _users RETURN u.user');
var usernames = query();
```

Using bind parameters:

```js
var query = Foxx.createQuery('FOR u IN _users RETURN u[@propName]');
var usernames = query({propName: 'user'});
```

Using named bind parameters:

```js
var query = Foxx.createQuery({
    query: 'FOR u IN _users RETURN u[@propName]',
    params: ['propName']
);
var usernames = query('user');
```

Using models:

```js
var joi = require('joi');
var UserModel = Foxx.Model.extend({
    schema: {
        user: joi.string().required()
    },
    getUsername: function () {
        return this.get('user');
    }
});
var query = Foxx.createQuery({
    query: 'FOR u IN _users RETURN u',
    model: UserModel
});
var users = query();
var username = users[0].getUsername();
```

Using a transformation:

```js
var query = Foxx.createQuery({
    query: 'FOR u IN _users SORT u.user ASC RETURN u',
    transform: function (results) {
        return results[0];
    }
});
var user = query(); // first user by username
```

Using a transformation with extra arguments:

```js
var query = Foxx.createQuery({
    query: 'FOR u IN _users SORT u.user ASC RETURN u[@propName]',
    transform: function (results, uppercase) {
        return uppercase ? results[0].toUpperCase() : results[0].toLowerCase();
    }
});
query({propName: 'user'}, true); // username of first user in uppercase
query({propName: 'user'}, false); // username of first user in lowercase
```

Using a transformation with extra arguments (using positional arguments):

```js
var query = Foxx.createQuery({
    query: 'FOR u IN _users SORT u.user ASC RETURN u[@propName]',
    params: ['propName'],
    transform: function (results, uppercase) {
        return uppercase ? results[0].toUpperCase() : results[0].toLowerCase();
    }
});
query('user', true); // username of first user in uppercase
query('user', false); // username of first user in lowercase
```

Using a transformation with extra arguments (and no query parameters):

```js
var query = Foxx.createQuery({
    query: 'FOR u IN _users SORT u.user ASC RETURN u.user',
    params: false, // an empty array would work, too
    transform: function (results, uppercase) {
        return uppercase ? results[0].toUpperCase() : results[0].toLowerCase();
    }
});
query(true); // username of first user in uppercase
query(false); // username of first user in lowercase
```
