<!-- don't edit here, it's from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# ArangoDB JavaScript Driver - Getting Started

## Compatibility

ArangoJS is compatible with the latest stable version of ArangoDB available at
the time of the driver release.

The [_arangoVersion_ option](../Reference/Database/README.md)
can be used to tell arangojs to target a specific
ArangoDB version. Depending on the version this will enable or disable certain
methods and change behavior to maintain compatibility with the given version.
The oldest version of ArangoDB supported by arangojs when using this option
is 2.8.0 (using `arangoVersion: 20800`).

The yarn/npm distribution of ArangoJS is compatible with Node.js versions 9.x
(latest), 8.x (LTS) and 6.x (LTS). Node.js version support follows
[the official Node.js long-term support schedule](https://github.com/nodejs/LTS).

The included browser build is compatible with Internet Explorer 11 and recent
versions of all modern browsers (Edge, Chrome, Firefox and Safari).

Versions outside this range may be compatible but are not actively supported.

**Note**: Starting with arangojs 6.0.0, all asynchronous functions return
promises. If you are using a version of Node.js older than Node.js 6.x LTS
("Boron") make sure you replace the native `Promise` implementation with a
substitute like [bluebird](https://github.com/petkaantonov/bluebird)
to avoid a known memory leak in older versions of the V8 JavaScript engine.

## Versions

The version number of this driver does not indicate supported ArangoDB versions!

This driver uses semantic versioning:

- A change in the bugfix version (e.g. X.Y.0 -> X.Y.1) indicates internal
  changes and should always be safe to upgrade.
- A change in the minor version (e.g. X.1.Z -> X.2.0) indicates additions and
  backwards-compatible changes that should not affect your code.
- A change in the major version (e.g. 1.Y.Z -> 2.0.0) indicates _breaking_
  changes that require changes in your code to upgrade.

If you are getting weird errors or functions seem to be missing, make sure you
are using the latest version of the driver and following documentation written
for a compatible version. If you are following a tutorial written for an older
version of arangojs, you can install that version using the `<name>@<version>`
syntax:

```sh
# for version 4.x.x
yarn add arangojs@4
# - or -
npm install --save arangojs@4
```

You can find the documentation for each version by clicking on the corresponding
date on the left in
[the list of version tags](https://github.com/arangodb/arangojs/tags).

## Install

### With Yarn or NPM

```sh
yarn add arangojs
# - or -
npm install --save arangojs
```

### With Bower

Starting with arangojs 6.0.0 Bower is no longer supported and the browser
build is now included in the NPM release (see below).

### From source

```sh
git clone https://github.com/arangodb/arangojs.git
cd arangojs
npm install
npm run dist
```

### For browsers

For production use arangojs can be installed with Yarn or NPM like any
other dependency. Just use arangojs like you would in your server code:

```js
import { Database } from "arangojs";
// -- or --
var arangojs = require("arangojs");
```

Additionally the NPM release comes with a precompiled browser build:

```js
var arangojs = require("arangojs/lib/web");
```

You can also use [unpkg](https://unpkg.com) during development:

```html
< !-- note the path includes the version number (e.g. 6.0.0) -- >
<script src="https://unpkg.com/arangojs@6.0.0/lib/web.js"></script>
<script>
var db = new arangojs.Database();
db.listCollections().then(function (collections) {
  alert("Your collections: " + collections.map(function (collection) {
    return collection.name;
  }).join(", "));
});
</script>
```

If you are targetting browsers older than Internet Explorer 11 you may want to
use [babel](https://babeljs.io) with a
[polyfill](https://babeljs.io/docs/usage/polyfill) to provide missing
functionality needed to use arangojs.

When loading the browser build with a script tag make sure to load the polyfill first:

```html
<script src="https://cdnjs.cloudflare.com/ajax/libs/babel-polyfill/6.26.0/polyfill.js"></script>
<script src="https://unpkg.com/arangojs@6.0.0/lib/web.js"></script>
```

## Basic usage example

```js
// Modern JavaScript
import { Database, aql } from "arangojs";
const db = new Database();
(async function() {
  const now = Date.now();
  try {
    const cursor = await db.query(aql`
      RETURN ${now}
    `);
    const result = await cursor.next();
    // ...
  } catch (err) {
    // ...
  }
})();

// or plain old Node-style
var arangojs = require("arangojs");
var db = new arangojs.Database();
var now = Date.now();
db.query({
  query: "RETURN @value",
  bindVars: { value: now }
})
  .then(function(cursor) {
    return cursor.next().then(function(result) {
      // ...
    });
  })
  .catch(function(err) {
    // ...
  });

// Using different databases
const db = new Database({
  url: "http://localhost:8529"
});
db.useDatabase("pancakes");
db.useBasicAuth("root", "");
// The database can be swapped at any time
db.useDatabase("waffles");
db.useBasicAuth("admin", "maplesyrup");

// Using ArangoDB behind a reverse proxy
const db = new Database({
  url: "http://myproxy.local:8000",
  isAbsolute: true // don't automatically append database path to URL
});

// Trigger ArangoDB 2.8 compatibility mode
const db = new Database({
  arangoVersion: 20800
});
```

For AQL please check out the [aql template tag](../Reference/Database/Queries.md#aql) for writing parametrized
AQL queries without making your code vulnerable to injection attacks.

## Error responses

If arangojs encounters an API error, it will throw an _ArangoError_ with an
[_errorNum_ error code](../../..//Manual/Appendix/ErrorCodes.html)
as well as a _code_ and _statusCode_ property indicating the intended and
actual HTTP status code of the response.

For any other error responses (4xx/5xx status code), it will throw an
_HttpError_ error with the status code indicated by the _code_ and _statusCode_ properties.

If the server response did not indicate an error but the response body could
not be parsed, a _SyntaxError_ may be thrown instead.

In all of these cases the error object will additionally have a _response_
property containing the server response object.

If the request failed at a network level or the connection was closed without
receiving a response, the underlying error will be thrown instead.

**Examples**

```js
// Using async/await
try {
  const info = await db.createDatabase("mydb");
  // database created
} catch (err) {
  console.error(err.stack);
}

// Using promises with arrow functions
db.createDatabase("mydb").then(
  info => {
    // database created
  },
  err => console.error(err.stack)
);
```

{% hint 'tip' %}
The examples in the remainder of this documentation use `async`/`await`
and other modern language features like multi-line strings and template tags.
When developing for an environment without support for these language features,
substitute promises for `await` syntax as in the above example.
{% endhint %}
