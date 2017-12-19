Documents, Identifiers, Handles
===============================


This is an introduction to ArangoDB's interface for documents to and how handle
documents from the JavaScript shell *arangosh*. For other languages see the
corresponding language API.

Documents in ArangoDB are JSON objects. These objects can be nested (to any depth) 
and may contain lists. Each document is uniquely identified by its [document handle](../Glossary/README.md#document-handle).

For example:

```js
{ 
  "_id" : "myusers/3456789", 
  "_key" : "3456789", 
  "_rev" : "14253647", 
  "firstName" : "John", 
  "lastName" : "Doe", 
  "address" : { 
    "city" : "Gotham", 
    "street" : "Road To Nowhere 1" 
  }, 
  "hobbies" : [ 
    {name: "swimming", howFavorite: 10},
    {name: "biking", howFavorite: 6},
    {name: "programming", howFavorite: 4}
  ]
}
```
All documents contain special attributes: the document handle in *_id*, the
document's unique key in *_key* and and the ETag aka [document revision](../Glossary/README.md#document-revision) in
*_rev*. The value of the *_key* attribute can be specified by the user when
creating a document.  *_id* and *_key* values are immutable once the document
has been created. The *_rev* value is maintained by ArangoDB autonomously.

A document handle uniquely identifies a document in the database. It is a string and 
consists of the collection's name and the [document key](../Glossary/README.md#document-key)
(_key attribute) separated by /.

As ArangoDB supports MVCC, documents can exist in more than 
one revision. The document revision is the MVCC token used to identify a particular 
revision of a document. It is a string value currently containing an integer number 
and is unique within the list of document revisions for a single document. Document 
revisions can be used to conditionally update, replace or delete documents in the 
database. In order to find a particular revision of a document, you need the document 
handle and the document revision.

ArangoDB currently uses 64bit unsigned integer values to maintain document revisions 
internally. When returning document revisions to clients, ArangoDB will put them 
into a string to ensure the revision id is not clipped by clients that do not support
big integers. Clients should treat the revision id returned by ArangoDB as an opaque 
string when they store or use it locally. This will allow ArangoDB to change the format 
of revision ids later if this should be required. Clients can use revisions ids to 
perform simple equality/non-equality comparisons (e.g. to check whether a document has 
changed or not), but they should not use revision ids to perform greater/less than 
comparisons with them to check if a document revision is older than one another, 
even if this might work for some cases.

**Note**: Revision ids have been returned as integers up to including ArangoDB 1.1

*Document Etag*: The document revision enclosed in double quotes. The revision is 
returned by several HTTP API methods in the Etag HTTP header.

### Coming from a relational background - how do browse vectors translate into document queries?

In traditional SQL you may either fetch all columns of a table row by row using `SELECT * FROM table`,
or select a subset of the columns.
The list of table columns to fetch is commonly called *column list* or *browse vector*:
`SELECT columnA, columnB, columnZ FROM table`

Since documents aren't 2 dimensional, and neither you want only to be able to return 2 dimensional lists, requirements to AQL are a little bit more complex.
The semantics how it does this leans mostly on the syntax used in JavaScript to work with structured data. 

#### Composing the return documents
The AQL `RETURN` statement returns one item per document it is handed. You can Return the whole document, or just parts of it:

The full example:

    RETURN oneDocument
    -> { 
      "_id" : "myusers/3456789", 
      "_key" : "3456789", 
      "_rev" : "14253647", 
      "firstName" : "John", 
      "lastName" : "Doe", 
      "address" : { 
        "city" : "Gotham", 
        "street" : "Road To Nowhere 1" 
      }, 
      "hobbies" : [ 
        {name: "swimming", howFavorite: 10},
        {name: "biking", howFavorite: 6},
        {name: "programming", howFavorite: 4}
      ]
    }

Only the hobbies sub-structure from the example:

    RETURN oneDocument.hobbies
    -> [
        {name: "swimming", howFavorite: 10},
        {name: "biking", howFavorite: 6},
        {name: "programming", howFavorite: 4}
    ]

The hobbies and the address: 

    RETURN { hobbies: oneDocument.hobbies, address: oneDocument.address }
    -> {
    hobbies: [
        {name: "swimming", howFavorite: 10},
        {name: "biking", howFavorite: 6},
        {name: "programming", howFavorite: 4}
    ],
    address: { 
        "city" : "Gotham", 
        "street" : "Road To Nowhere 1" 
      },
    }

The first hoby:

    RETURN oneDocument.hobbies[0].name
    ->"swimming"

A list of the hobbie strings: 

    RETURN { hobbies: oneDocument.hobbies[*].name }
    -> {hobbies: ["swimming", "biking", "porgramming"] }

More complex [array](../Aql/ArrayFunctions.md) and [object manipulations](../Aql/DocumentFunctions.md) can be done using AQL functions
