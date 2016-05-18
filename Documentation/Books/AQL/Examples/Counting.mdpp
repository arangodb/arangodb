!CHAPTER Counting

!SECTION Amount of documents in a collection

To return the count of documents that currently exist in a collection,
you can call the [LENGTH() function](../Functions/Array.md#length):

```
RETURN LENGTH(collection)
```

This type of call is optimized since 2.8 (no unnecessary intermediate result
is built up in memory) and it is therefore the prefered way to determine the count.
Internally, [COLLECTION_COUNT()](../Functions/Miscellaneous.md#collectioncount) is called.

In earlier versions with `COLLECT ... WITH COUNT INTO` available (since 2.4),
you may use the following code instead of *LENGTH()* for better performance:

```
FOR doc IN collection
    COLLECT WITH COUNT INTO length
    RETURN length
```
