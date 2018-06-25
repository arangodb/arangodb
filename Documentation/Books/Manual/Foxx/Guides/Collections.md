Working with collections
========================

Foxx provides the [`module.context.collection`]() method to provide easy access to ArangoDB collections. These collections are also called "prefixed collections" because Foxx will automatically prefix the name based on the mount path of the service.

The prefixes may initially feel unnecessarily verbose but help avoid conflicts between different services with similar collection names or even multiple copies of the same service sharing the same database. Keep in mind that you can also use collection objects when [writing queries](Queries.md), so you don't need to worry about writing out prefixes by hand.

As a rule of thumb you should always use `module.context.collection` to access collections in your service.

Low-level collection access
---------------------------

ArangoDB provides [a low-level API for managing collections](../../DataModeling/Collections/DatabaseMethods.html) via [the `db` object](../../Appendix/References/DBObject.html). These APIs are not very useful for most application logic but are allow you to create and destroy collections in your [lifecycle scripts and migrations](Migrations.md).

Using these methods requires you to work with fully qualified collection names. This means instead of using `module.context.collection` to get a _collection object_ you need to use `module.context.collectionName` to get the prefixed _collection name_ ArangoDB understands:

```js
"use strict";
const { db } = require("@arangodb");
const collectionName = module.context.collectionName("users");

if (!db._collection(collectionName)) {
  db._createDocumentCollection(collectionName);
}
```

Sharing collections
-------------------

The most obvious way to share collections between multiple services is to use an unprefixed collection name and then use the low-level `db._collection` method to access that collection from each service that needs access to it.

The downside of this approach is that it results in an implicit dependency of those services on a single collection as well as creating the potential for subtle problems if a different service uses the same unprefixed collection name in the future.

The cleanest approach is to instead decide on a single service which manages the collection and set up [explicit dependencies](Dependencies.md) between the different services using the collection:

```js
// in the service's main file:
exports.users = module.context.collection("users");

// in a dependent service's code:
const users = module.dependencies.usersService.users;
```

This approach not only makes the dependency on an externally managed collection explicit but also allows having those services still use different collections if necessary by providing multiple copies of the service that provides the shared collection.
