Getting Started
===============

This practical introduction will take you from an empty folder to a first
Foxx service querying data.

Manifest
--------

We're going to start with an empty folder. This will be the root folder of our
services. You can name it something clever but for the course of this guide
we'll assume it's called the name of your service: `getting-started`.

First we need to create a manifest. Create a new file called `manifest.json`
and add the following content:

```json
{
  "engines": {
    "arangodb": "^3.0.0"
  }
}
```

This just tells ArangoDB the service is compatible with versions 3.0.0 and
later (all the way up to but not including 4.0.0), allowing older versions
of ArangoDB to understand that this service likely won't work for them and
newer versions what behavior to emulate should they still support it.

The little hat to the left of the version number is not a typo, it's called a
"caret" and indicates the version range. Foxx uses semantic versioning (also
called "semver") for most of its version handling. You can find out more about
how semver works at the [official semver website](http://semver.org).

Next we'll need to specify an entry point to our service. This is the
JavaScript file that will be executed to define our service's HTTP endpoints.
We can do this by adding a "main" field to our manifest:

```json
{
  "engines": {
    "arangodb": "^3.0.0"
  },
  "main": "index.js"
}
```

That's all we need in our manifest for now.

Router
------

Let's next create the `index.js` file:

```js
'use strict';
const createRouter = require('@arangodb/foxx/router');
const router = createRouter();

module.context.use(router);
```

The first line causes our file to be interpreted using
[strict mode](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Strict_mode).
All examples in the ArangoDB documentation assume strict mode, so you might
want to familiarize yourself with it if you haven't encountered it before.

The second line imports the `@arangodb/foxx/router` module which provides a
function for creating new Foxx routers. We're using this function to create a
new `router` object which we'll be using for our service.

The `module.context` is the so-called Foxx context or service context.
This variable is available in all files that are part of your Foxx service and
provides access to Foxx APIs specific to the current service, like the `use`
method, which tells Foxx to mount the `router` in this service (and to expose
its routes to HTTP).

Next let's define a route that prints a generic greeting:

```js
// continued
router.get('/hello-world', function (req, res) {
  res.send('Hello World!');
})
.response(['text/plain'], 'A generic greeting.')
.summary('Generic greeting')
.description('Prints a generic greeting.');
```

The `router` provides the methods `get`, `post`, etc corresponding to each
HTTP verb as well as the catch-all `all`. These methods indicate that the given
route should be used to handle incoming requests with the given HTTP verb
(or any method when using `all`).

These methods take an optional path (if omitted, it defaults to `"/"`) as well
as a request handler, which is a function taking the `req`
([request](Reference/Routers/Request.md)) and `res`
([response](Reference/Routers/Response.md)) objects to handle the incoming
request and generate the outgoing response. If you have used the express
framework in Node.js, you may already be familiar with how this works,
otherwise check out the chapter on [routes](Reference/Routers/Endpoints.md).

The object returned by the router's methods provides additional methods to
attach metadata and validation to the route. We're using `summary` and
`description` to document what the route does -- these aren't strictly
necessary but give us some nice auto-generated documentation.
The `response` method lets us additionally document the response content
type and what the response body will represent.

Try it out
----------

At this point you can upload the service folder as a zip archive from the
web interface using the *Services* tab.

<!-- TODO [Screenshot of the Services tab with no services listed] -->

Click *Add Service* then pick the *Zip* option in the dialog. You will need
to provide a *mount path*, which is the URL prefix at which the service will
be mounted (e.g. `/getting-started`).

<!-- TODO [Screenshot of the Add Service dialog with the Zip tab active] -->

Once you have picked the zip archive using the file picker, the upload should
begin immediately and your service should be installed. Otherwise press the
*Install* button and wait for the dialog to disappear and the service to show
up in the service list.

<!-- TODO [Screenshot of the Services tab with the getting-started service listed] -->

Click anywhere on the card with your mount path on the label to open the
service's details.

<!-- TODO [Screenshot of the details for the getting-started service] -->

In the API documentation you should see the route we defined earlier
(`/hello-world`) with the word `GET` next to it indicating the HTTP method it
supports and the `summary` we provided on the right. By clicking on the
route's path you can open the documentation for the route.

<!-- TODO [Screenshot of the API docs with the hello-world route open] -->

Note that the `description` we provided appears in the generated documentation
as well as the description we added to the `response` (which should correctly
indicate the content type `text/plain`, i.e. plain text).

Click the *Try it out!* button to send a request to the route and you should
see an example request with the service's response: "Hello World!".

<!-- TODO [Screenshot of the API docs after the request] -->

Congratulations! You have just created, installed and used your first Foxx service.

Parameter validation
--------------------

Let's add another route that provides a more personalized greeting:

```js
// continued
const joi = require('joi');

router.get('/hello/:name', function (req, res) {
  res.send(`Hello ${req.pathParams.name}`);
})
.pathParam('name', joi.string().required(), 'Name to greet.')
.response(['text/plain'], 'A personalized greeting.')
.summary('Personalized greeting')
.description('Prints a personalized greeting.');
```

The first line imports the [`joi` module from npm](https://www.npmjs.com/package/joi)
which comes bundled with ArangoDB. Joi is a validation library that is used
throughout Foxx to define schemas and parameter types.

**Note**: You can bundle your own modules from npm by installing them in your
service folder and making sure the `node_modules` folder is included in your
zip archive. For more information see the chapter on
[bundling node modules](Guides/BundledNodeModules.md).

The `pathParam` method allows us to specify parameters we are expecting in
the path. The first argument corresponds to the parameter name in the path,
the second argument is a joi schema the parameter is expected to match and
the final argument serves to describe the parameter in the API documentation.

The path parameters are accessible from the `pathParams` property of the
request object. We're using a template string to generate the server's
response containing the parameter's value.

Note that routes with path parameters that fail to validate for the request URL
will be skipped as if they wouldn't exist. This allows you to define multiple
routes that are only distinguished by the schemas of their path parameters (e.g.
a route taking only numeric parameters and one taking any string as a fallback).

<!-- TODO [Screenshot of the API docs after a request to /hello/world] -->

Let's take this further and create a route that takes a JSON request body:

```js
// continued
router.post('/sum', function (req, res) {
  const values = req.body.values;
  res.send({
    result: values.reduce(function (a, b) {
      return a + b;
    }, 0)
  });
})
.body(joi.object({
  values: joi.array().items(joi.number().required()).required()
}).required(), 'Values to add together.')
.response(joi.object({
  result: joi.number().required()
}).required(), 'Sum of the input values.')
.summary('Add up numbers')
.description('Calculates the sum of an array of number values.');
```

Note that we used `post` to define this route instead of `get` (which does not
support request bodies). Trying to send a GET request to this route's URL
(in the absence of a `get` route for the same path) will result in Foxx
responding with an appropriate error response, indicating the supported
HTTP methods.

As this route not only expects a JSON object as input but also responds with
a JSON object as output we need to define two schemas. We don't strictly need
a response schema but it helps documenting what the route should be expected
to respond with and will show up in the API documentation.

Because we're passing a schema to the `response` method we don't need to
explicitly tell Foxx we are sending a JSON response. The presence of a schema
in the absence of a content type always implies we want JSON. Though we could
just add `["application/json"]` as an additional argument after the schema if
we wanted to make this more explicit.

The `body` method works the same way as the `response` method except the schema
will be used to validate the request body. If the request body can't be parsed
as JSON or doesn't match the schema, Foxx will reject the request with an
appropriate error response.

<!-- TODO [Screenshot of the API docs after a request with an array of numbers] -->

Creating collections
--------------------

The real power of Foxx comes from interacting with the database itself.
In order to be able to use a collection from within our service, we should
first make sure that the collection actually exists. The right place to create
collections your service is going to use is in
[a *setup* script](Guides/Scripts.md), which Foxx will execute for you when
installing or updating the service.

First create a new folder called "scripts" in the service folder, which will
be where our scripts are going to live. For simplicity's sake, our setup
script will live in a file called `setup.js` inside that folder:

```js
// continued
'use strict';
const db = require('@arangodb').db;
const collectionName = 'myFoxxCollection';

if (!db._collection(collectionName)) {
  db._createDocumentCollection(collectionName);
}
```

The script uses the [`db` object](../Appendix/References/DBObject.md) from
the `@arangodb` module, which lets us interact with the database the Foxx
service was installed in and the collections inside that database. Because the
script may be executed multiple times (i.e. whenever we update the service or
when the server is restarted) we need to make sure we don't accidentally try
to create the same collection twice (which would result in an exception);
we do that by first checking whether it already exists before creating it.

The `_collection` method looks up a collection by name and returns `null` if no
collection with that name was found. The `_createDocumentCollection` method
creates a new document collection by name (`_createEdgeCollection` also exists
and works analogously for edge collections).

**Note**: Because we have hardcoded the collection name, multiple copies of
the service installed alongside each other in the same database will share
the same collection.
Because this may not always be what you want, the [Foxx context](Reference/Context.md)
also provides the `collectionName` method which applies a mount point specific
prefix to any given collection name to make it unique to the service. It also
provides the `collection` method, which behaves almost exactly like `db._collection`
except it also applies the prefix before looking the collection up.

Next we need to tell our service about the script by adding it to the manifest file:

```json
{
  "engines": {
    "arangodb": "^3.0.0"
  },
  "main": "index.js",
  "scripts": {
    "setup": "scripts/setup.js"
  }
}
```

The only thing that has changed is that we added a "scripts" field specifying
the path of the setup script we just wrote.

Go back to the web interface and update the service with our new code, then
check the *Collections* tab. If everything worked right, you should see a new
collection called "myFoxxCollection".

<!-- TODO [Screenshot of the Collections tab with "myFoxxCollection" in the list] -->

Accessing collections
---------------------

Let's expand our service by adding a few more routes to our `index.js`:

```js
// continued
const db = require('@arangodb').db;
const errors = require('@arangodb').errors;
const foxxColl = db._collection('myFoxxCollection');
const DOC_NOT_FOUND = errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code;

router.post('/entries', function (req, res) {
  const data = req.body;
  const meta = foxxColl.save(req.body);
  res.send(Object.assign(data, meta));
})
.body(joi.object().required(), 'Entry to store in the collection.')
.response(joi.object().required(), 'Entry stored in the collection.')
.summary('Store an entry')
.description('Stores an entry in the "myFoxxCollection" collection.');

router.get('/entries/:key', function (req, res) {
  try {
    const data = foxxColl.document(req.pathParams.key);
    res.send(data)
  } catch (e) {
    if (!e.isArangoError || e.errorNum !== DOC_NOT_FOUND) {
      throw e;
    }
    res.throw(404, 'The entry does not exist', e);
  }
})
.pathParam('key', joi.string().required(), 'Key of the entry.')
.response(joi.object().required(), 'Entry stored in the collection.')
.summary('Retrieve an entry')
.description('Retrieves an entry from the "myFoxxCollection" collection by key.');
```

We're using the `save` and `document` methods of the collection object to store
and retrieve documents in the collection we created in our setup script.
Because we don't care what the documents look like we allow any attributes on
the request body and just accept an object.

Because the key will be automatically generated by ArangoDB when one wasn't
specified in the request body, we're using `Object.assign` to apply the
attributes of the metadata object returned by the `save` method to the document
before returning it from our first route.

The `document` method returns a document in a collection by its `_key` or `_id`.
However when no matching document exists it throws an `ArangoError` exception.
Because we want to provide a more descriptive error message than ArangoDB does
out of the box, we need to handle that error explicitly.

All `ArangoError` exceptions have a truthy attribute `isArangoError` that helps
you recognizing these errors without having to worry about `instanceof` checks.
They also provide an `errorNum` and an `errorMessage`. If you want to check for
specific errors you can just import the `errors` object from the `@arangodb`
module instead of having to memorize numeric error codes.

Instead of defining our own response logic for the error case we just use
`res.throw`, which makes the response object throw an exception Foxx can
recognize and convert to the appropriate server response. We also pass along
the exception itself so Foxx can provide more diagnostic information when we
want it to.

We could extend the post route to support arrays of objects as well, each
object following a certain schema:

```js
// store schema in variable to make it re-usable, see .body()
const docSchema = joi.object().required().keys({
  name: joi.string().required(),
  age: joi.number().required()
}).unknown(); // allow additional attributes

router.post('/entries', function (req, res) {
  const multiple = Array.isArray(req.body);
  const body = multiple ? req.body : [req.body];

  let data = [];
  for (var doc of body) {
    const meta = foxxColl.save(doc);
    data.push(Object.assign(doc, meta));
  }
  res.send(multiple ? data : data[0]);

})
.body(joi.alternatives().try(
  docSchema,
  joi.array().items(docSchema)
), 'Entry or entries to store in the collection.')
.response(joi.alternatives().try(
  joi.object().required(),
  joi.array().items(joi.object().required())
), 'Entry or entries stored in the collection.')
.summary('Store entry or entries')
.description('Store a single entry or multiple entries in the "myFoxxCollection" collection.');
```

Writing database queries
------------------------

Storing and retrieving entries is fine, but right now we have to memorize each
key when we create an entry. Let's add a route that gives us a list of the
keys of all entries so we can use those to look an entry up in detail.

The naïve approach would be to use the `toArray()` method to convert the entire
collection to an array and just return that. But we're only interested in the
keys and there might potentially be so many entries that first retrieving every
single document might get unwieldy. Let's write a short AQL query to do this instead:

```js
// continued
const aql = require('@arangodb').aql;

router.get('/entries', function (req, res) {
  const keys = db._query(aql`
    FOR entry IN ${foxxColl}
    RETURN entry._key
  `);
  res.send(keys);
})
.response(joi.array().items(
  joi.string().required()
).required(), 'List of entry keys.')
.summary('List entry keys')
.description('Assembles a list of keys of entries in the collection.');
```

Here we're using two new things:

The `_query` method executes an AQL query in the active database.

The `aql` template string handler allows us to write multi-line AQL queries and
also handles query parameters and collection names. Instead of hardcoding the
name of the collection we want to use in the query we can simply reference the
`foxxColl` variable we defined earlier – it recognizes the value as an ArangoDB
collection object and knows we are specifying a collection rather than a regular
value even though AQL distinguishes between the two.

**Note**: If you aren't used to JavaScript template strings and template string
handlers just think of `aql` as a function that receives the multiline string
split at every `${}` expression as well as an array of the values of those
expressions – that's actually all there is to it.

Alternatively, here's a version without template strings (notice how much
cleaner the `aql` version will be in comparison when you have multiple variables):

```js
const keys = db._query(
  'FOR entry IN @@coll RETURN entry._key',
  {'@coll': foxxColl.name()}
);
```

Next steps
----------

You now know how to create a Foxx service from scratch, how to handle user
input and how to access the database from within your Foxx service to store,
retrieve and query data you store inside ArangoDB. This should allow you to
build meaningful APIs for your own applications but there are many more things
you can do with Foxx. See the [Guides](Guides/README.md) chapter for more.
