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
