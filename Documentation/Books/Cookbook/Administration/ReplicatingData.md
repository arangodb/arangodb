# Replicating data from different databases

## Problem

You have two or more different databases with various data respectively collections in each one of this, but you want your data to be collected at one place.

**Note**: For this solution you need at least Arango 2.0 and you must run the script in every database you want to be collect data from.

## Solution

First of all you have to start a server on endpoint:

```
arangod --server.endpoint tcp://127.0.0.1:8529
```

Now you have to create two collections and name them *data* and *replicationStatus*

```js
db._create("data");
db._create("replicationStatus");
```

Save the following script in a file named *js/common/modules/org/mysync.js*

```js
var internal = require("internal");

// maximum number of changes that we can handle
var maxChanges = 1000;

// URL of central node
var transferUrl = "http://127.0.0.1:8599/_api/import?collection=central&type=auto&createCollection=true&complete=true";

var transferOptions = {
  method: "POST",
  timeout: 60
};

// the collection that keeps the status of what got replicated to central node
var replicationCollection = internal.db.replicationStatus;

// the collection containing all data changes
var changesCollection = internal.db.data;

function keyCompare (l, r) {
  if (l.length != r.length) {
    return l.length - r.length < 0 ? -1 : 1;
  }

  // length is equal
  for (i = 0; i < l.length; ++i) {
    if (l[i] != r[i]) {
      return l[i] < r[i] ? -1 : 1;
    }
  }

  return 0;
};

function logger (msg) {
  "use strict";

  require("console").log("%s", msg);
}
 
function replicate () {
  "use strict";

  var key = "status"; // const

  var status, newStatus;
  try {
    // fetch the previous replication state
    status = replicationCollection.document(key);
    newStatus = { _key: key, lastKey: status.lastKey };
  }
  catch (err) {
    // no previous replication state. start from the beginning
    newStatus = { _key: key, lastKey: "0" };
  }

  // fetch the latest changes (need to reverse them because `last` returns newest changes first)
  var changes = changesCollection.last(maxChanges).reverse(), change;
  var transfer = [ ];
  for (change in changes) {
    if (changes.hasOwnProperty(change)) {
      var doc = changes[change];
      if (keyCompare(doc._key, newStatus.lastKey) <= 0) {
        // already handled in a previous replication run
        continue;
      }

      // documents we need to transfer
      // if necessary, we could rewrite the documents here, e.g. insert
      // extra values, create client-specific keys etc.
      transfer.push(doc);

      if (keyCompare(doc._key, newStatus.lastKey) > 0) {
        // keep track of highest key
        newStatus.lastKey = doc._key;
      }
    }
  }

  if (transfer.length === 0) {
    // nothing to do
    logger("nothing to transfer");
    return;
  }

  logger("transferring " + transfer.length + " document(s)");

  // now transfer the documents to the remote server
  var result = internal.download(transferUrl, JSON.stringify(transfer), transferOptions);

  if (result.code >= 200 && result.code <= 202) {
    logger("central server accepted the documents: " + JSON.stringify(result));
  }
  else {
    // error
    logger("central server did not accept the documents: " + JSON.stringify(result));
    throw "replication error";
  }

  // update the replication state
  if (status) {
    // need to update the previous replication state
    replicationCollection.update(key, newStatus);
  }
  else {
    // need to insert the replication state (1st time)
    replicationCollection.save(newStatus);
  }
 
  logger("deleting old documents");

  // finally remove all elements that we transferred successfully from the changes collection
  // no need to keep them
  transfer.forEach(function (k) {
    changesCollection.remove(k);
  });
}

exports.execute = function (param) {
  "use strict";

  logger("replication wake up");
  replicate();
  logger("replication shutdown");
};
```

Afterwards change the URL of the central node in the script to the one you chosen before - e.g. *tcp://127.0.0.1:8599*

Now register the script as a recurring action:

```js
require("internal").definePeriodic(1, 10, "org/arangodb/mysync", "execute", "");
```

**Note**: At this point you can change the time the script will be executed.

## Comment

The server started on endpoint will be the central node. It collects changes from the local node by replicating its data.
The script will pick up everything that has been changed since the last alteration in your *data* collection.
Every 10 seconds - or the time you chosen - the script will be executed and send the changed data to the central 
node where it will be imported into a collection named *central*.
After that the transferred data will be removed from the *data* collection.  

If you want to test your script simply add some data to your *data* collection - e.g.: 

```js
for (i = 0; i < 100; ++i) db.data.save({ value: i });
```

**Author:** [Jan Steemann](https://github.com/jsteemann)

**Tags:** #database #collection