Testing Foxx services
=====================

Foxx provides out of the box support for running tests against an
installed service using an API similar to
the [Mocha test runner](https://mochajs.org).

Test files have full access to the [service context](../Reference/Context.md)
and all ArangoDB APIs but can not define Foxx routes.

Test files can be specified in the [service manifest](../Reference/Manifest.md)
using either explicit paths of each individual file or patterns that can
match multiple files (even if multiple patterns match the same file,
it will only be executed once):


```json
{
  "tests": [
    "some-specific-test-file.js",
    "test/**/*.js",
    "**/*.spec.js",
    "**/__tests__/**/*.js"
  ]
}
```

To run a service's tests you can use
the [web interface](../../Programs/WebInterface/Services.md),
the [Foxx CLI](../../Programs/FoxxCLI/README.md) or
the [Foxx HTTP API](../../../HTTP/Foxx/Miscellaneous.html).
Foxx will execute all test cases in the matching files and
generate a report in the desired format.

{% hint 'danger' %}
Running tests in a production environment is not recommended and
may result in data loss if the tests involve database access.
{% endhint %}

Writing tests
-------------

ArangoDB bundles the [`chai` library](http://www.chaijs.com),
which can be used to define test assertions:

```js
"use strict";
const { expect } = require("chai");

// later
expect("test".length).to.equal(4);
```

Alternatively ArangoDB also provides an implementation of
[Node's `assert` module](https://nodejs.org/api/assert.html):

```js
"use strict";
const assert = require("assert");

// later
assert.equal("test".length, 4);
```

Test cases can be defined in any of the following ways using helper functions
injected by Foxx when executing the test file:

### Functional style

Test cases are defined using the `it` function and can be grouped in
test suites using the `describe` function. Test suites can use the
`before` and `after` functions to prepare and cleanup the suite and
the `beforeEach` and `afterEach` functions to prepare and cleanup
each test case individually.

The `it` function also has the aliases `test` and `specify`.

The `describe` function also has the aliases `suite` and `context`.

The `before` and `after` functions also have
the aliases `suiteSetup` and `suiteTeardown`.

The `beforeEach` and `afterEach` functions also have
the aliases `setup` and `teardown`.

**Note**: These functions are automatically injected into the test file and
don't have to be imported explicitly. The aliases can be used interchangeably.

```js
"use strict";
const { expect } = require("chai");

test("a single test case", () => {
  expect("test".length).to.equal(4);
});

describe("a test suite", () => {
  before(() => {
    // This runs before the suite's first test case
  });
  after(() => {
    // This runs after the suite's last test case
  });
  beforeEach(() => {
    // This runs before each test case of the suite
  });
  afterEach(() => {
    // This runs after each test case of the suite
  });
  it("is a test case in the suite", () => {
    expect(4).to.be.greaterThan(3);
  });
  it("is another test case in the suite", () => {
    expect(4).to.be.lessThan(5);
  });
});

suite("another test suite", () => {
  test("another test case", () => {
    expect(4).to.be.a("number");
  });
});

context("yet another suite", () => {
  specify("yet another case", () => {
    expect(4).to.not.equal(5);
  });
});
```

### Exports style

Test cases are defined as methods of plain objects assigned to test suite
properties on the `exports` object:

```js
"use strict";
const { expect } = require("chai");

exports["this is a test suite"] = {
  "this is a test case": () => {
    expect("test".length).to.equal(4);
  }
};
```

Methods named `before`, `after`, `beforeEach` and `afterEach` behave similarly
to the corresponding functions in the functional style described above:

```js
exports["a test suite"] = {
  before: () => {
    // This runs before the suite's first test case
  },
  after: () => {
    // This runs after the suite's last test case
  },
  beforeEach: () => {
    // This runs before each test case of the suite
  },
  afterEach: () => {
    // This runs after each test case of the suite
  },
  "a test case in the suite": () => {
    expect(4).to.be.greaterThan(3);
  },
  "another test case in the suite": () => {
    expect(4).to.be.lessThan(5);
  }
};
```

Unit testing
------------

The easiest way to make your Foxx service unit-testable is to extract
critical logic into side-effect-free functions and move these functions into
modules your tests (and router) can require:

```js
// in your router
const lookupUser = require("../util/users/lookup");
const verifyCredentials = require("../util/users/verify");
const users = module.context.collection("users");

router.post("/login", function (req, res) {
  const { username, password } = req.body;
  const user = lookupUser(username, users);
  verifyCredentials(user, password);
  req.session.uid = user._id;
  res.json({ success: true });
});

// in your tests
const verifyCredentials = require("../util/users/verify");
describe("verifyCredentials", () => {
  it("should throw when credentials are invalid", () => {
    expect(() => verifyCredentials(
      { authData: "whatever" },
      "invalid password"
    )).to.throw()
  });
})
```

Integration testing
-------------------

{% hint 'warning' %}
You should avoid running integration tests while a service
is mounted in [development mode](DevelopmentMode.md) as each request
will cause the service to be reloaded.
{% endhint %}

You can [use the `@arangodb/request` module](MakingRequests.md)
to let tests talk to routes of the same service.

When the request module is used with a path instead of a full URL,
the path is resolved as relative to the ArangoDB instance.
Using the `baseUrl` property of the [service context](../Reference/Context.md)
we can use this to make requests to the service itself:

```js
"use strict";
const { expect } = require("chai");
const request = require("@arangodb/request");
const { baseUrl } = module.context;

describe("this service", () => {
  it("should say 'Hello World!' at the index route", () => {
    const response = request.get(baseUrl);
    expect(response.status).to.equal(200);
    expect(response.body).to.equal("Hello World!");
  });
  it("should greet us with name", () => {
    const response = request.get(`${baseUrl}/Steve`);
    expect(response.status).to.equal(200);
    expect(response.body).to.equal("Hello Steve!");
  });
});
```

An implementation passing the above tests could look like this:

```js
"use strict";
const createRouter = require("@arangodb/foxx/router");
const router = createRouter();
module.context.use(router);

router.get((req, res) => {
  res.write("Hello World!");
})
.response(["text/plain"]);

router.get("/:name", (req, res) => {
  res.write(`Hello ${req.pathParams.name}!`);
})
.response(["text/plain"]);
```
