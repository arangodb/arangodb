GraphQL integration
===================

`const createGraphQLRouter = require('@arangodb/foxx/graphql');`

Foxx bundles version 0.6 of the
[`graphql-sync` module](https://github.com/arangodb/graphql-sync), which is a
synchronous wrapper for the official JavaScript GraphQL reference
implementation, to allow writing GraphQL schemas directly inside Foxx.

Additionally the `@arangodb/foxx/graphql` lets you create routers for serving
GraphQL requests, which closely mimicks the behaviour of the
[`express-graphql` module](https://github.com/graphql/express-graphql).

For more information on `graphql-sync` see the
[`graphql-js` API reference](http://graphql.org/docs/api-reference-graphql/)
(note that `graphql-sync` never wraps results in promises).

For an example of a GraphQL schema in Foxx that resolves fields using the
database see [the GraphQL example service](https://github.com/arangodb-foxx/demo-graphql)
(also available from the Foxx store).

**Examples**

```js
const graphql = require('graphql-sync');
const graphqlSchema = new graphql.GraphQLSchema({
  // ...
});

// Mounting a graphql endpoint directly in a service:
module.context.use('/graphql', createGraphQLRouter({
  schema: graphqlSchema,
  graphiql: true
}));

// Or at the service's root URL:
module.context.use(createGraphQLRouter({
  schema: graphqlSchema,
  graphiql: true
}));

// Or inside an existing router:
router.get('/hello', function (req, res) {
  res.write('Hello world!');
});
router.use('/graphql', createGraphQLRouter({
  schema: graphqlSchema,
  graphiql: true
}));
```

**Note**: ArangoDB aims for stability which means bundled dependencies will
generally not be updated as quickly as their maintainers make updates
available on GitHub or NPM. Starting with ArangoDB 3.2, if you want to use a
newer version than the one bundled with your target version of ArangoDB, you
can provide your own version of the library by passing it via the `graphql` option:

```js
const graphql = require('graphql-sync');
const graphqlSchema = new graphql.Schema({
  //...
});
module.context.use(createGraphQLRouter({
  schema: graphqlSchema,
  graphiql: true,
  graphql: graphql
}))
```

Starting with `graphql` 0.12 you can also use
[the official graphql library](https://github.com/graphql/graphql-js) if you
include it in the `node_modules` folder of your service bundle:

```js
const graphql = require('graphql'); // 0.12 or later
const graphqlSchema = new graphql.Schema({
  //...
});
module.context.use(createGraphQLRouter({
  schema: graphqlSchema,
  graphiql: true,
  graphql: graphql
}))
```

Creating a router
-----------------

`createGraphQLRouter(options): Router`

This returns a new router object with POST and GET routes for serving GraphQL requests.

**Arguments**

* **options**: `object`

  An object with any of the following properties:

  * **schema**: `GraphQLSchema`

    A GraphQL Schema object from `graphql-sync`.

  * **context**: `any` (optional)

    The GraphQL context that will be passed to the `graphql()` function from
    `graphql-sync` to handle GraphQL queries.

  * **rootValue**: `object` (optional)

    The GraphQL root value that will be passed to the `graphql()` function
    from `graphql-sync` to handle GraphQL queries.

  * **pretty**: `boolean` (Default: `false`)

    If `true`, JSON responses will be pretty-printed.

  * **formatError**: `Function` (optional)

    A function that will be used to format errors produced by `graphql-sync`.
    If omitted, the `formatError` function from `graphql-sync` will be used instead.

  * **validationRules**: `Array<any>` (optional)

    Additional validation rules queries must satisfy in addition to those
    defined in the GraphQL spec.

  * **graphiql**: `boolean` (Default: `false`)

    If `true`, the [GraphiQL](https://github.com/graphql/graphiql) explorer
    will be served when loaded directly from a browser.

  * **graphql**: `object` (optional)

    If you need to use your own copy of the `graphql-sync` module instead of
    the one bundled with ArangoDB, here you can pass it in directly.

If a GraphQL Schema object is passed instead of an options object it will be
interpreted as the *schema* option.

Generated routes
----------------

The router handles GET and POST requests to its root path and accepts the
following parameters, which can be provided either as query parameters or
as the POST request body:

* **query**: `string`

  A GraphQL query that will be executed.

* **variables**: `object | string` (optional)

  An object or a string containing a JSON object with runtime values to use
  for any GraphQL query variables.

* **operationName**: `string` (optional)

  If the provided `query` contains multiple named operations, this specifies
  which operation should be executed.

* **raw**: `boolean` (Default: `false`)

  Forces a JSON response even if *graphiql* is enabled and the request was
  made using a browser.

The POST request body can be provided as JSON or as query string using
`application/x-www-form-urlencoded`. A request body passed as
`application/graphql` will be interpreted as the `query` parameter.
