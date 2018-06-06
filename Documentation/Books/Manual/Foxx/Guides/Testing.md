Writing tests
=============

If you have never written a test in a Foxx service before, we recommend you to take a look at the chapter [writing tests](../Reference/Testing.md) in our reference section.

This chapter deals with the two approaches of testing your Foxx service. One is most likely to fall into the category of unit testing. The second describes the easiest way to write an integration test for your Foxx service.

In both cases we take our minimal example from our [Getting Started](../GettingStarted.md) chapter.

```js
'use strict';
const createRouter = require('@arangodb/foxx/router');
const router = createRouter();

module.context.use(router);

router.get('/hello-world', function (req, res) {
  res.send('Hello World!');
})
.response(['text/plain'], 'A generic greeting.')
.summary('Generic greeting')
.description('Prints a generic greeting.');
```

Unit test
---------

In order to test the pure logic of the routes in our Foxx service, we recommend that you simply outsource these to separate modules and test them. In our example, this means that we extract out our `"Hello World!"` output from our router and write it to a file (e.g. `util/hello.js`) and then simply call it in our router.

Module `util/hello.js` with our business logic:

```js
"use strict";
module.exports = () => "Hello World!";
```

Our router only calls our new module and offers no further logi, with the exception of the handling of `req` and `res`:

```js
const hello = require("./util/hello");

router.get('/hello-world', function (req, res) {
  res.send(hello());
})
```

Now you can simply execute and test the newly created module in your test. We create a file (e.g. `test/hello.js`) with the following content

```js
"use strict";
const expect = require("chai").expect;
const request = require("@arangodb/request");
const hello = require("../util/hello");

describe("Hello World module", () => {
  it('should return "Hello World!"', () => {
    expect(hello()).to.equal("Hello World!");
  });
});
```

The last thing we have to do to get the test up and running is to extend our `manifest.json` with the property `"tests": "test/**/*.js"`.

Integration test
----------------

In case a simple unit test is not enough, or you still have enough logic in your routers which should be tested, you have the possibility to call your service within your tests directly via HTTP. For this we use the [request module](../../Appendix/JavaScriptModules/Request.md), which is provided by ArangoDB. With `module.context.baseUrl` we get the URL to our installed service. You only have to concat the path to the router you want to test.

```js
"use strict";
const expect = require("chai").expect;
const request = require("@arangodb/request");

describe("Hello World service", () => {
  it('should return "Hello World!"', () => {
    const res = request.get(`${module.context.baseUrl}/hello-world`);
    expect(res.body).to.equal("Hello World!");
  });
});
```

<!--
Writing tests
=============

Foxx provides out of the box support for running tests against an installed service
using the [Mocha](https://mochajs.org) test runner.

Test files have full access to the [service context](./Context.md) and all ArangoDB APIs
but like scripts can not define Foxx routes.

Running tests
-------------

An installed service's tests can be executed from the administrative web interface:

1. Open the "Services" tab of the web interface
2. Click on the installed service to be tested
3. Click on the "Settings" tab
4. Click on the flask icon in the top right
5. Accept the confirmation dialog

Note that running tests in a production database is not recommended
and may result in data loss if the tests access the database.

When running a service in development mode special care needs to be taken as
performing requests to the service's own routes as part of the test suites
may result in tests being executed while the database is in an inconsistent state,
leading to unexpected behaviour.

Test file paths
---------------

In order to tell Foxx about files containing test suites, one or more patterns need to be specified in the `tests` option of the [service manifest](./Manifest.md):

```json
{
  "tests": [
    "**/test_*.js",
    "**/*_test.js"
  ]
}
```

These patterns can be either relative file paths or "globstar" patterns where

* `*` matches zero or more characters in a filename
* `**` matches zero or more nested directories

For example, given the following directory structure:

```
++ test/
|++ a/
||+- a1.js
||+- a2.js
||+- test.js
|+- b.js
|+- c.js
|+- d_test.js
+- e_test.js
+- test.js
```

The following patterns would match the following files:

```
test.js:
  test.js

test/*.js:
  /test/b.js
  /test/c.js
  /test/d_test.js

test/**/*.js:
  /test/a/a1.js
  /test/a/a2.js
  /test/a/test.js
  /test/b.js
  /test/c.js
  /test/d_test.js

**/test.js:
  /test/a/test.js

**/*test.js:
  /test/a/test.js
  /test/d_test.js
  /e_test.js
  /test.js
```

Even if multiple patterns match the same file the tests in that file will only be run once.

The order of tests is always determined by the file paths,
not the order in which they are matched or specified in the manifest.

Test structure
--------------

Mocha test suites can be defined using one of three interfaces: BDD, TDD or Exports.

The QUnit interface of Mocha is not supported in ArangoDB.

Like all ArangoDB code, test code is always synchronous.

### BDD interface

The BDD interface defines test suites using the `describe` function
and each test case is defined using the `it` function:

```js
'use strict';
const assert = require('assert');
const trueThing = true;

describe('True things', () => {
  it('are true', () => {
    assert.equal(trueThing, true);
  });
});
```

The BDD interface also offers the alias `context` for `describe` and `specify` for `it`.

Test fixtures can be handled using `before` and `after` for suite-wide fixtures
and `beforeEach` and `afterEach` for per-test fixtures:

```js
describe('False things', () => {
  let falseThing;
  before(() => {
    falseThing = !true;
  });
  it('are false', () => {
    assert.equal(falseThing, false);
  });
});
```

### TDD interface

The TDD interface defines test suites using the `suite` function
and each test case is defined using the `test` function:

```js
'use strict';
const assert = require('assert');
const trueThing = true;

suite('True things', () => {
  test('are true', () => {
    assert.equal(trueThing, true);
  });
});
```

Test fixtures can be handled using `suiteSetup` and `suiteTeardown` for suite-wide fixtures
and `setup` and `teardown` for per-test fixtures:

```js
suite('False things', () => {
  let falseThing;
  suiteSetup(() => {
    falseThing = !true;
  });
  test('are false', () => {
    assert.equal(falseThing, false);
  });
});
```

### Exports interface

The Exports interface defines test cases as methods of plain object properties of the `module.exports` object:

```js
'use strict';
const assert = require('assert');
const trueThing = true;

exports['True things'] = {
  'are true': function() {
    assert.equal(trueThing, true);
  }
};
```

The keys `before`, `after`, `beforeEach` and `afterEach` are special-cased
and behave like the corresponding functions in the BDD interface:

```js
let falseThing;
exports['False things'] = {
  before () {
    falseThing = false;
  },
  'are false': function() {
    assert.equal(falseThing, false);
  }
};
```

Assertions
----------

ArangoDB provides two bundled modules to define assertions:

* `assert` corresponds to the [Node.js `assert` module](http://nodejs.org/api/assert.html),
  providing low-level assertions that can optionally specify an error message.

* `chai` is the popular [Chai Assertion Library](http://chaijs.com),
  providing both BDD and TDD style assertions using a familiar syntax.

-->

<!--
# Testing

Extract common code into modules and write unit tests:
```
api/
  index.js
  ...
queries/
  getUserEmails.js
  ...
util/
  sluggify.js
  formatDate.js
  auth.js
test/
  queries/
    getUserEmails.js
    ...
  util/
    sluggify.js
    ...
```

## Integration tests

Use `request` module to test JSON API with `module.context.baseUrl`.

Make sure to disable development mode to avoid instability. Also make sure you didn't accidentally lower the amount of V8 contexts so Foxx won't lock up trying to talk to itself.

-->