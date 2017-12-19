Database Methods
================

### Document
<!-- arangod/V8Server/v8-vocbase.cpp -->


`db._document(object)`

The *_document* method finds a document given an object *object*
containing the *_id* attribute. The method returns
the document if it can be found.

An error is thrown if *_rev* is specified but the document found has a
different revision already. An error is also thrown if no document exists
with the given *_id*.

Please note that if the method is executed on the arangod server (e.g. from
inside a Foxx application), an immutable document object will be returned
for performance reasons. It is not possible to change attributes of this
immutable object. To update or patch the returned document, it needs to be
cloned/copied into a regular JavaScript object first. This is not necessary
if the *_document* method is called from out of arangosh or from any other
client.

`db._document(document-handle)`

As before. Instead of *object* a *document-handle* can be passed as
first argument. No revision can be specified in this case.


**Examples**


Returns the document:

    @startDocuBlockInline documentsDocumentName
    @EXAMPLE_ARANGOSH_OUTPUT{documentsDocumentName}
    ~ db._create("example");
    ~ var myid = db.example.insert({_key: "12345"});
      db._document("example/12345");
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock documentsDocumentName



### Exists
<!-- arangod/V8Server/v8-vocbase.cpp -->


`db._exists(object)`

The *_exists* method determines whether a document exists given an object
`object` containing the *_id* attribute.

An error is thrown if *_rev* is specified but the document found has a
different revision already.

Instead of returning the found document or an error, this method will
only return an object with the attributes *_id*, *_key* and *_rev*, or
*false* if no document with the given *_id* or *_key* exists. It can
thus be used for easy existence checks.

This method will throw an error if used improperly, e.g. when called
with a non-document handle, a non-document, or when a cross-collection
request is performed.

`db._exists(document-handle)`

As before. Instead of *object* a *document-handle* can be passed as
first argument.

#### Changes in 3.0 from 2.8:

In the case of a revision mismatch *_exists* now throws an error instead
of simply returning *false*. This is to make it possible to tell the
difference between a revision mismatch and a non-existing document.


### Replace
<!-- arangod/V8Server/v8-vocbase.cpp -->


`db._replace(selector, data)`

Replaces an existing document described by the *selector*, which must
be an object containing the *_id* attribute. There must be
a document with that *_id* in the current database. This
document is then replaced with the *data* given as second argument.
Any attribute *_id*, *_key* or *_rev* in *data* is ignored.

The method returns a document with the attributes *_id*, *_key*, *_rev*
and *_oldRev*. The attribute *_id* contains the document handle of the
updated document, the attribute *_rev* contains the document revision of
the updated document, the attribute *_oldRev* contains the revision of
the old (now replaced) document.

If the selector contains a *_rev* attribute, the method first checks
that the specified revision is the current revision of that document.
If not, there is a conflict, and an error is thrown.

`collection.replace(selector, data, options)`

As before, but *options* must be an object that can contain the following 
boolean attributes:

  - *waitForSync*: One can force
    synchronization of the document creation operation to disk even in
    case that the *waitForSync* flag is been disabled for the entire
    collection. Thus, the *waitForSync* option can be used to force
    synchronization of just specific operations. To use this, set the
    *waitForSync* parameter to *true*. If the *waitForSync* parameter
    is not specified or set to *false*, then the collection's default
    *waitForSync* behavior is applied. The *waitForSync* parameter
    cannot be used to disable synchronization for collections that have
    a default *waitForSync* value of *true*.
  - *overwrite*: If this flag is set to *true*, a *_rev* attribute in
    the selector is ignored.
  - *returnNew*: If this flag is set to *true*, the complete new document
    is returned in the output under the attribute *new*.
  - *returnOld*: If this flag is set to *true*, the complete previous
    revision of the document is returned in the output under the 
    attribute *old*.
  - *silent*: If this flag is set to *true*, no output is returned.

`db._replace(document-handle, data)`

`db._replace(document-handle, data, options)`

As before. Instead of *selector* a *document-handle* can be passed as
first argument. No revision precondition is tested.


**Examples**


Create and replace a document:

    @startDocuBlockInline documentsDocumentReplace
    @EXAMPLE_ARANGOSH_OUTPUT{documentsDocumentReplace}
    ~ db._create("example");
      a1 = db.example.insert({ a : 1 });
      a2 = db._replace(a1, { a : 2 });
      a3 = db._replace(a1, { a : 3 });  // xpError(ERROR_ARANGO_CONFLICT);
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock documentsDocumentReplace


#### Changes in 3.0 from 2.8:

The options *silent*, *returnNew* and *returnOld* are new.


### Update
<!-- arangod/V8Server/v8-vocbase.cpp -->

`db._update(selector, data)`

Updates an existing document described by the *selector*, which must
be an object containing the *_id* attribute. There must be
a document with that *_id* in the current database. This
document is then patched with the *data* given as second argument.
Any attribute *_id*, *_key* or *_rev* in *data* is ignored.

The method returns a document with the attributes *_id*, *_key*, *_rev*
and *_oldRev*. The attribute *_id* contains the document handle of the
updated document, the attribute *_rev* contains the document revision of
the updated document, the attribute *_oldRev* contains the revision of
the old (now updated) document.

If the selector contains a *_rev* attribute, the method first checks
that the specified revision is the current revision of that document.
If not, there is a conflict, and an error is thrown.

`db._update(selector, data, options)`

