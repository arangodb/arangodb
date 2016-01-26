

@brief partially updates documents matching an example
`collection.updateByExample(example, newValue)`

Partially updates all documents matching an example with a new document body.
Specific attributes in the document body of each document matching the
*example* will be updated with the values from *newValue*.
The document meta-attributes such as *_id*, *_key*, *_from*,
*_to* cannot be updated.

Partial update could also be used to append new fields,
if there were no old field with same name.

`collection.updateByExample(document, newValue, keepNull, waitForSync)`

The optional *keepNull* parameter can be used to modify the behavior when
handling *null* values. Normally, *null* values are stored in the
database. By setting the *keepNull* parameter to *false*, this behavior
can be changed so that all attributes in *data* with *null* values will
be removed from the target document.

The optional *waitForSync* parameter can be used to force synchronization
of the document replacement operation to disk even in case that the
*waitForSync* flag had been disabled for the entire collection.  Thus,
the *waitForSync* parameter can be used to force synchronization of just
specific operations. To use this, set the *waitForSync* parameter to
*true*. If the *waitForSync* parameter is not specified or set to
*false*, then the collection's default *waitForSync* behavior is
applied. The *waitForSync* parameter cannot be used to disable
synchronization for collections that have a default *waitForSync* value
of *true*.

`collection.updateByExample(document, newValue, keepNull, waitForSync, limit)`

The optional *limit* parameter can be used to restrict the number of
updates to the specified value. If *limit* is specified but less than
the number of documents in the collection, it is undefined which documents are
updated.

`collection.updateByExample(document, newValue, options)`

Using this variant, the options for the operation can be passed using
an object with the following sub-attributes:
- *keepNull*
- *waitForSync*
- *limit*
- *mergeObjects*

@EXAMPLES

@EXAMPLE_ARANGOSH_OUTPUT{012_documentsCollectionUpdateByExample}
~ db._create("example");
  db.example.save({ Hello : "world", foo : "bar" });
  db.example.updateByExample({ Hello: "world" }, { Hello: "foo", World: "bar" }, false);
  db.example.byExample({ Hello: "foo" }).toArray()
~ db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

