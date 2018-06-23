UPDATE
======

The *UPDATE* keyword can be used to partially update documents in a collection. On a 
single server, updates are executed transactionally in an all-or-nothing fashion. 
For sharded collections, the entire update operation is not transactional.

Each *UPDATE* operation is restricted to a single collection, and the 
[collection name](../../Manual/Appendix/Glossary.html#collection-name) must not be dynamic.
Only a single *UPDATE* statement per collection is allowed per AQL query, and 
it cannot be followed by read operations that access the same collection, by
traversal operations, or AQL functions that can read documents.
The system attributes *_id*, *_key* and *_rev* cannot be updated, *_from* and *_to* can.

The two syntaxes for an update operation are:

```
UPDATE document IN collection options
UPDATE keyExpression WITH document IN collection options
```

*collection* must contain the name of the collection in which the documents should
be updated. *document* must be a document that contains the attributes and values 
to be updated. When using the first syntax, *document* must also contain the *_key*
attribute to identify the document to be updated. 

```js
FOR u IN users
  UPDATE { _key: u._key, name: CONCAT(u.firstName, " ", u.lastName) } IN users
```

The following query is invalid because it does not contain a *_key* attribute and
thus it is not possible to determine the documents to be updated:

```js
FOR u IN users
  UPDATE { name: CONCAT(u.firstName, " ", u.lastName) } IN users
```

When using the second syntax, *keyExpression* provides the document identification.
This can either be a string (which must then contain the document key) or a
document, which must contain a *_key* attribute.

The following queries are equivalent:

```js
FOR u IN users
  UPDATE u._key WITH { name: CONCAT(u.firstName, " ", u.lastName) } IN users

FOR u IN users
  UPDATE { _key: u._key } WITH { name: CONCAT(u.firstName, " ", u.lastName) } IN users

FOR u IN users
  UPDATE u WITH { name: CONCAT(u.firstName, " ", u.lastName) } IN users
```

An update operation may update arbitrary documents which do not need to be identical
to the ones produced by a preceding *FOR* statement:

```js
FOR i IN 1..1000
  UPDATE CONCAT('test', i) WITH { foobar: true } IN users

FOR u IN users
  FILTER u.active == false
  UPDATE u WITH { status: 'inactive' } IN backup
```

### Using the current value of a document attribute

The pseudo-variable `OLD` is not supported inside of `WITH` clauses (it is
available after `UPDATE`). To access the current attribute value, you can
usually refer to a document via the variable of the `FOR` loop, which is used
to iterate over a collection:

```js
FOR doc IN users
  UPDATE doc WITH {
    fullName: CONCAT(doc.firstName, " ", doc.lastName)
  } IN users
```

If there is no loop, because a single document is updated only, then there
might not be a variable like above (`doc`), which would let you refer to the
document which is being updated:

```js
UPDATE "users/john" WITH { ... } IN users
```

To access the current value in this situation, the document has to be retrieved
and stored in a variable first:

```js
LET doc = DOCUMENT("users/john")
UPDATE doc WITH {
  fullName: CONCAT(doc.firstName, " ", doc.lastName)
} IN users
```

An existing attribute can be modified based on its current value this way,
to increment a counter for instance:

```js
UPDATE doc WITH {
  karma: doc.karma + 1
} IN users
```

If the attribute `karma` doesn't exist yet, `doc.karma` is evaluated to *null*.
The expression `null + 1` results in the new attribute `karma` being set to *1*.
If the attribute does exist, then it is increased by *1*.

Arrays can be mutated too of course:

```js
UPDATE doc WITH {
  hobbies: PUSH(doc.hobbies, "swimming")
} IN users
```

If the attribute `hobbies` doesn't exist yet, it is conveniently initialized
as `[ "swimming" ]` and otherwise extended.

### Setting query options

*options* can be used to suppress query errors that may occur when trying to
update non-existing documents or violating unique key constraints:

```js
FOR i IN 1..1000
  UPDATE {
    _key: CONCAT('test', i)
  } WITH {
    foobar: true
  } IN users OPTIONS { ignoreErrors: true }
```

An update operation will only update the attributes specified in *document* and
leave other attributes untouched. Internal attributes (such as *_id*, *_key*, *_rev*,
*_from* and *_to*) cannot be updated and are ignored when specified in *document*.
Updating a document will modify the document's revision number with a server-generated value.

When updating an attribute with a null value, ArangoDB will not remove the attribute 
from the document but store a null value for it. To get rid of attributes in an update
operation, set them to null and provide the *keepNull* option:

```js
FOR u IN users
  UPDATE u WITH {
    foobar: true,
    notNeeded: null
  } IN users OPTIONS { keepNull: false }
```

The above query will remove the *notNeeded* attribute from the documents and update
the *foobar* attribute normally.

There is also the option *mergeObjects* that controls whether object contents will be
merged if an object attribute is present in both the *UPDATE* query and in the 
to-be-updated document.

The following query will set the updated document's *name* attribute to the exact
same value that is specified in the query. This is due to the *mergeObjects* option
being set to *false*:

```js
FOR u IN users
  UPDATE u WITH {
    name: { first: "foo", middle: "b.", last: "baz" }
  } IN users OPTIONS { mergeObjects: false }
```

Contrary, the following query will merge the contents of the *name* attribute in the
original document with the value specified in the query:

```js
FOR u IN users
  UPDATE u WITH {
    name: { first: "foo", middle: "b.", last: "baz" }
  } IN users OPTIONS { mergeObjects: true }
```

Attributes in *name* that are present in the to-be-updated document but not in the
query will now be preserved. Attributes that are present in both will be overwritten
with the values specified in the query.

Note: the default value for *mergeObjects* is *true*, so there is no need to specify it
explicitly.

To make sure data are durable when an update query returns, there is the *waitForSync* 
query option:

```js
FOR u IN users
  UPDATE u WITH {
    foobar: true
  } IN users OPTIONS { waitForSync: true }
```

### Returning the modified documents

The modified documents can also be returned by the query. In this case, the `UPDATE` 
statement needs to be followed a `RETURN` statement (intermediate `LET` statements
are allowed, too). These statements can refer to the pseudo-values `OLD` and `NEW`.
The `OLD` pseudo-value refers to the document revisions before the update, and `NEW` 
refers to document revisions after the update.

Both `OLD` and `NEW` will contain all document attributes, even those not specified 
in the update expression.

```
UPDATE document IN collection options RETURN OLD
UPDATE document IN collection options RETURN NEW
UPDATE keyExpression WITH document IN collection options RETURN OLD
UPDATE keyExpression WITH document IN collection options RETURN NEW
```

Following is an example using a variable named `previous` to capture the original
documents before modification. For each modified document, the document key is returned.

```js
FOR u IN users
  UPDATE u WITH { value: "test" } 
  LET previous = OLD 
  RETURN previous._key
```

The following query uses the `NEW` pseudo-value to return the updated documents,
without some of the system attributes:

```js
FOR u IN users
  UPDATE u WITH { value: "test" } 
  LET updated = NEW 
  RETURN UNSET(updated, "_key", "_id", "_rev")
```

It is also possible to return both `OLD` and `NEW`:

```js
FOR u IN users
  UPDATE u WITH { value: "test" } 
  RETURN { before: OLD, after: NEW }
```
