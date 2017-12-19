!CHAPTER Administrative Scripts in Cluster

If you want to scale the dataset you access via Foxx you have to take sharding into account.
This results in minor changes to your setup and teardown scripts.
These will be executed on all servers you deploy your Foxx to, hence you have to consider that they will be executed multiple times.
Also the setup for collections now requires you to give the amount of shards for the collection (recommended is the amount of servers squared).

!SECTION Setup script

This script has to take into consideration that collections might have been installed by other servers already.
Also you have to give the amount of shards (9 in this example):

```
var console = require("console");
var arangodb = require("org/arangodb");
var db = arangodb.db;

var texts = applicationContext.collectionName("texts");

if (db._collection(texts) === null) {
  // This is the first one running the script
  var collection = db._create(texts, {
    numberOfShards: 9
  });
  collection.save({ text: "entry 1 from collection texts" });
  collection.save({ text: "entry 2 from collection texts" });
  collection.save({ text: "entry 3 from collection texts" });
}
else {
  console.log("collection '%s' already exists. Leaving it untouched.", texts);
}
```

!SECTION Teardown script

Also this script has to take into account that it might be run several times.
You can also omit the teardown execution with passing `teardown:false` options to the uninstall process.

```
unix>foxx-manager uninstall /example teardown=false
```

The teardown script just has to check if the collection is not yet dropped:

```
var arangodb = require("org/arangodb");
var db = arangodb.db;

var texts = applicationContext.collectionName("texts");
var collection = db._collection(texts);

if (collection !== null) {
  // Not yet dropped. Drop it now
  collection.drop();
}
```
