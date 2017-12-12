#Crawling Github with Promises

## Problem

The new [ArangoDB Javascript driver][1] no longer imposes any promises implementation.
It follows the standard callback pattern with a callback using `err` and `res`. 

But what if we want to use a promise library - in this case the most popular one [promises][2]?
Lets give it a try and build a **github crawler** with the new Javascript driver and promises. 

## Solution

The following source code can be found on [github][3].

### Pagination with Promises made easy

The github driver has a function to get all followers. However, the result is paginated.
With two helper functions and promises it is straight forward to implement a function to
retrieve all followers of an user.

```js
function extractFollowers (name) {
  'use strict';

  return new Promise(function(resolve, reject) {
    github.user.getFollowers({ user: name }, promoteError(reject, function(res) {
      followPages(resolve, reject, [], res);
    }));
  });
}
```

The `followPages` function simply extends the result with the next page until the last
page is reached.

```js
function followPages (resolve, reject, result, res) {
  'use strict';

  var i;

  for (i = 0;  i < res.length;  ++i) {
    result.push(res[i]);
  }

  if (github.hasNextPage(res)) {
    github.getNextPage(res, promoteError(reject, function(res) {
      followPages(resolve, reject, result, res);
    }));
  }
  else {
    resolve(result);
  }
}
```

The promote error helper is a convenience function to bridge callbacks and promises.

```js
function promoteError (reject, resolve) {
  'use strict';

  return function(err, res) {
    if (err) {
      if (err.hasOwnProperty("message") && /rate limit exceeded/.test(err.message)) {
        rateLimitExceeded = true;
      }

      console.error("caught error: %s", err);
      reject(err);
    }
    else {
      resolve(res);
    }
  };
}
```

I've decided to stick to the sequence `reject` (aka `err`) followed by `resolve` (aka `res`) - like the callbacks.
The `promoteError` can be used for the github callback as well as the ArangoDB driver.

### Queues, Queues, Queues

I've only needed a very simple job queue, so [queue-it][4] is a good choice.
It provides a very simple API for handling job queues:

```
POST /queue/job
POST /queue/worker
DELETE /queue/job/:key
```

The new Javascript driver allows to access arbitrary endpoint.
First install a Foxx implementing the queue microservice in an ArangoDB instance.

```
foxx-manager install queue-it /queue
```

Adding a new job from node.js is now easy

```js
function addJob (data) {
  'use strict';

  return new Promise(function(resolve, reject) {
    db.endpoint("queue").post("job", data, promoteError(reject, resolve));
  });
}
```

## Transaction

I wanted to crawl users and their repos. The relations ("follows", "owns", "is_member", "stars")
is stored in an edge collection. I only add an edge if it is not already there. Therefore I check
inside a transaction, if the edge exists and add it, if it does not.

```js
createRepoDummy(repo.full_name, data).then(function(dummyData) {
  return db.transaction(
    "relations",
    String(function(params) {
      var me = params[0];
      var you = params[1];
      var type = params[2];
      var db = require("org/arangodb").db;

      if (db.relations.firstExample({ _from: me, _to: you, type: type }) === null) {
        db.relations.save(me, you, { type: type });
      }
    }),
    [ meId, "repos/" + data._key, type ],
    function(err) {
      if (err) {
        throw err;
      }

      return handleDummy(dummyData);
    });
})
```

Please note that the action function is executed on the server and not in the nodejs client.
Therefore we need to pass the relevant data as parameters. It is not possible to use the closure variables.

### Riding the Beast

Start an ArangoDB instance (i.e. inside a [docker container][5]) and install the simple queue.

```
foxx-manager install queue-it /queue
```

Start the arangosh and create collections `users`, `repos` and `relations`.

```
arangosh> db._create("users");
arangosh> db.users.ensureHashIndex("name");

arangosh> db._create("repos");
arangosh> db.users.ensureHashIndex("name");

arangosh> db._createEdgeCollection("relations");
```

Now everything is initialized. Fire up nodejs and start crawling:

```
node> var crawler = require("./crawler");
node> crawler.github.authenticate({ type: "basic", username: "username", password: "password" })
node> crawler.addJob({ type:"user", identifier:"username" })
node> crawler.runJobs();
```

## Comment

Please keep in mind that this is just an experiment. There is no good error handling and convenience
functions for setup and start. It is also not optimized for performance. For instance, it would easily
be possible to avoid nodejs / ArangoDB roundtrips using more transactions.

**Sources used in this example:**

*   [ArangoJS][1]
*   [npm promises][2]
*   [ArangoDB Foxx queue-it][4]

The source code of this example is available from Github: <https://github.com/fceller/Foxxmender>

**Author**: [Frank Celler](https://github.com/fceller)

**Tags**: #foxx #javascript #API #nodejs #driver

[1]: https://github.com/arangodb/arangojs
[2]: https://www.npmjs.com/package/promises
[3]: https://github.com/fceller/Foxxmender
[4]: https://github.com/arangodb/queue-it
[5]: ../Cloud/DockerContainer.md