As before, but *options* must be an object that can contain the following 
boolean attributes:

  - *waitForSync*: One can force
    synchronization of the document creation operation to disk even in
    case that the *waitForSync* flag is been disabled for the entire
    collection. Thus, the *waitForSync* option can be used to force
    synchronization of just specific operations. To use this, set the
    *waitForSync* parameter to *true*. If the *waitForSync* parameter
    is not specified or set to *false*, then the collection's default
    *waitForSync* behavior is applied. The *waitForSync* parameter
    cannot be used to disable synchronization for collections that have
    a default *waitForSync* value of *true*.
  - *overwrite*: If this flag is set to *true*, a *_rev* attribute in
    the selector is ignored.
  - *returnNew*: If this flag is set to *true*, the complete new document
    is returned in the output under the attribute *new*.
  - *returnOld*: If this flag is set to *true*, the complete previous
    revision of the document is returned in the output under the 
    attribute *old*.
  - *silent*: If this flag is set to *true*, no output is returned.
  - *keepNull*: The optional *keepNull* parameter can be used to modify 
    the behavior when handling *null* values. Normally, *null* values
    are stored in the database. By setting the *keepNull* parameter to
    *false*, this behavior can be changed so that all attributes in
    *data* with *null* values will be removed from the target document.
  - *mergeObjects*: Controls whether objects (not arrays) will be 
    merged if present in both the existing and the patch document. If
    set to *false*, the value in the patch document will overwrite the
    existing document's value. If set to *true*, objects will be merged.
    The default is *true*.


`db._update(document-handle, data)`

`db._update(document-handle, data, options)`

As before. Instead of *selector* a *document-handle* can be passed as
first argument. No revision precondition is tested.


**Examples**


Create and update a document:

    @startDocuBlockInline documentDocumentUpdate
    @EXAMPLE_ARANGOSH_OUTPUT{documentDocumentUpdate}
    ~ db._create("example");
      a1 = db.example.insert({ a : 1 });
      a2 = db._update(a1, { b : 2 });
      a3 = db._update(a1, { c : 3 }); // xpError(ERROR_ARANGO_CONFLICT);
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock documentDocumentUpdate


#### Changes in 3.0 from 2.8:

The options *silent*, *returnNew* and *returnOld* are new.


### Remove
<!-- arangod/V8Server/v8-vocbase.cpp -->


`db._remove(selector)`

Removes a document described by the *selector*, which must be an object
containing the *_id* attribute. There must be a document with
that *_id* in the current database. This document is then
removed. 

The method returns a document with the attributes *_id*, *_key* and *_rev*.
The attribute *_id* contains the document handle of the
removed document, the attribute *_rev* contains the document revision of
the removed eocument.

If the selector contains a *_rev* attribute, the method first checks
that the specified revision is the current revision of that document.
If not, there is a conflict, and an error is thrown.

`db._remove(selector, options)`

As before, but *options* must be an object that can contain the following 
boolean attributes:

  - *waitForSync*: One can force
    synchronization of the document creation operation to disk even in
    case that the *waitForSync* flag is been disabled for the entire
    collection. Thus, the *waitForSync* option can be used to force
    synchronization of just specific operations. To use this, set the
    *waitForSync* parameter to *true*. If the *waitForSync* parameter
    is not specified or set to *false*, then the collection's default
    *waitForSync* behavior is applied. The *waitForSync* parameter
    cannot be used to disable synchronization for collections that have
    a default *waitForSync* value of *true*.
  - *overwrite*: If this flag is set to *true*, a *_rev* attribute in
    the selector is ignored.
  - *returnOld*: If this flag is set to *true*, the complete previous
    revision of the document is returned in the output under the 
    attribute *old*.
  - *silent*: If this flag is set to *true*, no output is returned.

`db._remove(document-handle)`

`db._remove(document-handle, options)`

As before. Instead of *selector* a *document-handle* can be passed as
first argument. No revision check is performed.

**Examples**


Remove a document:

    @startDocuBlockInline documentsCollectionRemoveSuccess
    @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionRemoveSuccess}
    ~ db._create("example");
      a1 = db.example.insert({ a : 1 });
      db._remove(a1);
      db._remove(a1);  // xpError(ERROR_ARANGO_DOCUMENT_NOT_FOUND);
      db._remove(a1, {overwrite: true}); // xpError(ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock documentsCollectionRemoveSuccess

Remove the document in the revision `a1` with a conflict:

    @startDocuBlockInline documentsCollectionRemoveConflict
    @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionRemoveConflict}
    ~ db._create("example");
      a1 = db.example.insert({ a : 1 });
      a2 = db._replace(a1, { a : 2 });
      db._remove(a1);       // xpError(ERROR_ARANGO_CONFLICT)
      db._remove(a1, {overwrite: true} );
      db._document(a1);     // xpError(ERROR_ARANGO_DOCUMENT_NOT_FOUND)
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock documentsCollectionRemoveConflict

Remove a document using new signature:

    @startDocuBlockInline documentsCollectionRemoveSignature
    @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionRemoveSignature}
    ~ db._create("example");
    db.example.insert({ _key: "11265325374", a:  1 } );
    | db.example.remove("example/11265325374",
           { overwrite: true, waitForSync: false})
    ~ db._drop("example");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock documentsCollectionRemoveSignature

#### Changes in 3.0 from 2.8:

The method now returns not only *true* but information about the removed
document(s). The options *silent* and *returnOld* are new.
