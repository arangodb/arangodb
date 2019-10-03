---
layout: default
description: To return the count of documents that currently exist in a collection,you can call the LENGTH() function
---
Counting
========

Amount of documents in a collection
-----------------------------------

To return the count of documents that currently exist in a collection,
you can call the [LENGTH() function](functions-array.html#length):

```
RETURN LENGTH(collection)
```

This type of call is optimized since 2.8 (no unnecessary intermediate result
is built up in memory) and it is therefore the preferred way to determine the count.
Internally, [COLLECTION_COUNT()](functions-miscellaneous.html#collection_count) is called.

In earlier versions with `COLLECT ... WITH COUNT INTO` available (since 2.4),
you may use the following code instead of *LENGTH()* for better performance:

```
FOR doc IN collection
    COLLECT WITH COUNT INTO length
    RETURN length
```
